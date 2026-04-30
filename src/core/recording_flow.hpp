#pragma once
#include <optional>
#include <string>

namespace core {

/// Commands the recording flow understands.
/// Map 1:1 to keyboard keys, plus one synthetic timer event.
enum class RecordingCmd {
    Record,            // 'r'
    Stop,              // 's'
    Play,              // 'p'
    Redo,              // 'x'
    Next,              // 'n'
    Back,              // 'b'
    Quit,              // 'q'
    CountdownComplete, // synthetic: countdown timer fired, capture should begin
};

/// Parse a single-character key string to a command. Returns nullopt if unrecognised.
std::optional<RecordingCmd> parse_recording_cmd(const std::string& key);

/// Logical phase of the recording flow.
enum class FlowPhase { Idle, Countdown, Recording };

/// Snapshot of the recording flow's navigational and phase state.
/// Does NOT hold audio device state — the caller tracks that separately.
struct FlowState {
    int       current_idx = 0;
    int       total       = 0;
    FlowPhase phase       = FlowPhase::Idle;
    bool      has_take    = false; ///< current entry has a non-empty raw_take_path
};

/// Side-effects the caller must apply after a transition.
/// Multiple flags may be set simultaneously.
struct FlowEffects {
    bool start_countdown  = false; ///< open recorder + begin 3-2-1 timer
    bool cancel_countdown = false; ///< abort countdown thread
    bool activate_capture = false; ///< recorder.set_capture_active(true) — on CountdownComplete
    bool stop_recording   = false; ///< recorder.stop() + update entry.raw_take_path + save
    bool play_take        = false; ///< player.play(current entry's raw_take_path)
    bool stop_playback    = false; ///< player.stop()
    bool clear_take       = false; ///< reset entry take fields + save
    bool exit_to_session  = false; ///< navigate back to Session screen
};

struct FlowTransition {
    FlowState   next;
    FlowEffects effects;
};

/// Pure recording flow state machine. No audio, no TUI, no threads.
/// Accepts the current state and a command; returns the new state and effects to apply.
/// `has_take` in `state` gates the Play command.
/// `total` in `state` bounds-checks Next/Back.
FlowTransition recording_step(FlowState state, RecordingCmd cmd);

} // namespace core
