#include <iostream>
#include <string>
#include <filesystem>

#include "srt/parser.hpp"
#include "core/project.hpp"
#include "tui/app.hpp"
#include "audio/recorder.hpp"

int main(int argc, char* argv[]) {
    if (argc == 2 && std::string(argv[1]) == "--list-devices") {
        AudioRecorder::list_devices();
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

    if (first_pos >= argc) {
        std::cerr << "Usage: srt-dubber [--device N] <input.srt> [video.mp4]\n";
        std::cerr << "       srt-dubber --list-devices\n";
        return 1;
    }

    auto project = core::Project::load_or_create(argv[first_pos]);

    std::filesystem::path video_path;
    if (first_pos + 1 < argc) {
        video_path = argv[first_pos + 1];
    }

    fprintf(stderr, "[audio] Use --list-devices to see available inputs. Use --device N to select one.\n");

    App app(project, video_path, device_index);
    app.run();

    return 0;
}
