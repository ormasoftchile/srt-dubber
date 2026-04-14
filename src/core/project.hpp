#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "types.hpp"

namespace core {

struct ProjectEntry {
    int index                       = 0;
    int64_t start_ms                = 0;
    int64_t end_ms                  = 0;
    int64_t slot_duration_ms        = 0;
    std::string text;

    // Paths are stored as strings for easy JSON serialisation
    std::string raw_take_path;
    std::string processed_take_path;

    int64_t raw_duration_ms         = -1;
    int64_t processed_duration_ms   = -1;
    TakeStatus status               = TakeStatus::pending;
};

class Project {
public:
    /// Load an existing project.json next to `srt_path`, or create a fresh one
    /// by parsing the SRT file. On creation, takes/, processed/, and output/
    /// directories are created alongside the .srt file.
    static Project load_or_create(const std::filesystem::path& srt_path);

    /// Serialise current state to project.json.
    void save() const;

    std::vector<ProjectEntry>&       entries();
    const std::vector<ProjectEntry>& entries() const;

    std::filesystem::path takes_dir()     const;
    std::filesystem::path processed_dir() const;
    std::filesystem::path output_dir()    const;

    /// Human-readable project name derived from the project file stem
    /// (e.g.  "episode01-project.json" → "episode01").
    std::string display_name() const;

    // ---------------------------------------------------------------------------
    // Transient TUI state — not persisted to project.json.
    // Populated by the assembly thread and displayed by the assemble screen.
    // ---------------------------------------------------------------------------
    std::vector<std::string> assemble_log;
    bool                     assemble_complete {false};
    std::string              output_path;

private:
    std::filesystem::path    project_file_;
    std::vector<ProjectEntry> entries_;
};

} // namespace core

