#include "tui/screens/session_screen.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <string>

using namespace ftxui;

namespace tui {

ScreenAction run_session_screen(core::Project& project) {
    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::Quit;

    auto renderer = Renderer([&]() -> Element {
        const std::string title =
            project.entries().empty()
                ? project.display_name()
                : project.display_name() + "  (" +
                  std::to_string(project.entries().size()) + " entries)";

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

        return border(vbox({
            // ── Title ────────────────────────────────────────────────
            hbox({text("  "), bold(text(title)), filler()}),
            separator(),

            // ── Stats ────────────────────────────────────────────────
            hbox({
                text("  "),
                stat("total: ",     total),     text("   "),
                stat("recorded: ",  recorded),  text("   "),
                stat("processed: ", processed), text("   "),
                stat("overflow: ",  overflow),  filler(),
            }),
            separator(),

            // ── Menu ─────────────────────────────────────────────────
            text(""),
            hbox({
                text("  "),
                text("[r] "), bold(text("Record")),    text("    "),
                text("[v] "), bold(text("Review")),    text("    "),
                text("[a] "), bold(text("Assemble")),  text("    "),
                text("[q] "), bold(text("Quit")),
                filler(),
            }),
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
