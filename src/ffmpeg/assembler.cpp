#include "assembler.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>

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

// ---------------------------------------------------------------------------
// Duration of a video file via ffprobe
// ---------------------------------------------------------------------------
int64_t FfmpegAssembler::get_video_duration_ms(const std::filesystem::path& mp4)
{
    std::string cmd =
        "ffprobe -v error -show_entries format=duration "
        "-of csv=p=0 " + qa(mp4) + " 2>/dev/null";

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

    cmd << " " << qa(voiceover_out) << " 2>/dev/null";

    if (!run_asm(cmd.str())) {
        res.error = "ffmpeg voiceover mix failed";
        return res;
    }

    if (progress_cb) progress_cb(1, total_steps);

    // ------------------------------------------------------------------
    // Step 2 — mux video + voiceover
    // ------------------------------------------------------------------
    std::string mux_cmd =
        "ffmpeg -y -i " + qa(video_input) +
        " -i " + qa(voiceover_out) +
        " -c:v copy -c:a aac " +
        qa(video_out) + " 2>/dev/null";

    if (!run_asm(mux_cmd)) {
        res.error = "ffmpeg mux failed";
        return res;
    }

    if (progress_cb) progress_cb(2, total_steps);

    res.success = true;
    return res;
}
