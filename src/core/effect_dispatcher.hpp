#pragma once
#include "recording_effects.hpp"
#include <filesystem>
#include <vector>

// Forward declarations — dispatcher calls into audio and project but doesn't
// include their headers here (keeps srt_dubber_core independent of audio;
// dispatcher is compiled into srt_dubber_lib only).
class AudioRecorder;
class AudioPlayer;

namespace core {

// Forward-declare Project so the header stays lean.
class Project;

/// Centralised effect dispatcher for the recording flow.
/// Owns no state beyond references to external services and a small amount of
/// bookkeeping needed for path resolution (current_idx_, current_take_path_).
/// Called by the TUI screen after each recording_step() transition.
class RecordingEffectDispatcher {
public:
    RecordingEffectDispatcher(AudioRecorder& recorder,
                               AudioPlayer&   player,
                               core::Project& project);

    /// Apply a single effect. May be called from the TUI event thread.
    void apply(const RecordingEffect& effect);

    /// Apply all effects from a transition in order.
    void apply_all(const std::vector<RecordingEffect>& effects);

private:
    AudioRecorder& recorder_;
    AudioPlayer&   player_;
    core::Project& project_;

    int current_idx_ = 0; ///< last known entry index, updated by StartCountdown/StopRecording
    std::filesystem::path current_take_path_; ///< path opened at StartCountdown, used by StopRecording
};

} // namespace core
