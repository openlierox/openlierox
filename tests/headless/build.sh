#!/bin/bash
# Build the openlierox binary for the headless test suite.
#
# Linux: reuses the CI build script .github/workflows/build-linux.sh;
#        the apt dev packages from README.md must already be installed.
# macOS: installs the Homebrew build deps (idempotent) and builds,
#        mirroring .github/workflows/package-mac.yml.
#
# Output: build-output/bin/openlierox
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"

require_cmake3() {
    # The repo's CMake files use policies removed in CMake 4.x.
    local major
    major="$(cmake --version | head -1 | sed -E 's/[^0-9]*([0-9]+).*/\1/')"
    if [ "$major" -ge 4 ]; then
        echo "ERROR: CMake ${major}.x found, but this project needs CMake 3.x." >&2
        echo "       Install a 3.x release (see package-mac.yml) and put it first on PATH." >&2
        exit 1
    fi
}

case "$(uname)" in
    Darwin)
        echo ">>> Installing Homebrew build dependencies (idempotent) ..."
        brew install \
            ninja pkg-config sdl2 sdl2_image sdl2_mixer libzip gd \
            libvorbis freealut openal-soft boost yaml-cpp
        require_cmake3
        # libxml2 and openal-soft are keg-only; expose their .pc files.
        PKG_CONFIG_PATH="$(brew --prefix libxml2)/lib/pkgconfig:$(brew --prefix openal-soft)/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
        export PKG_CONFIG_PATH
        cmake -S . -B build-output -G Ninja \
            -DCMAKE_BUILD_TYPE=Release \
            -DHAWKNL_BUILTIN=Yes -DLIBZIP_BUILTIN=No -DLIBLUA_BUILTIN=Yes \
            -DBREAKPAD=No -DHASBFD=No -DDEBUG=No
        mkdir -p build-output/bin
        cmake --build build-output -j"$(sysctl -n hw.ncpu)"
        ;;
    *)
        require_cmake3
        ./.github/workflows/build-linux.sh
        ;;
esac

echo ">>> Built $ROOT/build-output/bin/openlierox"
