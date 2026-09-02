#include "core/project.hpp"
#include "ffmpeg/assembler.hpp"
#include "ffmpeg/processor.hpp"
#include "ffmpeg/take_pipeline.hpp"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

static int failures = 0;

#define ASSERT_TRUE(condition) do { \
    if (!(condition)) { \
        std::fprintf(stderr, "FAIL: %s:%d  %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (false)

#define ASSERT_EQ(actual, expected) do { \
    if ((actual) != (expected)) { \
        std::fprintf(stderr, "FAIL: %s:%d  %s != %s\n", __FILE__, __LINE__, #actual, #expected); \
        ++failures; \
    } \
} while (false)

class FakeProcessor final : public ffmpeg::TakeProcessor {
public:
    ffmpeg::ProcessResult next_result;
    int calls = 0;

    ffmpeg::ProcessResult process_take(
        const std::filesystem::path&,
        const std::filesystem::path& output,
        int64_t) override
    {
        ++calls;
        if (next_result.success) {
            std::ofstream(output) << "processed";
        }
        return next_result;
    }
};

static void write_test_wav(const std::filesystem::path& path) {
    constexpr std::uint32_t sample_rate = 44100;
    std::vector<std::int16_t> samples;
    auto append_silence = [&](double seconds) {
        samples.insert(samples.end(), static_cast<std::size_t>(sample_rate * seconds), 0);
    };
    auto append_signal = [&](double seconds) {
        const auto count = static_cast<std::size_t>(sample_rate * seconds);
        for (std::size_t i = 0; i < count; ++i)
            samples.push_back((i / 50) % 2 == 0 ? 8000 : -8000);
    };
    append_silence(0.5);
    append_signal(0.5);
    append_silence(0.8);
    append_signal(0.5);
    append_silence(0.5);

    const std::uint32_t data_size = static_cast<std::uint32_t>(samples.size() * sizeof(std::int16_t));
    const std::uint32_t riff_size = 36 + data_size;
    const std::uint16_t format = 1;
    const std::uint16_t channels = 1;
    const std::uint32_t byte_rate = sample_rate * sizeof(std::int16_t);
    const std::uint16_t block_align = sizeof(std::int16_t);
    const std::uint16_t bits_per_sample = 16;

    std::ofstream out(path, std::ios::binary);
    out.write("RIFF", 4);
    out.write(reinterpret_cast<const char*>(&riff_size), sizeof(riff_size));
    out.write("WAVEfmt ", 8);
    const std::uint32_t fmt_size = 16;
    out.write(reinterpret_cast<const char*>(&fmt_size), sizeof(fmt_size));
    out.write(reinterpret_cast<const char*>(&format), sizeof(format));
    out.write(reinterpret_cast<const char*>(&channels), sizeof(channels));
    out.write(reinterpret_cast<const char*>(&sample_rate), sizeof(sample_rate));
    out.write(reinterpret_cast<const char*>(&byte_rate), sizeof(byte_rate));
    out.write(reinterpret_cast<const char*>(&block_align), sizeof(block_align));
    out.write(reinterpret_cast<const char*>(&bits_per_sample), sizeof(bits_per_sample));
    out.write("data", 4);
    out.write(reinterpret_cast<const char*>(&data_size), sizeof(data_size));
    out.write(reinterpret_cast<const char*>(samples.data()), data_size);
}

static std::int16_t max_sample_in_last_quarter(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    in.seekg(12);
    while (in) {
        char id[4]{};
        std::uint32_t size = 0;
        in.read(id, 4);
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (!in) break;
        if (std::string(id, 4) == "data") {
            std::vector<std::int16_t> samples(size / sizeof(std::int16_t));
            in.read(reinterpret_cast<char*>(samples.data()), size);
            std::int16_t maximum = 0;
            for (std::size_t i = samples.size() * 3 / 4; i < samples.size(); ++i) {
                const auto magnitude = static_cast<std::int16_t>(std::abs(samples[i]));
                if (magnitude > maximum) maximum = magnitude;
            }
            return maximum;
        }
        in.seekg(size + (size & 1), std::ios::cur);
    }
    return 0;
}

static void test_processes_raw_take_and_updates_entry() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "srt-dubber-take-pipeline-test";
    fs::remove_all(root);
    fs::create_directories(root / "takes");
    std::ofstream(root / "takes" / "1.wav") << "raw";

    std::vector<core::ProjectEntry> entries(1);
    entries[0].index = 1;
    entries[0].start_ms = 1200;
    entries[0].slot_duration_ms = 2500;
    entries[0].raw_take_path = (root / "takes" / "1.wav").string();

    FakeProcessor processor;
    processor.next_result = {
        .success = true,
        .duration_ms = 2400,
        .was_stretched = true,
    };

    const auto result = ffmpeg::prepare_takes(entries, root / "processed", processor);

    ASSERT_TRUE(result.success);
    ASSERT_EQ(processor.calls, 1);
    ASSERT_EQ(result.clips.size(), std::size_t{1});
    ASSERT_EQ(result.clips[0].start_ms, int64_t{1200});
    ASSERT_EQ(entries[0].processed_duration_ms, int64_t{2400});
    ASSERT_EQ(entries[0].status, core::TakeStatus::stretched);
    ASSERT_TRUE(fs::exists(entries[0].processed_take_path));
    fs::remove_all(root);
}

static void test_reuses_existing_processed_take() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "srt-dubber-take-pipeline-reuse-test";
    fs::remove_all(root);
    fs::create_directories(root / "processed");
    const auto processed = root / "processed" / "2.wav";
    std::ofstream(processed) << "processed";

    std::vector<core::ProjectEntry> entries(1);
    entries[0].index = 2;
    entries[0].processed_take_path = processed.string();

    FakeProcessor processor;
    const auto result = ffmpeg::prepare_takes(entries, root / "processed", processor);

    ASSERT_TRUE(result.success);
    ASSERT_EQ(processor.calls, 0);
    ASSERT_EQ(result.clips.size(), std::size_t{1});
    fs::remove_all(root);
}

static void test_timeline_plan_extends_overflowing_slots() {
    const std::vector<ffmpeg::ProcessedClip> clips = {
        {"1.wav", 0,     1, 3374, 3600},
        {"2.wav", 13247, 2, 2337, 1543},
        {"3.wav", 14790, 3, 2713, 2504},
        {"4.wav", 17294, 4, 3745, 4400},
    };

    const auto plan = ffmpeg::build_timeline_plan(clips);

    ASSERT_EQ(plan.holds.size(), std::size_t{2});
    ASSERT_EQ(plan.holds[0].source_time_ms, int64_t{14790});
    ASSERT_EQ(plan.holds[0].duration_ms, int64_t{794});
    ASSERT_EQ(plan.holds[1].source_time_ms, int64_t{17294});
    ASSERT_EQ(plan.holds[1].duration_ms, int64_t{209});
    ASSERT_EQ(plan.clip_start_ms[1], int64_t{13247});
    ASSERT_EQ(plan.clip_start_ms[2], int64_t{15584});
    ASSERT_EQ(plan.clip_start_ms[3], int64_t{18297});
    ASSERT_EQ(plan.extension_ms, int64_t{1003});
}

static void test_imports_takes_by_srt_index() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "srt-dubber-take-import-test";
    fs::remove_all(root);
    fs::create_directories(root);
    std::ofstream(root / "3.wav") << "first";
    std::ofstream(root / "7.wav") << "second";

    std::vector<core::ProjectEntry> entries(2);
    entries[0].index = 3;
    entries[1].index = 7;

    const auto error = ffmpeg::import_takes(entries, root);

    ASSERT_TRUE(error.empty());
    ASSERT_EQ(entries[0].raw_take_path, (root / "3.wav").string());
    ASSERT_EQ(entries[1].raw_take_path, (root / "7.wav").string());
    fs::remove_all(root);
}

static void test_mux_command_selects_voiceover_when_source_has_no_audio() {
    const auto command = ffmpeg::build_mux_command(
        "source.mp4", "voiceover.wav", "output.mp4", 12345, false);

    ASSERT_TRUE(command.find("-map 0:v:0") != std::string::npos);
    ASSERT_TRUE(command.find("-map 1:a:0") != std::string::npos);
    ASSERT_TRUE(command.find("-map_metadata 0") != std::string::npos);
    ASSERT_TRUE(command.find("-t 12.345") != std::string::npos);
}

static void test_mux_command_mixes_source_audio_with_narration() {
    const auto command = ffmpeg::build_mux_command(
        "source.mp4", "voiceover.wav", "output.mp4", 12345, true);

    ASSERT_TRUE(command.find("[0:a:0][1:a:0]") != std::string::npos);
    ASSERT_TRUE(command.find("amix=inputs=2:normalize=0") != std::string::npos);
    ASSERT_TRUE(command.find("alimiter=limit=0.95[mixed]") != std::string::npos);
    ASSERT_TRUE(command.find("-map \"[mixed]\"") != std::string::npos);
}

static void test_timeline_extension_holds_video_and_source_audio() {
    const std::vector<ffmpeg::TimelineHold> holds = {
        {14790, 794},
        {17294, 209},
    };

    const auto command = ffmpeg::build_timeline_extension_command(
        "source.mp4", "extended.mp4", holds, true);

    ASSERT_TRUE(command.find("trim=start=0.000:end=14.790") != std::string::npos);
    ASSERT_TRUE(command.find("tpad=stop_mode=clone:stop_duration=0.794") != std::string::npos);
    ASSERT_TRUE(command.find("apad=pad_dur=0.794") != std::string::npos);
    ASSERT_TRUE(command.find("tpad=stop_mode=clone:stop_duration=0.209") != std::string::npos);
    ASSERT_TRUE(command.find("apad=pad_dur=0.209") != std::string::npos);
    ASSERT_TRUE(command.find("concat=n=3:v=1:a=0[vout]") != std::string::npos);
    ASSERT_TRUE(command.find("concat=n=3:v=0:a=1[aout]") != std::string::npos);
}

static void test_processing_preserves_signal_after_internal_pause() {
    namespace fs = std::filesystem;
    const auto root = fs::temp_directory_path() / "srt-dubber-processor-pause-test";
    fs::remove_all(root);
    fs::create_directories(root);
    const auto input = root / "input.wav";
    const auto output = root / "output.wav";
    write_test_wav(input);

    ffmpeg::FfmpegProcessor processor;
    const auto result = processor.process_take(input, output, 5000);

    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.duration_ms > 1400);
    ASSERT_TRUE(result.duration_ms < 2300);
    ASSERT_TRUE(max_sample_in_last_quarter(output) > 1000);
    fs::remove_all(root);
}

int main() {
    test_processes_raw_take_and_updates_entry();
    test_reuses_existing_processed_take();
    test_timeline_plan_extends_overflowing_slots();
    test_imports_takes_by_srt_index();
    test_mux_command_selects_voiceover_when_source_has_no_audio();
    test_mux_command_mixes_source_audio_with_narration();
    test_timeline_extension_holds_video_and_source_audio();
    test_processing_preserves_signal_after_internal_pause();

    if (failures == 0) {
        std::printf("All take pipeline tests passed.\n");
        return 0;
    }
    return 1;
}