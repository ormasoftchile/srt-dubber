#pragma once

#include "core/project.hpp"
#include "tui/screen_action.hpp"
#include "tui/navigate.hpp"

#include "ftxui/component/component_base.hpp"
#include "ftxui/component/screen_interactive.hpp"

#include <filesystem>

namespace tui {

/// Screen 4 — assembly progress log.
/// Wired to core::AssembleFlow state machine. Auto-starts assembly on entry.
/// Assembly thread posts commands via a shared queue; renderer reads from
/// assemble_render_state(). Keys: [q] back to session
ScreenAction run_assemble_screen(core::Project& project,
                                 const std::filesystem::path& video_path);

/// Component factory for the assemble screen.
ftxui::Component make_assemble_component(
    core::Project& project,
    const std::filesystem::path& video_path,
    ftxui::ScreenInteractive& screen,
    NavigateFunc navigate);

} // namespace tui
