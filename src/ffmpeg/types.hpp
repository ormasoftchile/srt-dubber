#pragma once
#include <filesystem>
#include <cstdint>
#include <vector>

// Shared data types used by both processor and assembler.
// Moved from core/types.hpp to avoid duplication.

namespace ffmpeg {

struct ProcessedClip {
    std::filesystem::path path;
    int64_t start_ms {0};
    int     index    {0};
    int64_t duration_ms {0};
    int64_t slot_duration_ms {0};
};

struct TimelineHold {
    int64_t source_time_ms {0};
    int64_t duration_ms {0};
};

struct TimelinePlan {
    std::vector<int64_t> clip_start_ms;
    std::vector<TimelineHold> holds;
    int64_t extension_ms {0};
};

} // namespace ffmpeg
