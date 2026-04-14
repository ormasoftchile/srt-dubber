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
                 std::filesystem::path video_path = {});
    void run();

private:
    core::Project&          project_;
    std::filesystem::path   video_path_;
    AudioRecorder           recorder_;
    AudioPlayer             player_;

    enum class Screen { Session, Recording, Review, Assemble };
    Screen current_screen_  {Screen::Session};
    int    recording_start_ {0};   // entry index to open recording screen at
    int    review_selected_ {0};   // last selected row in review list
};
