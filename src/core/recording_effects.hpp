#pragma once
#include "effects.hpp"
#include <filesystem>
#include <variant>
#include <vector>

namespace core {

// ─── Per-transition effects for the recording flow ─────────────────────────

/// Begin the 3-2-1-Go countdown for the given entry.
/// Caller spawns the countdown thread and posts CountdownComplete when done.
struct StartCountdown { int idx; };

/// Abort an in-progress countdown (user pressed Back/Quit during countdown).
struct CancelCountdown {};

/// Enable audio capture — called on CountdownComplete.
/// Caller calls recorder.set_capture_active(true).
struct ActivateCapture {};

/// Stop recording and persist the take.
/// Caller: recorder.stop(), update entry.raw_take_path = path, project.save().
struct StopRecording { int idx; std::filesystem::path path; };

/// Play back the current entry's take.
struct PlayTake { std::filesystem::path path; };

/// Stop active playback.
struct StopPlayback {};

/// All effects the recording flow can emit.
using RecordingEffect = std::variant<
    StartCountdown,
    CancelCountdown,
    ActivateCapture,
    StopRecording,
    PlayTake,
    StopPlayback,
    SaveTake,     // from effects.hpp
    ClearTake,    // from effects.hpp
    SaveProject,  // from effects.hpp
    ExitToSession // from effects.hpp
>;

// ─── Temporary compatibility shim — remove in Slice 4 ──────────────────────
// Allows recording_screen.cpp to compile while the dispatcher is being built.
// DO NOT use in new code.
struct [[deprecated("Use RecordingEffect variant — remove in Slice 4")]] FlowEffects {
    bool start_countdown  = false;
    bool cancel_countdown = false;
    bool activate_capture = false;
    bool stop_recording   = false;
    bool play_take        = false;
    bool stop_playback    = false;
    bool clear_take       = false;
    bool exit_to_session  = false;

    /// Build a FlowEffects from a variant effects list (for compat during transition)
    static FlowEffects from_variants(const std::vector<RecordingEffect>& effects) {
        FlowEffects r;
        for (const auto& e : effects) {
            std::visit([&](const auto& v) {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, StartCountdown>)    r.start_countdown  = true;
                else if constexpr (std::is_same_v<T, CancelCountdown>) r.cancel_countdown = true;
                else if constexpr (std::is_same_v<T, ActivateCapture>) r.activate_capture = true;
                else if constexpr (std::is_same_v<T, StopRecording>)   r.stop_recording   = true;
                else if constexpr (std::is_same_v<T, PlayTake>)        r.play_take        = true;
                else if constexpr (std::is_same_v<T, StopPlayback>)    r.stop_playback    = true;
                else if constexpr (std::is_same_v<T, ClearTake>)       r.clear_take       = true;
                else if constexpr (std::is_same_v<T, ExitToSession>)   r.exit_to_session  = true;
            }, e);
        }
        return r;
    }
};

} // namespace core
