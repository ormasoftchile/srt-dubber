#pragma once
#include <filesystem>
#include <cstdint>

// Shared data types used by both processor and assembler.

struct ProcessedClip {
    std::filesystem::path path;
    int64_t start_ms {0};
    int     index    {0};
};
