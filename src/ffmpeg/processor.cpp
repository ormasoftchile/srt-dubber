#include "processor.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool run(const std::string& cmd)
{
    return std::system(cmd.c_str()) == 0;
}

// Quote a path so spaces don't break shell commands (Unix-style).
static std::string q(const std::filesystem::path& p)
{
    return "\"" + p.string() + "\"";
}

// ---------------------------------------------------------------------------
// Step 1 – silence trim (leading + trailing, -50 dB threshold, 0.5 s)
// ---------------------------------------------------------------------------
bool FfmpegProcessor::trim_silence(const std::filesystem::path& in,
                                   const std::filesystem::path& out)
{
    std::string cmd =
        "ffmpeg -y -i " + q(in) +
        " -af silenceremove="
            "start_periods=1:start_silence=0.5:start_threshold=-50dB:"
            "stop_periods=1:stop_silence=0.5:stop_threshold=-50dB "
        + q(out) + " 2>/dev/null";
    return run(cmd);
}

// ---------------------------------------------------------------------------
// Step 2 – loudnorm
// ---------------------------------------------------------------------------
bool FfmpegProcessor::normalize(const std::filesystem::path& in,
                                const std::filesystem::path& out)
{
    std::string cmd =
        "ffmpeg -y -i " + q(in) +
        " -af loudnorm=I=-16:TP=-1.5:LRA=11 "
        + q(out) + " 2>/dev/null";
    return run(cmd);
}

// ---------------------------------------------------------------------------
// Step 3 – duration query via ffprobe
// ---------------------------------------------------------------------------
int64_t FfmpegProcessor::get_duration_ms(const std::filesystem::path& wav)
{
    std::string cmd =
        "ffprobe -v error -show_entries format=duration "
        "-of csv=p=0 " + q(wav) + " 2>/dev/null";

    std::array<char, 64> buf{};
    std::string result;

    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) return -1;
    while (fgets(buf.data(), static_cast<int>(buf.size()), pipe))
        result += buf.data();
    ::pclose(pipe);

    try {
        double secs = std::stod(result);
        return static_cast<int64_t>(secs * 1000.0);
    } catch (...) {
        return -1;
    }
}

// ---------------------------------------------------------------------------
// Step 4 – atempo
// ---------------------------------------------------------------------------
bool FfmpegProcessor::apply_atempo(const std::filesystem::path& in,
                                   const std::filesystem::path& out,
                                   double rate)
{
    std::ostringstream oss;
    oss.precision(6);
    oss << std::fixed << rate;
    std::string cmd =
        "ffmpeg -y -i " + q(in) +
        " -af atempo=" + oss.str() + " " +
        q(out) + " 2>/dev/null";
    return run(cmd);
}

// ---------------------------------------------------------------------------
// Public entry-point
// ---------------------------------------------------------------------------
ProcessResult FfmpegProcessor::process_take(
    const std::filesystem::path& input_wav,
    const std::filesystem::path& output_wav,
    int64_t slot_duration_ms)
{
    ProcessResult res;

    // Build temp paths next to output_wav to avoid cross-device issues.
    auto base  = output_wav.parent_path();
    auto stem  = output_wav.stem().string();
    auto tmp1  = base / (stem + "_trim.wav");
    auto tmp2  = base / (stem + "_norm.wav");

    // Step 1 – silence trim
    if (!trim_silence(input_wav, tmp1)) {
        res.error = "silence trim failed";
        return res;
    }

    // Step 2 – loudnorm
    if (!normalize(tmp1, tmp2)) {
        std::filesystem::remove(tmp1);
        res.error = "loudnorm failed";
        return res;
    }
    std::filesystem::remove(tmp1);

    // Step 3 – measure duration
    int64_t dur_ms = get_duration_ms(tmp2);
    if (dur_ms <= 0) {
        std::filesystem::remove(tmp2);
        res.error = "could not read duration";
        return res;
    }

    // Step 4 – atempo if needed
    constexpr double MAX_TEMPO = 1.08;

    if (slot_duration_ms > 0 && dur_ms > slot_duration_ms) {
        double rate = static_cast<double>(dur_ms) /
                      static_cast<double>(slot_duration_ms);

        if (rate > MAX_TEMPO) {
            // Overflow: mark it but still produce a stretched-to-max file.
            rate             = MAX_TEMPO;
            res.is_overflow  = true;
        }

        if (!apply_atempo(tmp2, output_wav, rate)) {
            std::filesystem::remove(tmp2);
            res.error = "atempo failed";
            return res;
        }
        std::filesystem::remove(tmp2);

        // Re-measure after stretch.
        dur_ms          = get_duration_ms(output_wav);
        res.was_stretched = true;
    } else {
        // No stretch needed – just rename temp to output.
        std::filesystem::rename(tmp2, output_wav);
    }

    res.success     = true;
    res.duration_ms = dur_ms;
    return res;
}
