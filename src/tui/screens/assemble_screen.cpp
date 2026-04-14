#include "tui/screens/assemble_screen.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <chrono>
#include <thread>

using namespace ftxui;

namespace tui {

ScreenAction run_assemble_screen(core::Project& project) {
    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::GoSession;

    // Refresh every 200 ms to pick up log lines pushed by the assembly thread.
    std::jthread refresh_thread([&](std::stop_token stop) {
        while (!stop.stop_requested()) {
            screen.PostEvent(Event::Custom);
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
    });

    auto renderer = Renderer([&]() -> Element {
        const auto& log = project.assemble_log;

        Elements lines;
        lines.reserve(log.size() + 1);
        for (const auto& line : log) {
            lines.push_back(text("  " + line));
        }
        if (lines.empty()) {
            lines.push_back(dim(text("  (no output yet)")));
        }

        Element footer;
        if (project.assemble_complete) {
            footer = vbox({
                color(Color::Green,
                      text("  ✓  Output: " + project.output_path)),
                separator(),
                hbox({text("  [q] back"), filler()}),
            });
        } else {
            footer = vbox({
                dim(text("  Assembling…")),
                separator(),
                hbox({text("  [q] back"), filler()}),
            });
        }

        return border(vbox({
            hbox({bold(text("  Assemble")), filler()}),
            separator(),
            vbox(lines) | yframe | flex,
            separator(),
            footer,
        }));
    });

    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        if (event.is_character() && event.character() == "q") {
            action = ScreenAction::GoSession;
            screen.ExitLoopClosure()();
            return true;
        }
        return false;
    });

    screen.Loop(component);
    return action;
}

} // namespace tui
