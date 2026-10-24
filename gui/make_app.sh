#!/bin/sh
# Builds eqapo.app (menu bar front end) with SwiftPM and bundles the eqapo daemon binary.
# Usage: ./make_app.sh [path/to/eqapo]   (default: ../build/eqapo)
set -euo pipefail
cd "$(dirname "$0")"
EQAPO_BIN="${1:-../build/eqapo}"
swift build -c release
BIN="$(swift build -c release --show-bin-path)"

echo "checking PC Equalizer config format against ../presets/eq ..."
"$BIN/ConfigWriterCheck" ../presets/eq

APP="build/eqapo.app"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"
cp Info.plist "$APP/Contents/Info.plist"
cp "$BIN/EqualizerAPO" "$APP/Contents/MacOS/eqapo-ui"
chmod +x "$APP/Contents/MacOS/eqapo-ui"
cp Assets/eqapo.icns "$APP/Contents/Resources/eqapo.icns"
cp -R "$BIN/EqualizerAPO_EqualizerAPO.bundle" "$APP/Contents/Resources/"
if [ -x "$EQAPO_BIN" ]; then
  cp "$EQAPO_BIN" "$APP/Contents/MacOS/eqapo"
  mkdir -p "$APP/Contents/Resources/presets"
  cp -R ../presets/. "$APP/Contents/Resources/presets/"
else
  echo "warning: eqapo binary not found at $EQAPO_BIN; the app will look for it in PATH locations"
fi
codesign --force --deep --sign - --identifier com.eqapo.gui "$APP"
echo "built $APP"
