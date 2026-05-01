#include "tui/app.hpp"

#include "core/app_session.hpp"
#include "tui/screens/session_screen.hpp"
#include "tui/screens/recording_screen.hpp"
#include "tui/screens/review_screen.hpp"
#include "tui/screens/assemble_screen.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <streambuf>

using namespace ftxui;

// ── Optional stdout tee for terminal output debugging ────────────────────────
// Enable: SRT_DEBUG_OUTPUT=/tmp/srt-debug.bin ./srt-dubber ...
// Inspect: xxd /tmp/srt-debug.bin | less

namespace {
class TeeBuf : public std::streambuf {
public:
    TeeBuf(std::streambuf* primary, std::streambuf* secondary)
        : primary_(primary), secondary_(secondary) {}
protected:
    int overflow(int c) override {
        if (c == EOF) return c;
        primary_->sputc(static_cast<char>(c));
        secondary_->sputc(static_cast<char>(c));
        return c;
    }
    std::streamsize xsputn(const char* s, std::streamsize n) override {
        primary_->sputn(s, n);
        secondary_->sputn(s, n);
        return n;
    }
    int sync() override {
        secondary_->pubsync();
        return primary_->pubsync();
    }
private:
    std::streambuf* primary_;
    std::streambuf* secondary_;
};
} // namespace

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
    // Optionally tee stdout to a file for escape-sequence debugging.
    std::ofstream debug_file;
    std::unique_ptr<TeeBuf> tee_buf;
    std::streambuf* original_buf = nullptr;
    if (const char* path = std::getenv("SRT_DEBUG_OUTPUT")) {
        debug_file.open(path, std::ios::binary);
        if (debug_file.is_open()) {
            tee_buf = std::make_unique<TeeBuf>(std::cout.rdbuf(), debug_file.rdbuf());
            original_buf = std::cout.rdbuf(tee_buf.get());
        }
    }

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

        // Trigger a re-render — FTXUI owns stdout; never write raw escape
        // sequences outside its control as that corrupts its cursor-position
        // tracking and produces stray character artifacts on screen transitions.
        // ForceFullClearOnNextDraw() makes the transition frame send \033[2K on
        // every line (same as a terminal resize) so no previous-screen content
        // leaks through on terminals that briefly render intermediate cursor
        // positions (e.g., iTerm2).
        screen.ForceFullClearOnNextDraw();
        screen.Post(Event::Custom);
    };
    
    // Build initial component (session screen)
    active = build_component(nav, screen, navigate, project_, recorder_, player_, video_path_);
    
    // Router: delegates to whatever `active` currently is
    auto router = Renderer([&]() -> Element {
        return active ? active->Render() : text("");
    });
    router = CatchEvent(router, [&](Event e) -> bool {
        // Hold a strong ref: navigate() may replace `active` (destroying the old
        // component) while its OnEvent is still on the call stack — use-after-free.
        Component current = active;
        return current ? current->OnEvent(e) : false;
    });
    
    // ONE call to Loop() — never exits until quit
    screen.Loop(router);

    if (original_buf) {
        std::cout.rdbuf(original_buf);
    }
}
