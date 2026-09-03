#include <iostream>
#include <string>
#include <filesystem>

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
static constexpr const char* kVersion = SRT_DUBBER_VERSION;

static void print_help() {
    std::cout <<
        "srt-dubber " << kVersion << "\n"
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
        "  srt-dubber subtitles.srt reference.mp4\n"
        "  srt-dubber --device 2 subtitles.srt\n"
        "  srt-dubber --resync updated.srt\n"
        "  srt-dubber --prepare narration.srt\n"
        "  srt-dubber --assemble narration.srt presentation.mp4\n"
        "  srt-dubber --assemble-with-takes subtitles.srt video.mp4 fixture-takes\n";
}

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        print_help();
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "--version") {
        std::cout << "srt-dubber " << kVersion << "\n";
        return 0;
    }

    if (argc == 2 && std::string(argv[1]) == "--list-devices") {
        AudioRecorder::list_devices();
        return 0;
    }

    if (argc >= 2 && std::string(argv[1]) == "--prepare") {
        if (argc != 3) {
            std::cerr << "Usage: srt-dubber --prepare <input.srt>\n";
            return 1;
        }

        const std::filesystem::path srt_path = argv[2];
        if (!std::filesystem::exists(srt_path)) {
            std::cerr << "Error: SRT file not found: " << srt_path << "\n";
            return 1;
        }

        auto project = core::Project::load_or_create(srt_path);
        ffmpeg::FfmpegProcessor processor;
        auto prepared = ffmpeg::prepare_takes(
            project.entries(), project.processed_dir(), processor,
            [](const std::string& line) { std::cout << line << "\n"; },
            false);
        project.save();
        if (!prepared.success || prepared.clips.size() != project.entries().size()) {
            std::cerr << "Error: "
                      << (prepared.error.empty() ? "record every narration cue before continuing"
                                                 : prepared.error)
                      << "\n";
            return 1;
        }

        std::cout << "Prepared " << prepared.clips.size() << " narration take(s).\n";
        return 0;
    }

    if (argc >= 2 && std::string(argv[1]) == "--assemble") {
        if (argc != 4) {
            std::cerr << "Usage: srt-dubber --assemble <input.srt> <video.mp4>\n";
            return 1;
        }

        const std::filesystem::path srt_path = argv[2];
        const std::filesystem::path video_path = argv[3];
        if (!std::filesystem::exists(srt_path) || !std::filesystem::exists(video_path)) {
            std::cerr << "Error: SRT or video file not found.\n";
            return 1;
        }

        auto project = core::Project::load_or_create(srt_path);
        ffmpeg::FfmpegProcessor processor;
        auto prepared = ffmpeg::prepare_takes(
            project.entries(), project.processed_dir(), processor,
            [](const std::string& line) { std::cout << line << "\n"; });
        project.save();
        if (!prepared.success || prepared.clips.size() != project.entries().size()) {
            std::cerr << "Error: "
                      << (prepared.error.empty() ? "not all narration cues have recorded takes"
                                                 : prepared.error)
                      << "\n";
            return 1;
        }

        const auto voiceover_path = project.output_dir() / "voiceover.wav";
        const auto output_path = project.dubbed_video_path();
        ffmpeg::FfmpegAssembler assembler;
        const auto assembled = assembler.assemble(
            prepared.clips, 0, video_path, voiceover_path, output_path, {});
        if (!assembled.success) {
            std::cerr << "Error: " << assembled.error << "\n";
            return 1;
        }

        std::cout << "Built: " << output_path << "\n";
        return 0;
    }

    if (argc >= 2 && std::string(argv[1]) == "--assemble-with-takes") {
        if (argc != 5) {
            std::cerr << "Usage: srt-dubber --assemble-with-takes <input.srt> <video.mp4> <takes-dir>\n";
            return 1;
        }

        const std::filesystem::path srt_path = argv[2];
        const std::filesystem::path video_path = argv[3];
        const std::filesystem::path takes_dir = argv[4];
        if (!std::filesystem::exists(srt_path) ||
            !std::filesystem::exists(video_path) ||
            !std::filesystem::is_directory(takes_dir)) {
            std::cerr << "Error: SRT, video, or takes directory not found.\n";
            return 1;
        }

        auto project = core::Project::load_or_create(srt_path);
        const auto import_error = ffmpeg::import_takes(project.entries(), takes_dir);
        if (!import_error.empty()) {
            std::cerr << "Error: " << import_error << "\n";
            return 1;
        }

        ffmpeg::FfmpegProcessor processor;
        auto prepared = ffmpeg::prepare_takes(
            project.entries(), project.processed_dir(), processor,
            [](const std::string& line) { std::cout << line << "\n"; });
        project.save();
        if (!prepared.success || prepared.clips.empty()) {
            std::cerr << "Error: " << (prepared.error.empty() ? "no takes to assemble" : prepared.error) << "\n";
            return 1;
        }

        const auto voiceover_path = project.output_dir() / "voiceover.wav";
        const auto output_path = project.dubbed_video_path();
        ffmpeg::FfmpegAssembler assembler;
        const auto assembled = assembler.assemble(
            prepared.clips, 0, video_path, voiceover_path, output_path, {});
        if (!assembled.success) {
            std::cerr << "Error: " << assembled.error << "\n";
            return 1;
        }

        std::cout << "Built: " << output_path << "\n";
        return 0;
    }

    // Parse optional --device N before the positional args.
    int device_index = -1;
    int first_pos = 1;   // index of first positional arg after flags

    if (argc >= 3 && std::string(argv[1]) == "--device") {
        try {
            device_index = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Error: --device requires an integer argument.\n";
            return 1;
        }
        first_pos = 3;
    }

    // Check for --resync flag
    if (first_pos < argc && std::string(argv[first_pos]) == "--resync") {
        if (first_pos + 1 >= argc) {
            std::cerr << "Error: --resync requires a new SRT file path.\n";
            std::cerr << "Usage: srt-dubber [--device N] --resync new.srt\n";
            return 1;
        }

        std::filesystem::path new_srt_path = argv[first_pos + 1];
        
        if (!std::filesystem::exists(new_srt_path)) {
            std::cerr << "Error: SRT file not found: " << new_srt_path << "\n";
            return 1;
        }

        // Determine project.json path (same directory as new SRT, named <stem>-project.json)
        std::filesystem::path project_path = 
            new_srt_path.parent_path() / (new_srt_path.stem().string() + "-project.json");
        
        if (!std::filesystem::exists(project_path)) {
            std::cerr << "Error: project.json not found: " << project_path << "\n";
            std::cerr << "Cannot resync without an existing project.\n";
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
        std::cout << "Resync complete.\n";
        std::cout << "  Matched (text):    " << result.matched_exact << "\n";
        std::cout << "  Matched (index):   " << result.matched_by_index << "\n";
        std::cout << "  New (no take):     " << result.new_entries << "\n";
        std::cout << "  Orphaned takes:    " << result.orphaned_takes << "\n";
        std::cout << "Total: " << result.total_new << " entries → project.json updated.\n";
        
        return 0;
    }

    if (first_pos >= argc) {
        std::cerr << "Usage: srt-dubber [--device N] <input.srt> [video.mp4]\n";
        std::cerr << "       srt-dubber [--device N] --resync new.srt\n";
        std::cerr << "       srt-dubber --list-devices\n";
        std::cerr << "       srt-dubber --version\n";
        return 1;
    }

    auto project = core::Project::load_or_create(argv[first_pos]);
    if (project.entries().empty()) {
        std::cerr << "Error: SRT contains no narration entries: " << argv[first_pos] << "\n";
        return 1;
    }

    std::filesystem::path video_path;
    if (first_pos + 1 < argc) {
        video_path = argv[first_pos + 1];
    }

    fprintf(stderr, "[audio] Use --list-devices to see available inputs. Use --device N to select one.\n");

    App app(project, video_path, device_index);
    app.run();

    return 0;
}
