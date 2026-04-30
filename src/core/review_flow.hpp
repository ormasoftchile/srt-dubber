#pragma once
#include "effects.hpp"
#include "project.hpp"
#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace core {

// ─── Commands ──────────────────────────────────────────────────────────────

enum class ReviewCmd {
    Up,        // 'k' or arrow up — move selection up
    Down,      // 'j' or arrow down — move selection down
    PlayTake,  // 'p' — play the selected entry's take
    Redo,      // Enter or 'r' — go to recording screen at selected index
    Back,      // 'q' or 'b' — return to session screen
};

std::optional<ReviewCmd> parse_review_cmd(const std::string& key);

// ─── State ─────────────────────────────────────────────────────────────────

struct ReviewState {
    int selected_idx = 0;
    int total        = 0;
};

// ─── Effects ───────────────────────────────────────────────────────────────

struct PlayReviewTake   { std::filesystem::path path; };
struct StopReviewPlay   {};
struct NavigateToRecord { int idx; };

using ReviewEffect = std::variant<
    PlayReviewTake,
    StopReviewPlay,
    NavigateToRecord,
    ExitToSession    // from effects.hpp
>;

// ─── Transition ────────────────────────────────────────────────────────────

struct ReviewTransition {
    ReviewState               next;
    std::vector<ReviewEffect> effects;
};

/// Pure review flow state machine.
/// \param state     current review state (selected_idx, total)
/// \param cmd       command to process
/// \param has_take  whether the currently selected entry has a recorded take (gates PlayTake)
/// \param take_path path of the selected entry's take (for PlayReviewTake effect)
ReviewTransition review_step(ReviewState           state,
                              ReviewCmd             cmd,
                              bool                  has_take,
                              std::filesystem::path take_path);

// ─── Render state ──────────────────────────────────────────────────────────

struct ReviewEntryRow {
    int         idx;
    std::string text_preview;  // first ~40 chars of subtitle text
    int64_t     slot_ms;
    int64_t     proc_ms;       // 0 if not processed
    std::string status_label;  // "pending" | "ok" | "stretched" | "overflow"
    bool        has_take;
};

struct ReviewRenderState {
    int                         selected_idx;
    int                         total;
    std::vector<ReviewEntryRow> rows;
};

/// Build a render snapshot. Call once per render frame.
ReviewRenderState review_render_state(const ReviewState&               state,
                                       const std::vector<ProjectEntry>& entries);

} // namespace core
