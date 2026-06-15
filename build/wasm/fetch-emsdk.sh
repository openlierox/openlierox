#!/usr/bin/env bash
# fetch-emsdk.sh — install Emscripten SDK under $WASM_TOOLS_DIR/emsdk.
#
# The SDK is a generic toolchain (not OLX-specific), so it lives
# outside the repo to keep the working tree small and to share a
# single ~1.6 GB install across projects.
#
# Default location: $HOME/wasm-tools/emsdk
# Override with:   WASM_TOOLS_DIR=/some/path ./fetch-emsdk.sh
#
# build-wasm.sh sources emsdk_env.sh from this checkout to bring
# emcc/emcmake/emmake into PATH. Pinned to a known-working release so
# the Wasm build is reproducible. Re-running is idempotent.

set -euo pipefail

WASM_TOOLS_DIR="${WASM_TOOLS_DIR:-$HOME/wasm-tools}"
EMSDK_DIR="$WASM_TOOLS_DIR/emsdk"
EMSDK_VERSION="3.1.74"

mkdir -p "$WASM_TOOLS_DIR"

if [ ! -d "$EMSDK_DIR/.git" ]; then
    echo "Cloning emsdk into $EMSDK_DIR..."
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi

cd "$EMSDK_DIR"
"$EMSDK_DIR/emsdk" install "$EMSDK_VERSION"
"$EMSDK_DIR/emsdk" activate "$EMSDK_VERSION"

echo "Emscripten $EMSDK_VERSION ready under $EMSDK_DIR"
