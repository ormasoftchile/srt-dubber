#pragma once

#include "core/project.hpp"
#include "tui/screen_action.hpp"
#include "tui/navigate.hpp"

#include "ftxui/component/component_base.hpp"

namespace tui {

/// Screen 1 — project summary and navigation menu.
/// Keys: [r] Record  [v] Review  [a] Assemble  [q] Quit
ScreenAction run_session_screen(core::Project& project);

/// Component factory for the session screen.
ftxui::Component make_session_component(core::Project& project,
                                         NavigateFunc navigate);

} // namespace tui
