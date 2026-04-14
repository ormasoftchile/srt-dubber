# Audio Quality Fix — Session Log

**Date:** 2026-04-14T15:30:00Z  
**Scribe:** Scribe  
**Agent:** Butch (Audio & Backend Dev)

## Objective

Fix three Bluetooth-related recording issues reported by users of AirPods Max and similar Bluetooth headsets.

## Problems Addressed

1. **First part of recording cut off** — Bluetooth A2DP→HFP/SCO profile switch causes ~400 ms of absent/garbled samples immediately after `ma_device_start()`.
2. **~1s glitch** — same profile-switch transient also introduces an audible click in the early audio stream.
3. **Diagnostic blindness** — no feedback on which device was selected or whether the sample rate matched the requested 44100 Hz.

## Changes Made

### `src/audio/recorder.hpp`
- Added `static constexpr unsigned int kWarmupSamples = 44100 * 4 / 10` (400 ms at 44100 Hz)
- Added `std::atomic<bool> m_warmed_up` and `std::atomic<uint64_t> m_warmup_samples_discarded`
- Added `static void list_devices()` public method

### `src/audio/recorder.cpp`
- **Warm-up discard in `data_callback`:** Counts discarded frames; once ≥ `kWarmupSamples`, flips `m_warmed_up` and stamps `m_start_epoch_ms` — so `elapsed_ms()` reflects only real recorded audio.
- **Warm-up state reset in `start()`:** Resets both atomics before each new recording session.
- **Device name log after `ma_device_init()`:** `fprintf(stderr, "[audio] Recording device: %s\n", m_device->capture.name)`
- **Sample rate warning:** If `m_device->sampleRate != 44100`, prints a warning with the actual rate.
- **`list_devices()` implementation:** Uses `ma_context_get_devices()` to enumerate and print all capture devices with index and default marker; uninits context after use.

### `main.cpp`
- Added `--list-devices` flag check before TUI launch; calls `AudioRecorder::list_devices()` and exits.
- Added `--list-devices` to usage message.

## Outcomes

- ✅ Warm-up discard eliminates "first part cut off" and 1 s glitch for Bluetooth devices
- ✅ Device name printed to stderr on every `start()` call
- ✅ Sample rate mismatch surfaced proactively
- ✅ `--list-devices` allows users to identify and select the correct input device
- ✅ Build clean (no regressions)

## Decision Recorded

See `.squad/decisions.md` → **Audio Warm-up & Device Diagnostics** section for rationale and trade-offs.
