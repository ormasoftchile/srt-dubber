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

    if (argc < 2) {
        std::cerr << "Usage: srt-dubber <input.srt> [video.mp4]\n";
        std::cerr << "       srt-dubber --list-devices\n";
        return 1;
    }

    auto project = core::Project::load_or_create(argv[1]);
    
    std::filesystem::path video_path;
    if (argc >= 3) {
        video_path = argv[2];
    }

    App app(project, video_path);
    app.run();
    
    return 0;
}
