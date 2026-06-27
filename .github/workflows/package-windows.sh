#!/bin/bash
# Build a standalone Windows bundle folder containing openlierox.exe,
# its MinGW runtime DLLs, and the game data from share/gamedir/.
#
# Inputs (env):
#   BUILD_DIR - cmake build directory (default: build-cmake)
#
# Output: distrib/openlierox-<version>-windows/ folder.

set -euo pipefail

cd $(dirname $0)"/../.."

BUILD_DIR="${BUILD_DIR:-build-cmake}"
BINARY="$BUILD_DIR/bin/openlierox.exe"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found — build the project first" >&2
    exit 1
fi

REPO_ROOT="$(pwd)"

PACKAGE_NAME="openlierox-$(./get_version.sh)-windows"
PACKAGE_DIR="$REPO_ROOT/distrib/$PACKAGE_NAME"
rm -rf "$PACKAGE_DIR"
mkdir -p "$PACKAGE_DIR"

cp "$BINARY" "$PACKAGE_DIR/"

# Game data — everything OLX searches for at ./ at runtime.
mkdir -p "$PACKAGE_DIR/gamedir"
cp -r share/gamedir/. "$PACKAGE_DIR/gamedir"

# Pull in every MinGW runtime DLL the binary depends on, recursively.
# ntldd prints lines like "    libfoo.dll => C:\msys64\mingw64\bin\libfoo.dll (0x...)".
ntldd -R "$PACKAGE_DIR/openlierox.exe" \
    | awk -F'=>' '/=>/ { print $2 }' \
    | awk '{ print $1 }' \
    | grep -i 'mingw64' \
    | while read -r dll; do
        winpath="$(cygpath -u "$dll")"
        cp -n "$winpath" "$PACKAGE_DIR/"
      done

echo ">>> built $PACKAGE_DIR"
