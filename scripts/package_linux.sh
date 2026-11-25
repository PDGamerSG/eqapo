#!/bin/sh
# Package the portable Linux CLI and presets after a Linux build.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-$ROOT/build}
OUT=${1:-$ROOT/release/eqapo-linux-$(uname -m).tar.gz}

if [ ! -x "$BUILD_DIR/eqapo" ]; then
  echo "error: Linux binary not found at $BUILD_DIR/eqapo" >&2
  echo "build it with: cmake -S . -B build && cmake --build build" >&2
  exit 1
fi

STAGE=$(mktemp -d "${TMPDIR:-/tmp}/eqapo-linux.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT INT TERM
mkdir -p "$STAGE/eqapo/bin" "$STAGE/eqapo/share/eqapo"
cp "$BUILD_DIR/eqapo" "$STAGE/eqapo/bin/eqapo"
cp -R "$ROOT/presets" "$STAGE/eqapo/share/eqapo/presets"
cp "$ROOT/README.md" "$STAGE/eqapo/"
chmod +x "$STAGE/eqapo/bin/eqapo"
mkdir -p "$(dirname "$OUT")"
tar -C "$STAGE" -czf "$OUT" eqapo
echo "$OUT"
