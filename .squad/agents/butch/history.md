# Butch — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Butch's role:** Audio & Backend Dev — owns miniaudio capture/playback, all ffmpeg filter chains, WAV output, duration computation
- **Key collaborators:** Alan (architecture/SRT), Steven (TUI), Richard (build)
- **Audio spec:** mono, 44.1kHz, WAV PCM 16-bit; atempo max 1.08x; overflow = require redo

### Consolidated Learning — Bluetooth Warm-up, Device Selection, FFmpeg Pipeline (2025–2026-04-14)

**Bluetooth warm-up fix:**
- Root cause: A2DP→HFP/SCO profile switch takes 500–1000ms on first capture device open, delivers garbled samples during transition
- Solution: Discard first 400ms of samples (17640 frames at 44100 Hz); atomic flags track warm-up completion; visible timer starts post-warm-up
- Dual-write pattern on m_start_epoch_ms: fallback + data_callback canonical write; memory_order_relaxed sufficient (UI thread only reads)

**Device selection:**
- AudioRecorder constructor accepts device_index (-1 = default, ≥0 = select by index)
- ma_device_id lifetime safe: extracted into local, used synchronously in ma_device_init, never accessed after
- Device name + sample rate diagnostics printed after init; list_devices() uses ma_context with automatic cleanup

**miniaudio & FFmpeg patterns:**
- MINIAUDIO_IMPLEMENTATION defined only in recorder.cpp; player.cpp includes header without define
- PlayerContext pattern: heap-allocate struct with (ma_device, ma_decoder, AudioPlayer*), cast in callback
- Temp files placed next to output_wav to avoid cross-device rename() failures
- Overflow policy: stretch at 1.08x, set is_overflow flag, let TUI decide redo
- atempo filter chain: single filter (supports 0.5–2.0x, our cap is 1.08x)
- amix filter_complex: adelay labels + single amix=inputs=N:normalize=0 (preserves clip levels post-loudnorm)
- ffprobe output: popen + fgets loop + stod (robust to trailing newlines)

## Recent Work — Slices 4 & 6B Complete
- Read `recorder.hpp`, `player.hpp`, `project.hpp`, `recording_effects.hpp`, `recording_screen.cpp`
- Dispatch spec written to `.squad/decisions/inbox/butch-slice4-dispatch-spec.md`
- Key findings:
  - `recorder_.start(path)` opens device + starts warm-up; `set_capture_active(true)` gates actual WAV writes — must preserve this two-step in dispatcher
  - `CancelCountdown` MUST call `recorder_.stop()` if `is_recording()` — current inline code omits this, leaving device open on cancel
  - `PlayTake` needs `!recorder_.is_recording()` guard + defensive `player_.stop()` before `play()` (player has no "already playing" check)
  - `SaveTake` is in-memory only; `SaveProject` is the explicit flush — do not conflate them
  - `ExitToSession` is TUI-only; dispatcher no-op

### Slice 4 Complete — RecordingEffectDispatcher Implementation — 2026-04-30
**With Alan (collaborative implementation)**
- **Spec implemented:** All findings from dispatch prep become RecordingEffectDispatcher centralisation rules
- **Key architecture:**
  - `src/core/effect_dispatcher.hpp/.cpp` defines `RecordingEffectDispatcher` with `apply_all(const std::vector<RecordingEffect>&)` method
  - Dispatcher owns AudioRecorder + AudioPlayer + Project references (no cross-module leak from TUI)
  - std::visit pattern safely dispatches each effect type → correct method call on owned resources
  - Recorder two-step (`start(path)` + `set_capture_active(true)`) preserved in StartCountdown handler
  - CancelCountdown bug FIXED: now correctly calls `recorder_.stop()` when device is open (was missing in inline code)
  - PlayTake guards with `!recorder_.is_recording()` + defensive `player_.stop()` before playback
  - SaveTake/SaveProject contract preserved (in-memory vs flush)
  - ExitToSession no-op (TUI still owns navigation)
- **Integration:**
  - `recording_screen.cpp` uses `dispatcher_.apply_all(transition.effects)` — no more inline audio/project calls
  - [[deprecated]] FlowEffects::from_variants() no longer called from TUI → warning eliminated
  - recording_screen.cpp now thin adapter: parse keys → call `recording_step()` → use dispatcher
- **Testing:**
  - Stub headers (tests/audio/recorder.hpp, tests/audio/player.hpp) shadow real headers in test build
  - effect_dispatcher_test.cpp: 10 assertions across 4 test cases, all pass
  - 22 recording-flow-tests + 3 recording-render-state-tests still pass
  - Build clean, zero errors

**Benefit:** TUI layer no longer knows about audio/project APIs. All mutations flow through dispatcher — single audit point for side effects.

### Recording Flow State Machine Pattern — 2026-04-30

**Alan extracted recording screen interaction logic into pure state machine** (`src/core/recording_flow.{hpp,cpp}`).

**Pattern:** FlowState (idx/total/phase/has_take) + RecordingCmd enum (r/s/p/x/n/b/q + CountdownComplete) + FlowEffects struct for side-effect delegation.

**Turn-based harness:** `tools/recording_harness.cpp` — JSON stdin/stdout protocol enables scripted testing without TUI/audio.

**Test suite:** `tests/recording_flow_test.cpp` — 22 tests, 100% pass.

**Benefit for Butch:** The new `srt_dubber_core` library (core + srt parser, no audio/TUI deps) enables future audio processing tests to link against project state without instantiating full audio devices. Consider extracting processor/assembler logic into harness-compatible patterns for integration testing of ffmpeg chains.

### Slice 6B — AssembleFlow TUI Wiring — 2026-04-30

- **Command queue pattern:** Assembly thread cannot call `assemble_step()` directly (it runs concurrently). Instead it pushes `(AssembleCmd, payload)` pairs into a `std::vector` protected by a mutex, then fires `screen.PostEvent(Event::Custom)`. The FTXUI event handler drains the queue on every event and calls `assemble_step()` serially — keeps the state machine single-threaded.
- **Auto-start on entry:** `assemble_step(state, AssembleCmd::Start)` called immediately before the screen loop. `SpawnAssembly{}` effect handler captures clips, voiceover_out, video_out and spawns the `std::jthread`. Clips are collected inside the effect handler, not in the thread lambda, to keep capture clean.
- **jthread stop is best-effort:** ffmpeg call inside the thread is blocking. `assembly_thread.request_stop()` on 'q' sets the stop token but the thread only checks it at the next cooperative point. Thread destructor auto-joins — no dangling thread.
- **Removed from `Project`:** `assemble_log`, `assemble_complete`, `output_path` — all three were transient and only read by `assemble_screen.cpp`. Now replaced by `AssembleState` owned locally in `run_assemble_screen`. Confirmed safe — no other file used these fields.
- **Renderer decoupled:** Reads from `assemble_render_state(assemble_state)` snapshot only; no direct lock or project access.
- **Effect handler inlined:** No separate `AssembleEffectDispatcher` class — `apply_effects` lambda handles all four effect types inline. `SaveProject` calls `project.save()`; `ExitToSession` calls `screen.ExitLoopClosure()()`.

### Slices 6B Complete — Full Assembly Pipeline Testable
**Date:** 2026-04-30

**Wiring Phase (Phase B):** assemble_screen.cpp fully wired to AssembleFlow machine using command queue pattern. Assembly thread posts AssembleCmd to queue protected by mutex; FTXUI event handler drains queue on every custom event and calls assemble_step() serially.

**Project Cleanup:** Removed three transient fields from Project struct: `assemble_log`, `assemble_complete`, `output_path`. All three were read/written only in assemble_screen.cpp. Replacement: `AssembleState` now owned locally in `run_assemble_screen()`, fed to assemble_step() and assemble_render_state() pure functions.

**Threading Safety:** jthread pattern with stop_token. Thread is blocking (ffmpeg subprocess); stop token is best-effort only. Thread destructor auto-joins.

**Benefit:** Assembly screen no longer mutates Project directly. All state flows through pure state machine. Decouples threading concerns from business logic. Machine is testable in isolation (Phase A); TUI wiring (Phase B) adds threading without changing core logic.

