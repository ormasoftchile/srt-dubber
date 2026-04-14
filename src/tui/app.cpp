#include "tui/app.hpp"

#include "tui/screens/session_screen.hpp"
#include "tui/screens/recording_screen.hpp"
#include "tui/screens/review_screen.hpp"
#include "tui/screens/assemble_screen.hpp"

App::App(core::Project& project, std::filesystem::path video_path, int device_index)
    : project_(project), video_path_(std::move(video_path)), recorder_(device_index) {}

void App::run() {
    bool running = true;
    while (running) {
        switch (current_screen_) {

            case Screen::Session: {
                const auto act = tui::run_session_screen(project_);
                switch (act) {
                    case tui::ScreenAction::GoRecord:
                        recording_start_ = 0;
                        current_screen_  = Screen::Recording;
                        break;
                    case tui::ScreenAction::GoReview:
                        current_screen_  = Screen::Review;
                        break;
                    case tui::ScreenAction::GoAssemble:
                        current_screen_  = Screen::Assemble;
                        break;
                    case tui::ScreenAction::Quit:
                    default:
                        running = false;
                        break;
                }
                break;
            }

            case Screen::Recording: {
                const auto act = tui::run_recording_screen(
                    project_, recorder_, player_, recording_start_);
                switch (act) {
                    case tui::ScreenAction::GoSession:
                    default:
                        current_screen_ = Screen::Session;
                        break;
                    case tui::ScreenAction::Quit:
                        running = false;
                        break;
                }
                break;
            }

            case Screen::Review: {
                const auto act = tui::run_review_screen(
                    project_, player_, review_selected_);
                switch (act) {
                    case tui::ScreenAction::GoRecord:
                        // Open recording at the entry the user selected.
                        recording_start_ = review_selected_;
                        current_screen_  = Screen::Recording;
                        break;
                    case tui::ScreenAction::GoSession:
                    default:
                        current_screen_ = Screen::Session;
                        break;
                }
                break;
            }

            case Screen::Assemble: {
                tui::run_assemble_screen(project_, video_path_);
                current_screen_ = Screen::Session;
                break;
            }
        }
    }
}
