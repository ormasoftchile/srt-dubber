#pragma once

#include "core/project.hpp"
#include "ffmpeg/processor.hpp"
#include "ffmpeg/types.hpp"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace ffmpeg {

struct PrepareTakesResult {
    bool success {false};
    std::vector<ProcessedClip> clips;
    std::string error;
};

std::string import_takes(
    std::vector<core::ProjectEntry>& entries,
    const std::filesystem::path& input_dir
);

PrepareTakesResult prepare_takes(
    std::vector<core::ProjectEntry>& entries,
    const std::filesystem::path& output_dir,
    TakeProcessor& processor,
    std::function<void(const std::string&)> progress_cb = {},
    bool fit_to_slots = true
);

} // namespace ffmpeg