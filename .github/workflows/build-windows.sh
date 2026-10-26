#!/bin/bash
# Configure and build openlierox.exe for Windows (MSYS2 UCRT64).
#
# Inputs (env):
#   BUILD_DIR - cmake build directory (default: build-cmake)
#
# Output: $BUILD_DIR/bin/openlierox.exe

set -euo pipefail

cd $(dirname $0)"/../.."

# Ensure UCRT64 toolchain is on PATH. In GitHub Actions the `msys2 {0}`
# shell already does this; when invoked from a plain bash (e.g. local runs
# where /ucrt64 is not auto-mapped) we need to add it explicitly.
# Skip this in CI: the runner image has its own MSYS2 in C:\msys64, separate
# from the one setup-msys2 installs, and its git/sh must not shadow ours.
if ! command -v cmake >/dev/null 2>&1; then
    export PATH="/c/msys64/ucrt64/bin:/c/msys64/usr/bin:$PATH"
fi

BUILD_DIR="${BUILD_DIR:-build-cmake}"

cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    -DCMAKE_BUILD_TYPE=Release \
    -DHAWKNL_BUILTIN=Yes \
    -DLIBZIP_BUILTIN=No \
    -DLIBLUA_BUILTIN=Yes \
    -DBREAKPAD=No \
    -DHASBFD=No \
    -DDEBUG=No

mkdir -p "$BUILD_DIR/bin"
cmake --build "$BUILD_DIR" -j$(nproc)
