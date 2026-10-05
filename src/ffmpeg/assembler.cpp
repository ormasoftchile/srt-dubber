#include "ffmpeg/assembler.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef _MSC_VER
#define popen  _popen
#define pclose _pclose
#endif

namespace ffmpeg {

#ifdef _WIN32
constexpr std::string_view kDevNull = "2>NUL";
#else
constexpr std::string_view kDevNull = "2>/dev/null";
#endif

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static bool run_asm(const std::string& cmd)
{
#ifdef _WIN32
    SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE null_stream = CreateFileW(
        L"NUL", GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, &attributes,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (null_stream == INVALID_HANDLE_VALUE)
        return false;

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = null_stream;
    startup.hStdOutput = null_stream;
    startup.hStdError = null_stream;
    PROCESS_INFORMATION process{};
    std::string command = cmd;
    const bool started = CreateProcessA(
        nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
        nullptr, nullptr, &startup, &process) != FALSE;
    CloseHandle(null_stream);
    if (!started)
        return false;

    const DWORD wait_result = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 1;
    const bool success = wait_result == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process.hProcess, &exit_code) && exit_code == 0;
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return success;
#else
    return std::system(cmd.c_str()) == 0;
#endif
}

static std::string qa(const std::filesystem::path& p)
{
    return "\"" + p.string() + "\"";
}

static std::string seconds(int64_t milliseconds)
{
    return std::format("{}.{:03d}", milliseconds / 1000, milliseconds % 1000);
}

TimelinePlan build_timeline_plan(const std::vector<ProcessedClip>& clips)
{
    TimelinePlan plan;
    plan.clip_start_ms.reserve(clips.size());

    int64_t accumulated_extension_ms = 0;
    for (const auto& clip : clips) {
        plan.clip_start_ms.push_back(clip.start_ms + accumulated_extension_ms);
        if (clip.slot_duration_ms <= 0 || clip.duration_ms <= clip.slot_duration_ms)
            continue;

        const int64_t extension_ms = clip.duration_ms - clip.slot_duration_ms;
        plan.holds.push_back({
            .source_time_ms = clip.start_ms + clip.slot_duration_ms,
            .duration_ms = extension_ms,
        });
        accumulated_extension_ms += extension_ms;
    }

    plan.extension_ms = accumulated_extension_ms;
    return plan;
}

std::string build_timeline_extension_command(
    const std::filesystem::path& input,
    const std::filesystem::path& output,
    const std::vector<TimelineHold>& holds,
    bool source_has_audio)
{
    if (holds.empty()) return {};

    std::vector<std::string> filters;
    int64_t segment_start_ms = 0;
    for (std::size_t i = 0; i < holds.size(); ++i) {
        const auto& hold = holds[i];
        const std::string start = seconds(segment_start_ms);
        const std::string end = seconds(hold.source_time_ms);
        const std::string duration = seconds(hold.duration_ms);

        filters.push_back(
            "[0:v]trim=start=" + start + ":end=" + end +
            ",setpts=PTS-STARTPTS,tpad=stop_mode=clone:stop_duration=" +
            duration + "[v" + std::to_string(i) + "]");
        if (source_has_audio) {
            filters.push_back(
                "[0:a:0]atrim=start=" + start + ":end=" + end +
                ",asetpts=PTS-STARTPTS,apad=pad_dur=" + duration +
                "[a" + std::to_string(i) + "]");
        }
        segment_start_ms = hold.source_time_ms;
    }

    const std::size_t last = holds.size();
    const std::string last_start = seconds(segment_start_ms);
    filters.push_back(
        "[0:v]trim=start=" + last_start +
        ",setpts=PTS-STARTPTS[v" + std::to_string(last) + "]");
    if (source_has_audio) {
        filters.push_back(
            "[0:a:0]atrim=start=" + last_start +
            ",asetpts=PTS-STARTPTS[a" + std::to_string(last) + "]");
    }

    std::string video_inputs;
    std::string audio_inputs;
    for (std::size_t i = 0; i <= last; ++i) {
        video_inputs += "[v" + std::to_string(i) + "]";
        audio_inputs += "[a" + std::to_string(i) + "]";
    }
    filters.push_back(video_inputs + "concat=n=" + std::to_string(last + 1) +
                      ":v=1:a=0[vout]");
    if (source_has_audio) {
        filters.push_back(audio_inputs + "concat=n=" + std::to_string(last + 1) +
                          ":v=0:a=1[aout]");
    }

    std::string cmd = std::format("ffmpeg -y -i {} -filter_complex \"", qa(input));
    for (std::size_t i = 0; i < filters.size(); ++i) {
        if (i > 0) cmd += ";";
        cmd += filters[i];
    }
    cmd += "\" -map \"[vout]\"";
    if (source_has_audio)
        cmd += " -map \"[aout]\"";
    cmd += " -map_metadata 0 -c:v libx264 -preset medium -crf 18 -pix_fmt yuv420p";
    if (source_has_audio)
        cmd += " -c:a aac";
    else
        cmd += " -an";
    cmd += std::format(" {} {}", qa(output), kDevNull);
    return cmd;
}

std::string build_mux_command(
    const std::filesystem::path& video_input,
    const std::filesystem::path& voiceover_input,
    const std::filesystem::path& video_output,
    int64_t video_duration_ms,
    bool source_has_audio)
{
    std::string cmd;
    if (source_has_audio) {
        cmd = std::format(
            "ffmpeg -y -i {} -i {} -filter_complex \"[0:a:0][1:a:0]amix=inputs=2:normalize=0,alimiter=limit=0.95[mixed]\" -map 0:v:0 -map \"[mixed]\"",
            qa(video_input), qa(voiceover_input));
    } else {
        cmd = std::format(
            "ffmpeg -y -i {} -i {} -map 0:v:0 -map 1:a:0",
            qa(video_input), qa(voiceover_input));
    }
    cmd += " -map_metadata 0 -c:v copy -c:a aac";
    if (video_duration_ms > 0) {
        cmd += std::format(" -t {}.{:03d}", video_duration_ms / 1000, video_duration_ms % 1000);
    }
    cmd += std::format(" {} {}", qa(video_output), kDevNull);
    return cmd;
}

bool FfmpegAssembler::has_audio_stream(const std::filesystem::path& media)
{
    std::string cmd = std::format(
        "ffprobe -v error -select_streams a:0 -show_entries stream=index -of csv=p=0 {} {}",
        qa(media), kDevNull);

    std::array<char, 32> buf{};
    FILE* pipe = ::popen(cmd.c_str(), "r");
    if (!pipe) return false;
    const bool found = fgets(buf.data(), static_cast<int>(buf.size()), pipe) != nullptr;
    ::pclose(pipe);
    return found;
}

// ---------------------------------------------------------------------------
// Duration of a video file via ffprobe
// ---------------------------------------------------------------------------
int64_t FfmpegAssembler::get_video_duration_ms(const std::filesystem::path& mp4)
{
    std::string cmd = std::format(
        "ffprobe -v error -show_entries format=duration -of csv=p=0 {} {}",
        qa(mp4), kDevNull);

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

    if (clips.empty()) {
        res.error = "no clips to assemble";
        return res;
    }

    if (!video_input.empty() && !std::filesystem::exists(video_input)) {
        res.error = "video file not found: " + video_input.string();
        return res;
    }

    const TimelinePlan timeline = build_timeline_plan(clips);
    const bool has_video = !video_input.empty();
    const int total_steps = !has_video ? 1 : (timeline.holds.empty() ? 2 : 3);

    // ------------------------------------------------------------------
    // Step 1 — build voiceover.wav
    // ------------------------------------------------------------------
    if (progress_cb) progress_cb(0, total_steps);

    // Resolve actual video duration (use provided hint if ffprobe fails).
    int64_t vid_dur = get_video_duration_ms(video_input);
    if (vid_dur <= 0) vid_dur = video_duration_ms;

    std::string inputs;
    for (const auto& clip : clips) {
        std::format_to(std::back_inserter(inputs), " -i {}", qa(clip.path));
    }

    std::string filter_complex;
    for (std::size_t i = 0; i < clips.size(); ++i) {
        int64_t delay = timeline.clip_start_ms[i];
        std::format_to(std::back_inserter(filter_complex), "[{}]adelay={}|{}[d{}];", i, delay, delay, i);
    }
    for (std::size_t i = 0; i < clips.size(); ++i) {
        std::format_to(std::back_inserter(filter_complex), "[d{}]", i);
    }
    std::format_to(std::back_inserter(filter_complex), "amix=inputs={}:normalize=0", clips.size());

    std::string cmd = std::format(
        "ffmpeg -y{} -filter_complex \"{}\" {} {}",
        inputs, filter_complex, qa(voiceover_out), kDevNull);

    if (!run_asm(cmd)) {
        res.error = "ffmpeg voiceover mix failed";
        return res;
    }

    if (progress_cb) progress_cb(1, total_steps);

    if (!has_video) {
        res.success = true;
        return res;
    }

    std::filesystem::path mux_input = video_input;
    std::filesystem::path extended_input;
    const bool input_has_audio = has_audio_stream(video_input);
    if (!timeline.holds.empty()) {
        extended_input = voiceover_out.parent_path() / ".timeline-extended.mp4";
        const std::string extend_cmd = build_timeline_extension_command(
            video_input, extended_input, timeline.holds, input_has_audio);
        if (!run_asm(extend_cmd)) {
            res.error = "ffmpeg timeline extension failed";
            return res;
        }
        mux_input = extended_input;
        if (progress_cb) progress_cb(2, total_steps);
    }

    // ------------------------------------------------------------------
    // Step 2 — mux video + voiceover
    // ------------------------------------------------------------------
    const std::string mux_cmd = build_mux_command(
        mux_input, voiceover_out, video_out, vid_dur + timeline.extension_ms,
        input_has_audio);

    if (!run_asm(mux_cmd)) {
        if (!extended_input.empty()) std::filesystem::remove(extended_input);
        res.error = "ffmpeg mux failed";
        return res;
    }

    if (!extended_input.empty()) std::filesystem::remove(extended_input);

    if (progress_cb) progress_cb(total_steps, total_steps);

    res.success = true;
    return res;
}

} // namespace ffmpeg
