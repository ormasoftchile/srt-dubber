# Alan — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Alan's role:** Lead & Architect — owns project structure, SRT parsing, session state model, code review
- **Key collaborators:** Butch (audio/ffmpeg), Steven (TUI/FTXUI), Richard (build/CMake)

## Learnings

### Session — Core data model & SRT parser (initial implementation)
- **Hand-rolled JSON**: No nlohmann/json was present in the repo; implemented a minimal JSON read/write layer in `project.cpp` covering the flat schema. If the team adds nlohmann/json later, `project.cpp` is the only file that needs updating.
- **Namespace discipline**: `srt::` for parser, `core::` for types/project — keeps modules independently includable and testable without cross-dependency (parser has zero dependency on core).
- **State machine parser**: The SRT parser uses a simple linear state machine (index → timecode → text lines → blank) rather than regex, which keeps it dependency-free and easy to extend for edge cases (extra metadata on timecode line, trailing `\r`, empty subtitle blocks).
- **Designated initialisers**: Used C++20 designated initialisers (`SrtEntry{.index = …}`) in parser.cpp for clarity and to avoid positional argument errors.
- **project_file naming**: project file is named `<stem>-project.json` (not `project.json`) so multiple SRT files in the same directory don't collide.
- **Directory creation**: `takes/`, `processed/`, `output/` are created in `load_or_create` on both the create *and* load path — ensures directories always exist even if manually deleted between sessions.

### Wave 1 Integration Audit (2025-01-21)
- **Namespace isolation is critical**: FFmpeg modules (processor, assembler, types) were in global namespace. Wrapped all in `namespace ffmpeg {}` to prevent collisions with core or TUI types. Pattern: every module owns its namespace (srt::, core::, ffmpeg::, tui::).
- **Canonical include paths**: Switched from relative (`"parser.hpp"`, `"../../vendor/miniaudio.h"`) to canonical from `src/` root (`"srt/parser.hpp"`, `"miniaudio.h"`). Rationale: `target_include_directories` sets `src/` as root; canonical paths make modules relocatable and dependencies explicit.
- **miniaudio single-implementation rule**: Verified `#define MINIAUDIO_IMPLEMENTATION` appears in exactly one .cpp (`audio/recorder.cpp`). `audio/player.cpp` includes without the define. This is a hard requirement for single-header libraries.
- **Unused dependency removal**: `src/CMakeLists.txt` linked `nlohmann_json` despite `project.cpp` using hand-rolled JSON. Removed from `target_link_libraries` to keep the build lean.
- **TUI-first design in main.cpp**: Original `main.cpp` had complex CLI verbs (`record`, `process`, `assemble`, `full`) calling non-existent APIs. Simplified to minimal TUI launcher: parse args → load project → run App. Batch operations are future TUI features, not CLI verbs.
- **Type ownership verification**: `TakeStatus` (core::), `ProcessedClip` (ffmpeg::), `ProjectEntry` (core::) each defined once in their owning module. No duplicates. Cross-module usage via correct includes.
- **Pre-build audit pattern**: Reading all files before first build attempt caught 4 blocking issues (namespace, includes, unused lib, main API). Zero remaining build blockers. Surgical fixes preserve working code.


### Resync Feature Implementation
**Date:** 2026-04-14

- Implemented --resync flag for remapping takes to new SRT timing
- Text normalization: trim + collapse whitespace + lowercase
- Index fallback for edited text
- Orphaned takes counted but not deleted
- Files: src/core/resync.hpp, src/core/resync.cpp


### Recording Flow State Machine Extraction
**Date:** 2025-01-29

- Extracted recording screen interaction logic into pure state machine (`src/core/recording_flow.{hpp,cpp}`)
- Pattern: FlowState struct (idx, total, phase, has_take) + RecordingCmd enum (r/s/p/x/n/b/q + CountdownComplete) + FlowEffects struct for side-effect delegation
- TUI becomes thin adapter: parse keys → call `recording_step()` → apply effects (audio/project/navigation)
- Test harness: `tools/recording_harness.cpp` — JSON stdin/stdout protocol, no audio/TUI deps
- Test suite: `tests/recording_flow_test.cpp` — 22 tests, 100% pass rate
- Build: Added `srt_dubber_core` library (core + srt parser only, no audio/FTXUI) for testing
- Key insight: Countdown timer stays in TUI (timing is display concern); harness uses synthetic event to simulate completion
- Future slice: Apply same pattern to Review screen (similar interaction model)


### Slice 1 — Effect Variant Foundation
**Date:** 2026-04-30

- Created `src/core/effects.hpp` — standalone header (only `<filesystem>` include), no `.cpp` needed
- Defines four shared effect types in `namespace core`: `SaveTake` (int idx + path), `ClearTake` (int idx), `SaveProject` (empty), `ExitToSession` (empty)
- No CMake changes required (header-only, not yet included by any translation unit)
- Build verified clean (`srt_dubber_lib` target, zero warnings/errors)
- These types are the foundation for per-screen effect variants (Slices 2–6)


### Slice 2 — RecordingFlow Effect Variant Upgrade
**Date:** 2026-04-30

- Slice 2 complete: FlowEffects replaced with std::variant, RecordingEffect defined in recording_effects.hpp
- FlowEffects compat shim added to recording_effects.hpp for recording_screen.cpp — remove in Slice 4
- 22 tests pass with has_effect<T>() helper pattern
- recording_screen.cpp still compiles via FlowEffects::from_variants()
- New types in recording_effects.hpp: StartCountdown{idx}, CancelCountdown{}, ActivateCapture{}, StopRecording{idx, path}, PlayTake{path}, StopPlayback{}
- FlowTransition.effects changed from FlowEffects struct to std::vector<RecordingEffect>
- Harness emits effects as JSON string array (e.g. ["StartCountdown"])
- [[deprecated]] attribute on FlowEffects produces intentional compiler warnings as Slice 4 reminders
