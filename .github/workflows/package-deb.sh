#!/bin/bash
# Build a .deb package from an already-compiled openlierox binary.
#
# Inputs (env):
#   BUILD_DIR - cmake build directory (default: build)
#
# Output: openlierox_<version>_<arch>.deb in the current directory.

set -euo pipefail

cd $(dirname $0)"/../.."
REPO_ROOT=$(pwd)

BUILD_DIR=./build-output
BINARY="$BUILD_DIR/bin/openlierox"

if [ ! -x "$BINARY" ]; then
    echo "ERROR: $BINARY not found — build the project first" >&2
    exit 1
fi


STAGE="$REPO_ROOT/deb-stage"
rm -rf "$STAGE"
mkdir -p \
    "$STAGE/DEBIAN" \
    "$STAGE/usr/games" \
    "$STAGE/usr/share/games" \
    "$STAGE/usr/share/doc" \
    "$STAGE/usr/share/applications" \
    "$STAGE/usr/share/pixmaps" \
    "$STAGE/usr/share/icons/hicolor/scalable/apps" \
    "$STAGE/usr/share/icons/hicolor/128x128/apps" \
    "$STAGE/usr/share/icons/hicolor/32x32/apps" \
    "$STAGE/usr/share/icons/hicolor/16x16/apps"

# install.sh looks for bin/openlierox in cwd; point it at the build output.
rm -rf "$REPO_ROOT/bin"
mkdir -p "$REPO_ROOT/bin"
cp "$BINARY" "$REPO_ROOT/bin/openlierox"

PREFIX="$STAGE" \
BIN_DIR=usr/games \
SYSTEM_DATA_DIR=usr/share/games \
DOC_DIR=usr/share/doc \
    ./install.sh

install -m 644 share/OpenLieroX.xpm     "$STAGE/usr/share/pixmaps/"
install -m 644 share/OpenLieroX.svg     "$STAGE/usr/share/icons/hicolor/scalable/apps/"
install -m 644 share/OpenLieroX.128.png "$STAGE/usr/share/icons/hicolor/128x128/apps/"
install -m 644 share/OpenLieroX.32.png  "$STAGE/usr/share/icons/hicolor/32x32/apps/"
install -m 644 share/OpenLieroX.16.png  "$STAGE/usr/share/icons/hicolor/16x16/apps/"
install -m 644 debian/openlierox.desktop "$STAGE/usr/share/applications/"

# Auto-compute runtime deps from the linked ELF. dpkg-shlibdeps reads
# debian/control (which exists at the repo root) to learn the package name.
DEPS="$(dpkg-shlibdeps -O --ignore-missing-info "$BINARY" \
        | sed 's/^shlibs:Depends=//')"
if [ -z "$DEPS" ]; then
    echo "ERROR: dpkg-shlibdeps produced no Depends line" >&2
    exit 1
fi

# dpkg version strings can't contain underscores; use tilde so "beta"
# sorts before the final release.
VERSION="$(tr '_' '~' < VERSION)"
ARCH="$(dpkg --print-architecture)"
INSTALLED_SIZE="$(du -sk "$STAGE" | cut -f1)"

cat > "$STAGE/DEBIAN/control" <<EOF
Package: openlierox
Version: $VERSION
Section: games
Priority: optional
Architecture: $ARCH
Installed-Size: $INSTALLED_SIZE
Maintainer: OpenLieroX CI <noreply@openlierox.net>
Depends: $DEPS
Homepage: https://openlierox.net
Description: a real-time excessive Worms-clone with network support
 Remake of the original Liero game with network support,
 better graphics and many maps and mods.
EOF

DEB="openlierox_${VERSION}_${ARCH}.deb"
dpkg-deb --build --root-owner-group "$STAGE" "$REPO_ROOT/$DEB"
echo ">>> built $DEB"
