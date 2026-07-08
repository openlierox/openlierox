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

cd $(dirname $0)"/../.."

BUILD_DIR="${BUILD_DIR:-build}"
BINARY="$BUILD_DIR/bin/openlierox"

if [ ! -x "$BINARY" ]; then
    echo "ERROR: $BINARY not found — build the project first" >&2
    exit 1
fi

REPO_ROOT="$(pwd)"

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
VERSION_RAW="$(./get_version.sh)"
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

# Homebrew's "sdl2" is really sdl2-compat:
# an SDL2-ABI shim that dlopens the real SDL3 at runtime.
# Because that load goes through dlopen and not a link-time reference,
# dylibbundler never sees SDL3 and leaves it out,
# so the app dies at startup with "Failed loading SDL3 library."
# Copy SDL3 in by hand, next to the bundled libSDL2,
# under the name sdl2-compat looks for first (@loader_path/libSDL3.dylib).
# SDL3 itself depends only on system frameworks,
# so nothing else needs bundling.
SDL3_SRC="$(brew --prefix sdl3 2>/dev/null || true)/lib/libSDL3.dylib"
if [ ! -f "$SDL3_SRC" ]; then
    SDL3_SRC="$(brew --prefix)/lib/libSDL3.dylib"
fi
if [ ! -f "$SDL3_SRC" ]; then
    echo "ERROR: libSDL3.dylib not found -- sdl2-compat needs it at runtime" >&2
    exit 1
fi
cp -L "$SDL3_SRC" "$APP/Contents/Frameworks/libSDL3.dylib"
chmod u+w "$APP/Contents/Frameworks/libSDL3.dylib"
install_name_tool -id "@executable_path/../Frameworks/libSDL3.dylib" \
    "$APP/Contents/Frameworks/libSDL3.dylib"
codesign --force --sign - "$APP/Contents/Frameworks/libSDL3.dylib"

# The bundle must be self-contained:
# a load path still pointing into /opt/homebrew or /usr/local
# means a dependency was missed
# and the app would fail on a machine without that library
# (exactly the SDL3 bug above, which shipped silently).
# Fail the build here instead.
leaked=0
for f in "$APP/Contents/MacOS/openlierox" "$APP/Contents/Frameworks/"*.dylib; do
    refs="$(otool -L "$f" | tail -n +2 | grep -E '/opt/homebrew|/usr/local/' || true)"
    if [ -n "$refs" ]; then
        echo "ERROR: $f still references non-bundled libraries:" >&2
        echo "$refs" >&2
        leaked=1
    fi
done
if [ "$leaked" -ne 0 ]; then
    exit 1
fi

# Seal the whole bundle with a fresh ad-hoc signature, last of all.
# dylibbundler signs the binary and its bundled libs and seals the bundle,
# but we add libSDL3.dylib afterwards,
# which invalidates that seal ("a sealed resource is missing or invalid"),
# so a downloaded (quarantined) app reads as "damaged and can't be opened" to Gatekeeper.
# Re-signing here, after everything is in place, makes the seal cover it all.
codesign --force --deep --sign - "$APP"

# Sanity check the finished bundle: the signature must be valid and every
# sealed resource must match its recorded hash. This is exactly what was
# broken in #1037 (libSDL3 added after signing), so verify it now and fail
# the build rather than ship a "damaged" app.
if ! codesign --verify --deep --strict "$APP"; then
    echo "ERROR: bundle code signature verification failed" >&2
    exit 1
fi

# Zip uses tilde instead of underscore so e.g. "beta9" sorts correctly.
VERSION="$(./get_version.sh | tr '_' '~')"
ZIP="openlierox_${VERSION}_macos.zip"
rm -f "$REPO_ROOT/$ZIP"
(cd "$REPO_ROOT/mac-stage" && zip -qry "$REPO_ROOT/$ZIP" OpenLieroX.app)
echo ">>> built $ZIP"
