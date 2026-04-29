#!/usr/bin/env bash
# build-android.sh — Build the OpenLieroX APK.
#
# This script lives in build/android/ and drives the entire Android
# build:
#
#   1. Ensures third-party native deps are cloned under build/android/deps
#      (SDL2, SDL2_image, SDL2_mixer, libxml2, libgd, libpng, libjpeg-turbo,
#      libogg, libvorbis, openal-soft, freealut, libcurl).
#   2. Stages OpenLieroX game data into build/android/output/assets so the
#      APK bundles share/gamedir.
#   3. Runs the gradle wrapper to produce the debug APK.
#
# Every generated artefact lands under build/android/output/:
#     output/assets/         — staged game data (APK input)
#     output/build/          — Gradle build dir (was app/build/)
#     output/cxx/            — CMake build dir (was app/.cxx/)
#     output/gradle-cache/   — Gradle project cache (was .gradle/)
#     output/openlierox-*.apk — final APK
#
# Usage: ./build/android/build-android.sh [--release] [--clean] [--install] [--run]
#
# Options:
#   --release       Build release APK instead of debug.
#   --clean         Run gradle clean first.
#   --install       adb install the resulting APK.
#   --run           Implies --install; launch the activity after install.
#   --skip-data     Don't restage game data into assets/.
#   --skip-fetch    Don't run deps/fetch.sh (assume deps already present).

set -euo pipefail

ANDROID_DIR="$(cd "$(dirname "$0")" && pwd)"
OLX_ROOT="$(cd "$ANDROID_DIR/../.." && pwd)"

BUILD_TYPE="debug"
DO_CLEAN=0
DO_INSTALL=0
DO_RUN=0
SKIP_DATA=0
SKIP_FETCH=0

while [ $# -gt 0 ]; do
    case "$1" in
        --release)    BUILD_TYPE="release" ;;
        --debug)      BUILD_TYPE="debug" ;;
        --clean)      DO_CLEAN=1 ;;
        --install|-i) DO_INSTALL=1 ;;
        --run|-r)     DO_INSTALL=1; DO_RUN=1 ;;
        --skip-data)  SKIP_DATA=1 ;;
        --skip-fetch) SKIP_FETCH=1 ;;
        -h|--help)
            sed -n '2,30p' "$0" | sed 's/^# \?//'
            exit 0 ;;
        *) echo "Unknown argument: $1" >&2; exit 2 ;;
    esac
    shift
done

# ---------- environment ----------------------------------------------------

if [ -z "${ANDROID_SDK_ROOT:-}" ]; then
    if [ -n "${ANDROID_HOME:-}" ]; then
        export ANDROID_SDK_ROOT="$ANDROID_HOME"
    elif [ -d "$HOME/android-sdk" ]; then
        export ANDROID_SDK_ROOT="$HOME/android-sdk"
    elif [ -d "$HOME/Android/Sdk" ]; then
        export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
    else
        echo "ERROR: ANDROID_SDK_ROOT (or ANDROID_HOME) is not set and no" >&2
        echo "       SDK was found at \$HOME/android-sdk or \$HOME/Android/Sdk." >&2
        exit 1
    fi
fi
export ANDROID_HOME="$ANDROID_SDK_ROOT"

if [ -z "${ANDROID_NDK_HOME:-}" ] && [ -d "$ANDROID_SDK_ROOT/ndk" ]; then
    NDK_VER="$(ls -1 "$ANDROID_SDK_ROOT/ndk" | sort -V | tail -1)"
    export ANDROID_NDK_HOME="$ANDROID_SDK_ROOT/ndk/$NDK_VER"
fi

echo "ANDROID_SDK_ROOT=$ANDROID_SDK_ROOT"
echo "ANDROID_NDK_HOME=${ANDROID_NDK_HOME:-(unset — gradle will pick one)}"

# ---------- third-party deps ----------------------------------------------

if [ "$SKIP_FETCH" -eq 0 ]; then
    "$ANDROID_DIR/deps/fetch.sh"
fi

# ---------- stage game data ------------------------------------------------

if [ "$SKIP_DATA" -eq 0 ]; then
    echo "Staging game data from share/gamedir into build/android/output/assets..."
    rm -rf "$ANDROID_DIR/output/assets/gamedir"
    mkdir -p "$ANDROID_DIR/output/assets"
    cp -a "$OLX_ROOT/share/gamedir" "$ANDROID_DIR/output/assets/gamedir"
    # Drop a marker so the runtime can check that the data was extracted.
    echo "$(cat "$OLX_ROOT/VERSION" 2>/dev/null || echo unknown)" \
        > "$ANDROID_DIR/output/assets/gamedir.version"
fi

# ---------- gradle build ---------------------------------------------------

cd "$ANDROID_DIR"

# Keep Gradle's project cache (the conventional .gradle/ dir) under
# output/ alongside every other generated artefact.
GRADLE_CACHE_DIR="$ANDROID_DIR/output/gradle-cache"
mkdir -p "$GRADLE_CACHE_DIR"
GRADLE_OPTS_COMMON=(--no-daemon --console=plain --project-cache-dir "$GRADLE_CACHE_DIR")

if [ "$DO_CLEAN" -eq 1 ]; then
    ./gradlew "${GRADLE_OPTS_COMMON[@]}" clean
fi

GRADLE_TASK="assemble${BUILD_TYPE^}"   # assembleDebug / assembleRelease

./gradlew "${GRADLE_OPTS_COMMON[@]}" "$GRADLE_TASK"

# ---------- output ---------------------------------------------------------

APK_SRC="$ANDROID_DIR/output/build/outputs/apk/$BUILD_TYPE/app-${BUILD_TYPE}.apk"
APK_DST_DIR="$ANDROID_DIR/output"
APK_DST="$APK_DST_DIR/openlierox-${BUILD_TYPE}.apk"

mkdir -p "$APK_DST_DIR"
cp -f "$APK_SRC" "$APK_DST"
echo "APK: $APK_DST"

# ---------- install + run --------------------------------------------------

if [ "$DO_INSTALL" -eq 1 ]; then
    PLATFORM_TOOLS="$ANDROID_SDK_ROOT/platform-tools"
    "$PLATFORM_TOOLS/adb" install -r "$APK_DST"
fi

if [ "$DO_RUN" -eq 1 ]; then
    "$PLATFORM_TOOLS/adb" shell am start -n net.openlierox/net.openlierox.OpenLieroXActivity
fi
