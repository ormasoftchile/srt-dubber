#include "core/resync.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>

namespace core {

namespace {

/// Normalize whitespace: trim leading/trailing, collapse interior runs to single space, lowercase.
std::string normalize_text(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    
    bool in_whitespace = true;  // start as true to skip leading whitespace
    
    for (unsigned char c : text) {
        if (std::isspace(c)) {
            if (!in_whitespace && !result.empty()) {
                result += ' ';
                in_whitespace = true;
            }
        } else {
            result += std::tolower(c);
            in_whitespace = false;
        }
    }
    
    // Trim trailing whitespace
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    
    return result;
}

} // anonymous namespace

ResyncResult resync(core::Project& project, const std::vector<srt::SrtEntry>& new_entries) {
    ResyncResult result{};
    result.total_new = static_cast<int>(new_entries.size());
    
    // Build a map of normalized text -> old entry (for exact matching)
    std::unordered_map<std::string, ProjectEntry*> text_map;
    std::vector<bool> old_matched(project.entries().size(), false);
    
    for (auto& old_entry : project.entries()) {
        std::string norm = normalize_text(old_entry.text);
        // For multiple entries with same text, first one wins
        if (text_map.find(norm) == text_map.end()) {
            text_map[norm] = &old_entry;
        }
    }
    
    // Build new entries list
    std::vector<ProjectEntry> new_project_entries;
    new_project_entries.reserve(new_entries.size());
    
    for (std::size_t i = 0; i < new_entries.size(); ++i) {
        const auto& srt_entry = new_entries[i];
        ProjectEntry new_entry;
        
        // Set new timing from SRT
        new_entry.index            = srt_entry.index;
        new_entry.start_ms         = srt_entry.start_ms;
        new_entry.end_ms           = srt_entry.end_ms;
        new_entry.slot_duration_ms = srt_entry.slot_duration_ms;
        new_entry.text             = srt_entry.text;
        
        // Try exact text match first
        std::string norm = normalize_text(srt_entry.text);
        auto it = text_map.find(norm);
        
        if (it != text_map.end()) {
            // Exact match found
            ProjectEntry* old = it->second;
            
            // Find old entry index to mark it as matched
            for (std::size_t j = 0; j < project.entries().size(); ++j) {
                if (&project.entries()[j] == old) {
                    old_matched[j] = true;
                    break;
                }
            }
            
            // Preserve take data from old entry
            new_entry.raw_take_path         = old->raw_take_path;
            new_entry.processed_take_path   = old->processed_take_path;
            new_entry.raw_duration_ms       = old->raw_duration_ms;
            new_entry.processed_duration_ms = old->processed_duration_ms;
            new_entry.status                = old->status;
            
            ++result.matched_exact;
            
            // Remove from text_map so it can't be matched again
            text_map.erase(it);
        } else if (i < project.entries().size()) {
            // Index fallback: check if old project has an entry at position i
            ProjectEntry& old = project.entries()[i];
            
            if (!old_matched[i]) {
                // This old entry hasn't been matched yet, use it
                new_entry.raw_take_path         = old.raw_take_path;
                new_entry.processed_take_path   = old.processed_take_path;
                new_entry.raw_duration_ms       = old.raw_duration_ms;
                new_entry.processed_duration_ms = old.processed_duration_ms;
                new_entry.status                = old.status;
                
                old_matched[i] = true;
                ++result.matched_by_index;
            } else {
                // Already matched by exact text, this is a new entry
                new_entry.status = TakeStatus::pending;
                ++result.new_entries;
            }
        } else {
            // No old entry at this index, new entry
            new_entry.status = TakeStatus::pending;
            ++result.new_entries;
        }
        
        new_project_entries.push_back(std::move(new_entry));
    }
    
    // Count orphaned takes (old entries with takes that weren't matched)
    for (std::size_t i = 0; i < project.entries().size(); ++i) {
        if (!old_matched[i] && !project.entries()[i].raw_take_path.empty()) {
            ++result.orphaned_takes;
        }
    }
    
    // Replace project entries with new ones
    project.entries() = std::move(new_project_entries);
    
    return result;
}

} // namespace core
