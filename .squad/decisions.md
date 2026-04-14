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

**FTXUI v5.0.0 → main** — initial v5.0.0; updated to main branch after Windows threading fixes (posix model required).

**nlohmann/json via tarball URL** — faster than git clone; SHA256 pinning for reproducibility; `JSON_BuildTests OFF` avoids pulling Catch2.

**miniaudio.h gitignored** — avoids inflating repo history; documented download in `vendor/README.md` and `README.md`.

**Static library `srt_dubber_lib`** — all non-`main` code compiles here; allows future tests to link without re-compiling.

**macOS: CoreAudio + AudioToolbox + CoreFoundation** — miniaudio requires all three at link time; omitting CoreFoundation causes undefined-symbol errors.

**ASan/UBSan in Debug (non-MSVC)** — sanitizer flags differ for MSVC (`/fsanitize`); linker flags mirror compile flags.

**Output directory convention:** `takes/`, `processed/`, `output/` relative to working directory; created at runtime; globally gitignored.

---

### Integration Audit — Wave 1 Pre-Build Fixes
**Author:** Alan  
**Date:** 2025-04-14  
**Status:** Implemented

**Namespace discipline:** Wrapped FFmpeg types in `namespace ffmpeg {}` — `ProcessedClip`, `ProcessResult`, `AssembleResult`, `FfmpegProcessor`, `FfmpegAssembler` now explicit module boundaries.

**Include path standardization:** All `.cpp` files use canonical paths relative to `src/` root. Local headers via quotes, FetchContent via angle brackets.

**Removed nlohmann_json link:** `core/project.cpp` uses hand-rolled JSON; dependency was unused and removed from CMakeLists.txt target_link_libraries.

**Simplified main.cpp:** Replaced complex CLI with minimal TUI launcher — `core::Project::load_or_create()` → `App::run()`.

**Result:** Zero build-blocking issues identified.

---

### Windows Cross-Compile Build Verification
**Author:** Flood  
**Date:** 2025-04-14  
**Status:** Verified

**MinGW threading model:** Changed to posix threading (`g++-mingw-w64-x86-64-posix`) — required for std::thread, FTXUI async components.

**MSVC-only flags:** Wrapped `/utf-8` and `CMAKE_MSVC_RUNTIME_LIBRARY` in `if(MSVC)` guards to allow both MSVC and MinGW paths.

**FTXUI pinning:** Updated to main branch after threading fixes (v5.0.0 → main).

**Deliverable:** 4.3 MB PE32+ executable (x86-64); clean build in ~60s, cached ~20s.

**Files modified:** CMakeLists.txt, Dockerfile.windows-cross, cmake/toolchains/mingw-w64.cmake, cmake/windows-flags.cmake.

---

### --device N Flag for Input Device Selection
**Author:** Butch  
**Date:** 2026-04-14  
**Status:** Implemented

**Problem:** Mac mini and other systems without built-in microphones may enumerate multiple capture devices. Without a selection mechanism the app always opens the system default, which may be the wrong device (e.g. AirPods Max instead of ZoomAudioDevice).

**`AudioRecorder` constructor:** `explicit AudioRecorder(int device_index = -1)` — stores `m_device_index`.

**In `start()`:** When `m_device_index >= 0`, opens a temporary `ma_context`, enumerates capture devices, copies the `ma_device_id` at that index, and passes `&selected_id` to `dev_cfg.capture.pDeviceID`. Context uninited immediately after. Falls back to default with a stderr warning if index is out of range. When `m_device_index == -1`, `pDeviceID` remains `nullptr` — identical to previous behaviour.

**`App`:** Constructor gains `int device_index = -1`; passes it to `AudioRecorder` in member initialiser list.

**`main.cpp`:** Parses `--device N` (two tokens) before positional args using a `first_pos` offset; prints `--list-devices` hint to stderr; updated usage message.

**Non-changes:** `recording_screen.cpp` untouched — device selection fully encapsulated in `AudioRecorder`. Warm-up / Bluetooth fix logic untouched.

---

### Audio Warm-up & Device Diagnostics
**Author:** Butch  
**Date:** 2026-04-14  
**Status:** Implemented

**Warm-up discard (fixes "first part cut off" + 1 s glitch):** After `ma_device_start()`, discard the first `kWarmupSamples = 44100 * 4 / 10` (400 ms) of captured frames before writing to the WAV encoder. This absorbs the Bluetooth A2DP→HFP/SCO profile-switch transient and device ramp-up period.

- `m_warmed_up` (atomic bool) — gates writes in `data_callback`
- `m_warmup_samples_discarded` (atomic uint64) — counts discarded frames in the audio thread
- `m_start_epoch_ms` is stamped by the data callback the moment warm-up completes — `elapsed_ms()` reflects only real recorded audio, not warm-up dead time

**Device name + sample rate logging:** After `ma_device_init()` succeeds, prints `[audio] Recording device: <name>`. If `device.sampleRate != 44100`, prints a warning with the actual rate.

**`--list-devices` flag:** `AudioRecorder::list_devices()` uses `ma_context_get_devices()` to enumerate and print all capture devices with index and default marker. `main.cpp` checks for `--list-devices` before launching the TUI.

**Trade-off:** 400 ms of audio at the start of every take is silently discarded. Harmless on wired/USB mics (discards silence); eliminates Bluetooth artefacts. Constant is a named `constexpr` for easy tuning.

---

### First Build — macOS Verification
**Author:** Richard  
**Date:** 2025-04-14  
**Status:** Clean

**Result:** Zero compilation errors on first attempt (Apple Silicon, clang 17.0.0).

**Build metrics:** FetchContent FTXUI v5.0.0, nlohmann/json 3.11.3; 12 library sources + main.cpp; ~120s total.

**What went RIGHT:** Include hygiene, miniaudio integration (single-definition rule), type system, C++20 compliance, platform flags.

**Validation:** Binary runs correctly; usage message displays properly.

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
