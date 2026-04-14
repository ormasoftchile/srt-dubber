#pragma once

#include "core/project.hpp"
#include "tui/screen_action.hpp"

namespace tui {

/// Screen 1 — project summary and navigation menu.
/// Keys: [r] Record  [v] Review  [a] Assemble  [q] Quit
ScreenAction run_session_screen(core::Project& project);

} // namespace tui
