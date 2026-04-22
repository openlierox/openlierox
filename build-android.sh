#!/usr/bin/env bash
# build-android.sh — Build an OpenLieroX APK for Android.
#
# Architecture:
#   • The canonical Android port files live in THIS repo under
#     build/android/ (AndroidAppSettings.cfg, AndroidPreBuild.sh,
#     liero.raw, *.patch). Source code additions for Android live
#     directly in src/ / include/ as usual (guarded by #ifdef __ANDROID__).
#   • A separate "commandergenius" checkout
#     (https://github.com/pelya/commandergenius) is used as an external
#     build harness: it ships the SDL2 Android scaffolding, JNI/Java
#     glue, and the Gradle/NDK build. We overlay THIS repo onto
#     commandergenius/project/jni/application/openlierox/ on each build.
#
# On every invocation this script:
#   1. Copies build/android/* into the port's openlierox/ dir.
#   2. Rsyncs src/, include/, libs/, VERSION (plus a couple of share/*
#      Android icons) onto the port's openlierox/src/ tree.
#   3. Optionally (--sync-data) repacks AndroidData/data.zip.xz from
#      this repo's share/gamedir so the APK ships current game content.
#   4. Runs commandergenius/build.sh openlierox to produce the APK.
#   5. Copies the APK to build-output/openlierox-android.apk.
#
# Usage: ./build-android.sh [--debug] [--sync-data] [-i] [-r] [-s] [-b] [-z]
#
# Options:
#   --debug      Build debug APK (default: release, signed with debug key)
#   --sync-data  Repack AndroidData/data.zip.xz from share/gamedir
#   -i           adb install after build
#   -r           adb install and launch
#   -s           Sign APK with the port's sign.sh
#   -b           Also build/sign a release .aab app bundle
#   -z           Skip zipalign + apksigner (faster, unsigned)
#   -h           Show this help
#
# Env:
#   PORT_DIR            commandergenius checkout (default: ~/git/commandergenius)
#   ANDROID_NDK_HOME    Android NDK path (required; inferred from ndk-build)
#   ANDROID_SDK_ROOT    Android SDK path (required; ANDROID_HOME accepted)
#
# Output: build-output/openlierox-android.apk

set -euo pipefail

OLX_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT_DIR="${PORT_DIR:-$HOME/git/commandergenius}"
BUILD_TYPE="release"
SYNC_DATA=0
FORWARD_FLAGS=()

show_help() {
    sed -n '2,44p' "$0" | sed 's/^# \?//'
}

while [ $# -gt 0 ]; do
    case "$1" in
        --debug)        BUILD_TYPE="debug" ;;
        --release)      BUILD_TYPE="release" ;;
        --sync-data)    SYNC_DATA=1 ;;
        -i|-r|-s|-b|-z) FORWARD_FLAGS+=("$1") ;;
        -h|--help)      show_help; exit 0 ;;
        *)              echo "Unknown argument: $1" >&2; show_help >&2; exit 2 ;;
    esac
    shift
done

# --- prerequisites --------------------------------------------------------

if [ -z "${ANDROID_NDK_HOME:-}" ]; then
    if command -v ndk-build >/dev/null 2>&1; then
        export ANDROID_NDK_HOME="$(dirname "$(readlink -f "$(command -v ndk-build)")")"
    else
        echo "ERROR: ANDROID_NDK_HOME is not set and ndk-build is not in PATH." >&2
        exit 1
    fi
fi

if [ -z "${ANDROID_SDK_ROOT:-}" ]; then
    export ANDROID_SDK_ROOT="${ANDROID_HOME:-}"
fi
if [ -z "${ANDROID_SDK_ROOT:-}" ] && [ -d "$HOME/android-sdk" ]; then
    export ANDROID_SDK_ROOT="$HOME/android-sdk"
fi
if [ -z "${ANDROID_SDK_ROOT:-}" ]; then
    echo "ERROR: ANDROID_SDK_ROOT (or ANDROID_HOME) is not set." >&2
    exit 1
fi
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export PATH="$ANDROID_SDK_ROOT/platform-tools:$ANDROID_SDK_ROOT/build-tools/$(ls -1 "$ANDROID_SDK_ROOT/build-tools" 2>/dev/null | sort -V | tail -n1):$PATH"

if [ ! -d "$PORT_DIR/project/jni/application/openlierox" ]; then
    echo "ERROR: commandergenius port not found at $PORT_DIR." >&2
    echo "       Clone https://github.com/pelya/commandergenius and set PORT_DIR." >&2
    exit 1
fi

for tool in java zipalign apksigner; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "WARN: '$tool' not in PATH — gradle or signing may fail." >&2
    fi
done

if printf '%s\n' "${FORWARD_FLAGS[@]:-}" | grep -qE '^-[ir]$'; then
    if ! command -v adb >/dev/null 2>&1; then
        echo "ERROR: -i/-r requested but adb is not in PATH." >&2
        exit 1
    fi
fi

cat <<EOF
>>> OLX_DIR            = $OLX_DIR
>>> PORT_DIR           = $PORT_DIR
>>> ANDROID_NDK_HOME   = $ANDROID_NDK_HOME
>>> ANDROID_SDK_ROOT   = $ANDROID_SDK_ROOT
>>> BUILD_TYPE         = $BUILD_TYPE
>>> SYNC_DATA          = $SYNC_DATA
>>> FORWARD_FLAGS      = ${FORWARD_FLAGS[*]:-<none>}
EOF

# --- 1. Overlay port metadata from build/android/ -------------------------

PORT_APP="$PORT_DIR/project/jni/application/openlierox"
ANDROID_META="$OLX_DIR/build/android"

if [ ! -d "$ANDROID_META" ]; then
    echo "ERROR: $ANDROID_META missing. This script relies on it for" >&2
    echo "       AndroidAppSettings.cfg, AndroidPreBuild.sh, liero.raw, patches." >&2
    exit 1
fi

echo ">>> syncing build/android metadata into port"
for f in AndroidAppSettings.cfg AndroidPreBuild.sh liero.raw java.patch project.patch; do
    if [ -f "$ANDROID_META/$f" ]; then
        cp -f "$ANDROID_META/$f" "$PORT_APP/$f"
    fi
done
chmod +x "$PORT_APP/AndroidPreBuild.sh" 2>/dev/null || true

# --- 2. Overlay 0.59 source onto port's openlierox/src/ -------------------

PORT_SRC="$PORT_APP/src"
echo ">>> syncing 0.59 source into port src tree"
mkdir -p "$PORT_SRC"/{src,include,libs,share}
rsync -a --delete \
    --exclude='.git' \
    --exclude='build/' \
    --exclude='build-output/' \
    --exclude='CMakeCache.txt' \
    --exclude='CMakeFiles/' \
    --exclude='cmake_install.cmake' \
    --exclude='Makefile' \
    --exclude='bin/' \
    --exclude='obj/' \
    --exclude='.idea/' \
    "$OLX_DIR/src/"     "$PORT_SRC/src/"
rsync -a --delete "$OLX_DIR/include/" "$PORT_SRC/include/"
rsync -a --delete "$OLX_DIR/libs/"    "$PORT_SRC/libs/"
cp -f "$OLX_DIR/VERSION" "$PORT_SRC/VERSION"
[ -f "$OLX_DIR/CMakeLists.txt" ]       && cp -f "$OLX_DIR/CMakeLists.txt"       "$PORT_SRC/CMakeLists.txt"
[ -f "$OLX_DIR/CMakeOlxCommon.cmake" ] && cp -f "$OLX_DIR/CMakeOlxCommon.cmake" "$PORT_SRC/CMakeOlxCommon.cmake"

# Android-specific static assets that live in share/. These end up
# referenced by the port's banner.png / icon.png symlinks (into src/share).
for f in android-icon.png tv-banner.png; do
    [ -f "$OLX_DIR/share/$f" ] && cp -f "$OLX_DIR/share/$f" "$PORT_SRC/share/$f"
done
# share/gamedir is handled separately via --sync-data below (big; slow).
# The port needs *some* gamedir dir for AndroidPreBuild.sh / symlink targets,
# so if it doesn't already exist, drop an empty stub.
mkdir -p "$PORT_SRC/share/gamedir"

# --- 3. Optional: repack AndroidData/data.zip.xz from 0.59 gamedir --------

if [ "$SYNC_DATA" = 1 ]; then
    SRC_GAMEDIR="$OLX_DIR/share/gamedir"
    if [ ! -d "$SRC_GAMEDIR" ]; then
        echo "ERROR: --sync-data: $SRC_GAMEDIR not found" >&2
        exit 1
    fi
    echo ">>> --sync-data: repacking AndroidData/data.zip.xz from $SRC_GAMEDIR"
    mkdir -p "$PORT_APP/AndroidData"
    TMP_ZIP="$(mktemp -t olx-gamedir.XXXXXX.zip)"
    trap 'rm -f "$TMP_ZIP"' EXIT
    (cd "$SRC_GAMEDIR" && zip -0 -rq "$TMP_ZIP" .)
    xz -9 --threads=0 < "$TMP_ZIP" > "$PORT_APP/AndroidData/data.zip.xz.new"
    mv "$PORT_APP/AndroidData/data.zip.xz.new" "$PORT_APP/AndroidData/data.zip.xz"
    echo ">>> data.zip.xz: $(stat -c %s "$PORT_APP/AndroidData/data.zip.xz") bytes"
fi

# --- 4a. Patch port's wavpack Android.mk for arm64-v8a --------------------
#
# The port's jni/sdl2_mixer/external/wavpack/Android.mk only handles
# armeabi, armeabi-v7a, x86, x86_64 — when building arm64-v8a, PLATFORM_SRC
# from the previous armeabi-v7a iteration persists (it's set with := rather
# than explicit per-arch reset) and the build tries to assemble the ARM32
# unpack_armv7.S with aarch64, which fails on the @ comment syntax.
# Guarantee PLATFORM_SRC is empty on arm64-v8a.

WAVPACK_MK="$PORT_DIR/project/jni/sdl2_mixer/external/wavpack/Android.mk"
if [ -f "$WAVPACK_MK" ] && ! grep -q 'TARGET_ARCH_ABI),arm64-v8a' "$WAVPACK_MK"; then
    echo ">>> patching $WAVPACK_MK for arm64-v8a"
    sed -i '/^ifeq ($(TARGET_ARCH_ABI),x86_64)/i\ifeq ($(TARGET_ARCH_ABI),arm64-v8a)\nPLATFORM_CFLAGS :=\nPLATFORM_SRC :=\nendif\n' \
        "$WAVPACK_MK"
fi

# --- 4. Add INTERNET permission to the SDL2 manifest ----------------------
#
# With LibSdlVersion=2 the port uses its SDL2 Android template manifest
# (project/app/src/main/AndroidManifest.xml) which is regenerated by
# changeAppSettings.sh and does NOT honour AndroidAppSettings.cfg's
# AccessInternet. OpenLieroX calls socket() during startup via hawknl, so
# without INTERNET the app aborts at "Failed to open a socket for
# networking". Inject the permission if missing.

SDL2_MANIFEST="$PORT_DIR/project/app/src/main/AndroidManifest.xml"
if [ -f "$SDL2_MANIFEST" ] && ! grep -q 'android.permission.INTERNET' "$SDL2_MANIFEST"; then
    echo ">>> adding android.permission.INTERNET to $SDL2_MANIFEST"
    sed -i '/<uses-permission android:name="android.permission.VIBRATE"/a\    <uses-permission android:name="android.permission.INTERNET" />\n    <uses-permission android:name="android.permission.ACCESS_NETWORK_STATE" />' \
        "$SDL2_MANIFEST"
fi

# --- 5. Drop stale application object files ------------------------------
#
# rsync -a preserves mtimes, so a file refreshed from this repo often has an
# older mtime than the .o that ndk-build produced on a previous run against
# the 0.58 bundled source. ndk-build then treats the .o as up-to-date and
# links against stale code. Wipe just the application subtree so the port
# libs (SDL2, boost, etc.) remain cached.

rm -rf "$PORT_DIR/project/obj/local"/*/objs/application/openlierox 2>/dev/null || true
rm -f  "$PORT_DIR/project/obj/local"/*/libapplication.so 2>/dev/null || true

# --- 6. Build via commandergenius/build.sh --------------------------------

cd "$PORT_DIR"
./build.sh "${FORWARD_FLAGS[@]:+${FORWARD_FLAGS[@]}}" "$BUILD_TYPE" openlierox

# --- 5. Copy result back --------------------------------------------------

RELEASE_APK="$PORT_DIR/project/app/build/outputs/apk/release/app-release.apk"
DEBUG_APK="$PORT_DIR/project/app/build/outputs/apk/debug/app-debug.apk"

if [ -f "$RELEASE_APK" ]; then
    SRC_APK="$RELEASE_APK"
elif [ -f "$DEBUG_APK" ]; then
    SRC_APK="$DEBUG_APK"
else
    echo "ERROR: no APK found under $PORT_DIR/project/app/build/outputs/apk" >&2
    exit 1
fi

mkdir -p "$OLX_DIR/build-output"
OUT_APK="$OLX_DIR/build-output/openlierox-android.apk"
cp -f "$SRC_APK" "$OUT_APK"

echo ">>> built $OUT_APK"
echo ">>> source APK: $SRC_APK"
