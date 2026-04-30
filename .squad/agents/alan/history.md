# Alan — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Alan's role:** Lead & Architect — owns project structure, SRT parsing, session state model, code review
- **Key collaborators:** Butch (audio/ffmpeg), Steven (TUI/FTXUI), Richard (build/CMake)

### Consolidated Learning — Initial Implementation & Wave 1 Integration (2025–2026-04-14)

**Core data model:**
- Hand-rolled JSON layer in project.cpp; if nlohmann/json is added later, only project.cpp needs updates
- Namespace discipline (srt::, core::, ffmpeg::, tui::) prevents collisions and enables independent testing
- SRT parser uses linear state machine pattern, not regex — keeps dependencies minimal
- C++20 designated initialisers for clarity and type safety
- Project file named `<stem>-project.json` to avoid collision when multiple SRT files exist
- Directory creation (takes/, processed/, output/) happens on both create and load paths

**Integration patterns:**
- Canonical include paths from src/ root make modules relocatable and dependencies explicit
- MINIAUDIO_IMPLEMENTATION defined in exactly one .cpp (audio/recorder.cpp); audio/player.cpp includes without
- Type ownership verified: TakeStatus (core::), ProcessedClip (ffmpeg::), ProjectEntry (core::) each defined once
- TUI-first design: main.cpp is thin dispatcher; complex business logic lives in srt_dubber_lib

**Resync feature (2026-04-14):**
- Added --resync flag for remapping takes to new SRT timing
- Text normalization with index fallback for edited text; orphaned takes counted (not deleted)

**Recording flow extraction (pre-2026-04-30):**
- Pure state machine pattern proven effective for recording screen
- srt_dubber_core library established (core + parser only, no audio/TUI) for testable unit tests
- Harness and render_state patterns establish foundation for Slices 2–7

## Recent Work — Slices 1–7 Complete


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


### Slice 3 — Render State Extraction
**Date:** 2026-04-30

- Slice 3 done: RecordingRenderState + render_state() added
- Created `src/core/recording_render_state.hpp` — pure snapshot struct (current_idx, total, phase_label, has_take, entry_text, slot_duration_ms, elapsed_ms)
- Added `render_state()` declaration to `recording_flow.hpp` and implementation to `recording_flow.cpp`
- recording_screen.cpp renderer now reads from rs.* snapshot instead of querying flow/recorder directly
- recorder.is_warming_up() remains a direct call (not in snapshot — display-only warmup state)
- Harness emits render field in JSON output (idx, total, phase, has_take) using fixture entry data
- recording_render_state_test.cpp: 3 tests, all pass
- All 22 recording-flow-tests still pass; build clean (only expected [[deprecated]] warning)


### Slice 4 — Effect Dispatcher Infrastructure
**Date:** 2026-04-30

- Slice 4 done: RecordingEffectDispatcher created (src/core/effect_dispatcher.hpp + effect_dispatcher.cpp)
- Dispatcher centralises all audio (AudioRecorder, AudioPlayer) and project mutation side effects
- recording_screen.cpp now uses dispatcher.apply_all() — FlowEffects compat bridge no longer called from TUI
- Bug fixed: CancelCountdown now calls recorder_.stop() if recorder is_recording() or is_warming_up()
- StartCountdown computes take path from project_.takes_dir() / entries[idx].index + ".wav"
- StopRecording uses current_take_path_ (stored at StartCountdown) — AudioRecorder has no output_path()
- PlayTake: guarded by is_recording(), resolves empty path from project_.entries()[current_idx_]
- [[deprecated]] warning from FlowEffects::from_variants() is now gone (nothing calls it)
- Countdown thread management stays in TUI (Slice 4b deferred)
- ExitToSession is no-op in dispatcher; TUI still handles navigation
- Stub headers: tests/audio/recorder.hpp + tests/audio/player.hpp shadow real headers for test target
- Effect dispatcher integration test: 10 assertions pass (4 test cases)
- All 22 recording-flow-tests pass; all 3 recording-render-state-tests pass; build clean zero errors


### Slice 5 — ReviewFlow State Machine
**Date:** 2026-04-30

- Created `src/core/review_flow.hpp` — ReviewCmd enum (Up/Down/PlayTake/Redo/Back), ReviewState (selected_idx, total), ReviewEffect variant (PlayReviewTake, StopReviewPlay, NavigateToRecord, ExitToSession), ReviewTransition, ReviewRenderState + ReviewEntryRow
- Created `src/core/review_flow.cpp` — parse_review_cmd(), review_step() pure state machine, review_render_state() snapshot builder
- Created `src/core/review_effect_dispatcher.hpp/.cpp` — ReviewEffectDispatcher (player-only, no recorder/project); take_navigate_to_record() consumer pattern; exit_to_session_requested()
- Created `tests/review_flow_test.cpp` — 18 tests, 100% pass rate
- Wired `src/tui/screens/review_screen.cpp` to machine: 156→139 LOC; renderer reads from review_render_state() snapshot; key handler routes through parse_review_cmd + review_step + dispatcher
- CMakeLists: review_flow.cpp added to srt_dubber_core + srt_dubber_lib; review_effect_dispatcher.cpp added to srt_dubber_lib; review-flow-tests executable added
- All tests pass: 22 recording-flow, 3 render-state, 10 effect-dispatcher, 18 review-flow; build clean


### Slice 6 Phase A — AssembleFlow Pure State Machine
**Date:** 2026-04-30

- Created `src/core/assemble_flow.hpp` — AssembleCmd enum, AssemblePhase enum, AssembleState struct, AssembleEffect variant (SpawnAssembly, AppendLog, SaveProject, ExitToSession), AssembleTransition, assemble_step(), AssembleRenderState, assemble_render_state()
- Created `src/core/assemble_flow.cpp` — pure state machine, no TUI/audio/threading deps
- Created `tests/assemble_flow_test.cpp` — 10 tests, 10/10 pass; has_effect<T>() helper pattern
- Updated `src/CMakeLists.txt` — added assemble_flow.cpp to srt_dubber_lib
- Updated root `CMakeLists.txt` — added assemble_flow.cpp to srt_dubber_core; added assemble-flow-tests executable
- assemble_screen.cpp NOT touched — threading/TUI wiring is Phase B (separate commit)
- All prior tests still pass: 22 recording-flow, 3 recording-render-state, 10 effect-dispatcher, 10 assemble-flow
- Build clean, zero errors


### Slice 7 — AppSession Router State Machine
**Date:** 2026-04-30

- Created `src/core/app_session.hpp` — NavState struct (screen, recording_idx, review_idx, running), NavCmd enum, NavTransition, nav_step() declaration in namespace core
- Created `src/core/app_session.cpp` — pure nav_step() switch: no TUI/audio deps, no side effects
- Updated `src/tui/app.cpp` — replaced `while(running) { switch(current_screen_) }` with loop over NavState; each screen result translated to NavCmd and fed to nav_step()
- Updated `src/tui/app.hpp` — removed stale private members (current_screen_, recording_start_, review_selected_) now owned by NavState
- Updated root CMakeLists.txt: added app_session.cpp to srt_dubber_core, added enable_testing() + add_test() for all test executables (they existed but were unregistered with CTest), added app-session-tests executable
- Updated src/CMakeLists.txt: added app_session.cpp to srt_dubber_lib
- Created `tests/app_session_test.cpp` — 10 tests, 10/10 pass: initial state, GoRecord, GoReview, GoAssemble, RecordingDone, ReviewGoRecord(payload=3), ReviewGoSession, Quit, AssembleDone, review_idx preservation
- Discovered that enable_testing()/add_test() were missing — all prior test executables were unregistered; fixed as part of this slice
- All 6 CTest targets pass: recording-flow, recording-render-state, effect-dispatcher, assemble-flow, review-flow, app-session
- Build clean, zero errors or warnings

### Slices 5–7 Complete — Full Interaction Architecture Ready
**Date:** 2026-04-30

**Slice 5: ReviewFlow** — review_flow.hpp/cpp extracted interaction logic from review_screen.cpp. ReviewState + ReviewCmd + ReviewEffect variant. 18 tests pass. review_screen.cpp wired: 156→139 LOC reduction. Effect dispatcher handles all side effects (player only, no audio capture).

**Slice 6: AssembleFlow** — assemble_flow.hpp/cpp pure state machine (no threading). AssembleState, AssembleCmd, AssembleEffect variant. 10 tests pass. Phase A complete; TUI wiring deferred to Phase B.

**Slice 7: AppSession Router** — nav_step() pure routing function in srt_dubber_core. App::run() now driven by NavState machine. 10 tests pass. All screen transitions testable. recording_idx and review_idx belong in nav state (routing context); screens still own display state internally.

**Architecture Pattern:** Every screen has a pure state machine (recording_flow, review_flow, assemble_flow, nav_step). TUI adapters read effects and render snapshots. Dispatcher centralizes side effects. srt_dubber_core contains all testable logic.

**Key Achievement:** Full interaction layer now testable without TUI/audio. Follows variant+dispatcher pattern consistently across all four screens.
