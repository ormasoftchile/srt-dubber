# Richard — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Richard's role:** Build & Integration — owns CMakeLists.txt, miniaudio vendor embedding, FTXUI FetchContent, cross-platform flags, README, `srt-dubber full` command
- **Key collaborators:** Alan (architecture), Butch (audio), Steven (TUI)
- **Build requirements:** CMake ≥ 3.20, C++20, ffmpeg on PATH

## Learnings

### 2026-04-30 — Slice 8: Multi-screen app harness

- **app-harness links srt_dubber_core only** — no audio, no FTXUI; `nlohmann_json::nlohmann_json` listed explicitly alongside `srt_dubber_core` because it is used directly in the harness (not via transitive PUBLIC link).
- **ProjectEntry fixture:** Harness builds `std::vector<ProjectEntry>` inline — no SRT file needed; `raw_take_path` is a std::string that starts empty and is set to `"fake_take.wav"` when `StopRecording` effect is observed.
- **review_render_state() takes ProjectEntry vector** — so harness maintains a `std::vector<ProjectEntry>` fixture that doubles as ground truth for both recording and review screens.
- **AssemblyDone requires Assembling phase** — calling `assemble_step(state, AssemblyDone)` while phase != Assembling is a no-op; test must call `Start` first to enter Assembling state before injecting AssemblyDone.
- **Test JSON parsing:** Shell test uses a Python3 brace-depth parser (not jq) to split multi-object NDJSON-like output into individual frames; no jq dependency.
- **Wrong-screen error path:** Session screen returns an error for any screen-level (non-nav) command; harness uses the literal screen name in error strings for debuggability.
- **CTest add_test WORKING_DIRECTORY:** Integration test runs with `WORKING_DIRECTORY ${CMAKE_BINARY_DIR}` so the harness path `./app-harness` resolves; test script receives absolute harness path via `${CMAKE_SOURCE_DIR}`.

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

### 2026-04-30 — Slice 8: Multi-screen app harness

**Harness Architecture:**
- `tools/app_harness.cpp`: Standalone JSON stdin/stdout interface
- Drives nav_step() + all screen machines (recording_flow, review_flow, assemble_flow)
- No TUI, no audio, no threading — links against srt_dubber_core + nlohmann_json only
- ProjectEntry fixture built inline (no SRT file needed)

**Test Suite:**
- `tests/app_harness_test.sh`: 5 end-to-end flows with 26 total assertions
- Test cases: record→review→assemble, error paths, screen transitions
- Shell test uses Python3 JSON parser (no jq dependency) for NDJSON splitting
- 100% pass rate

**CMake Integration:**
- Harness target added to `src/CMakeLists.txt`
- CTest integration: `WORKING_DIRECTORY ${CMAKE_BINARY_DIR}` resolves harness path correctly
- Test script receives absolute harness path via CMAKE variable

**Design Decisions:**
- SpawnAssembly effect is a no-op (threading is TUI-only, harness never spawns ffmpeg)
- AssemblyDone must be injected manually in tests (simulates async completion)
- Fixture provides ground truth for recording/review/assemble screens

**Closure:**
- **Slices 1–8 complete:** Full interaction architecture implemented
- **Pure machines:** Recording, Review, Assemble, Navigation all testable without TUI
- **Effect dispatchers:** Centralized side-effect handling across all screens
- **Validation:** 64 unit tests + multi-screen harness with 26 integration assertions
- **Next phase:** Countdown timer in dispatcher, event sourcing/replay infrastructure
