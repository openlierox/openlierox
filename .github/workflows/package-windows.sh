#!/bin/bash
# Build a standalone Windows zip containing the OpenLieroX.exe
# binary, the vcpkg DLL dependencies it needs at runtime, and the
# game data from share/gamedir/.
#
# Inputs (env):
#   BUILD_DIR - cmake build directory (default: build)
#
# Output: openlierox_<version>_windows.zip in the repo root.
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"

cd "$(dirname "$0")/../.."
REPO_ROOT="$(pwd)"

# The cmake target writes to bin/OpenLieroX.exe (see CMakeLists.txt).
BINARY="$REPO_ROOT/bin/OpenLieroX.exe"
if [ ! -f "$BINARY" ]; then
    echo "ERROR: $BINARY not found - build the project first" >&2
    exit 1
fi

PACKAGE_NAME="openlierox-$(./get_version.sh)-windows"
PACKAGE_DIR="$REPO_ROOT/win-stage/$PACKAGE_NAME"

rm -rf "$REPO_ROOT/win-stage"
mkdir -p "$PACKAGE_DIR"

cp "$BINARY" "$PACKAGE_DIR/OpenLieroX.exe"
cp -av share/gamedir "$PACKAGE_DIR/gamedir"

mkdir -p "$PACKAGE_DIR/doc"
cp -R doc/. "$PACKAGE_DIR/doc/"
cp README.md COPYING.LIB "$PACKAGE_DIR/doc/" 2>/dev/null || true

# Pull in every vcpkg DLL the binary depends on. vcpkg's applocal
# deployment is invoked automatically by cmake when the toolchain file
# is used, so the DLLs end up next to the .exe in the build tree.
shopt -s nullglob
for dll in "$REPO_ROOT"/bin/*.dll; do
    cp "$dll" "$PACKAGE_DIR/"
done
shopt -u nullglob

# Mac uses ~ to keep beta sort order; mirror that for consistency.
VERSION="$(tr '_' '~' < VERSION)"
ZIP="openlierox_${VERSION}_windows.zip"
rm -f "$REPO_ROOT/$ZIP"
(cd "$REPO_ROOT/win-stage" && powershell -NoProfile -Command \
    "Compress-Archive -Path '$PACKAGE_NAME' -DestinationPath '$REPO_ROOT/$ZIP' -Force")
echo ">>> built $ZIP"
