#pragma once

#include "core/project.hpp"
#include "tui/screen_action.hpp"

#include <functional>

namespace tui {

/// Screen 4 — assembly progress log.
/// The caller starts assembly in a background thread; this screen displays
/// project.assemble_log as lines accumulate and project.assemble_complete
/// when done.
/// Keys: [q] back to session
ScreenAction run_assemble_screen(core::Project& project);

} // namespace tui
