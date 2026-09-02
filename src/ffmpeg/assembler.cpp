#include "ffmpeg/assembler.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

#ifdef _MSC_VER
#define popen  _popen
#define pclose _pclose
#endif

namespace ffmpeg {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool run_asm(const std::string& cmd)
{
    return std::system(cmd.c_str()) == 0;
}

static std::string qa(const std::filesystem::path& p)
{
    return "\"" + p.string() + "\"";
}

std::string build_mux_command(
    const std::filesystem::path& video_input,
    const std::filesystem::path& voiceover_input,
    const std::filesystem::path& video_output,
    int64_t video_duration_ms)
{
    std::ostringstream cmd;
    cmd << "ffmpeg -y -i " << qa(video_input)
        << " -i " << qa(voiceover_input)
        << " -map 0:v:0 -map 1:a:0 -map_metadata 0"
        << " -c:v copy -c:a aac";
    if (video_duration_ms > 0) {
        cmd << " -t " << (video_duration_ms / 1000)
            << "." << std::setfill('0') << std::setw(3)
            << (video_duration_ms % 1000);
    }
    cmd << " " << qa(video_output);
#ifdef _WIN32
    cmd << " 2>NUL";
#else
    cmd << " 2>/dev/null";
#endif
    return cmd.str();
}

// ---------------------------------------------------------------------------
// Duration of a video file via ffprobe
// ---------------------------------------------------------------------------
int64_t FfmpegAssembler::get_video_duration_ms(const std::filesystem::path& mp4)
{
    std::string cmd =
        "ffprobe -v error -show_entries format=duration "
#ifdef _WIN32
        "-of csv=p=0 " + qa(mp4) + " 2>NUL";
#else
        "-of csv=p=0 " + qa(mp4) + " 2>/dev/null";
#endif

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
// assemble()
// ---------------------------------------------------------------------------
AssembleResult FfmpegAssembler::assemble(
    const std::vector<ProcessedClip>& clips,
    int64_t                           video_duration_ms,
    const std::filesystem::path&      video_input,
    const std::filesystem::path&      voiceover_out,
    const std::filesystem::path&      video_out,
    std::function<void(int, int)>     progress_cb)
{
    AssembleResult res;
    const int total_steps = 2; // step 1: mix, step 2: mux

    if (clips.empty()) {
        res.error = "no clips to assemble";
        return res;
    }

    // ------------------------------------------------------------------
    // Step 1 — build voiceover.wav
    // ------------------------------------------------------------------
    if (progress_cb) progress_cb(0, total_steps);

    // Resolve actual video duration (use provided hint if ffprobe fails).
    int64_t vid_dur = get_video_duration_ms(video_input);
    if (vid_dur <= 0) vid_dur = video_duration_ms;

    // Build ffmpeg command:
    //   ffmpeg -y -i clip0.wav -i clip1.wav ... \
    //     -filter_complex "[0]adelay=T|T[d0];[1]adelay=T|T[d1];...amix=inputs=N:normalize=0" \
    //     voiceover.wav
    std::ostringstream cmd;
    cmd << "ffmpeg -y";

    // Inputs
    for (const auto& clip : clips)
        cmd << " -i " << qa(clip.path);

    // filter_complex
    cmd << " -filter_complex \"";
    for (std::size_t i = 0; i < clips.size(); ++i) {
        int64_t delay = clips[i].start_ms;
        cmd << "[" << i << "]adelay=" << delay << "|" << delay << "[d" << i << "];";
    }
    // amix all labelled streams
    for (std::size_t i = 0; i < clips.size(); ++i)
        cmd << "[d" << i << "]";
    cmd << "amix=inputs=" << clips.size() << ":normalize=0\"";

#ifdef _WIN32
    cmd << " " << qa(voiceover_out) << " 2>NUL";
#else
    cmd << " " << qa(voiceover_out) << " 2>/dev/null";
#endif

    if (!run_asm(cmd.str())) {
        res.error = "ffmpeg voiceover mix failed";
        return res;
    }

    if (progress_cb) progress_cb(1, total_steps);

    // ------------------------------------------------------------------
    // Step 2 — mux video + voiceover
    // ------------------------------------------------------------------
        const std::string mux_cmd = build_mux_command(
        video_input, voiceover_out, video_out, vid_dur);

    if (!run_asm(mux_cmd)) {
        res.error = "ffmpeg mux failed";
        return res;
    }

    if (progress_cb) progress_cb(2, total_steps);

    res.success = true;
    return res;
}

} // namespace ffmpeg
