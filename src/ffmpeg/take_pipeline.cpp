#include "ffmpeg/take_pipeline.hpp"

namespace ffmpeg {

std::string import_takes(
    std::vector<core::ProjectEntry>& entries,
    const std::filesystem::path& input_dir)
{
    for (auto& entry : entries) {
        const auto take = input_dir / (std::to_string(entry.index) + ".wav");
        if (!std::filesystem::exists(take)) {
            return "take not found for subtitle " + std::to_string(entry.index) +
                   ": " + take.string();
        }
        entry.raw_take_path = take.string();
        entry.processed_take_path.clear();
        entry.processed_duration_ms = -1;
        entry.status = core::TakeStatus::pending;
    }
    return {};
}

PrepareTakesResult prepare_takes(
    std::vector<core::ProjectEntry>& entries,
    const std::filesystem::path& output_dir,
    TakeProcessor& processor,
    std::function<void(const std::string&)> progress_cb)
{
    PrepareTakesResult result;
    std::filesystem::create_directories(output_dir);

    for (auto& entry : entries) {
        if (!entry.processed_take_path.empty() &&
            std::filesystem::exists(entry.processed_take_path)) {
            result.clips.push_back({entry.processed_take_path, entry.start_ms, entry.index});
            continue;
        }

        if (entry.raw_take_path.empty()) {
            continue;
        }
        if (!std::filesystem::exists(entry.raw_take_path)) {
            result.error = "raw take not found for subtitle " + std::to_string(entry.index);
            return result;
        }

        if (progress_cb) {
            progress_cb("Processing take " + std::to_string(entry.index) + "...");
        }

        const auto output = output_dir / (std::to_string(entry.index) + ".wav");
        const auto processed = processor.process_take(
            entry.raw_take_path, output, entry.slot_duration_ms);
        if (!processed.success) {
            result.error = "take " + std::to_string(entry.index) + ": " + processed.error;
            return result;
        }

        entry.processed_take_path = output.string();
        entry.processed_duration_ms = processed.duration_ms;
        entry.status = processed.is_overflow
            ? core::TakeStatus::overflow
            : processed.was_stretched
                ? core::TakeStatus::stretched
                : core::TakeStatus::ok;
        result.clips.push_back({output, entry.start_ms, entry.index});
    }

    result.success = true;
    return result;
}

} // namespace ffmpeg