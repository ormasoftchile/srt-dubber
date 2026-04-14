#pragma once

#include "core/project.hpp"
#include "audio/player.hpp"
#include "tui/screen_action.hpp"

namespace tui {

/// Screen 3 — scrollable review list.
/// Keys: [↑/↓] navigate  [enter] redo entry  [p] play take  [q] back
ScreenAction run_review_screen(core::Project& project,
                               AudioPlayer&   player,
                               int&           selected_index);

} // namespace tui
