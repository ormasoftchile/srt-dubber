#include "tui/screens/review_screen.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <cstdio>
#include <string>

using namespace ftxui;

namespace tui {

// ── Helpers ─────────────────────────────────────────────────────────────────

static std::string truncate_to(const std::string& s, std::size_t max_len) {
    if (s.size() <= max_len) return s;
    return s.substr(0, max_len - 3) + "...";
}

static std::string fmt_dur_ms(int64_t ms) {
    if (ms < 0) return "  —  ";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1fs", ms / 1000.0);
    return buf;
}

static Element status_cell(core::TakeStatus s) {
    const std::string label{core::take_status_to_string(s)};
    switch (s) {
        case core::TakeStatus::pending:   return dim(text(label));
        case core::TakeStatus::ok:        return text(label);
        case core::TakeStatus::stretched: return color(Color::Yellow, text(label));
        case core::TakeStatus::overflow:  return color(Color::Red,    text(label));
    }
    return text(label);
}

// ── Screen ──────────────────────────────────────────────────────────────────

ScreenAction run_review_screen(core::Project& project,
                               AudioPlayer&   player,
                               int&           selected_index) {
    auto& entries = project.entries();
    if (entries.empty()) return ScreenAction::GoSession;

    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::GoSession;

    // Clamp selection to valid range.
    selected_index = std::max(0, std::min(selected_index,
                              static_cast<int>(entries.size()) - 1));

    auto renderer = Renderer([&]() -> Element {
        const int total = static_cast<int>(entries.size());

        // ── Column header ────────────────────────────────────────────────
        auto header = hbox({
            dim(text("  IDX ")),
            dim(text("  TEXT PREVIEW                    ")),
            dim(text("  SLOT  ")),
            dim(text("  PROC  ")),
            dim(text("  STATUS  ")),
        });

        // ── Rows ─────────────────────────────────────────────────────────
        Elements rows;
        rows.reserve(total);
        for (int i = 0; i < total; ++i) {
            const auto& e = entries[i];

            char idx_buf[8];
            std::snprintf(idx_buf, sizeof(idx_buf), "%4d", e.index);

            const std::string preview =
                truncate_to([&]{
                    std::string t = e.text;
                    for (auto& c : t) if (c == '\n') c = ' ';
                    return t;
                }(), 32);

            auto row = hbox({
                text(std::string(idx_buf) + "  "),
                text(preview) | flex,
                text("  " + fmt_dur_ms(e.slot_duration_ms)      + "  "),
                text("  " + fmt_dur_ms(e.processed_duration_ms) + "  "),
                text("  "),
                status_cell(e.status),
                text("  "),
            });

            if (i == selected_index) {
                rows.push_back(row | inverted | focus);
            } else {
                rows.push_back(row);
            }
        }

        auto list = vbox(rows) | yframe | flex;

        return border(vbox({
            hbox({bold(text("  Review")),
                  dim(text("  (" + std::to_string(total) + " entries)")),
                  filler()}),
            separator(),
            header,
            separator(),
            list,
            separator(),
            hbox({text("  [↑/↓] navigate  [enter] redo  [p] play take  [q] back"),
                  filler()}),
        }));
    });

    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        const int total = static_cast<int>(entries.size());

        if (event == Event::ArrowUp || (event.is_character() && event.character() == "k")) {
            if (selected_index > 0) --selected_index;
            return true;
        }
        if (event == Event::ArrowDown || (event.is_character() && event.character() == "j")) {
            if (selected_index < total - 1) ++selected_index;
            return true;
        }
        if (event == Event::Return) {
            // Open recording screen at this entry.
            player.stop();
            action = ScreenAction::GoRecord;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event.is_character()) {
            const auto ch = event.character();
            if (ch == "p") {
                const auto& e = entries[selected_index];
                if (!e.raw_take_path.empty()) player.play(e.raw_take_path);
                return true;
            }
            if (ch == "q") {
                player.stop();
                action = ScreenAction::GoSession;
                screen.ExitLoopClosure()();
                return true;
            }
        }
        return false;
    });

    screen.Loop(component);
    return action;
}

} // namespace tui
