#pragma once
#include <cstdint>
#include <string>

namespace core {

/// A pure snapshot of what the recording screen should display.
/// Produced by render_state() — frontends (TUI, harness) render this;
/// they do NOT read FlowState or FlowPhase directly.
struct RecordingRenderState {
    int     current_idx;       ///< 0-based index of the current entry
    int     total;             ///< total number of entries
    std::string phase_label;   ///< "idle" | "countdown" | "recording"
    bool    has_take;          ///< current entry has a recorded take
    std::string entry_text;    ///< subtitle text for the current entry
    int64_t slot_duration_ms;  ///< allocated slot duration (display only)
    int64_t elapsed_ms;        ///< recording elapsed time (0 when not recording)
};

} // namespace core
