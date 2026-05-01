#include "tui/screens/recording_screen.hpp"
#include "tui/app_header.hpp"

#include "core/recording_flow.hpp"
#include "core/recording_effects.hpp"
#include "core/recording_render_state.hpp"
#include "core/effect_dispatcher.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
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

// ── Recording state (shared_ptr across lambdas) ──────────────────────────────

struct RecordingState {
    core::FlowState flow;
    std::atomic<CountdownState> countdown_state{CountdownState::None};
    std::jthread countdown_thread;
    core::RecordingEffectDispatcher dispatcher;
    
    RecordingState(int start_idx, int total, bool has_take,
                   AudioRecorder& rec, AudioPlayer& pl, core::Project& proj)
        : flow{start_idx, total, core::FlowPhase::Idle, has_take}
        , dispatcher{rec, pl, proj}
    {}
    
    // non-copyable, non-movable (jthread, atomic)
    RecordingState(const RecordingState&) = delete;
    RecordingState& operator=(const RecordingState&) = delete;
};

// ── Component factory ────────────────────────────────────────────────────────

Component make_recording_component(
    core::Project& project,
    AudioRecorder& recorder,
    AudioPlayer& player,
    int start_index,
    ScreenInteractive& screen,
    NavigateFunc navigate)
{
    auto& entries = project.entries();
    if (entries.empty()) {
        navigate(ScreenAction::GoSession, 0);
        return Renderer([]() { return text(""); });
    }

    int start_idx = std::max(0, std::min(start_index, static_cast<int>(entries.size()) - 1));
    
    auto state = std::make_shared<RecordingState>(
        start_idx,
        static_cast<int>(entries.size()),
        !entries[start_idx].raw_take_path.empty(),
        recorder, player, project
    );

    // Helper to sync flow state from project (call after mutations)
    auto sync_flow = [state, &project]() {
        state->flow.total    = static_cast<int>(project.entries().size());
        state->flow.has_take = !project.entries()[state->flow.current_idx].raw_take_path.empty();
    };

    // Background refresh thread — drives the elapsed-time counter and countdown.
    std::jthread refresh_thread([&screen](std::stop_token stop) {
        while (!stop.stop_requested()) {
            screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    // Cancel any in-progress countdown thread (recorder is stopped by the dispatcher).
    auto cancel_countdown = [state]() {
        if (state->countdown_state.load() != CountdownState::None) {
            state->countdown_thread.request_stop();
            state->countdown_thread = std::jthread{};
            state->countdown_state.store(CountdownState::None);
        }
    };

    // Helper to apply effects from state machine.
    auto apply_effects = [state, &screen, navigate, cancel_countdown](
        const std::vector<core::RecordingEffect>& effects)
    {
        state->dispatcher.apply_all(effects);
        for (const auto& e : effects) {
            std::visit([&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, core::StartCountdown>) {
                    state->countdown_state.store(CountdownState::Three);
                    state->countdown_thread = std::jthread([state, &screen](std::stop_token stop) {
                        using namespace std::chrono_literals;
                        auto sleep_or_abort = [&](auto dur) -> bool {
                            std::this_thread::sleep_for(dur);
                            return stop.stop_requested();
                        };
                        if (sleep_or_abort(1s)) { state->countdown_state.store(CountdownState::None); return; }
                        state->countdown_state.store(CountdownState::Two);
                        if (sleep_or_abort(1s)) { state->countdown_state.store(CountdownState::None); return; }
                        state->countdown_state.store(CountdownState::One);
                        if (sleep_or_abort(1s)) { state->countdown_state.store(CountdownState::None); return; }
                        screen.PostEvent(Event::Special("\x01"));
                        state->countdown_state.store(CountdownState::Go);
                        if (sleep_or_abort(300ms)) { state->countdown_state.store(CountdownState::None); return; }
                        state->countdown_state.store(CountdownState::None);
                    });
                } else if constexpr (std::is_same_v<T, core::CancelCountdown>) {
                    cancel_countdown();
                } else if constexpr (std::is_same_v<T, core::ExitToSession>) {
                    navigate(ScreenAction::GoSession, 0);
                }
            }, e);
        }
    };

    auto renderer = Renderer([state, &recorder, &project]() -> Element {
        const auto& entries = project.entries();
        
        // Build render snapshot
        auto rs = core::render_state(
            state->flow,
            (state->flow.current_idx < (int)entries.size())
                ? entries[state->flow.current_idx].text
                : std::string{},
            (state->flow.current_idx < (int)entries.size())
                ? entries[state->flow.current_idx].slot_duration_ms
                : int64_t{0},
            recorder.elapsed_ms()
        );
        const bool  rec    = (rs.phase_label == "recording");
        const auto  cstate = state->countdown_state.load();

        // ── Header: app identity + caption progress, or live recording state ──
        char caption_buf[32];
        std::snprintf(caption_buf, sizeof(caption_buf), "Caption %d/%d",
                      rs.current_idx + 1, rs.total);
        Element header_elem = rec
            ? app_header_recording(recorder.is_warming_up())
            : app_header(caption_buf);

        // ── Subtitle text ────────────────────────────────────────────────
        std::string display_text = rs.entry_text;
        for (auto& c : display_text) if (c == '\n') c = ' ';

        // ── Context: prev / next ─────────────────────────────────────────
        const std::string prev_preview =
            rs.current_idx > 0
                ? truncate_to(first_line(entries[rs.current_idx - 1].text), 32)
                : std::string{};
        const std::string next_preview =
            (rs.current_idx + 1 < rs.total)
                ? truncate_to(first_line(entries[rs.current_idx + 1].text), 32)
                : std::string{};

        // ── Status badge ─────────────────────────────────────────────────
        std::string status_str{core::take_status_to_string(entries[rs.current_idx].status)};

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

        return borderRounded(vbox({
            header_elem,
            separator(),
            body,
            text(""),
            hbox({
                text("  "),
                dim(text("r record  s stop  p play  x redo  n next  b back  q quit")),
                filler(),
            }),
            text(""),
        }));
    });

    return CatchEvent(renderer, [state, sync_flow, apply_effects](Event event) -> bool {
        // Handle special CountdownComplete event
        if (event == Event::Special("\x01")) {
            auto t = core::recording_step(state->flow, core::RecordingCmd::CountdownComplete);
            apply_effects(t.effects);
            state->flow = t.next;
            return true;
        }

        if (!event.is_character()) return false;
        const auto ch = event.character();

        auto cmd_opt = core::parse_recording_cmd(ch);
        if (!cmd_opt.has_value()) return false;

        auto cmd = cmd_opt.value();
        auto t = core::recording_step(state->flow, cmd);
        apply_effects(t.effects);
        state->flow = t.next;
        sync_flow();
        
        return true;
    });
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

    // Centralised effect dispatcher — owns all audio and project mutations.
    core::RecordingEffectDispatcher dispatcher{recorder, player, project};

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

    // Cancel any in-progress countdown thread (recorder is stopped by the dispatcher).
    auto cancel_countdown = [&]() {
        if (countdown_state.load() != CountdownState::None) {
            countdown_thread.request_stop();
            countdown_thread = std::jthread{};
            countdown_state.store(CountdownState::None);
        }
    };

    // Helper to apply effects from state machine.
    // Dispatcher handles all audio and project mutations.
    // Thread management and TUI navigation are handled here.
    auto apply_effects = [&](const std::vector<core::RecordingEffect>& effects) {
        dispatcher.apply_all(effects);
        for (const auto& e : effects) {
            std::visit([&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, core::StartCountdown>) {
                    // Dispatcher opened the recorder. TUI owns the countdown thread.
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
                        screen.PostEvent(Event::Special("\x01"));
                        countdown_state.store(CountdownState::Go);
                        if (sleep_or_abort(300ms)) { countdown_state.store(CountdownState::None); return; }
                        countdown_state.store(CountdownState::None);
                    });
                } else if constexpr (std::is_same_v<T, core::CancelCountdown>) {
                    cancel_countdown();
                } else if constexpr (std::is_same_v<T, core::ExitToSession>) {
                    action = ScreenAction::GoSession;
                    screen.ExitLoopClosure()();
                }
            }, e);
        }
    };

    auto renderer = Renderer([&]() -> Element {
        // Build render snapshot — renderer reads display data from rs, not flow/recorder directly
        auto rs = core::render_state(
            flow,
            (flow.current_idx < (int)entries.size())
                ? entries[flow.current_idx].text
                : std::string{},
            (flow.current_idx < (int)entries.size())
                ? entries[flow.current_idx].slot_duration_ms
                : int64_t{0},
            recorder.elapsed_ms()
        );
        const bool  rec    = (rs.phase_label == "recording");
        const auto  cstate = countdown_state.load();

        // ── Header: app identity + caption progress, or live recording state ──
        char caption_buf[32];
        std::snprintf(caption_buf, sizeof(caption_buf), "Caption %d/%d",
                      rs.current_idx + 1, rs.total);
        Element header_elem = rec
            ? app_header_recording(recorder.is_warming_up())
            : app_header(caption_buf);

        // ── Subtitle text ────────────────────────────────────────────────
        // Replace embedded newlines with spaces for wrapping purposes.
        std::string display_text = rs.entry_text;
        for (auto& c : display_text) if (c == '\n') c = ' ';

        // ── Context: prev / next ─────────────────────────────────────────
        const std::string prev_preview =
            rs.current_idx > 0
                ? truncate_to(first_line(entries[rs.current_idx - 1].text), 32)
                : std::string{};
        const std::string next_preview =
            (rs.current_idx + 1 < rs.total)
                ? truncate_to(first_line(entries[rs.current_idx + 1].text), 32)
                : std::string{};

        // ── Status badge ─────────────────────────────────────────────────
        std::string status_str{core::take_status_to_string(entries[rs.current_idx].status)};

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

        return borderRounded(vbox({
            // header
            header_elem,
            separator(),

            // body (countdown overlay or subtitle)
            body,

            text(""),
            // key hints — single dim line
            hbox({
                text("  "),
                dim(text("r record  s stop  p play  x redo  n next  b back  q quit")),
                filler(),
            }),
            text(""),
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
