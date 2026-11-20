#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

IMAGE="hearth64-libdragon:preview"

if [[ "${1:-}" == "regen" ]] || [[ ! -d assets_raw/textures ]] || [[ ! -d assets_raw/audio ]]; then
    echo "==> Generating procedural N64 textures & cozy audio..."
    python3 tools/gen_textures.py
    python3 tools/gen_audio.py
fi

if ! docker info >/dev/null 2>&1; then
    echo "==> Starting Colima container runtime..."
    colima start --cpu 4 --memory 4
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
    echo "==> Building cached libdragon preview toolchain image (one-time setup)..."
    docker build -t "$IMAGE" .
fi

echo "==> Building hearth64.z64..."
docker run --rm -v "$(pwd)":/work -w /work "$IMAGE" make -j4

ls -lh hearth64.z64
echo "==> Build complete: $(pwd)/hearth64.z64"

if [[ "${1:-}" == "run" ]] || [[ "${2:-}" == "run" ]]; then
    echo "==> Launching in ares..."
    /Applications/ares.app/Contents/MacOS/ares --system "Nintendo 64" --no-file-prompt "$(pwd)/hearth64.z64" &
fi
