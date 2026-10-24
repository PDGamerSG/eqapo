#!/bin/sh
# Build a drag-installable eqapo.dmg containing the app and an Applications alias.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
OUT="${1:-$ROOT/build/eqapo.dmg}"
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/eqapo-dmg.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT INT TERM

"$ROOT/gui/make_app.sh" "$ROOT/build/eqapo"
ditto --rsrc --extattr "$ROOT/gui/build/eqapo.app" "$STAGE/eqapo.app"
ln -s /Applications "$STAGE/Applications"
mkdir -p "$(dirname "$OUT")"
hdiutil create -volname eqapo -srcfolder "$STAGE" -ov -format UDZO "$OUT" >/dev/null
echo "$OUT"
