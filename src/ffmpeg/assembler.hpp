#pragma once
#include "types.hpp"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <cstdint>

struct AssembleResult {
    bool        success {false};
    std::string error;
};

class FfmpegAssembler {
public:
    // Build voiceover.wav aligned to SRT timing then mux into video_out.
    AssembleResult assemble(
        const std::vector<ProcessedClip>& clips,
        int64_t video_duration_ms,
        const std::filesystem::path& video_input,
        const std::filesystem::path& voiceover_out,
        const std::filesystem::path& video_out,
        std::function<void(int, int)> progress_cb  // (current, total)
    );

private:
    int64_t get_video_duration_ms(const std::filesystem::path& mp4);
};
