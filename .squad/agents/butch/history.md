# Butch — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Butch's role:** Audio & Backend Dev — owns miniaudio capture/playback, all ffmpeg filter chains, WAV output, duration computation
- **Key collaborators:** Alan (architecture/SRT), Steven (TUI), Richard (build)
- **Audio spec:** mono, 44.1kHz, WAV PCM 16-bit; atempo max 1.08x; overflow = require redo

## Learnings

### Session — Audio & FFmpeg module creation
- **MA_IMPLEMENTATION guard:** `recorder.cpp` defines `#define MINIAUDIO_IMPLEMENTATION` before including `vendor/miniaudio.h`; `player.cpp` includes the header without the define. This satisfies the single-definition rule for the miniaudio implementation blob. (Macro name confirmed from `vendor/README.md` — use `MINIAUDIO_IMPLEMENTATION`, not `MA_IMPLEMENTATION`.)
- **PlayerContext pattern:** miniaudio's `pUserData` is a single `void*`. We heap-allocate a `PlayerContext` struct (holds `ma_device + ma_decoder + AudioPlayer*`) and cast it in the callback. The `m_device` field in `AudioPlayer` is repurposed as an opaque handle to this context.
- **Temp file placement:** processor temp files (`_trim.wav`, `_norm.wav`) are placed next to `output_wav` (same directory) to avoid cross-device `rename()` failures.
- **Overflow policy:** when `rate > 1.08x`, we still produce a file stretched at exactly 1.08x and set `is_overflow = true`. The TUI layer decides whether to prompt a redo.
- **atempo chain:** `atempo` filter only supports rates 0.5–2.0. Since our cap is 1.08x we never need to chain multiple `atempo` filters.
- **amix filter_complex:** each clip gets an `adelay={ms}|{ms}` label, then all labels feed into a single `amix=inputs=N:normalize=0`. `normalize=0` preserves individual clip levels post-loudnorm.
- **ffprobe output parsing:** `popen` + `fgets` loop then `std::stod`. Robust to trailing newline from ffprobe CSV output.
