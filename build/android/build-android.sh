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
DO_BUNDLE=0
DO_SIGN=0
SKIP_DATA=0
SKIP_FETCH=0
ARCHITECTURES="arm64-v8a"

while [ $# -gt 0 ]; do
    case "$1" in
        --release)    BUILD_TYPE="release" ;;
        --debug)      BUILD_TYPE="debug" ;;
        --clean)      DO_CLEAN=1 ;;
        --install|-i) DO_INSTALL=1 ;;
        --run|-r)     DO_INSTALL=1; DO_RUN=1 ;;
        --bundle)     DO_BUNDLE=1 ;;
        --sign)       DO_SIGN=1 ;;
        --arch)       ARCHITECTURES="$2" ; shift ;;
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
    "$ANDROID_DIR/fetch-dependencies.sh"
fi

# ---------- stage game data ------------------------------------------------

if [ "$SKIP_DATA" -eq 0 ]; then
    echo "Staging game data from share/gamedir into build/android/output/assets..."
    rm -rf "$ANDROID_DIR/output/assets/gamedir"
    mkdir -p "$ANDROID_DIR/output/assets"
    cp -a "$OLX_ROOT/share/gamedir" "$ANDROID_DIR/output/assets/gamedir"
    # Ship Mozilla's CA bundle next to the gamedir so libcurl's mbedTLS
    # backend can verify HTTPS certificates (Android exposes no cert file
    # in a format curl/mbedTLS can read). Fail loudly rather than silently
    # producing an APK whose every https:// request rejects the peer.
    if [ ! -f "$ANDROID_DIR/deps/cacert.pem" ]; then
        echo "ERROR: $ANDROID_DIR/deps/cacert.pem is missing." >&2
        echo "       HTTPS in the APK depends on it. Run" >&2
        echo "       build/android/fetch-dependencies.sh (without --skip-fetch)" >&2
        echo "       to download it from curl.se." >&2
        exit 1
    fi
    cp "$ANDROID_DIR/deps/cacert.pem" "$ANDROID_DIR/output/assets/gamedir/cacert.pem"
    # Drop a version marker the runtime compares against to decide whether to
    # re-extract the bundled data (see OpenLieroXActivity.extractGamedirIfNeeded).
    #
    # This must change whenever the *bundled data* changes, otherwise the device
    # keeps using a stale copy from a previous install. The git version alone is
    # not enough: editing data without committing (or rebuilding the same commit)
    # leaves the version string unchanged, so the new data never gets extracted.
    #
    # So base the marker on a content hash of the fully-staged gamedir (the git
    # version is kept in the string too, for readable logs). Any change to any
    # bundled file -> different hash -> the runtime re-extracts.
    olx_version="$("$OLX_ROOT/get_version.sh")"   # fails the build if undeterminable
    data_hash="$(cd "$ANDROID_DIR/output/assets" \
        && find gamedir -type f -print0 \
        | LC_ALL=C sort -z \
        | xargs -0 sha1sum \
        | sha1sum | cut -c1-40)"
    if [ -z "$data_hash" ]; then
        echo "ERROR: failed to hash staged gamedir for the version marker." >&2
        exit 1
    fi
    printf '%s data:%s\n' "$olx_version" "$data_hash" \
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

./gradlew "${GRADLE_OPTS_COMMON[@]}" "$GRADLE_TASK" -Parchitectures="$ARCHITECTURES"

# ---------- output ---------------------------------------------------------

APK_SRC="$ANDROID_DIR/output/build/outputs/apk/$BUILD_TYPE/app-${BUILD_TYPE}.apk"
APK_DST_DIR="$ANDROID_DIR/output"
APK_DST="$APK_DST_DIR/openlierox-${BUILD_TYPE}.apk"

mkdir -p "$APK_DST_DIR"
cp -f "$APK_SRC" "$APK_DST"
echo "APK: $APK_DST"

# ---------- bundle ---------------------------------------------------------

BUNDLE_SRC="$ANDROID_DIR/output/build/outputs/bundle/releaseWithDebugInfo/app-releaseWithDebugInfo.aab"
BUNDLE_DST="$ANDROID_DIR/output/openlierox-release.aab"

if [ "$DO_BUNDLE" -eq 1 ]; then
    # Always build release with debug info, the bundle is used only for uploading to Play Store
    ./gradlew "${GRADLE_OPTS_COMMON[@]}" bundleReleaseWithDebugInfo -Parchitectures="$ARCHITECTURES"
    cp -f "$BUNDLE_SRC" "$BUNDLE_DST"
fi

# ---------- sign ---------------------------------------------------------

if [ "$DO_SIGN" -eq 1 ]; then
    apksigner sign --ks "$ANDROID_KEYSTORE_FILE" --ks-key-alias "$ANDROID_KEYSTORE_ALIAS" \
      --ks-pass env:ANDROID_KEYSTORE_PASS "$APK_DST"

    if [ "$DO_BUNDLE" -eq 1 ]; then
      zip -d "$BUNDLE_DST" "META-INF/*"
      jarsigner -sigalg SHA1withRSA -digestalg SHA1 \
        -keystore "$ANDROID_UPLOAD_KEYSTORE_FILE" \
        -storepass:env ANDROID_UPLOAD_KEYSTORE_PASS \
        "$BUNDLE_DST" \
        "$ANDROID_UPLOAD_KEYSTORE_ALIAS"
    fi
fi

# ---------- install + run --------------------------------------------------

if [ "$DO_INSTALL" -eq 1 ]; then
    PLATFORM_TOOLS="$ANDROID_SDK_ROOT/platform-tools"
    "$PLATFORM_TOOLS/adb" install -r "$APK_DST"
fi

if [ "$DO_RUN" -eq 1 ]; then
    "$PLATFORM_TOOLS/adb" shell am start -n openlierox.net/openlierox.net.OpenLieroXActivity
fi
