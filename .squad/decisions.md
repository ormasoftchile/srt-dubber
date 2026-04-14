# Squad Decisions

## Architectural Decisions (ADR)

### Core Architecture — SRT Parser & Project Model
**Author:** Alan  
**Date:** 2025  
**Status:** Accepted

**Module boundary:** `srt::` for pure SRT parsing (zero dependencies); `core::types` for shared enums; `core::project` for session state.

**No external JSON library:** Hand-rolled reader/writer (~150 lines) covers the flat schema without third-party headers.

**project.json naming:** `<srt-stem>-project.json` to avoid collisions with multiple SRT files.

**Directory layout:** On load/create, creates sibling `takes/`, `processed/`, `output/` directories next to `.srt` file.

**TakeStatus enum:** Four states — `pending`, `ok`, `stretched`, `overflow`.

**SRT parser robustness:** Handles `\r\n` endings, strips metadata, preserves multi-line text, skips malformed blocks gracefully.

---

### Audio & FFmpeg Module Design
**Author:** Butch  
**Date:** 2025  
**Status:** Implemented

**MA_IMPLEMENTATION guard:** Lives only in `recorder.cpp` to satisfy single-definition rule.

**PlayerContext heap struct:** Carries both `ma_decoder` and `AudioPlayer*` back-pointer via opaque `ma_device*` handle.

**Temp file placement:** Temp files in same directory as output to guarantee atomic `std::filesystem::rename()`.

**Overflow handling:** Clamp to `atempo=1.08`, produce file, set `ProcessResult::is_overflow = true` — caller decides redo.

**atempo single-filter:** No chaining needed; cap is 1.08x (within [0.5, 2.0] range).

**amix normalize=0:** Sums all delayed streams without re-normalizing; preserves relative levels.

**ffprobe via popen:** Outputs bare float with trailing newline; `popen`/`fgets` + `std::stod` handles it transparently.

---

### TUI Design — All Four Screens
**Author:** Steven  
**Date:** 2025  
**Status:** Accepted

**One ScreenInteractive per screen visit:** Each screen creates its own fullscreen loop, runs to completion, returns `ScreenAction` enum.

**ScreenAction enum as navigation contract:** Screens return enum; App decides next screen — keeps screens decoupled.

**App owns audio devices:** `App` owns `recorder` and `player`; recording screen calls them directly (no callbacks).

**Recording timer:** `std::jthread` posts `Event::Custom` every 100 ms; renderer reads `elapsed_ms()` and `is_recording()` directly.

**Review screen scrolling:** `focus(row) | inverted` + `yframe` for automatic scroll-to-focus; j/k/arrow keys for navigation.

**Status colours:** pending=`dim`, ok=default, stretched=`Color::Yellow`, overflow=`Color::Red`. No green for ok — reserved for assembly only.

**Assemble screen display-only:** Renders `project.assemble_log` and polls every 200 ms; does not trigger assembly itself.

**Minimal Project additions:** Three transient TUI fields — `assemble_log`, `assemble_complete`, `output_path` — plus `display_name()` helper. Not persisted in JSON.

---

### Windows Cross-Platform Build Setup
**Author:** Flood  
**Date:** 2025-07  
**Status:** Accepted

**MinGW-w64 cross-compilation via Docker:** Primary build path — produces `.exe` from macOS via `Dockerfile.windows-cross` + `mingw-w64.cmake` toolchain.

**Static linking:** `-static -static-libgcc -static-libstdc++` — binary is self-contained; acceptable size trade-off for developer tool.

**GitHub Actions `windows-latest` as CI gate:** MSVC + Ninja validates CMakeLists.txt is not MinGW-only; catches Win32 and CRT failures early.

**UTM (QEMU) for audio validation:** x86_64 QEMU emulation on Apple Silicon lets developer test WASAPI audio without physical Windows machine.

**`windows-flags.cmake` as opt-in:** Keeps Windows-specific flags isolated; explicit inclusion shows Windows intent.

**miniaudio.h not committed:** Large (~100KB), changes infrequently. CI and Docker fetch fresh; keeps repo lean.

---

### Build System Decisions
**Author:** Richard  
**Date:** 2024 (initial scaffold)  
**Status:** Active

**CMake minimum: 3.20** — for `FetchContent` stability and `cmake_path()` availability.

**C++20 with `CMAKE_CXX_EXTENSIONS OFF`** — enforces strict standard compliance; avoids divergent behavior between Apple Clang and GCC.

**FTXUI v5.0.0 pinned by git tag** — first stable release with Component-based event model; FetchContent ensures reproducible builds.

**nlohmann/json via tarball URL** — faster than git clone; SHA256 pinning for reproducibility; `JSON_BuildTests OFF` avoids pulling Catch2.

**miniaudio.h gitignored** — avoids inflating repo history; documented download in `vendor/README.md` and `README.md`.

**Static library `srt_dubber_lib`** — all non-`main` code compiles here; allows future tests to link without re-compiling.

**macOS: CoreAudio + AudioToolbox + CoreFoundation** — miniaudio requires all three at link time; omitting CoreFoundation causes undefined-symbol errors.

**ASan/UBSan in Debug (non-MSVC)** — sanitizer flags differ for MSVC (`/fsanitize`); linker flags mirror compile flags.

**Output directory convention:** `takes/`, `processed/`, `output/` relative to working directory; created at runtime; globally gitignored.

---

## Feature Backlog

### 2026-04-14: Take Re-sync on Video Re-record
**By:** ormasoftchile (via Copilot)  
**Status:** Deferred

**Problem:** When user re-records source video (same subtitles, different timing), voice takes should be re-synced without re-recording.

**Proposed approach:** 
- New command: `srt-dubber resync new.srt`
- Match entries by subtitle text (or index if text unchanged)
- Re-map `raw_take_path` from old project.json → new project.json entries
- New `slot_duration_ms` may differ → re-run process step
- If new slot is shorter and atempo > 1.08x → mark overflow → require redo
- If new slot is longer → take fits fine, may not need stretching
- Entries with edited subtitle text → marked pending

**Edge cases to discuss:**
- Subtitle text edited between versions (fuzzy match? index match?)
- Order changes in SRT (reordered scenes)
- New subtitles added or old ones removed
- How to present diff to user before committing remap

**Decision:** Backlog — do not implement yet. Revisit at end of current build sprint.

---

## Governance

- All meaningful changes require team consensus
- Document architectural decisions here
- Keep history focused on work, decisions focused on direction
