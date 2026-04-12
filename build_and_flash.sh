#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
PICOTOOL="$(nix develop --command which picotool)"

cd "$SCRIPT_DIR"

echo "==> Configuring (cmake)..."
nix develop --command cmake -G Ninja -B "$BUILD_DIR" -S . -Wno-dev

echo "==> Building..."
nix develop --command ninja -C "$BUILD_DIR"

echo "==> Flashing..."
sudo env PATH="$(dirname "$PICOTOOL"):$PATH" bash "$SCRIPT_DIR/flash.sh"
