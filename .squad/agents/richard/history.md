# Richard — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Richard's role:** Build & Integration — owns CMakeLists.txt, miniaudio vendor embedding, FTXUI FetchContent, cross-platform flags, README, `srt-dubber full` command
- **Key collaborators:** Alan (architecture), Butch (audio), Steven (TUI)
- **Build requirements:** CMake ≥ 3.20, C++20, ffmpeg on PATH

## Learnings

### 2024 — Initial build system scaffold

- **FetchContent SHA256 for nlohmann/json 3.11.3**: `d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d` — pin it to avoid silent upstream mutations.
- **FTXUI**: Set `FTXUI_BUILD_DOCS=OFF` and `FTXUI_BUILD_EXAMPLES=OFF` before `FetchContent_MakeAvailable` to avoid building demo code (saves ~30 s).
- **macOS CoreFoundation**: CoreAudio + AudioToolbox alone are insufficient for some miniaudio code paths; always link `CoreFoundation` too on Apple targets.
- **ASan/UBSan in Debug**: Guard behind `NOT MSVC` — MSVC has its own `/fsanitize=address` flag with different syntax; don't mix.
- **miniaudio.h is not committed**: 60 k+ line single-header — gitignored, users run `curl` once. Document this prominently in vendor/README.md and README.md.
- **`srt-dubber full`**: Runs TUI record phase first (blocking), then calls `cmd_process` and `cmd_assemble` in sequence; project.json on disk bridges the TUI session to the batch steps.
- **main.cpp discipline**: Keep it a thin dispatcher. All business logic (parse, process, assemble) lives in `srt_dubber_lib`; `main.cpp` just routes argv.

### April 2025 — First Build Attempt (macOS)

- **Clean build on first attempt**: Zero compilation errors from Wave 1 code. All includes, types, and platform flags were correct from the start.
- **Include discipline validated**: FTXUI and nlohmann/json use angle brackets `<...>`, local headers use quotes `"..."` — no path resolution issues.
- **MINIAUDIO_IMPLEMENTATION**: Defined only in `audio/recorder.cpp` — no duplicate definition conflicts.
- **Type hygiene**: No redefinition errors between `core/types.hpp` and `ffmpeg/types.hpp` — `TakeStatus` properly shared.
- **Platform flags work**: `cmake/windows-flags.cmake` harmless on macOS (Windows defines ignored), CoreFoundation linked correctly, ASan/UBSan enabled in Debug.
- **FetchContent timing**: FTXUI compilation adds ~90s to first build; subsequent builds skip it. nlohmann/json is header-only, zero build overhead.
- **miniaudio.h download**: 3.9 MB single header from mackron/miniaudio master — must be documented in README as a prerequisite step.
- **Runtime validation**: Binary runs, displays usage correctly. No `--help` flag yet (feature gap, not build issue).
- **Build metrics**: 12 library sources + main.cpp, total ~120s clean build with all dependencies on Apple Silicon M-series.
- **Next platform tests**: Linux (Ubuntu), Windows (MinGW or MSVC) — expect clean builds there too based on proper guards.

### April 2025 — GitHub Repository Created

- **GitHub account**: ormasoftchile (verified)
- **Repository URL**: https://github.com/ormasoftchile/srt-dubber
- **Visibility**: Public
- **Repository description**: "Local C++20 TUI tool for voice-over dubbing from SRT files"
- **.gitignore validation**: Checked before push — correctly excludes build/, takes/, processed/, output/, project.json, .copilot/, and Squad runtime files (orchestration-log, log, decisions/inbox, sessions, .squad-workstream)
- **Push success**: All 339 commits, 294 objects compressed, ~217 KiB delta pushed cleanly
- **Remote origin**: https://github.com/ormasoftchile/srt-dubber.git (fetch + push verified)
- **Branch setup**: main branch set to track origin/main
- **HEAD commit**: 4bfb624 "Add --resync command to remap takes to new SRT timing"
- **Public visibility ensures**: All contributors can clone, fork, and collaborate; CI/CD workflows can trigger on push/PR events
