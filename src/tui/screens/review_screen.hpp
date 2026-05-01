#pragma once

#include "core/project.hpp"
#include "audio/player.hpp"
#include "tui/screen_action.hpp"
#include "tui/navigate.hpp"

#include "ftxui/component/component_base.hpp"

namespace tui {

/// Screen 3 — scrollable review list.
/// Keys: [↑/↓] navigate  [enter] redo entry  [p] play take  [q] back
ScreenAction run_review_screen(core::Project& project,
                               AudioPlayer&   player,
                               int&           selected_index);

/// Component factory for the review screen.
ftxui::Component make_review_component(
    core::Project& project,
    AudioPlayer& player,
    int start_index,
    NavigateFunc navigate);

} // namespace tui
