#pragma once
#include <filesystem>
#include <string>
#include <cstdint>

struct ProcessResult {
    bool        success      {false};
    int64_t     duration_ms  {0};
    bool        was_stretched{false};
    bool        is_overflow  {false};
    std::string error;
};

class FfmpegProcessor {
public:
    // Trim silence, loudnorm, optionally speed up to fit slot_duration_ms.
    // output_wav is written only on success.
    ProcessResult process_take(
        const std::filesystem::path& input_wav,
        const std::filesystem::path& output_wav,
        int64_t slot_duration_ms
    );

private:
    // Step 1 – remove leading/trailing silence.
    bool trim_silence(const std::filesystem::path& in,
                      const std::filesystem::path& out);

    // Step 2 – loudnorm to -16 LUFS / -1.5 TP.
    bool normalize(const std::filesystem::path& in,
                   const std::filesystem::path& out);

    // Step 3 – query duration with ffprobe (returns -1 on error).
    int64_t get_duration_ms(const std::filesystem::path& wav);

    // Step 4 – atempo stretch (rate already clamped by caller).
    bool apply_atempo(const std::filesystem::path& in,
                      const std::filesystem::path& out,
                      double rate);
};
