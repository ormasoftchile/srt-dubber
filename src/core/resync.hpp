#pragma once

#include <vector>
#include "core/project.hpp"
#include "srt/parser.hpp"

namespace core {

struct ResyncResult {
    int matched_exact;    // matched by text
    int matched_by_index; // matched by position fallback
    int new_entries;      // new in SRT, no matching take
    int orphaned_takes;   // old takes not placed in new SRT
    int total_new;        // total entries in new SRT
};

/// Takes the loaded project (already has old entries) and the new parsed SRT.
/// Mutates project entries in-place. Returns stats.
ResyncResult resync(core::Project& project, const std::vector<srt::SrtEntry>& new_entries);

} // namespace core
