#include "tui/screens/assemble_screen.hpp"

#include "core/assemble_flow.hpp"
#include "ffmpeg/assembler.hpp"
#include "ffmpeg/types.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <chrono>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

using namespace ftxui;

namespace tui {

ScreenAction run_assemble_screen(core::Project& project,
                                 const std::filesystem::path& video_path) {
    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::GoSession;

    core::AssembleState assemble_state;

    // Command queue: assembly thread → event loop.
    std::mutex cmd_mutex;
    std::vector<std::pair<core::AssembleCmd, std::string>> cmd_queue;

    // jthread for the blocking ffmpeg work (declared before apply_effects so
    // the SpawnAssembly handler can assign to it).
    std::jthread assembly_thread;

    // ------------------------------------------------------------------
    // Effect handler — inline, no separate dispatcher class needed.
    // ------------------------------------------------------------------
    auto apply_effects = [&](const std::vector<core::AssembleEffect>& effects) {
        for (const auto& eff : effects) {
            std::visit([&](const auto& v) {
                using T = std::decay_t<decltype(v)>;

                if constexpr (std::is_same_v<T, core::SpawnAssembly>) {
                    // Collect clips here so the lambda captures are clean.
                    std::vector<ffmpeg::ProcessedClip> clips;
                    for (const auto& e : project.entries()) {
                        std::filesystem::path p;
                        if (!e.processed_take_path.empty()) {
                            std::filesystem::path pp(e.processed_take_path);
                            if (std::filesystem::exists(pp)) p = pp;
                        }
                        if (p.empty() && !e.raw_take_path.empty()) {
                            std::filesystem::path rp(e.raw_take_path);
                            if (std::filesystem::exists(rp)) p = rp;
                        }
                        if (!p.empty())
                            clips.push_back({p, e.start_ms, e.index});
                    }

                    auto voiceover_out = project.output_dir() / "voiceover.wav";
                    auto video_out     = project.output_dir() / "output.mp4";

                    assembly_thread = std::jthread([&, clips, voiceover_out, video_out](std::stop_token) {
                        auto post_cmd = [&](core::AssembleCmd cmd, std::string payload = {}) {
                            {
                                std::lock_guard<std::mutex> lk(cmd_mutex);
                                cmd_queue.emplace_back(cmd, std::move(payload));
                            }
                            screen.PostEvent(Event::Custom);
                        };

                        if (clips.empty()) {
                            post_cmd(core::AssembleCmd::ProgressLine,
                                     "⚠  No takes found — record something first.");
                            post_cmd(core::AssembleCmd::AssemblyFailed, "No takes found.");
                            return;
                        }

                        post_cmd(core::AssembleCmd::ProgressLine,
                                 "Starting assembly (" + std::to_string(clips.size()) + " clips)…");

                        ffmpeg::FfmpegAssembler assembler;
                        auto result = assembler.assemble(
                            clips,
                            /*video_duration_ms=*/0,
                            video_path,
                            voiceover_out,
                            video_out,
                            [&](int cur, int total) {
                                if (cur == 1)
                                    post_cmd(core::AssembleCmd::ProgressLine,
                                             "✓  Voiceover mix complete.");
                                if (cur == total)
                                    post_cmd(core::AssembleCmd::ProgressLine,
                                             "✓  Video mux complete.");
                            }
                        );

                        if (result.success) {
                            post_cmd(core::AssembleCmd::ProgressLine,
                                     "✓  Done → " + video_out.string());
                            post_cmd(core::AssembleCmd::AssemblyDone, video_out.string());
                        } else {
                            post_cmd(core::AssembleCmd::ProgressLine,
                                     "✗  Assembly failed: " + result.error);
                            post_cmd(core::AssembleCmd::AssemblyFailed, result.error);
                        }
                    });

                } else if constexpr (std::is_same_v<T, core::AppendLog>) {
                    // State already updated by assemble_step; just let the
                    // renderer pick it up on the next redraw.
                    (void)v;

                } else if constexpr (std::is_same_v<T, core::SaveProject>) {
                    project.save();

                } else if constexpr (std::is_same_v<T, core::ExitToSession>) {
                    action = ScreenAction::GoSession;
                    screen.ExitLoopClosure()();
                }
            }, eff);
        }
    };

    // ------------------------------------------------------------------
    // Auto-start: dispatch Start immediately on screen entry.
    // ------------------------------------------------------------------
    {
        auto t = core::assemble_step(assemble_state, core::AssembleCmd::Start);
        assemble_state = t.next;
        apply_effects(t.effects);
    }

    // ------------------------------------------------------------------
    // Refresh thread (drives UI redraws while assembling).
    // ------------------------------------------------------------------
    std::jthread refresh_thread([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    // ------------------------------------------------------------------
    // Renderer — reads from render-state snapshot; no direct state access.
    // ------------------------------------------------------------------
    auto renderer = Renderer([&]() -> Element {
        auto rs = core::assemble_render_state(assemble_state);

        Elements lines;
        lines.reserve(rs.log_lines.size() + 1);
        for (const auto& line : rs.log_lines)
            lines.push_back(text("  " + line));
        if (lines.empty())
            lines.push_back(dim(text("  (starting…)")));

        Element footer;
        if (rs.phase_label == "complete") {
            footer = vbox({
                color(Color::Green, text("  " + rs.footer)),
                separator(),
                hbox({text("  [q] back"), filler()}),
            });
        } else if (rs.phase_label == "failed") {
            footer = vbox({
                color(Color::Red, text("  Assembly failed — see log above.")),
                separator(),
                hbox({text("  [q] back"), filler()}),
            });
        } else {
            footer = vbox({
                dim(text("  " + rs.footer)),
                separator(),
                hbox({text("  [q] back"), filler()}),
            });
        }

        return border(vbox({
            hbox({bold(text("  Assemble")), filler()}),
            separator(),
            vbox(lines) | yframe | flex,
            separator(),
            footer,
        }));
    });

    // ------------------------------------------------------------------
    // Event handler — drains the command queue and handles key presses.
    // ------------------------------------------------------------------
    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        // Drain any commands posted by the assembly thread.
        std::vector<std::pair<core::AssembleCmd, std::string>> pending;
        {
            std::lock_guard<std::mutex> lk(cmd_mutex);
            pending.swap(cmd_queue);
        }
        for (auto& [cmd, payload] : pending) {
            auto t = core::assemble_step(assemble_state, cmd, payload);
            assemble_state = t.next;
            apply_effects(t.effects);
        }

        if (!event.is_character()) return false;
        const auto ch = event.character();

        auto cmd_opt = core::parse_assemble_cmd(ch);
        if (!cmd_opt.has_value()) return false;

        // On 'q' during assembly, request stop on the thread (best-effort).
        if (cmd_opt.value() == core::AssembleCmd::Back &&
            assemble_state.phase == core::AssemblePhase::Assembling) {
            assembly_thread.request_stop();
        }

        auto t = core::assemble_step(assemble_state, cmd_opt.value());
        assemble_state = t.next;
        apply_effects(t.effects);
        return true;
    });

    screen.Loop(component);
    return action;
}

} // namespace tui
