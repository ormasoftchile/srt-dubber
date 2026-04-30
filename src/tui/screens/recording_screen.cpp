#include "tui/screens/recording_screen.hpp"

#include "core/recording_flow.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <atomic>
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

// ── Countdown state ──────────────────────────────────────────────────────────

enum class CountdownState { None, Three, Two, One, Go };

// ── Screen ──────────────────────────────────────────────────────────────────

ScreenAction run_recording_screen(core::Project& project,
                                  AudioRecorder&  recorder,
                                  AudioPlayer&    player,
                                  int             start_index) {
    auto& entries = project.entries();
    if (entries.empty()) return ScreenAction::GoSession;

    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::GoSession;

    // Initialize flow state
    int start_idx = std::max(0, std::min(start_index, static_cast<int>(entries.size()) - 1));
    core::FlowState flow{
        .current_idx = start_idx,
        .total       = static_cast<int>(entries.size()),
        .phase       = core::FlowPhase::Idle,
        .has_take    = !entries[start_idx].raw_take_path.empty()
    };

    std::atomic<CountdownState> countdown_state{CountdownState::None};
    std::jthread countdown_thread;

    // Helper to sync flow state from project (call after mutations)
    auto sync_flow = [&]() {
        flow.total    = static_cast<int>(entries.size());
        flow.has_take = !entries[flow.current_idx].raw_take_path.empty();
    };

    // Background refresh thread — drives the elapsed-time counter and countdown.
    std::jthread refresh_thread([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Cancel any in-progress countdown (does NOT stop an already-started recording).
    auto cancel_countdown = [&]() {
        if (countdown_state.load() != CountdownState::None) {
            countdown_thread.request_stop();
            // join so the thread sees the stop before we reassign state
            countdown_thread = std::jthread{};
            countdown_state.store(CountdownState::None);
        }
    };

    // Begin a 3-2-1-Go! countdown. Mic opens immediately so warmup (1.5s)
    // runs in parallel with the countdown (~3.3s) — by Go! the mic is hot.
    auto start_countdown = [&](int idx) {
        auto path = project.takes_dir() /
                    (std::to_string(entries[idx].index) + ".wav");
        recorder.start(path); // warmup begins now, completes well before Go!
        countdown_state.store(CountdownState::Three);
        countdown_thread = std::jthread([&](std::stop_token stop) {
            using namespace std::chrono_literals;
            auto sleep_or_abort = [&](auto dur) -> bool {
                std::this_thread::sleep_for(dur);
                return stop.stop_requested();
            };
            if (sleep_or_abort(1s)) { countdown_state.store(CountdownState::None); return; }
            countdown_state.store(CountdownState::Two);
            if (sleep_or_abort(1s)) { countdown_state.store(CountdownState::None); return; }
            countdown_state.store(CountdownState::One);
            if (sleep_or_abort(1s)) { countdown_state.store(CountdownState::None); return; }
            // Signal countdown complete via special event
            screen.PostEvent(Event::Special("\x01"));
            countdown_state.store(CountdownState::Go);
            if (sleep_or_abort(300ms)) { countdown_state.store(CountdownState::None); return; }
            countdown_state.store(CountdownState::None);
        });
    };

    // Helper to apply effects from state machine
    auto apply_effects = [&](const core::FlowEffects& fx) {
        if (fx.cancel_countdown) {
            cancel_countdown();
        }
        if (fx.start_countdown) {
            start_countdown(flow.current_idx);
        }
        if (fx.activate_capture) {
            recorder.set_capture_active(true);
        }
        if (fx.stop_recording) {
            recorder.stop();
            auto& e = entries[flow.current_idx];
            e.raw_take_path = (project.takes_dir() /
                               (std::to_string(e.index) + ".wav")).string();
            project.save();
        }
        if (fx.play_take) {
            const auto& e = entries[flow.current_idx];
            if (!e.raw_take_path.empty())
                player.play(e.raw_take_path);
        }
        if (fx.stop_playback) {
            player.stop();
        }
        if (fx.clear_take) {
            auto& e = entries[flow.current_idx];
            e.raw_take_path.clear();
            e.processed_take_path.clear();
            e.raw_duration_ms       = -1;
            e.processed_duration_ms = -1;
            e.status                = core::TakeStatus::pending;
            project.save();
        }
        if (fx.exit_to_session) {
            action = ScreenAction::GoSession;
            screen.ExitLoopClosure()();
        }
    };

    auto renderer = Renderer([&]() -> Element {
        const auto& entry  = entries[flow.current_idx];
        const bool  rec    = recorder.is_recording();
        const int   total  = static_cast<int>(entries.size());
        const auto  cstate = countdown_state.load();

        // ── Top bar ──────────────────────────────────────────────────────
        Elements bar;
        bar.push_back(bold(text(fmt_idx(flow.current_idx + 1, total))));
        bar.push_back(filler());
        bar.push_back(dim(text(" slot: ")));
        bar.push_back(text(fmt_dur_ms(entry.slot_duration_ms) + " "));
        if (rec) {
            bar.push_back(text(" " + fmt_dur_ms(recorder.elapsed_ms()) + " "));
            if (recorder.is_warming_up()) {
                bar.push_back(bold(color(Color::Yellow, text(" ● warming up… "))));
            } else {
                bar.push_back(bold(color(Color::Red, text(" ●REC "))));
            }
        } else if (cstate != CountdownState::None) {
            bar.push_back(dim(text(" counting… ")));
        } else {
            bar.push_back(dim(text(" ●    ")));
        }

        // ── Subtitle text ────────────────────────────────────────────────
        // Replace embedded newlines with spaces for wrapping purposes.
        std::string display_text = entry.text;
        for (auto& c : display_text) if (c == '\n') c = ' ';

        // ── Context: prev / next ─────────────────────────────────────────
        const std::string prev_preview =
            flow.current_idx > 0
                ? truncate_to(first_line(entries[flow.current_idx - 1].text), 32)
                : std::string{};
        const std::string next_preview =
            (flow.current_idx + 1 < total)
                ? truncate_to(first_line(entries[flow.current_idx + 1].text), 32)
                : std::string{};

        // ── Status badge ─────────────────────────────────────────────────
        std::string status_str{core::take_status_to_string(entry.status)};

        // ── Body: subtitle always visible; countdown number overlaid beneath ──
        Element body;
        if (cstate != CountdownState::None) {
            Element count_elem;
            if (cstate == CountdownState::Go) {
                count_elem = bold(color(Color::Green, text("Go!")));
            } else {
                const char* label =
                    cstate == CountdownState::Three ? "3" :
                    cstate == CountdownState::Two   ? "2" : "1";
                count_elem = bold(text(label));
            }
            body = vbox({
                text(""),
                paragraphAlignCenter("\"" + display_text + "\"") | bold,
                text(""),
                hbox({filler(), count_elem | size(WIDTH, GREATER_THAN, 3), filler()}),
                filler(),
            }) | flex;
        } else {
            body = vbox({
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
            }) | flex;
        }

        return border(vbox({
            // top bar
            hbox(bar),
            separator(),

            // body (countdown overlay or subtitle)
            body,

            separator(),
            // key hints
            hbox({text("  [r]record  [s]stop  [p]play  [x]redo"), filler()}),
            hbox({text("  [n]next    [b]back  [q]save&quit"),      filler()}),
        }));
    });

    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        // Handle special CountdownComplete event
        if (event == Event::Special("\x01")) {
            auto t = core::recording_step(flow, core::RecordingCmd::CountdownComplete);
            apply_effects(t.effects);
            flow = t.next;
            return true;
        }

        if (!event.is_character()) return false;
        const auto ch = event.character();

        // Parse command from key
        auto cmd_opt = core::parse_recording_cmd(ch);
        if (!cmd_opt.has_value()) return false;

        auto cmd = cmd_opt.value();
        
        // Apply state machine transition
        auto t = core::recording_step(flow, cmd);
        apply_effects(t.effects);
        flow = t.next;
        sync_flow();
        
        return true;
    });

    screen.Loop(component);
    return action;
}

} // namespace tui
