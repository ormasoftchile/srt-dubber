#include "core/app_session.hpp"

namespace core {

NavTransition nav_step(NavState state, NavCmd cmd, int payload) {
    switch (cmd) {
        case NavCmd::GoRecord:
            state.screen        = AppScreen::Recording;
            state.recording_idx = payload;
            break;
        case NavCmd::GoReview:
            state.screen = AppScreen::Review;
            break;
        case NavCmd::GoAssemble:
            state.screen = AppScreen::Assemble;
            break;
        case NavCmd::Quit:
            state.running = false;
            break;
        case NavCmd::RecordingDone:
            state.screen = AppScreen::Session;
            break;
        case NavCmd::ReviewGoRecord:
            state.recording_idx = payload;
            state.screen        = AppScreen::Recording;
            break;
        case NavCmd::ReviewGoSession:
            state.screen = AppScreen::Session;
            break;
        case NavCmd::AssembleDone:
            state.screen = AppScreen::Session;
            break;
    }
    return NavTransition{state};
}

} // namespace core
