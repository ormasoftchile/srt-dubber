#!/usr/bin/env bash
# Build Windows .exe from macOS using Docker
set -e

docker build -f Dockerfile.windows-cross -t srt-dubber-windows-builder .
docker run --rm -v "$(pwd)/dist-windows:/src/dist-windows" \
    srt-dubber-windows-builder \
    sh -c "mkdir -p /src/dist-windows && cp /src/build-windows/srt-dubber.exe /src/dist-windows/"

echo "Built: dist-windows/srt-dubber.exe"
