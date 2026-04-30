# Decisions Log

## Recent Decisions

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

## Previous Decisions

See orchestration log and session log for historical decisions and team context.
