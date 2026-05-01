#include "tui/screens/review_screen.hpp"
#include "tui/app_header.hpp"
#include "core/review_flow.hpp"
#include "core/review_effect_dispatcher.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

#include <cstdio>
#include <memory>
#include <string>

using namespace ftxui;

namespace tui {

static std::string fmt_dur_ms(int64_t ms) {
    if (ms <= 0) return "  \xe2\x80\x94  ";
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.1fs", ms / 1000.0);
    return buf;
}

static Element status_cell(const std::string& label, core::TakeStatus s) {
    switch (s) {
        case core::TakeStatus::pending:   return dim(text(label));
        case core::TakeStatus::ok:        return text(label);
        case core::TakeStatus::stretched: return color(Color::Yellow, text(label));
        case core::TakeStatus::overflow:  return color(Color::Red,    text(label));
    }
    return text(label);
}

// ── Review state (shared_ptr across lambdas) ─────────────────────────────────

struct ReviewStateWrapper {
    core::ReviewState review_state;
    core::ReviewEffectDispatcher dispatcher;
    
    ReviewStateWrapper(int start_idx, int total, AudioPlayer& pl)
        : review_state{start_idx, total}
        , dispatcher{pl}
    {}
};

// ── Component factory ────────────────────────────────────────────────────────

Component make_review_component(
    core::Project& project,
    AudioPlayer& player,
    int start_index,
    NavigateFunc navigate)
{
    auto& entries = project.entries();
    if (entries.empty()) {
        navigate(ScreenAction::GoSession, 0);
        return Renderer([]() { return text(""); });
    }

    int start_idx = std::max(0, std::min(start_index, static_cast<int>(entries.size()) - 1));
    
    auto state = std::make_shared<ReviewStateWrapper>(
        start_idx,
        static_cast<int>(entries.size()),
        player
    );

    auto renderer = Renderer([state, &project]() -> Element {
        const auto& entries = project.entries();
        auto rs = core::review_render_state(state->review_state, entries);

        auto header = hbox({
            text("   "),
            dim(text("  IDX ")),
            dim(text("  TEXT PREVIEW                    ")),
            dim(text("  SLOT  ")),
            dim(text("  PROC  ")),
            dim(text("  STATUS  ")),
        });

        Elements rows;
        rows.reserve(rs.rows.size());
        for (int i = 0; i < static_cast<int>(rs.rows.size()); ++i) {
            const auto& r = rs.rows[i];
            char idx_buf[8];
            std::snprintf(idx_buf, sizeof(idx_buf), "%4d", r.idx);

            auto row = hbox({
                text(std::string(idx_buf) + "  "),
                text(r.text_preview) | flex,
                text("  " + fmt_dur_ms(r.slot_ms) + "  "),
                text("  " + fmt_dur_ms(r.proc_ms) + "  "),
                text("  "),
                status_cell(r.status_label, core::take_status_from_string(r.status_label)),
                text("  "),
            });

            if (i == rs.selected_idx) {
                rows.push_back(hbox({
                    color(Color::Cyan, text(" \xe2\x80\xba ")),
                    bold(row),
                }) | focus);
            } else {
                rows.push_back(hbox({
                    text("   "),
                    dim(row),
                }));
            }
        }

        auto list = vbox(rows) | yframe | flex;

        return borderRounded(vbox({
            app_header("Review"),
            separator(),
            text(""),
            header,
            list,
            text(""),
            hbox({
                text("  "),
                dim(text("\xe2\x86\x91/\xe2\x86\x93 navigate  \xc2\xb7  enter/r redo  \xc2\xb7  p play  \xc2\xb7  q/b back")),
                filler(),
            }),
            text(""),
        }));
    });

    return CatchEvent(renderer, [state, &project, navigate](Event event) -> bool {
        std::string key;
        if      (event == Event::ArrowUp)   key = "\x1B[A";
        else if (event == Event::ArrowDown) key = "\x1B[B";
        else if (event == Event::Return)    key = "\r";
        else if (event.is_character())      key = event.character();
        else return false;

        auto cmd = core::parse_review_cmd(key);
        if (!cmd) return false;

        const auto& entries = project.entries();
        bool has_take = false;
        std::filesystem::path take_path;
        if (state->review_state.selected_idx < static_cast<int>(entries.size())) {
            const auto& e = entries[state->review_state.selected_idx];
            has_take  = !e.raw_take_path.empty();
            take_path = e.raw_take_path;
        }

        auto transition = core::review_step(state->review_state, *cmd, has_take, take_path);
        state->review_state = transition.next;
        state->dispatcher.apply_all(transition.effects);

        if (auto nav = state->dispatcher.take_navigate_to_record()) {
            navigate(ScreenAction::GoRecord, *nav);
        } else if (state->dispatcher.exit_to_session_requested()) {
            navigate(ScreenAction::GoSession, 0);
        }

        return true;
    });
}

ScreenAction run_review_screen(core::Project& project,
                                AudioPlayer&   player,
                                int&           selected_index) {
    auto& entries = project.entries();
    if (entries.empty()) return ScreenAction::GoSession;

    auto screen = ScreenInteractive::Fullscreen();
    ScreenAction action = ScreenAction::GoSession;

    selected_index = std::max(0, std::min(selected_index,
                              static_cast<int>(entries.size()) - 1));

    core::ReviewState            review_state{selected_index, static_cast<int>(entries.size())};
    core::ReviewEffectDispatcher dispatcher{player};

    auto renderer = Renderer([&]() -> Element {
        auto rs = core::review_render_state(review_state, entries);

        // Header: 3-char prefix aligns with "  › " / "    " row prefixes
        auto header = hbox({
            text("   "),
            dim(text("  IDX ")),
            dim(text("  TEXT PREVIEW                    ")),
            dim(text("  SLOT  ")),
            dim(text("  PROC  ")),
            dim(text("  STATUS  ")),
        });

        Elements rows;
        rows.reserve(rs.rows.size());
        for (int i = 0; i < static_cast<int>(rs.rows.size()); ++i) {
            const auto& r = rs.rows[i];
            char idx_buf[8];
            std::snprintf(idx_buf, sizeof(idx_buf), "%4d", r.idx);

            auto row = hbox({
                text(std::string(idx_buf) + "  "),
                text(r.text_preview) | flex,
                text("  " + fmt_dur_ms(r.slot_ms) + "  "),
                text("  " + fmt_dur_ms(r.proc_ms) + "  "),
                text("  "),
                status_cell(r.status_label, core::take_status_from_string(r.status_label)),
                text("  "),
            });

            if (i == rs.selected_idx) {
                rows.push_back(hbox({
                    color(Color::Cyan, text(" \xe2\x80\xba ")),
                    bold(row),
                }) | focus);
            } else {
                rows.push_back(hbox({
                    text("   "),
                    dim(row),
                }));
            }
        }

        auto list = vbox(rows) | yframe | flex;

        return borderRounded(vbox({
            app_header("Review"),
            separator(),
            text(""),
            header,
            list,
            text(""),
            hbox({
                text("  "),
                dim(text("\xe2\x86\x91/\xe2\x86\x93 navigate  \xc2\xb7  enter/r redo  \xc2\xb7  p play  \xc2\xb7  q/b back")),
                filler(),
            }),
            text(""),
        }));
    });

    auto component = CatchEvent(renderer, [&](Event event) -> bool {
        std::string key;
        if      (event == Event::ArrowUp)   key = "\x1B[A";
        else if (event == Event::ArrowDown) key = "\x1B[B";
        else if (event == Event::Return)    key = "\r";
        else if (event.is_character())      key = event.character();
        else return false;

        auto cmd = core::parse_review_cmd(key);
        if (!cmd) return false;

        bool has_take = false;
        std::filesystem::path take_path;
        if (review_state.selected_idx < static_cast<int>(entries.size())) {
            const auto& e = entries[review_state.selected_idx];
            has_take  = !e.raw_take_path.empty();
            take_path = e.raw_take_path;
        }

        auto transition = core::review_step(review_state, *cmd, has_take, take_path);
        review_state   = transition.next;
        selected_index = review_state.selected_idx;
        dispatcher.apply_all(transition.effects);

        if (auto nav = dispatcher.take_navigate_to_record()) {
            selected_index = *nav;
            action = ScreenAction::GoRecord;
            screen.ExitLoopClosure()();
        } else if (dispatcher.exit_to_session_requested()) {
            action = ScreenAction::GoSession;
            screen.ExitLoopClosure()();
        }

        return true;
    });

    screen.Loop(component);
    return action;
}

} // namespace tui
