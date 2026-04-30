#include "tui/app.hpp"

#include "core/app_session.hpp"
#include "tui/screens/session_screen.hpp"
#include "tui/screens/recording_screen.hpp"
#include "tui/screens/review_screen.hpp"
#include "tui/screens/assemble_screen.hpp"

App::App(core::Project& project, std::filesystem::path video_path, int device_index)
    : project_(project), video_path_(std::move(video_path)), recorder_(device_index) {}

void App::run() {
    core::NavState nav;

    while (nav.running) {
        switch (nav.screen) {

            case core::AppScreen::Session: {
                const auto act = tui::run_session_screen(project_);
                core::NavCmd cmd = core::NavCmd::Quit;
                switch (act) {
                    case tui::ScreenAction::GoRecord:   cmd = core::NavCmd::GoRecord;   break;
                    case tui::ScreenAction::GoReview:   cmd = core::NavCmd::GoReview;   break;
                    case tui::ScreenAction::GoAssemble: cmd = core::NavCmd::GoAssemble; break;
                    default:                            cmd = core::NavCmd::Quit;        break;
                }
                nav = core::nav_step(nav, cmd).next;
                break;
            }

            case core::AppScreen::Recording: {
                const auto act = tui::run_recording_screen(
                    project_, recorder_, player_, nav.recording_idx);
                core::NavCmd cmd = core::NavCmd::RecordingDone;
                if (act == tui::ScreenAction::Quit) cmd = core::NavCmd::Quit;
                nav = core::nav_step(nav, cmd).next;
                break;
            }

            case core::AppScreen::Review: {
                const auto act = tui::run_review_screen(
                    project_, player_, nav.review_idx);
                core::NavCmd cmd  = core::NavCmd::ReviewGoSession;
                int          payload = nav.review_idx;
                if (act == tui::ScreenAction::GoRecord) {
                    cmd = core::NavCmd::ReviewGoRecord;
                }
                nav = core::nav_step(nav, cmd, payload).next;
                break;
            }

            case core::AppScreen::Assemble: {
                tui::run_assemble_screen(project_, video_path_);
                nav = core::nav_step(nav, core::NavCmd::AssembleDone).next;
                break;
            }
        }
    }
}
