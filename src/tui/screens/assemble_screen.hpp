#pragma once

#include "core/project.hpp"
#include "tui/screen_action.hpp"

#include <filesystem>

namespace tui {

/// Screen 4 — assembly progress log.
/// Launches the assembly thread internally, streams progress into
/// project.assemble_log, and flips project.assemble_complete when done.
/// Keys: [q] back to session
ScreenAction run_assemble_screen(core::Project& project,
                                 const std::filesystem::path& video_path);

} // namespace tui
