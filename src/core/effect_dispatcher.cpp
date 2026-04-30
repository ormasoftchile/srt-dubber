#include "effect_dispatcher.hpp"
#include "project.hpp"
#include "audio/recorder.hpp"  // resolved via include path (src/ for lib, tests/ for test target)
#include "audio/player.hpp"    // resolved via include path
#include <variant>

namespace core {

RecordingEffectDispatcher::RecordingEffectDispatcher(AudioRecorder& recorder,
                                                     AudioPlayer&   player,
                                                     core::Project& project)
    : recorder_(recorder), player_(player), project_(project) {}

void RecordingEffectDispatcher::apply(const RecordingEffect& effect) {
    std::visit([&](const auto& v) {
        using T = std::decay_t<decltype(v)>;

        if constexpr (std::is_same_v<T, StartCountdown>) {
            current_idx_ = v.idx;
            auto& entries = project_.entries();
            int entry_index = (v.idx < (int)entries.size())
                              ? entries[v.idx].index : v.idx;
            current_take_path_ = project_.takes_dir() /
                                 (std::to_string(entry_index) + ".wav");
            recorder_.start(current_take_path_);

        } else if constexpr (std::is_same_v<T, CancelCountdown>) {
            // Bug fix: close the recorder if the device was opened for countdown warmup.
            if (recorder_.is_recording() || recorder_.is_warming_up())
                recorder_.stop();

        } else if constexpr (std::is_same_v<T, ActivateCapture>) {
            recorder_.set_capture_active(true);

        } else if constexpr (std::is_same_v<T, StopRecording>) {
            current_idx_ = v.idx;
            recorder_.stop();
            // Use the path we opened (recorder has no output_path() accessor).
            project_.entries()[v.idx].raw_take_path = current_take_path_.string();
            project_.save();

        } else if constexpr (std::is_same_v<T, PlayTake>) {
            if (recorder_.is_recording()) return;
            player_.stop(); // defensive: stop any prior playback
            std::filesystem::path path = v.path;
            if (path.empty()) {
                auto& entries = project_.entries();
                if (current_idx_ < (int)entries.size())
                    path = entries[current_idx_].raw_take_path;
            }
            if (!path.empty())
                player_.play(path);

        } else if constexpr (std::is_same_v<T, StopPlayback>) {
            if (player_.is_playing()) player_.stop();

        } else if constexpr (std::is_same_v<T, SaveTake>) {
            project_.entries()[v.idx].raw_take_path = v.path.string();

        } else if constexpr (std::is_same_v<T, ClearTake>) {
            auto& e = project_.entries()[v.idx];
            e.raw_take_path.clear();
            e.processed_take_path.clear();
            e.raw_duration_ms       = -1;
            e.processed_duration_ms = -1;
            e.status                = TakeStatus::pending;
            project_.save();

        } else if constexpr (std::is_same_v<T, SaveProject>) {
            project_.save();

        } else if constexpr (std::is_same_v<T, ExitToSession>) {
            // Navigation is handled by the TUI return value — no-op here.
        }
    }, effect);
}

void RecordingEffectDispatcher::apply_all(const std::vector<RecordingEffect>& effects) {
    for (const auto& e : effects)
        apply(e);
}

} // namespace core
