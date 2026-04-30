# Decisions Log

## Recent Decisions

### 2026-04-30: Slice 5 complete — ReviewFlow state machine
**By:** Alan

**What:** review_flow.hpp/.cpp, review_effect_dispatcher.hpp/.cpp, review_render_state created. 18 tests pass. review_screen.cpp wired to machine (156→139 LOC reduction).

**Why:** Review screen interaction now testable without TUI/audio. State machine follows same variant+dispatcher pattern as recording flow (Slices 2–4).

**Key Achievement:** Complete interaction layer for Review screen; reusable dispatcher pattern proven.

---

### 2026-04-30: Slice 6 Phase A complete — AssembleFlow pure machine
**By:** Alan

**What:** assemble_flow.hpp/.cpp created with AssembleState, AssembleCmd, AssembleEffect variant, assemble_step(), assemble_render_state(). 10 tests pass. assemble_screen.cpp NOT yet touched (threading wiring deferred to Phase B).

**Why:** High-risk slice — machine tested in isolation before threading integration reduces risk of regression.

**Design:** Pure state machine abstraction over assembly orchestration.

---

### 2026-04-30: Slice 6B complete — AssembleFlow TUI wiring
**By:** Butch

**What:** assemble_screen.cpp wired to AssembleFlow machine using command queue pattern (thread→machine posts). Removed transient Project fields: assemble_log, complete, output_path (safe — only used in assemble_screen.cpp, verified by grep).

**Why:** Eliminates direct state mutation in TUI; machine owns all assembly logic. Decouples thread from business logic.

**Tradeoffs:** Assembly thread remains blocking (ffmpeg subprocess); stop_token is best-effort only.

**Key Achievement:** Project struct simplified; all assembly state now flows through machine.

---

### 2026-04-30: Slice 7 complete — AppSession Router
**By:** Alan

**What:** NavState + nav_step() pure routing function in srt_dubber_core. App::run() now driven by nav_step() state machine. 10 navigation tests pass; all screen transitions covered.

**Why:** Navigation logic is testable without TUI. Routing context (recording_idx, review_idx) belongs in NavState, not UI layer — screens still own display state internally.

**Key Achievement:** Full session navigation testable in isolation; clean separation of routing from screen concerns.

**Next:** Multi-screen harness can now simulate full session flows.

---

### 2026-04-30: Slice 8 complete — Multi-screen app harness
**By:** Richard

**What:** tools/app_harness.cpp standalone harness drives nav_step + all screen machines (recording, review, assemble) via JSON stdin/stdout. tests/app_harness_test.sh covers 5 end-to-end flows with 26 assertions. Links against srt_dubber_core + nlohmann_json only (no TUI, audio, or threading).

**Why:** Proves full interaction architecture is testable without TUI. Closes the 8-slice plan.

**Design:** Pure machines + effect dispatch layer + independent harness validation.

**Tradeoffs:** SpawnAssembly effect is a no-op in harness (threading is TUI-only); AssemblyDone injected manually in tests.

**Limitations:** No multi-turn session persistence; no real project.json loading; fixture hardcoded.

**Key Achievement:** Complete interaction layer architecture validated end-to-end without TUI dependency.

**Phase 2 Planning:** Countdown timer in dispatcher, event sourcing/replay infrastructure.

---

### 2026-04-30: Slice 1 complete — effects.hpp created
**By:** Alan

**What:** Created src/core/effects.hpp with shared effect types: SaveTake, ClearTake, SaveProject, ExitToSession. Header-only, no CMake changes needed.

**Why:** Foundation for all per-screen effect variants (Slices 2–6).

---

### 2026-04-30: Slice 2 complete — RecordingFlow effect variants
**By:** Alan

**What:** FlowEffects bool struct replaced with std::vector<RecordingEffect> variant list. recording_effects.hpp defines StartCountdown, CancelCountdown, ActivateCapture, StopRecording, PlayTake, StopPlayback plus the shared types from effects.hpp. FlowEffects compat shim added — to be removed in Slice 4. All 22 tests pass. Harness emits effect names in JSON.

**Why:** Foundation for typed effect dispatch (Slice 4). Variant allows effects to carry payloads (path, idx) — bool flags cannot.

---

### 2026-04-30: Slice 3 complete — render state extraction
**By:** Alan

**What:** Added RecordingRenderState struct and render_state() pure function. recording_screen.cpp renderer now reads from snapshot (rs.*). Harness emits render field. 3 render state tests pass.

**Why:** Frontends must not read machine internals directly. render_state() is the contract between state machine and display layer.

---

### 2026-04-30: Slice 4 complete — RecordingEffectDispatcher
**By:** Alan + Butch

**What:** Created RecordingEffectDispatcher in src/core/effect_dispatcher.hpp/.cpp. Centralises all audio and project mutation calls. recording_screen.cpp uses dispatcher.apply_all() — FlowEffects compat bridge no longer called from TUI. Bug fixed: CancelCountdown now closes recorder if open.

**Why:** Single place for all side effects. TUI screen no longer knows about AudioRecorder/Project API directly.

---

## Previous Decisions

See orchestration log and session log for historical decisions and team context.
