#!/bin/sh
# Builds eqapo, installs the binary and presets, and registers the LaunchAgent.
# Usage: scripts/install.sh [prefix]   (default prefix: /usr/local if writable, else ~/.local)
set -eu
cd "$(dirname "$0")/.."
PREFIX="${1:-}"
if [ -z "$PREFIX" ]; then
  if [ -w /usr/local/bin ] 2>/dev/null; then PREFIX=/usr/local; else PREFIX="$HOME/.local"; fi
fi
cmake -S . -B build >/dev/null
cmake --build build -j8
./build/eqapo_tests >/dev/null && echo "unit tests: OK"
cmake --install build --prefix "$PREFIX" >/dev/null
echo "installed $PREFIX/bin/eqapo"
"$PREFIX/bin/eqapo" uninstall >/dev/null 2>&1 || true
"$PREFIX/bin/eqapo" install
echo
echo "If macOS shows a 'System Audio Recording' prompt, click Allow. Then check: $PREFIX/bin/eqapo status"
