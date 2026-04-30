#pragma once
#include <filesystem>

namespace core {

// ─── Shared effects used by multiple screen machines ───────────────────────

/// Persist a recorded take path to a project entry.
struct SaveTake {
    int idx;
    std::filesystem::path path;
};

/// Reset a project entry's take fields (path, status, processed path).
struct ClearTake {
    int idx;
};

/// Write the project JSON to disk.
struct SaveProject {};

/// Navigate back to the Session screen from any screen.
struct ExitToSession {};

} // namespace core
