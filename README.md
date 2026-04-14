# srt-dubber

Record your own voice-over for SRT-subtitled videos. One take per subtitle slot, automatic timing alignment and normalization, final mux into MP4.

## Requirements

| Dependency | Notes |
|---|---|
| CMake ≥ 3.20 | Build system |
| C++20 compiler | clang++ 14+ or g++ 12+ |
| ffmpeg + ffprobe | Must be on `PATH` |
| miniaudio.h | Place in `vendor/` (see [vendor/README.md](vendor/README.md)) |

> **macOS**: Xcode Command Line Tools provide clang++.  
> **Linux**: `apt install cmake g++ ffmpeg`  
> **Windows**: MinGW-w64 cross-compile supported; ffmpeg must be available in the build environment.

## Build

```sh
# 1. Download miniaudio (one-time)
curl -L https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h \
     -o vendor/miniaudio.h

# 2. Configure & build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

The binary lands at `build/srt-dubber`.

### Debug build (with ASan/UBSan)

```sh
cmake .. -DCMAKE_BUILD_TYPE=Debug
cmake --build . -j$(nproc)
```

## Usage

```sh
# Launch interactive TUI to record one take per subtitle
srt-dubber record input.srt

# Batch-process all recorded takes (trim silence, normalize, fit duration)
srt-dubber process input.srt

# Assemble processed takes and mux into final video
srt-dubber assemble input.srt input.mp4

# All three steps in sequence (TUI first, then auto batch)
srt-dubber full input.srt input.mp4
```

## Output layout

```
takes/          raw recordings  (N.wav per subtitle)
processed/      trimmed & normalized (N.wav)
output/
  voiceover.wav assembled narration track
  output.mp4    final video with voice-over
```

## Project state

`project.json` tracks recording status per subtitle slot and is written next to
the working directory. It is `.gitignore`d — do not commit it.

## Architecture

```
src/
  srt/          SRT parser
  core/         Project state (types, load/save)
  audio/        miniaudio recorder & player
  ffmpeg/       ffmpeg/ffprobe wrappers (processor, assembler)
  tui/          FTXUI screens (session, recording, review, assemble)
main.cpp        Thin CLI dispatcher
vendor/
  miniaudio.h   Single-header audio library (not in git)
```

Dependencies fetched automatically by CMake FetchContent:
- [FTXUI v5.0.0](https://github.com/ArthurSonzogni/FTXUI) — TUI framework
- [nlohmann/json v3.11.3](https://github.com/nlohmann/json) — project.json serialization
