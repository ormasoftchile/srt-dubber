#include "review_flow.hpp"
#include "types.hpp"
#include <algorithm>

namespace core {

std::optional<ReviewCmd> parse_review_cmd(const std::string& key) {
    if (key == "k" || key == "\x1B[A") return ReviewCmd::Up;
    if (key == "j" || key == "\x1B[B") return ReviewCmd::Down;
    if (key == "p")                     return ReviewCmd::PlayTake;
    if (key == "\r" || key == "r")      return ReviewCmd::Redo;
    if (key == "q" || key == "b")       return ReviewCmd::Back;
    return std::nullopt;
}

ReviewTransition review_step(ReviewState           state,
                              ReviewCmd             cmd,
                              bool                  has_take,
                              std::filesystem::path take_path) {
    std::vector<ReviewEffect> effects;

    switch (cmd) {
        case ReviewCmd::Up:
            if (state.selected_idx > 0)
                --state.selected_idx;
            break;

        case ReviewCmd::Down:
            if (state.selected_idx < state.total - 1)
                ++state.selected_idx;
            break;

        case ReviewCmd::PlayTake:
            if (has_take)
                effects.push_back(PlayReviewTake{std::move(take_path)});
            break;

        case ReviewCmd::Redo:
            effects.push_back(StopReviewPlay{});
            effects.push_back(NavigateToRecord{state.selected_idx});
            break;

        case ReviewCmd::Back:
            effects.push_back(StopReviewPlay{});
            effects.push_back(ExitToSession{});
            break;
    }

    return ReviewTransition{state, std::move(effects)};
}

ReviewRenderState review_render_state(const ReviewState&               state,
                                       const std::vector<ProjectEntry>& entries) {
    ReviewRenderState rs;
    rs.selected_idx = state.selected_idx;
    rs.total        = state.total;
    rs.rows.reserve(entries.size());

    for (const auto& e : entries) {
        ReviewEntryRow row;
        row.idx = e.index;

        // Build text preview — replace newlines with spaces first
        std::string flat = e.text;
        for (auto& c : flat) if (c == '\n') c = ' ';
        const std::size_t preview_len = std::min<std::size_t>(40, flat.size());
        row.text_preview = flat.substr(0, preview_len);

        row.slot_ms      = e.slot_duration_ms;
        row.proc_ms      = (e.processed_duration_ms >= 0) ? e.processed_duration_ms : 0;
        row.status_label = take_status_to_string(e.status);
        row.has_take     = !e.raw_take_path.empty();

        rs.rows.push_back(std::move(row));
    }

    return rs;
}

} // namespace core
