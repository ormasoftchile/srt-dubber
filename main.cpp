#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <string>
#include <string_view>

#include "srt/parser.hpp"
#include "core/project.hpp"
#include "core/resync.hpp"
#include "ffmpeg/assembler.hpp"
#include "ffmpeg/processor.hpp"
#include "ffmpeg/take_pipeline.hpp"
#include "tui/app.hpp"
#include "audio/recorder.hpp"

#ifndef SRT_DUBBER_VERSION
#define SRT_DUBBER_VERSION "0.0.0-dev"
#endif
static constexpr std::string_view kVersion = SRT_DUBBER_VERSION;

static void print_help() {
    std::println(
        "srt-dubber {}\n"
        "Voice-over dubbing tool for SRT subtitle files.\n"
        "\n"
        "USAGE:\n"
        "  srt-dubber [OPTIONS] <input.srt> [video.mp4]\n"
        "  srt-dubber [OPTIONS] --resync <new.srt>\n"
        "  srt-dubber --prepare <input.srt>\n"
        "  srt-dubber --assemble <input.srt> <video.mp4>\n"
        "  srt-dubber --assemble-with-takes <input.srt> <video.mp4> <takes-dir>\n"
        "\n"
        "ARGUMENTS:\n"
        "  <input.srt>   Path to the SRT subtitle file (required)\n"
        "  [video.mp4]   Source video used when assembling the final MP4\n"
        "\n"
        "OPTIONS:\n"
        "  --device N        Select audio input device by index (see --list-devices)\n"
        "  --countdown-ms N  Duration of each countdown beat in ms (default: 650, 0 to disable)\n"
        "  --resync <new.srt> Re-sync an existing project to a new SRT file\n"
        "  --prepare          Process and measure recorded takes without fitting SRT slots\n"
        "  --assemble         Assemble an existing project with a recorded video\n"
        "  --assemble-with-takes  Import N.wav files and assemble without the TUI\n"
        "  --list-devices    List available audio input devices and exit\n"
        "  --version         Print version and exit\n"
        "  --help, -h        Show this help and exit\n"
        "\n"
        "EXAMPLES:\n"
        "  srt-dubber subtitles.srt\n"
        "  srt-dubber --countdown-ms 300 subtitles.srt\n"
        "  srt-dubber subtitles.srt reference.mp4\n"
        "  srt-dubber --device 2 subtitles.srt\n"
        "  srt-dubber --resync updated.srt\n"
        "  srt-dubber --prepare narration.srt\n"
        "  srt-dubber --assemble narration.srt presentation.mp4\n"
        "  srt-dubber --assemble-with-takes subtitles.srt video.mp4 fixture-takes",
        kVersion
    );
}

int main(int argc, char* argv[]) {
    if (argc == 2) {
        std::string_view arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h") {
            print_help();
            return 0;
        }
        if (arg1 == "--version") {
            std::println("srt-dubber {}", kVersion);
            return 0;
        }
        if (arg1 == "--list-devices") {
            AudioRecorder::list_devices();
            return 0;
        }
    }

    if (argc >= 2 && std::string_view(argv[1]) == "--prepare") {
        if (argc != 3) {
            std::println(stderr, "Usage: srt-dubber --prepare <input.srt>");
            return 1;
        }

        const std::filesystem::path srt_path = argv[2];
        if (!std::filesystem::exists(srt_path)) {
            std::println(stderr, "Error: SRT file not found: {}", srt_path.string());
            return 1;
        }

        auto project = core::Project::load_or_create(srt_path);
        ffmpeg::FfmpegProcessor processor;
        auto prepared = ffmpeg::prepare_takes(
            project.entries(), project.processed_dir(), processor,
            [](const std::string& line) { std::println("{}", line); },
            false);
        project.save();
        if (!prepared.success || prepared.clips.size() != project.entries().size()) {
            std::println(stderr, "Error: {}",
                         prepared.error.empty() ? "record every narration cue before continuing"
                                                : prepared.error);
            return 1;
        }

        std::println("Prepared {} narration take(s).", prepared.clips.size());
        return 0;
    }

    if (argc >= 2 && std::string_view(argv[1]) == "--assemble") {
        if (argc != 4) {
            std::println(stderr, "Usage: srt-dubber --assemble <input.srt> <video.mp4>");
            return 1;
        }

        const std::filesystem::path srt_path = argv[2];
        const std::filesystem::path video_path = argv[3];
        if (!std::filesystem::exists(srt_path)) {
            std::println(stderr, "Error: SRT file not found: {}", srt_path.string());
            return 1;
        }
        if (!std::filesystem::exists(video_path)) {
            std::println(stderr, "Error: Video file not found: {}", video_path.string());
            return 1;
        }

        auto project = core::Project::load_or_create(srt_path);
        ffmpeg::FfmpegProcessor processor;
        auto prepared = ffmpeg::prepare_takes(
            project.entries(), project.processed_dir(), processor,
            [](const std::string& line) { std::println("{}", line); });
        project.save();
        if (!prepared.success || prepared.clips.size() != project.entries().size()) {
            std::println(stderr, "Error: {}",
                         prepared.error.empty() ? "not all narration cues have recorded takes"
                                                : prepared.error);
            return 1;
        }

        const auto voiceover_path = project.output_dir() / "voiceover.wav";
        const auto output_path = project.dubbed_video_path();
        ffmpeg::FfmpegAssembler assembler;
        const auto assembled = assembler.assemble(
            prepared.clips, 0, video_path, voiceover_path, output_path, {});
        if (!assembled.success) {
            std::println(stderr, "Error: {}", assembled.error);
            return 1;
        }

        std::println("Built: {}", output_path.string());
        return 0;
    }

    if (argc >= 2 && std::string_view(argv[1]) == "--assemble-with-takes") {
        if (argc != 5) {
            std::println(stderr, "Usage: srt-dubber --assemble-with-takes <input.srt> <video.mp4> <takes-dir>");
            return 1;
        }

        const std::filesystem::path srt_path = argv[2];
        const std::filesystem::path video_path = argv[3];
        const std::filesystem::path takes_dir = argv[4];
        if (!std::filesystem::exists(srt_path)) {
            std::println(stderr, "Error: SRT file not found: {}", srt_path.string());
            return 1;
        }
        if (!std::filesystem::exists(video_path)) {
            std::println(stderr, "Error: Video file not found: {}", video_path.string());
            return 1;
        }
        if (!std::filesystem::is_directory(takes_dir)) {
            std::println(stderr, "Error: Takes directory not found: {}", takes_dir.string());
            return 1;
        }

        auto project = core::Project::load_or_create(srt_path);
        const auto import_error = ffmpeg::import_takes(project.entries(), takes_dir);
        if (!import_error.empty()) {
            std::println(stderr, "Error: {}", import_error);
            return 1;
        }

        ffmpeg::FfmpegProcessor processor;
        auto prepared = ffmpeg::prepare_takes(
            project.entries(), project.processed_dir(), processor,
            [](const std::string& line) { std::println("{}", line); });
        project.save();
        if (!prepared.success || prepared.clips.empty()) {
            std::println(stderr, "Error: {}", prepared.error.empty() ? "no takes to assemble" : prepared.error);
            return 1;
        }

        const auto voiceover_path = project.output_dir() / "voiceover.wav";
        const auto output_path = project.dubbed_video_path();
        ffmpeg::FfmpegAssembler assembler;
        const auto assembled = assembler.assemble(
            prepared.clips, 0, video_path, voiceover_path, output_path, {});
        if (!assembled.success) {
            std::println(stderr, "Error: {}", assembled.error);
            return 1;
        }

        std::println("Built: {}", output_path.string());
        return 0;
    }

    // Parse optional flags (--device N, --countdown-ms N) before positional args.
    int device_index = -1;
    int countdown_ms = 650; // default countdown (650ms per beat, ~2.0s total)

    if (const char* env_cd = std::getenv("SRT_COUNTDOWN_MS")) {
        try {
            countdown_ms = std::max(0, std::stoi(env_cd));
        } catch (...) {}
    }

    int arg_i = 1;
    while (arg_i < argc) {
        const std::string_view arg = argv[arg_i];
        if (arg == "--device") {
            if (arg_i + 1 >= argc) {
                std::println(stderr, "Error: --device requires an integer argument.");
                return 1;
            }
            std::string_view dev_str = argv[arg_i + 1];
            auto [ptr, ec] = std::from_chars(dev_str.data(), dev_str.data() + dev_str.size(), device_index);
            if (ec != std::errc{} || ptr != dev_str.data() + dev_str.size()) {
                std::println(stderr, "Error: --device requires an integer argument.");
                return 1;
            }
            arg_i += 2;
        } else if (arg == "--countdown-ms") {
            if (arg_i + 1 >= argc) {
                std::println(stderr, "Error: --countdown-ms requires an integer argument (ms per count).");
                return 1;
            }
            std::string_view cd_str = argv[arg_i + 1];
            auto [ptr, ec] = std::from_chars(cd_str.data(), cd_str.data() + cd_str.size(), countdown_ms);
            if (ec != std::errc{} || ptr != cd_str.data() + cd_str.size()) {
                std::println(stderr, "Error: --countdown-ms requires an integer argument (ms per count).");
                return 1;
            }
            countdown_ms = std::max(0, countdown_ms);
            arg_i += 2;
        } else {
            break;
        }
    }
    int first_pos = arg_i;

    // Check for --resync flag
    if (first_pos < argc && std::string_view(argv[first_pos]) == "--resync") {
        if (first_pos + 1 >= argc) {
            std::println(stderr, "Error: --resync requires a new SRT file path.");
            std::println(stderr, "Usage: srt-dubber [OPTIONS] --resync new.srt");
            return 1;
        }

        std::filesystem::path new_srt_path = argv[first_pos + 1];
        
        if (!std::filesystem::exists(new_srt_path)) {
            std::println(stderr, "Error: SRT file not found: {}", new_srt_path.string());
            return 1;
        }

        // Determine project.json path (same directory as new SRT, named <stem>-project.json)
        std::filesystem::path project_path = 
            new_srt_path.parent_path() / (new_srt_path.stem().string() + "-project.json");
        
        if (!std::filesystem::exists(project_path)) {
            std::println(stderr, "Error: project.json not found: {}", project_path.string());
            std::println(stderr, "Cannot resync without an existing project.");
            return 1;
        }

        // Load existing project
        auto project = core::Project::load_or_create(new_srt_path);
        
        // Parse new SRT
        auto new_entries = srt::SrtParser::parse(new_srt_path);
        
        // Perform resync
        auto result = core::resync(project, new_entries);
        
        // Save updated project
        project.save();
        
        // Print summary
        std::println("Resync complete.");
        std::println("  Matched (text):    {}", result.matched_exact);
        std::println("  Matched (index):   {}", result.matched_by_index);
        std::println("  New (no take):     {}", result.new_entries);
        std::println("  Orphaned takes:    {}", result.orphaned_takes);
        std::println("Total: {} entries → project.json updated.", result.total_new);
        
        return 0;
    }

    if (first_pos >= argc) {
        std::println(stderr, "Usage: srt-dubber [OPTIONS] <input.srt> [video.mp4]");
        std::println(stderr, "       srt-dubber [OPTIONS] --resync new.srt");
        std::println(stderr, "       srt-dubber --list-devices");
        std::println(stderr, "       srt-dubber --version");
        return 1;
    }

    auto project = core::Project::load_or_create(argv[first_pos]);
    if (project.entries().empty()) {
        std::println(stderr, "Error: SRT contains no narration entries: {}", argv[first_pos]);
        return 1;
    }

    std::filesystem::path video_path;
    if (first_pos + 1 < argc) {
        video_path = argv[first_pos + 1];
    }

    std::println(stderr, "[audio] Use --list-devices to see available inputs. Use --device N to select one.");

    App app(project, video_path, device_index, countdown_ms);
    app.run();

    return 0;
}
