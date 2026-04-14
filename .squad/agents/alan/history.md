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

