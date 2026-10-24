#!/bin/sh
# Build and install the complete eqapo macOS app, daemon, presets, and startup agents.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
APP_PREFIX="${1:-/Applications}"
APP_PATH="$APP_PREFIX/eqapo.app"

cd "$ROOT"
cmake -S . -B build >/dev/null
cmake --build build -j8
./build/eqapo_tests >/dev/null

"$ROOT/gui/make_app.sh" "$ROOT/build/eqapo"
mkdir -p "$APP_PREFIX"
# Replace a running menu-bar process cleanly so an upgrade uses the new bundle
# immediately; the LaunchAgent will start it again after the copy completes.
uid=$(id -u)
launchctl bootout "gui/$uid/com.eqapo.gui" 2>/dev/null || true
killall -TERM eqapo-ui 2>/dev/null || true
ditto --rsrc --extattr "$ROOT/gui/build/eqapo.app" "$APP_PATH"

# Install the menu-bar LaunchAgent before starting the app. Starting the app
# manually first would cause its first-run self-registration to launch a second
# copy; bootstrapping it here keeps the running instance launchd-owned.
GUI_PLIST="$HOME/Library/LaunchAgents/com.eqapo.gui.plist"
mkdir -p "$(dirname "$GUI_PLIST")"
cat > "$GUI_PLIST" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>Label</key><string>com.eqapo.gui</string>
<key>ProgramArguments</key><array><string>$APP_PATH/Contents/MacOS/eqapo-ui</string></array>
<key>RunAtLoad</key><true/>
<key>KeepAlive</key><false/>
<key>ProcessType</key><string>Interactive</string>
<key>LimitLoadToSessionType</key><string>Aqua</string>
</dict></plist>
EOF
for attempt in 1 2 3 4 5; do
  launchctl bootstrap "gui/$uid" "$GUI_PLIST" 2>/dev/null && break
  sleep 0.2
done
launchctl kickstart -k "gui/$uid/com.eqapo.gui"

# Register the bundled daemon so audio processing starts at login. The app
# also keeps the startup setting available in its menu.
"$APP_PATH/Contents/MacOS/eqapo" install

echo "installed $APP_PATH"
echo "daemon: $APP_PATH/Contents/MacOS/eqapo"
echo "menu bar app: $APP_PATH/Contents/MacOS/eqapo-ui"
echo "use the eqapo menu to toggle Start eqapo at login"
