#!/bin/bash
# Render an OpenLieroX menu offscreen (no real display) and dump it to a PNG.
#
# The whole menu is drawn into a software surface before it reaches the GPU.
# So a non-dedicated run under the dummy video driver renders the real menu offscreen,
# which lets its layout be inspected in CI or over SSH.
# (The dedicated headless suite, run.sh, has no video at all.)
#
# Usage:
#   ./tests/headless/render_offscreen.sh <out.png> [start_menu] [width] [timeout_s]
#
#   out.png     where to copy the rendered frame (relative to $PWD or absolute)
#   start_menu  which menu to open: "main" (default) or "options-system"
#   width       force the screen width (default: let the dummy driver pick 640)
#   timeout_s   how long to let the menu settle before capturing (default 6)
#
# The cursor is parked in the top-left corner so it never covers the widgets.
set -euo pipefail

OUT="${1:?usage: render_offscreen.sh <out.png> [start_menu] [width] [timeout_s]}"
START_MENU="${2:-main}"
WIDTH="${3:-}"
TIMEOUT="${4:-6}"

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BIN="${OLX_BINARY:-$ROOT/build-output/bin/openlierox}"
# The game finds its data (data/, cfg/, ...) relative to the working directory;
# share/gamedir is the read-only data root, same as the headless harness uses.
GAMEDIR="$ROOT/share/gamedir"
if [ ! -x "$BIN" ]; then
    echo ">>> openlierox not built yet; building ..."
    "$ROOT/tests/headless/build.sh"
fi

# Isolated HOME, so the run can't touch the user's real config.
# It also fixes where OLX writes:
# $HOME/Library/Application Support/OpenLieroX (mac), or $HOME/.OpenLieroX (Linux).
# Both are gitignored under $ROOT.
RENDER_HOME="$ROOT/.render-home"
OUT_DIR="$ROOT/.render-out"
rm -rf "$RENDER_HOME"
mkdir -p "$RENDER_HOME" "$OUT_DIR"

# OLX writes to its first search path, the home config dir.
# Under the isolated HOME that lands somewhere below $RENDER_HOME,
# so the dump can't dirty the repo.
DUMP_NAME="render_frame.png"

# Absolute output path (the game runs from a different cwd, see below).
ABS_OUT="$OUT"
case "$OUT" in
    /*) : ;;
    *)  ABS_OUT="$PWD/$OUT" ;;
esac

echo ">>> rendering '$START_MENU' menu offscreen (timeout ${TIMEOUT}s) ..."
set +e
( cd "$GAMEDIR" && exec env \
    HOME="$RENDER_HOME" \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy \
    OLX_START_MENU="$START_MENU" \
    OLX_DUMP_FRAME="$DUMP_NAME" \
    OLX_FORCE_MOUSE_X=2 OLX_FORCE_MOUSE_Y=2 \
    ${WIDTH:+OLX_FORCE_SCREEN_WIDTH=$WIDTH} \
    "$BIN" ) > "$OUT_DIR/render.log" 2>&1 &
PID=$!
sleep "$TIMEOUT"
kill "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
set -e

DUMP_PATH="$(find "$RENDER_HOME" -name "$DUMP_NAME" -print -quit 2>/dev/null)"
if [ -z "$DUMP_PATH" ] || [ ! -f "$DUMP_PATH" ]; then
    echo "ERROR: no frame was dumped (searched under $RENDER_HOME)." >&2
    echo "       See $OUT_DIR/render.log for what the run did." >&2
    exit 1
fi

mkdir -p "$(dirname "$ABS_OUT")"
cp "$DUMP_PATH" "$ABS_OUT"
echo ">>> wrote $ABS_OUT"
