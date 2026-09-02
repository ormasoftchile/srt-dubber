#pragma once
#include "recording_effects.hpp"
#include "recording_render_state.hpp"
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
    CountdownFailed,   // synthetic: device failed or produced no warm-up samples
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
/// Multiple effects may be present simultaneously.
struct FlowTransition {
    FlowState next;
    std::vector<RecordingEffect> effects;
};

/// Pure recording flow state machine. No audio, no TUI, no threads.
/// Accepts the current state and a command; returns the new state and effects to apply.
/// `has_take` in `state` gates the Play command.
/// `total` in `state` bounds-checks Next/Back.
FlowTransition recording_step(FlowState state, RecordingCmd cmd);

/// Build a render snapshot from current flow state + entry data + elapsed timer.
/// Pure function — no audio, no TUI, no threads.
/// \param state     current flow state (phase, idx, has_take)
/// \param text      subtitle text for current entry (pass "" if out of bounds)
/// \param slot_ms   allocated slot duration in ms (pass 0 if unknown)
/// \param elapsed   recording elapsed time from AudioRecorder::elapsed_ms() (0 when not recording)
RecordingRenderState render_state(const FlowState& state,
                                  const std::string& text,
                                  int64_t slot_ms,
                                  int64_t elapsed);

} // namespace core
