#pragma once

#include "core/project.hpp"
#include "audio/recorder.hpp"
#include "audio/player.hpp"
#include "tui/screen_action.hpp"
#include "tui/navigate.hpp"

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"

namespace tui {

/// Screen 2 — per-subtitle recording flow.
/// Keys: [r] record  [s] stop  [p] play  [x] redo  [n] next  [b] back  [q] save&quit
ScreenAction run_recording_screen(core::Project& project,
                                  AudioRecorder&  recorder,
                                  AudioPlayer&    player,
                                  int             start_index = 0);

/// Component factory for the recording screen.
ftxui::Component make_recording_component(
    core::Project& project,
    AudioRecorder& recorder,
    AudioPlayer& player,
    int start_index,
    ftxui::ScreenInteractive& screen,
    NavigateFunc navigate);

} // namespace tui
