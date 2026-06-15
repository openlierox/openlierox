#!/usr/bin/env bash
# build-wasm.sh — build OpenLieroX as a WebAssembly module.
#
# Produces output/openlierox.html + .js + .wasm + .data, ready to serve
# from any HTTP server with COOP/COEP headers (see serve.py).
#
# Usage: ./build-wasm.sh [--clean] [--release] [--skip-emsdk] [--skip-fetch] [--skip-data]

set -euo pipefail

WASM_DIR="$(cd "$(dirname "$0")" && pwd)"
OLX_ROOT="$(cd "$WASM_DIR/../.." && pwd)"
WASM_TOOLS_DIR="${WASM_TOOLS_DIR:-$HOME/wasm-tools}"
export WASM_TOOLS_DIR

BUILD_TYPE="Debug"
DO_CLEAN=0
SKIP_EMSDK=0
SKIP_FETCH=0
SKIP_DATA=0

while [ $# -gt 0 ]; do
    case "$1" in
        --release)     BUILD_TYPE="Release" ;;
        --debug)       BUILD_TYPE="Debug" ;;
        --clean)       DO_CLEAN=1 ;;
        --skip-emsdk)  SKIP_EMSDK=1 ;;
        --skip-fetch)  SKIP_FETCH=1 ;;
        --skip-data)   SKIP_DATA=1 ;;
        -h|--help)
            sed -n '2,12p' "$0" | sed 's/^# \?//'
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
    shift
done

# ---------- emsdk ----------------------------------------------------------

if [ "$SKIP_EMSDK" -eq 0 ]; then
    "$WASM_DIR/fetch-emsdk.sh"
fi

# Source the activated env so emcc/emcmake are on PATH.
EMSDK_ENV="$WASM_TOOLS_DIR/emsdk/emsdk_env.sh"
if [ ! -f "$EMSDK_ENV" ]; then
    echo "ERROR: $EMSDK_ENV missing — run without --skip-emsdk first." >&2
    echo "       (looked under WASM_TOOLS_DIR=$WASM_TOOLS_DIR)" >&2
    exit 1
fi
# shellcheck disable=SC1090
source "$EMSDK_ENV" >/dev/null

echo "Using emcc: $(command -v emcc) ($(emcc --version | head -1))"

# Pre-build the Emscripten port sysroot for our exact flag set.
# Without this, the first parallel compile fights over cache locks
# while emcc lazily builds libSDL2.a / libpng / etc.
echo "Warming Emscripten port cache..."
PREBUILD_TMP="$(mktemp -d)"
trap 'rm -rf "$PREBUILD_TMP"' EXIT
cat > "$PREBUILD_TMP/empty.c" <<'EOF'
#include <SDL.h>
#include <SDL_image.h>
#include <png.h>
#include <jpeglib.h>
#include <ogg/ogg.h>
#include <vorbis/vorbisfile.h>
#include <zlib.h>
#include <AL/al.h>
int main(void) { return 0; }
EOF
emcc -pthread \
    -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 \
    -sUSE_LIBPNG=1 -sUSE_LIBJPEG=1 -sUSE_OGG=1 -sUSE_VORBIS=1 -sUSE_ZLIB=1 \
    -sSDL2_IMAGE_FORMATS='["bmp","gif","jpg","png","tga"]' \
    -lopenal \
    "$PREBUILD_TMP/empty.c" \
    -o "$PREBUILD_TMP/empty.js" 2>&1 | tail -3
echo "Cache warmed."

# ---------- third-party deps ----------------------------------------------

if [ "$SKIP_FETCH" -eq 0 ]; then
    "$WASM_DIR/fetch-dependencies.sh"
fi

# ---------- stage game data ------------------------------------------------

DATA_STAGE="$WASM_DIR/output/preload"
if [ "$SKIP_DATA" -eq 0 ]; then
    # CMake doesn't track files inside the preload dir as link deps,
    # so a stale .data sidecar would otherwise stick around even when
    # we add/remove staged files. Wipe the output's link artefacts to
    # force a fresh emcc link each time data changes.
    rm -f "$WASM_DIR/output/build/libgd/Bin/openlierox.html" \
          "$WASM_DIR/output/build/libgd/Bin/openlierox.js" \
          "$WASM_DIR/output/build/libgd/Bin/openlierox.wasm" \
          "$WASM_DIR/output/build/libgd/Bin/openlierox.data"
    echo "Staging slim game data into $DATA_STAGE/gamedir..."
    # Bundling all of share/gamedir would package ~8000 files (136MB).
    # Each file becomes a preload-file dependency and the runtime
    # registers them serially, which takes minutes before main() can
    # run. Stage only the bits the menu needs to boot, plus a single
    # mod (Classic) so weapon-restrictions / map listing aren't empty.
    rm -rf "$DATA_STAGE"
    mkdir -p "$DATA_STAGE/gamedir"
    GD="$OLX_ROOT/share/gamedir"
    SD="$DATA_STAGE/gamedir"
    cp -a "$GD/cfg"           "$SD/cfg"
    cp -a "$GD/data"          "$SD/data"
    cp -a "$GD/scripts"       "$SD/scripts"
    cp -a "$GD/themes"        "$SD/themes"
    cp -a "$GD/skins"         "$SD/skins"
    cp -a "$GD/games"         "$SD/games"
    cp -a "$GD/Classic"       "$SD/Classic"
    cp -a "$GD/Gusanos"       "$SD/Gusanos"
    # Touchscreen on-screen control layouts (+ button artwork). The web
    # build enables touch controls by default (like Android), so the
    # selected layout YAML and its images must be present.
    [ -d "$GD/touchscreen" ] && cp -a "$GD/touchscreen" "$SD/touchscreen"
    # The bundled Introduction game references "MW 1.0" and "promode"
    # in its later levels; stage them too so the menu's mod dropdown
    # has them and the Introduction's intro2/3 actually load.
    [ -d "$GD/MW 1.0"  ] && cp -a "$GD/MW 1.0"  "$SD/MW 1.0"
    [ -d "$GD/promode" ] && cp -a "$GD/promode" "$SD/promode"
    cp -f "$GD/GeoIP.dat"     "$SD/GeoIP.dat"
    cp -f "$GD/startup.lua"   "$SD/startup.lua"
    # SinglePlayer paths look like "levels/../games/foo/bar". POSIX
    # path normalization needs every directory component to exist
    # before it can resolve `..`, so stage at least a few sample
    # levels (and the empty `levels/` dir) for the introduction
    # game's level lookup to succeed.
    mkdir -p "$SD/levels"
    for lvl in "747.lxl" "Base Fight.lxl" "BetaBoxDE.lxl" "Castle Strike.lxl" \
               "Dirt Level.lxl"; do
        if [ -f "$GD/levels/$lvl" ]; then
            cp -f "$GD/levels/$lvl" "$SD/levels/$lvl"
        fi
    done
    # gamesettings presets at the gamedir root
    cp -f "$GD"/*.gamesettings "$SD/" 2>/dev/null || true
    echo "Staged $(find "$SD" -type f | wc -l) files ($(du -sh "$SD" | cut -f1))"
fi

# ---------- cmake configure + build ---------------------------------------

BUILD_DIR="$WASM_DIR/output/build"
if [ "$DO_CLEAN" -eq 1 ]; then
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

emcmake cmake -S "$WASM_DIR" -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DOLX_ROOT="$OLX_ROOT" \
    -DOLX_PRELOAD_DIR="$DATA_STAGE"

cmake --build "$BUILD_DIR" -j"$(nproc)"

# ---------- output -------------------------------------------------------

OUT_DIR="$WASM_DIR/output"
# libgd's CMakeLists sets a global EXECUTABLE_OUTPUT_PATH, redirecting
# our binary into libgd/Bin/. Look there too.
for ext in html js wasm data worker.js; do
    for src in "$BUILD_DIR/openlierox.$ext" \
               "$BUILD_DIR/libgd/Bin/openlierox.$ext"; do
        if [ -f "$src" ]; then
            cp -f "$src" "$OUT_DIR/"
            break
        fi
    done
done

# Make openlierox.html accessible as index.html so plain `python -m
# http.server` can serve it without a path argument.
if [ -f "$OUT_DIR/openlierox.html" ]; then
    cp -f "$OUT_DIR/openlierox.html" "$OUT_DIR/index.html"
fi

# Copy the shell's static web assets (PWA manifest + icons) next to the
# output so `serve.py` (which serves output/ directly) doesn't 404 on the
# <link rel="manifest"> / <link rel="icon"> the shell references. build.sh
# stages the same files into the redistributable bundle.
for asset in manifest.webmanifest icon-256.png icon-512.png; do
    [ -f "$WASM_DIR/shell/$asset" ] && cp -f "$WASM_DIR/shell/$asset" "$OUT_DIR/"
done

echo
echo "Wasm artefacts ready under $OUT_DIR:"
ls -lh "$OUT_DIR"/openlierox.* "$OUT_DIR/index.html" 2>/dev/null || true
echo
echo "Serve with:  python3 $WASM_DIR/serve.py"
