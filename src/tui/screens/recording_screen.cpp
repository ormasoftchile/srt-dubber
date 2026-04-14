#include "tui/screens/recording_screen.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using namespace ftxui;

namespace tui {

// ── Formatting helpers ──────────────────────────────────────────────────────

static std::string fmt_dur_ms(int64_t ms) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1fs", ms / 1000.0);
    return buf;
}

static std::string fmt_idx(int one_based, int total) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), " %d / %d ", one_based, total);
    return buf;
}

static std::string truncate_to(const std::string& s, std::size_t max_len) {
    if (s.size() <= max_len) return s;
    return s.substr(0, max_len - 3) + "...";
}

/// Return the first line of a potentially multi-line subtitle.
static std::string first_line(const std::string& s) {
    const auto pos = s.find('\n');
    return pos != std::string::npos ? s.substr(0, pos) : s;
}

// ── Screen ──────────────────────────────────────────────────────────────────

ScreenAction run_recording_screen(core::Project& project,
                                  AudioRecorder&  recorder,
                                  AudioPlayer&    player,
                                  int             start_index) {
    auto& entries = project.entries();
    if (entries.empty()) return ScreenAction::GoSession;

    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::GoSession;

    int  current_idx = std::max(0, std::min(start_index,
                                            static_cast<int>(entries.size()) - 1));

    // Background refresh thread — drives the elapsed-time counter.
    std::jthread refresh_thread([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    auto renderer = Renderer([&]() -> Element {
        const auto& entry = entries[current_idx];
        const bool  rec   = recorder.is_recording();
        const int   total = static_cast<int>(entries.size());

        // ── Top bar ──────────────────────────────────────────────────────
        Elements bar;
        bar.push_back(bold(text(fmt_idx(current_idx + 1, total))));
        bar.push_back(filler());
        bar.push_back(dim(text(" slot: ")));
        bar.push_back(text(fmt_dur_ms(entry.slot_duration_ms) + " "));
        if (rec) {
            bar.push_back(text(" " + fmt_dur_ms(recorder.elapsed_ms()) + " "));
            bar.push_back(bold(color(Color::Red, text(" ●REC "))));
        } else {
            bar.push_back(dim(text(" ●    ")));
        }

        // ── Subtitle text ────────────────────────────────────────────────
        // Replace embedded newlines with spaces for wrapping purposes.
        std::string display_text = entry.text;
        for (auto& c : display_text) if (c == '\n') c = ' ';

        // ── Context: prev / next ─────────────────────────────────────────
        const std::string prev_preview =
            current_idx > 0
                ? truncate_to(first_line(entries[current_idx - 1].text), 32)
                : std::string{};
        const std::string next_preview =
            (current_idx + 1 < total)
                ? truncate_to(first_line(entries[current_idx + 1].text), 32)
                : std::string{};

        // ── Status badge (bottom-left of subtitle box) ───────────────────
        std::string status_str{core::take_status_to_string(entry.status)};

        return border(vbox({
            // top bar
            hbox(bar),
            separator(),

            // subtitle body
            vbox({
                text(""),
                paragraphAlignCenter("\"" + display_text + "\"") | bold,
                text(""),
                hbox({
                    dim(text(prev_preview.empty() ? "  " : " ← " + prev_preview)),
                    filler(),
                    dim(text(next_preview.empty() ? "  " : next_preview + " → ")),
                }),
                text(""),
                hbox({filler(), dim(text(" " + status_str + " "))}),
            }) | flex,

            separator(),
            // key hints
            hbox({text("  [r]record  [s]stop  [p]play  [x]redo"), filler()}),
            hbox({text("  [n]next    [b]back  [q]save&quit"),      filler()}),
        }));
    });

    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        if (!event.is_character()) return false;
        const auto ch = event.character();

        if (ch == "r") {
            if (!recorder.is_recording()) {
                auto& e    = entries[current_idx];
                auto  path = project.takes_dir() /
                             (std::to_string(e.index) + ".wav");
                recorder.start(path);
            }
            return true;
        }

        if (ch == "s") {
            if (recorder.is_recording()) {
                recorder.stop();
                auto& e = entries[current_idx];
                e.raw_take_path = (project.takes_dir() /
                                   (std::to_string(e.index) + ".wav")).string();
                // Status remains pending until ffmpeg processing runs.
                project.save();
            }
            return true;
        }

        if (ch == "p") {
            if (!recorder.is_recording()) {
                const auto& e = entries[current_idx];
                if (!e.raw_take_path.empty())
                    player.play(e.raw_take_path);
            }
            return true;
        }

        if (ch == "x") {
            // Redo: stop any ongoing recording, clear take.
            if (recorder.is_recording()) recorder.stop();
            player.stop();
            auto& e = entries[current_idx];
            e.raw_take_path.clear();
            e.processed_take_path.clear();
            e.raw_duration_ms       = -1;
            e.processed_duration_ms = -1;
            e.status                = core::TakeStatus::pending;
            project.save();
            return true;
        }

        if (ch == "n") {
            if (recorder.is_recording()) { recorder.stop(); project.save(); }
            player.stop();
            if (current_idx + 1 < static_cast<int>(entries.size()))
                ++current_idx;
            return true;
        }

        if (ch == "b") {
            if (recorder.is_recording()) { recorder.stop(); project.save(); }
            player.stop();
            if (current_idx > 0) --current_idx;
            return true;
        }

        if (ch == "q") {
            if (recorder.is_recording()) { recorder.stop(); project.save(); }
            player.stop();
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
