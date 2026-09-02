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
```

The application opens an interactive session:

1. Press `r` to enter the recording screen. Each SRT entry is one narration
  slot; use `r` to record, `s` to stop, `n`/`b` to navigate, and `p` to play
  the current take.
2. Press `v` from the session screen to review or redo recorded takes.
3. Press `a` to assemble. Raw takes are trimmed, normalized, and sped up by at
  most 8% when needed to fit their SRT slots. Overflowing takes are flagged.
4. Assembly places every take at its SRT start time and replaces the source
  video's audio with the new voice-over track.

The source video is not played inside the recording TUI.

### Deckpilot handoff

Deckpilot auto-record exports an MP4 and SRT with the same basename. Pass that
pair directly to srt-dubber:

```sh
srt-dubber session-ID.srt session-ID.mp4
```

## Output layout

Files are created beside the input SRT:

```
input-project.json   recording state and take metadata
takes/               raw recordings (N.wav per subtitle)
processed/           trimmed, normalized, and time-fitted takes (N.wav)
output/
  voiceover.wav      assembled narration track
  output.mp4         source video with the voice-over audio
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
