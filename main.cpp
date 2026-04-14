#include <iostream>
#include <string>
#include <filesystem>

#include "srt/parser.hpp"
#include "core/project.hpp"
#include "tui/app.hpp"
#include "ffmpeg/processor.hpp"
#include "ffmpeg/assembler.hpp"

namespace fs = std::filesystem;

static void print_usage() {
    std::cerr <<
        "Usage:\n"
        "  srt-dubber record  <input.srt>              # Launch TUI to record takes\n"
        "  srt-dubber process <input.srt>              # Trim, normalize, fit all takes\n"
        "  srt-dubber assemble <input.srt> <video.mp4> # Assemble takes into output.mp4\n"
        "  srt-dubber full    <input.srt> <video.mp4>  # record → process → assemble\n";
}

// ---------------------------------------------------------------------------
// process command — batch, no TUI
// ---------------------------------------------------------------------------
static int cmd_process(const fs::path& srt_path) {
    auto subtitles = SrtParser::parse(srt_path);
    if (subtitles.empty()) {
        std::cerr << "No subtitles found in " << srt_path << "\n";
        return 1;
    }

    Processor processor;
    int done = 0;
    for (const auto& sub : subtitles) {
        fs::path take = fs::path("takes") / (std::to_string(sub.index) + ".wav");
        if (!fs::exists(take)) {
            std::cout << "  [skip] take " << sub.index << " — file not found\n";
            continue;
        }
        fs::path out = fs::path("processed") / (std::to_string(sub.index) + ".wav");
        fs::create_directories("processed");

        std::cout << "  [" << sub.index << "/" << subtitles.size() << "] processing " << take << " ...\n";
        processor.process(take, out, sub);
        ++done;
    }
    std::cout << "Done. Processed " << done << " take(s).\n";
    return 0;
}

// ---------------------------------------------------------------------------
// assemble command — batch, no TUI
// ---------------------------------------------------------------------------
static int cmd_assemble(const fs::path& srt_path, const fs::path& video_path) {
    auto subtitles = SrtParser::parse(srt_path);
    if (subtitles.empty()) {
        std::cerr << "No subtitles found in " << srt_path << "\n";
        return 1;
    }

    fs::create_directories("output");

    Assembler assembler;
    std::cout << "Assembling narration track ...\n";
    fs::path voiceover = assembler.assemble(subtitles, "processed", "output/voiceover.wav");

    std::cout << "Muxing into output/output.mp4 ...\n";
    assembler.mux(video_path, voiceover, "output/output.mp4");

    std::cout << "Done. Output: output/output.mp4\n";
    return 0;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "record" && argc == 3) {
        auto project = Project::load_or_create(argv[2]);
        App app(project);
        app.run();
        return 0;

    } else if (cmd == "process" && argc == 3) {
        return cmd_process(argv[2]);

    } else if (cmd == "assemble" && argc == 4) {
        return cmd_assemble(argv[2], argv[3]);

    } else if (cmd == "full" && argc == 4) {
        // Phase 1: interactive recording
        {
            auto project = Project::load_or_create(argv[2]);
            App app(project);
            app.run();
        }
        // Phase 2: batch process
        if (int rc = cmd_process(argv[2]); rc != 0) return rc;
        // Phase 3: assemble
        return cmd_assemble(argv[2], argv[3]);

    } else {
        print_usage();
        return 1;
    }
}
