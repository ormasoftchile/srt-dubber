# Windows Development

## Option 1: Cross-compile from macOS (no VM needed)

Requires Docker Desktop.

    ./scripts/build-windows.sh

Output: `dist-windows/srt-dubber.exe`

To run and test the binary, use Option 2 or 3.

## Option 2: UTM (Apple Silicon — free)

1. Install UTM: https://mac.getutm.app/
2. Download Windows ARM ISO or x86_64 ISO
3. Create VM in UTM (QEMU backend for x86_64, Apple Virtualization for ARM)
4. Install ffmpeg via: `winget install Gyan.FFmpeg`
5. Copy `srt-dubber.exe` from `dist-windows/` and test

## Option 3: GitHub Actions

Every push to `main` builds a Windows binary.
Download from the Actions tab → **build-windows** artifact.

## ffmpeg on Windows

Install via one of:
- **winget:** `winget install Gyan.FFmpeg`
- **MSYS2:** `pacman -S mingw-w64-x86_64-ffmpeg`
- **Manual:** https://ffmpeg.org/download.html

Ensure `ffmpeg.exe` and `ffprobe.exe` are on `PATH`.

## Windows-specific notes

- **Audio backend:** WASAPI (handled automatically by miniaudio — no code changes needed)
- **Terminal:** Windows Terminal recommended for best FTXUI rendering
- **FTXUI rendering:** works on Windows Terminal, cmd.exe, and PowerShell
- **Static linking:** the `.exe` includes all C++ runtime libs (no DLL dependencies outside system DLLs)
- **cmake/windows-flags.cmake:** include from `CMakeLists.txt` for UTF-8 support, NOMINMAX, and WIN32_LEAN_AND_MEAN
