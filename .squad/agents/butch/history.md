# Butch — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Butch's role:** Audio & Backend Dev — owns miniaudio capture/playback, all ffmpeg filter chains, WAV output, duration computation
- **Key collaborators:** Alan (architecture/SRT), Steven (TUI), Richard (build)
- **Audio spec:** mono, 44.1kHz, WAV PCM 16-bit; atempo max 1.08x; overflow = require redo

## Learnings

### Session — Bluetooth warm-up fix & device diagnostics
- **Root cause of "first part lost" + 1s glitch:** Bluetooth devices (AirPods Max) undergo an A2DP→HFP/SCO profile switch when a capture device is opened. This takes ~500–1000ms and delivers garbled/absent samples during the transition. Discarding the first 400ms of samples (17640 frames at 44100 Hz) eliminates both symptoms in one shot.
- **Warm-up implementation:** Added `m_warmed_up` + `m_warmup_samples_discarded` atomics. The `data_callback` counts discarded frames; once ≥ `kWarmupSamples` (= `44100 * 4 / 10`), it flips `m_warmed_up` and records the start timestamp. The visible `elapsed_ms()` timer therefore starts only after warm-up — no confusing "0.4s" offset shown to user.
- **`m_start_epoch_ms` dual-write pattern:** `start()` still writes an initial timestamp as a safe fallback, but the canonical "real start" is written by the data callback on warm-up completion. Because `elapsed_ms()` is read from the UI thread (not audio thread), `std::memory_order_relaxed` is fine for both the atomic write and read — no ordering guarantee needed across these unrelated operations.
- **Device name + sample rate diagnostics:** Printing `device.capture.name` after `ma_device_init()` lets the user confirm they're on Bluetooth vs built-in mic. A sample-rate mismatch warning (if device doesn't honour 44100 Hz) surfaces quality issues proactively.
- **`list_devices()` via `ma_context`:** Uses `ma_context_get_devices()` which returns a contiguous array owned by the context. Must uninit context after use. No heap allocation needed for device info arrays — miniaudio manages them internally within the context lifetime.

### Session — Audio & FFmpeg module creation
- **MA_IMPLEMENTATION guard:** `recorder.cpp` defines `#define MINIAUDIO_IMPLEMENTATION` before including `vendor/miniaudio.h`; `player.cpp` includes the header without the define. This satisfies the single-definition rule for the miniaudio implementation blob. (Macro name confirmed from `vendor/README.md` — use `MINIAUDIO_IMPLEMENTATION`, not `MA_IMPLEMENTATION`.)
- **PlayerContext pattern:** miniaudio's `pUserData` is a single `void*`. We heap-allocate a `PlayerContext` struct (holds `ma_device + ma_decoder + AudioPlayer*`) and cast it in the callback. The `m_device` field in `AudioPlayer` is repurposed as an opaque handle to this context.
- **Temp file placement:** processor temp files (`_trim.wav`, `_norm.wav`) are placed next to `output_wav` (same directory) to avoid cross-device `rename()` failures.
- **Overflow policy:** when `rate > 1.08x`, we still produce a file stretched at exactly 1.08x and set `is_overflow = true`. The TUI layer decides whether to prompt a redo.
- **atempo chain:** `atempo` filter only supports rates 0.5–2.0. Since our cap is 1.08x we never need to chain multiple `atempo` filters.
- **amix filter_complex:** each clip gets an `adelay={ms}|{ms}` label, then all labels feed into a single `amix=inputs=N:normalize=0`. `normalize=0` preserves individual clip levels post-loudnorm.
- **ffprobe output parsing:** `popen` + `fgets` loop then `std::stod`. Robust to trailing newline from ffprobe CSV output.

### Session — --device N flag for audio input selection
- **Motivation:** Mac mini has no built-in mic; user has two devices (AirPods Max at [0], ZoomAudioDevice at [1]) and needs to select by index.
- **AudioRecorder constructor now takes `int device_index = -1`:** stored as `m_device_index`. Default -1 = use system default (pDeviceID = nullptr, existing behavior unchanged).
- **Device selection in `start()`:** when `m_device_index >= 0`, opens a short-lived `ma_context`, calls `ma_context_get_devices()`, copies the `ma_device_id` at that index into a local `selected_id`, and sets `dev_cfg.capture.pDeviceID = &selected_id` before `ma_device_init()`. Context is immediately uninited after extracting the ID. Falls back to default with a warning if index is out of range.
- **`ma_device_id` lifetime:** the id must survive `ma_device_init()` — storing in a local on the stack within `start()` is safe since `ma_device_init` completes synchronously before the local goes out of scope.
- **App constructor updated:** `App(project, video_path, device_index)` — passes `device_index` straight to `AudioRecorder(device_index)` in the member initialiser list. No separate setter needed.
- **main.cpp arg parsing:** `--device N` consumed before the positional args via a `first_pos` offset. `--list-devices` path unchanged. Hint `[audio] Use --list-devices...` printed to stderr before TUI launch so user sees it regardless of device choice.
- **No changes to recording_screen.cpp:** device selection is encapsulated in the recorder itself, so no screen-level plumbing was needed.
