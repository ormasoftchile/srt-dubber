#pragma once
#include <filesystem>
#include <cstdint>

// Shared data types used by both processor and assembler.
// Moved from core/types.hpp to avoid duplication.

namespace ffmpeg {

struct ProcessedClip {
    std::filesystem::path path;
    int64_t start_ms {0};
    int     index    {0};
};

} // namespace ffmpeg
