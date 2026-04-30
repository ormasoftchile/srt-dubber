#pragma once
#include <optional>
#include <variant>
#include <vector>

namespace core {

enum class AppScreen { Session, Recording, Review, Assemble };

struct NavState {
    AppScreen screen        = AppScreen::Session;
    int       recording_idx = 0;  // entry to open recording at
    int       review_idx    = 0;  // last selected row in review list
    bool      running       = true;
};

enum class NavCmd {
    // From Session screen:
    GoRecord,
    GoReview,
    GoAssemble,
    Quit,
    // From Recording screen:
    RecordingDone,   // → back to Session
    // From Review screen:
    ReviewGoRecord,  // → Recording at selected index (payload: index)
    ReviewGoSession, // → Session
    // From Assemble screen:
    AssembleDone,    // → Session
};

struct NavTransition {
    NavState next;
};

NavTransition nav_step(NavState state, NavCmd cmd, int payload = 0);

} // namespace core
