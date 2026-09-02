#include "core/project.hpp"
#include "ffmpeg/assembler.hpp"
#include "ffmpeg/take_pipeline.hpp"

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

static void test_mux_command_selects_voiceover_audio() {
    const auto command = ffmpeg::build_mux_command(
        "source.mp4", "voiceover.wav", "output.mp4", 12345);

    ASSERT_TRUE(command.find("-map 0:v:0") != std::string::npos);
    ASSERT_TRUE(command.find("-map 1:a:0") != std::string::npos);
    ASSERT_TRUE(command.find("-map_metadata 0") != std::string::npos);
    ASSERT_TRUE(command.find("-t 12.345") != std::string::npos);
}

int main() {
    test_processes_raw_take_and_updates_entry();
    test_reuses_existing_processed_take();
    test_mux_command_selects_voiceover_audio();

    if (failures == 0) {
        std::printf("All take pipeline tests passed.\n");
        return 0;
    }
    return 1;
}