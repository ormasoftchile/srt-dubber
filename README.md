# srt-dubber

Record your own voice-over for SRT-subtitled videos. One take per subtitle slot, automatic timing alignment and normalization, final mux into MP4.

## Requirements

| Dependency | Notes |
|---|---|
| CMake ≥ 3.20 | Build system |
| C++20 compiler | clang++ 14+, g++ 12+, or Visual Studio 2022 |
| Git and `patch` | CMake downloads and patches FTXUI during configuration |
| ffmpeg + ffprobe | Runtime dependency; both must be on `PATH` |
| miniaudio.h | Place in `vendor/` (see [vendor/README.md](vendor/README.md)) |

The first CMake configure also needs internet access to download FTXUI and
nlohmann/json.

## Build on Linux

On Debian or Ubuntu:

```sh
sudo apt update
sudo apt install cmake g++ ffmpeg git curl patch
```

Then, from the repository root:

```sh
# 1. Download miniaudio (one-time)
curl -L https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h \
     -o vendor/miniaudio.h

# 2. Configure and build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The binary lands at `build/srt-dubber`.

## Build on macOS

Install Xcode Command Line Tools, CMake, and ffmpeg:

```sh
xcode-select --install
brew install cmake ffmpeg
```

Then run the same configure and build commands shown for Linux. macOS already
provides Git, curl, and `patch` with the Command Line Tools.

## Build on Windows

For a native build, install Visual Studio 2022 with the **Desktop development
with C++** workload, Git for Windows, and ffmpeg. Then run this from PowerShell:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\build-windows.ps1
```

The script locates Visual Studio's bundled CMake, enables Git's `patch` tool,
downloads miniaudio when needed, and builds `build/Release/srt-dubber.exe`.
Docker-based cross-compilation is also available; see
[Windows development](docs/windows-dev.md).

## Debug build

Debug builds enable AddressSanitizer and UndefinedBehaviorSanitizer on
non-MSVC compilers. Use a separate build directory so release and debug
artifacts do not get mixed:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel
```

## Usage

```sh
# Record narration and assemble it into the source video
srt-dubber input.srt input.mp4

# Record without a video; assembly will require restarting with a video path
srt-dubber input.srt

# Select a non-default microphone
srt-dubber --list-devices
srt-dubber --device 2 input.srt input.mp4

# Preserve matching takes after updating subtitle text or timing
srt-dubber --resync updated.srt

# Process and measure every take without fitting the current SRT slots
srt-dubber --prepare input.srt

# Assemble an existing project after the final video timing is known
srt-dubber --assemble input.srt input.mp4
```

The application opens an interactive session:

1. Press `r` to enter the recording screen. Each SRT entry is one narration
  slot; use `r` to record, `s` to stop, `n`/`b` to navigate, and `p` to play
  the current take.
2. Press `v` from the session screen to review or redo recorded takes.
3. Press `a` to assemble. Raw takes are trimmed, normalized, and sped up by at
  most 8% when needed to fit their SRT slots. If a take still overflows, the
  output timeline is extended with a held video frame and silent source audio.
4. Assembly places every take at its SRT start time and mixes the new
  voice-over with source audio, shifting later narration by any inserted holds
  and preserving video-item audio from Deckpilot. A limiter prevents clipping.
  Videos without audio continue to work.

The source video is not played inside the recording TUI.

### Deckpilot handoff

Deckpilot Auto-Record owns a narration-first handoff. It runs these stages
automatically; the only interactive step is recording and reviewing each take:

```sh
srt-dubber narration.srt
srt-dubber --prepare narration.srt
# Deckpilot captures the presentation using the measured take durations,
# then replaces the provisional SRT timestamps with the capture timestamps.
srt-dubber --resync narration.srt
srt-dubber --assemble narration.srt presentation.mp4
```

`--prepare` trims and normalizes the takes without tempo-fitting them to the
provisional cue slots. Their processed durations therefore remain authoritative
for presentation pacing. `--resync` preserves matching takes after Deckpilot
writes the final timing, and `--assemble` produces `output/narration-dubbed.mp4`.

For deterministic automation, pre-recorded takes named by SRT index can be
imported and assembled without opening the TUI:

```sh
srt-dubber --assemble-with-takes session-ID.srt session-ID.mp4 fixture-takes/
```

This command is intended for tests and scripted workflows. Normal narration
still uses the interactive microphone-recording flow.

Remote microphone quality is controlled by the remote desktop client and the
local input device. If redirected audio is narrowband, record PCM WAV takes on
the local computer, name them `1.wav`, `2.wav`, and so on by subtitle index,
then copy the directory to the remote computer and use `--assemble-with-takes`.
This avoids remote microphone compression.

## Output layout

Files are created beside the input SRT:

```
input-project.json   recording state and take metadata
takes/               raw recordings (N.wav per subtitle)
processed/           trimmed/normalized takes; standard assembly may slot-fit them
output/
  voiceover.wav      assembled narration track
  <name>-dubbed.mp4  source video with the voice-over audio
```

## Project state

`<srt-name>-project.json` tracks recording status per subtitle slot and is
written beside the input SRT. It is `.gitignore`d — do not commit it.

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
- [FTXUI](https://github.com/ArthurSonzogni/FTXUI) — TUI framework (pinned commit)
- [nlohmann/json v3.11.3](https://github.com/nlohmann/json) — project.json serialization
