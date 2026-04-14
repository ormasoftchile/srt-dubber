# Flood — History

## Core Context
- **Project:** srt-dubber — a local C++20 TUI tool for recording voice-over takes per subtitle slot and assembling them into a dubbed video track
- **Owner:** ormasoftchile
- **Tech stack:** C++20, FTXUI (TUI), miniaudio (single-header audio), ffmpeg (CLI invocation), std::filesystem
- **Flood's role:** Windows & Cross-Platform — owns MinGW cross-compilation from Mac via Docker, Windows CI on GitHub Actions, WASAPI validation, Windows CMake flags
- **Key collaborators:** Richard (CMake/build), Butch (audio — WASAPI vs CoreAudio), Steven (TUI — Windows Terminal rendering)
- **Platform context:** Dev machine is macOS (Apple Silicon). Windows target is x86_64. Cross-compile via Docker + MinGW-w64. Local testing via UTM. CI via GitHub Actions `windows-latest`.
- **Audio backend on Windows:** miniaudio WASAPI — compiles automatically, but requires actual Windows for runtime validation
- **ffmpeg on Windows:** user installs via PATH (or MSYS2/winget) — document both

## Learnings

### Windows Build Infrastructure Setup (initial)
- **MinGW-w64 toolchain file** (`cmake/toolchains/mingw-w64.cmake`): uses `-static -static-libgcc -static-libstdc++` linker flags to produce a self-contained `.exe` with no MinGW DLL dependencies.
- **Dockerfile.windows-cross** (`debian:bookworm-slim` + `mingw-w64` apt package): cross-compiles `.exe` inside a Linux container from macOS. `curl` must be listed explicitly in the `apt-get install` line or the `miniaudio.h` download step will fail.
- **scripts/build-windows.sh**: mounts `dist-windows/` as a volume so the `.exe` lands on the host without extracting the container manually.
- **GitHub Actions `build-windows.yml`**: uses `windows-latest` (x86_64), installs via `choco`, downloads `miniaudio.h` at CI time. No `vendor/` is committed — header is fetched fresh each run.
- **cmake/windows-flags.cmake**: `NOMINMAX` and `WIN32_LEAN_AND_MEAN` are essential for FTXUI + miniaudio to not clash with Windows.h macro pollution. `/utf-8` is needed for C++20 string literals with non-ASCII subtitle text.
- **Static linking decision**: preferred over distributing DLLs. The resulting `.exe` is larger but portable without MSYS2/MinGW runtime on the target machine.
- **UTM vs GitHub Actions**: UTM is for audio runtime validation (WASAPI must be heard, not just compiled). GitHub Actions is the definitive build correctness gate.

### Docker Cross-Compile Verification (2025-04-14)
- **FTXUI v5.0.0 → main branch**: Switched from tagged release to main branch due to ongoing updates. Both v5.0.0 and main compile cleanly once threading is fixed.
- **MinGW threading model critical**: Default `mingw-w64` package in Debian Bookworm uses win32 threading, which doesn't expose `std::mutex`. Must use `g++-mingw-w64-x86-64-posix` and `gcc-mingw-w64-x86-64-posix` packages, then set alternatives to posix variants.
- **MSVC-only flags isolated**: `/utf-8` and `CMAKE_MSVC_RUNTIME_LIBRARY` are MSVC-specific. Wrapped in `if(MSVC)` guard in `windows-flags.cmake` to avoid breaking MinGW builds.
- **pthread flags added**: Added `-pthread` to `CMAKE_CXX_FLAGS` and `CMAKE_C_FLAGS` in `mingw-w64.cmake` toolchain file to enable POSIX threading support.
- **Build output**: Successfully produced 4.3MB PE32+ executable (x86-64) for MS Windows at `dist-windows/srt-dubber.exe`.
- **Docker build time**: ~60 seconds for full clean build including FetchContent downloads of FTXUI (from main).
