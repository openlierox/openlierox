#!/bin/bash
# Build a standalone Windows .zip bundle containing openlierox.exe,
# its MinGW runtime DLLs, and the game data from share/gamedir/.
#
# Inputs (env):
#   BUILD_DIR - cmake build directory (default: build)
#
# Output: openlierox_<version>_win64.zip in the current directory.

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
BINARY="$BUILD_DIR/bin/openlierox.exe"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found — build the project first" >&2
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

STAGE="$REPO_ROOT/win-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE"

cp "$BINARY" "$STAGE/"

# Game data — everything OLX searches for at ./ at runtime.
cp -r share/gamedir/. "$STAGE/"

# Docs and licence
mkdir -p "$STAGE/doc"
cp -r doc/. "$STAGE/doc/"
cp README.md COPYING.LIB "$STAGE/doc/" 2>/dev/null || true

# Pull in every MinGW runtime DLL the binary depends on, recursively.
# ntldd prints lines like "    libfoo.dll => C:\msys64\mingw64\bin\libfoo.dll (0x...)".
ntldd -R "$STAGE/openlierox.exe" \
    | awk -F'=>' '/=>/ { print $2 }' \
    | awk '{ print $1 }' \
    | grep -i 'mingw64' \
    | while read -r dll; do
        winpath="$(cygpath -u "$dll")"
        cp -n "$winpath" "$STAGE/"
      done

VERSION="$(tr '_' '~' < VERSION)"
ZIP="openlierox_${VERSION}_win64.zip"
rm -f "$REPO_ROOT/$ZIP"
(cd "$STAGE" && zip -qr "$REPO_ROOT/$ZIP" .)
echo ">>> built $ZIP"
