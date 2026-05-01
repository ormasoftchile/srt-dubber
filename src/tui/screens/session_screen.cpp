#include "tui/screens/session_screen.hpp"
#include "tui/app_header.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <string>

using namespace ftxui;

namespace tui {

Component make_session_component(core::Project& project, NavigateFunc navigate) {
    auto& entries = project.entries();  // ref, project outlives component
    
    auto renderer = Renderer([&project, &entries]() -> Element {
        // Stats row
        int total     = static_cast<int>(entries.size());
        int recorded  = 0;
        int processed = 0;
        int overflow  = 0;
        for (const auto& e : entries) {
            if (!e.raw_take_path.empty())                                   ++recorded;
            if (e.status == core::TakeStatus::ok ||
                e.status == core::TakeStatus::stretched)                    ++processed;
            if (e.status == core::TakeStatus::overflow)                     ++overflow;
        }

        auto stat = [](const char* label, int value) -> Element {
            return hbox({
                dim(text(label)),
                text(std::to_string(value)),
            });
        };

        // ── Project subtitle line ─────────────────────────────────────────
        auto project_line = entries.empty()
            ? hbox({text("  "), text(project.display_name()), filler()})
            : hbox({
                text("  "),
                text(project.display_name()),
                dim(text("   \xc2\xb7  ")),
                dim(text(std::to_string(total) + " entries")),
                filler(),
              });

        return borderRounded(vbox({
            // ── App header ───────────────────────────────────────────
            text(""),
            app_header(),
            text(""),

            // ── Project context ──────────────────────────────────────
            project_line,
            text(""),

            // ── Stats ────────────────────────────────────────────────
            hbox({
                text("  "),
                stat("recorded: ",  recorded),  text("   "),
                stat("processed: ", processed), text("   "),
                stat("overflow: ",  overflow),  filler(),
            }),

            // ── Menu ─────────────────────────────────────────────────
            text(""),
            text(""),
            hbox({text("  "), dim(text("r")), text("  record"),   filler()}),
            hbox({text("  "), dim(text("v")), text("  review"),   filler()}),
            hbox({text("  "), dim(text("a")), text("  assemble"), filler()}),
            hbox({text("  "), dim(text("q")), text("  quit"),     filler()}),
            text(""),
        }));
    });

    return CatchEvent(renderer, [navigate](Event event) -> bool {
        if (!event.is_character()) return false;
        const auto ch = event.character();
        if (ch == "r") { navigate(ScreenAction::GoRecord, 0); return true; }
        if (ch == "v") { navigate(ScreenAction::GoReview, 0); return true; }
        if (ch == "a") { navigate(ScreenAction::GoAssemble, 0); return true; }
        if (ch == "q") { navigate(ScreenAction::Quit, 0); return true; }
        return false;
    });
}

ScreenAction run_session_screen(core::Project& project) {
    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::Quit;

    auto renderer = Renderer([&]() -> Element {
        // Stats row
        const auto& entries = project.entries();
        int total     = static_cast<int>(entries.size());
        int recorded  = 0;
        int processed = 0;
        int overflow  = 0;
        for (const auto& e : entries) {
            if (!e.raw_take_path.empty())                                   ++recorded;
            if (e.status == core::TakeStatus::ok ||
                e.status == core::TakeStatus::stretched)                    ++processed;
            if (e.status == core::TakeStatus::overflow)                     ++overflow;
        }

        auto stat = [](const char* label, int value) -> Element {
            return hbox({
                dim(text(label)),
                text(std::to_string(value)),
            });
        };

        // ── Project subtitle line ─────────────────────────────────────────
        auto project_line = entries.empty()
            ? hbox({text("  "), text(project.display_name()), filler()})
            : hbox({
                text("  "),
                text(project.display_name()),
                dim(text("   \xc2\xb7  ")),
                dim(text(std::to_string(total) + " entries")),
                filler(),
              });

        return borderRounded(vbox({
            // ── App header ───────────────────────────────────────────
            text(""),
            app_header(),
            text(""),

            // ── Project context ──────────────────────────────────────
            project_line,
            text(""),

            // ── Stats ────────────────────────────────────────────────
            hbox({
                text("  "),
                stat("recorded: ",  recorded),  text("   "),
                stat("processed: ", processed), text("   "),
                stat("overflow: ",  overflow),  filler(),
            }),

            // ── Menu ─────────────────────────────────────────────────
            text(""),
            text(""),
            hbox({text("  "), dim(text("r")), text("  record"),   filler()}),
            hbox({text("  "), dim(text("v")), text("  review"),   filler()}),
            hbox({text("  "), dim(text("a")), text("  assemble"), filler()}),
            hbox({text("  "), dim(text("q")), text("  quit"),     filler()}),
            text(""),
        }));
    });

    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        if (!event.is_character()) return false;
        const auto ch = event.character();
        if (ch == "r") {
            action = ScreenAction::GoRecord;
            screen.ExitLoopClosure()();
            return true;
        }
        if (ch == "v") {
            action = ScreenAction::GoReview;
            screen.ExitLoopClosure()();
            return true;
        }
        if (ch == "a") {
            action = ScreenAction::GoAssemble;
            screen.ExitLoopClosure()();
            return true;
        }
        if (ch == "q") {
            action = ScreenAction::Quit;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return action;
}

} // namespace tui
