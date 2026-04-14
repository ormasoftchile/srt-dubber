#include "tui/screens/assemble_screen.hpp"

#include "ffmpeg/assembler.hpp"
#include "ffmpeg/types.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <chrono>
#include <mutex>
#include <thread>

using namespace ftxui;

namespace tui {

ScreenAction run_assemble_screen(core::Project& project,
                                 const std::filesystem::path& video_path) {
    // Reset transient state from any prior run.
    project.assemble_log.clear();
    project.assemble_complete = false;
    project.output_path.clear();

    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::GoSession;
    std::mutex log_mutex;

    auto push_log = [&](const std::string& line) {
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            project.assemble_log.push_back(line);
        }
        screen.PostEvent(Event::Custom);
    };

    // ------------------------------------------------------------------
    // Collect processed clips
    // ------------------------------------------------------------------
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

    // ------------------------------------------------------------------
    // Assembly thread
    // ------------------------------------------------------------------
    std::jthread assembly_thread([&, clips](std::stop_token) {
        if (clips.empty()) {
            push_log("⚠  No takes found — record something first.");
            project.assemble_complete = true;
            screen.PostEvent(Event::Custom);
            return;
        }

        push_log("Starting assembly (" + std::to_string(clips.size()) + " clips)…");

        auto voiceover_out = project.output_dir() / "voiceover.wav";
        auto video_out     = project.output_dir() / "output.mp4";

        ffmpeg::FfmpegAssembler assembler;
        auto result = assembler.assemble(
            clips,
            /*video_duration_ms=*/0,
            video_path,
            voiceover_out,
            video_out,
            [&](int cur, int total) {
                if (cur == 1) push_log("✓  Voiceover mix complete.");
                if (cur == total) push_log("✓  Video mux complete.");
            }
        );

        if (result.success) {
            project.output_path = video_out.string();
            push_log("✓  Done → " + video_out.string());
            project.save();
        } else {
            push_log("✗  Assembly failed: " + result.error);
        }

        project.assemble_complete = true;
        screen.PostEvent(Event::Custom);
    });

    // ------------------------------------------------------------------
    // Refresh thread (drives UI redraws while assembling)
    // ------------------------------------------------------------------
    std::jthread refresh_thread([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    // ------------------------------------------------------------------
    // Renderer
    // ------------------------------------------------------------------
    auto renderer = Renderer([&]() -> Element {
        std::vector<std::string> log_snapshot;
        {
            std::lock_guard<std::mutex> lock(log_mutex);
            log_snapshot = project.assemble_log;
        }

        Elements lines;
        lines.reserve(log_snapshot.size() + 1);
        for (const auto& line : log_snapshot)
            lines.push_back(text("  " + line));
        if (lines.empty())
            lines.push_back(dim(text("  (starting…)")));

        Element footer;
        if (project.assemble_complete) {
            if (!project.output_path.empty()) {
                footer = vbox({
                    color(Color::Green,
                          text("  ✓  Output: " + project.output_path)),
                    separator(),
                    hbox({text("  [q] back"), filler()}),
                });
            } else {
                footer = vbox({
                    color(Color::Red, text("  Assembly failed — see log above.")),
                    separator(),
                    hbox({text("  [q] back"), filler()}),
                });
            }
        } else {
            footer = vbox({
                dim(text("  Assembling…")),
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

    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        if (event.is_character() && event.character() == "q") {
            action = ScreenAction::GoSession;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return action;
}

} // namespace tui
