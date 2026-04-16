#!/bin/bash
# Build a standalone macOS .app bundle containing the openlierox
# binary, its Homebrew dylib dependencies (rewritten to @executable_path
# via dylibbundler) and the game data from share/gamedir/.
#
# Inputs (env):
#   BUILD_DIR - cmake build directory (default: build)
#
# Output: openlierox_<version>_macos.zip in the current directory,
#         containing OpenLieroX.app/.
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
BINARY="$BUILD_DIR/bin/openlierox"

if [ ! -x "$BINARY" ]; then
    echo "ERROR: $BINARY not found — build the project first" >&2
    exit 1
fi

REPO_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_ROOT"

APP="$REPO_ROOT/mac-stage/OpenLieroX.app"
rm -rf "$REPO_ROOT/mac-stage"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources" "$APP/Contents/Frameworks"

cp "$BINARY" "$APP/Contents/MacOS/openlierox"
chmod +x "$APP/Contents/MacOS/openlierox"

# Game data — everything OLX searches for at ./ at runtime.
cp -R share/gamedir/. "$APP/Contents/Resources/gamedir/"

# Docs and licence
mkdir -p "$APP/Contents/Resources/doc"
cp -R doc/. "$APP/Contents/Resources/doc/"
cp README.md COPYING.LIB "$APP/Contents/Resources/doc/" 2>/dev/null || true

# Icon
if [ -f share/macosx.icns ]; then
    cp share/macosx.icns "$APP/Contents/Resources/macosx.icns"
fi

# Info.plist
VERSION_RAW="$(cat VERSION)"
cat > "$APP/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>English</string>
    <key>CFBundleExecutable</key>
    <string>openlierox</string>
    <key>CFBundleIconFile</key>
    <string>macosx.icns</string>
    <key>CFBundleIdentifier</key>
    <string>net.openlierox.OpenLieroX</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>OpenLieroX</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VERSION_RAW}</string>
    <key>CFBundleVersion</key>
    <string>${VERSION_RAW}</string>
    <key>LSApplicationCategoryType</key>
    <string>public.app-category.action-games</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.13</string>
    <key>NSHighResolutionCapable</key>
    <true/>
    <key>NSPrincipalClass</key>
    <string>NSApplication</string>
</dict>
</plist>
EOF

# Pull in every Homebrew dylib the binary depends on, rewrite the load
# paths to @executable_path/../Frameworks, and copy them into the bundle.
# dylibbundler handles the transitive closure.
if ! command -v dylibbundler >/dev/null 2>&1; then
    echo "ERROR: dylibbundler not found — install with 'brew install dylibbundler'" >&2
    exit 1
fi
dylibbundler -od -b \
    -x "$APP/Contents/MacOS/openlierox" \
    -d "$APP/Contents/Frameworks" \
    -p "@executable_path/../Frameworks/"

# Zip uses tilde instead of underscore so e.g. "beta9" sorts correctly.
VERSION="$(tr '_' '~' < VERSION)"
ZIP="openlierox_${VERSION}_macos.zip"
rm -f "$REPO_ROOT/$ZIP"
(cd "$REPO_ROOT/mac-stage" && zip -qry "$REPO_ROOT/$ZIP" OpenLieroX.app)
echo ">>> built $ZIP"
