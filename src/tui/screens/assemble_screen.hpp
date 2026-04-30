#pragma once

#include "core/project.hpp"
#include "tui/screen_action.hpp"

#include <filesystem>

namespace tui {

/// Screen 4 — assembly progress log.
/// Wired to core::AssembleFlow state machine. Auto-starts assembly on entry.
/// Assembly thread posts commands via a shared queue; renderer reads from
/// assemble_render_state(). Keys: [q] back to session
ScreenAction run_assemble_screen(core::Project& project,
                                 const std::filesystem::path& video_path);

} // namespace tui
