#pragma once

#include "core/project.hpp"
#include "audio/recorder.hpp"
#include "audio/player.hpp"
#include "tui/screen_action.hpp"

#include <filesystem>

/// Top-level TUI controller. Owns the audio devices and drives screen navigation.
class App {
public:
    explicit App(core::Project& project,
                 std::filesystem::path video_path = {},
                 int device_index = -1);
    void run();

private:
    core::Project&          project_;
    std::filesystem::path   video_path_;
    AudioRecorder           recorder_;
    AudioPlayer             player_;


};
