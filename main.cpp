#include <iostream>
#include <string>
#include <filesystem>

#include "srt/parser.hpp"
#include "core/project.hpp"
#include "core/resync.hpp"
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
