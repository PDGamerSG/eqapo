#!/bin/sh
# Builds EqualizerAPO.app (menu bar front end) with SwiftPM and bundles the eqapo daemon binary.
# Usage: ./make_app.sh [path/to/eqapo]   (default: ../build/eqapo)
set -eu
cd "$(dirname "$0")"
EQAPO_BIN="${1:-../build/eqapo}"
swift build -c release 2>&1 | grep -vE "^\s*$" | tail -5
BIN="$(swift build -c release --show-bin-path)"

echo "checking PC Equalizer config format against ../presets/eq ..."
"$BIN/ConfigWriterCheck" ../presets/eq

APP="build/EqualizerAPO.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp Info.plist "$APP/Contents/Info.plist"
cp "$BIN/EqualizerAPO" "$APP/Contents/MacOS/EqualizerAPO"
if [ -x "$EQAPO_BIN" ]; then
  cp "$EQAPO_BIN" "$APP/Contents/MacOS/eqapo"
  mkdir -p "$APP/Contents/Resources/presets"
  cp -R ../presets/. "$APP/Contents/Resources/presets/"
else
  echo "warning: eqapo binary not found at $EQAPO_BIN; the app will look for it in PATH locations"
fi
codesign --force --deep --sign - --identifier com.eqapo.gui "$APP"
echo "built $APP"
