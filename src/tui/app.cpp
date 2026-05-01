#include "tui/app.hpp"

#include "core/app_session.hpp"
#include "tui/screens/session_screen.hpp"
#include "tui/screens/recording_screen.hpp"
#include "tui/screens/review_screen.hpp"
#include "tui/screens/assemble_screen.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

using namespace ftxui;

// ── Helper: build component for current NavState ─────────────────────────────

static Component build_component(
    const core::NavState& nav,
    ScreenInteractive& screen,
    tui::NavigateFunc& navigate,
    core::Project& project,
    AudioRecorder& recorder,
    AudioPlayer& player,
    const std::filesystem::path& video_path)
{
    switch (nav.screen) {
        case core::AppScreen::Session:
            return tui::make_session_component(project, navigate);
        case core::AppScreen::Recording:
            return tui::make_recording_component(project, recorder, player,
                                                 nav.recording_idx, screen, navigate);
        case core::AppScreen::Review:
            return tui::make_review_component(project, player, nav.review_idx, navigate);
        case core::AppScreen::Assemble:
            return tui::make_assemble_component(project, video_path, screen, navigate);
    }
    return nullptr;
}

// ── App implementation ────────────────────────────────────────────────────────

::App::App(core::Project& project, std::filesystem::path video_path, int device_index)
    : project_(project), video_path_(std::move(video_path)), recorder_(device_index) {}

void ::App::run() {
    auto screen = ScreenInteractive::Fullscreen();
    core::NavState nav;
    
    Component active;
    
    // Navigate callback — swaps active component without exiting the event loop.
    tui::NavigateFunc navigate = [&](tui::ScreenAction action, int payload) {
        // Map ScreenAction to NavCmd
        core::NavCmd cmd = core::NavCmd::Quit;
        
        // Special-case logic for context-dependent transitions
        if (action == tui::ScreenAction::GoRecord && nav.screen == core::AppScreen::Review) {
            cmd = core::NavCmd::ReviewGoRecord;
        } else if (action == tui::ScreenAction::GoSession && nav.screen == core::AppScreen::Review) {
            cmd = core::NavCmd::ReviewGoSession;
        } else if (action == tui::ScreenAction::GoSession && nav.screen == core::AppScreen::Recording) {
            cmd = core::NavCmd::RecordingDone;
        } else if (action == tui::ScreenAction::GoSession && nav.screen == core::AppScreen::Assemble) {
            cmd = core::NavCmd::AssembleDone;
        } else {
            // Default mapping
            switch (action) {
                case tui::ScreenAction::GoRecord:   cmd = core::NavCmd::GoRecord;   break;
                case tui::ScreenAction::GoReview:   cmd = core::NavCmd::GoReview;   break;
                case tui::ScreenAction::GoAssemble: cmd = core::NavCmd::GoAssemble; break;
                case tui::ScreenAction::GoSession:  cmd = core::NavCmd::RecordingDone; break;
                case tui::ScreenAction::Quit:       cmd = core::NavCmd::Quit;       break;
                default: break;
            }
        }
        
        nav = core::nav_step(nav, cmd, payload).next;
        
        if (!nav.running) {
            screen.ExitLoopClosure()();
            return;
        }
        
        // Build the new screen's component
        active = build_component(nav, screen, navigate, project_, recorder_, player_, video_path_);
        
        // Trigger a re-render
        screen.PostEvent(Event::Custom);
    };
    
    // Build initial component (session screen)
    active = build_component(nav, screen, navigate, project_, recorder_, player_, video_path_);
    
    // Router: delegates to whatever `active` currently is
    auto router = Renderer([&]() -> Element {
        return active ? active->Render() : text("");
    });
    router = CatchEvent(router, [&](Event e) -> bool {
        return active ? active->OnEvent(e) : false;
    });
    
    // ONE call to Loop() — never exits until quit
    screen.Loop(router);
}
