#!/usr/bin/env bash
# Boot the headless 'olx' AVD, install the OpenLieroX APK, launch it,
# and verify it stays alive for at least N seconds without crashing.
#
# This is the test loop that proves "boots and doesn't crash".
#
# Usage: ./build/android/run-android-emulator.sh [APK] [SECONDS]
#   APK     Path to the APK (default: build/android/output/openlierox-debug.apk)
#   SECONDS How long the process must stay alive (default: 12)
#
# Env:
#   AVD_NAME           AVD to launch  (default: olx)
#   ANDROID_SDK_ROOT   Android SDK    (default: $HOME/android-sdk)

set -euo pipefail

ANDROID_DIR="$(cd "$(dirname "$0")" && pwd)"
APK="${1:-$ANDROID_DIR/output/openlierox-debug.apk}"
SECS="${2:-12}"
AVD_NAME="${AVD_NAME:-olx}"
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/android-sdk}"
ADB="$ANDROID_SDK_ROOT/platform-tools/adb"
EMULATOR="$ANDROID_SDK_ROOT/emulator/emulator"

PKG="net.openlierox"
ACTIVITY="$PKG/.OpenLieroXActivity"

if [ ! -f "$APK" ]; then
    echo "ERROR: APK not found at $APK" >&2
    exit 1
fi

# ---------- ensure the emulator is up -------------------------------------

if ! "$ADB" devices | grep -q "emulator-.*device$"; then
    echo "Booting AVD '$AVD_NAME'..."
    nohup "$EMULATOR" -avd "$AVD_NAME" -no-window -no-snapshot-save \
        -no-audio -gpu swiftshader_indirect -wipe-data \
        -partition-size 2048 \
        > /tmp/emulator.log 2>&1 &
    EMU_PID=$!
    echo "$EMU_PID" > /tmp/emulator.pid
fi

echo "Waiting for device..."
"$ADB" wait-for-device

echo "Waiting for boot to complete..."
for _ in $(seq 1 60); do
    if [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ]; then
        break
    fi
    sleep 2
done

if [ "$("$ADB" shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" != "1" ]; then
    echo "ERROR: emulator did not finish booting" >&2
    exit 2
fi

# ---------- install -------------------------------------------------------

echo "Installing $APK..."
"$ADB" install -r -t "$APK"

# Clear logcat so we only see our session.
"$ADB" logcat -c

# ---------- launch + verify -----------------------------------------------

echo "Launching $ACTIVITY..."
"$ADB" shell am start -W -n "$ACTIVITY"

# Wait briefly for the process to come up.
PID=""
for _ in $(seq 1 10); do
    PID="$("$ADB" shell pidof "$PKG" 2>/dev/null | tr -d '\r')"
    if [ -n "$PID" ]; then break; fi
    sleep 1
done

if [ -z "$PID" ]; then
    echo "ERROR: $PKG never appeared in pidof" >&2
    "$ADB" logcat -d | tail -200
    exit 3
fi

echo "$PKG started as pid $PID. Watching for $SECS seconds..."

t=0
while [ "$t" -lt "$SECS" ]; do
    sleep 1
    t=$((t + 1))
    LIVE="$("$ADB" shell pidof "$PKG" 2>/dev/null | tr -d '\r')"
    if [ -z "$LIVE" ]; then
        echo "FAIL: $PKG died after $t seconds" >&2
        echo "----- last 200 logcat lines -----" >&2
        "$ADB" logcat -d | tail -200 >&2
        exit 4
    fi
done

echo "PASS: $PKG ran for $SECS seconds (still alive as pid $PID)"

# Save logcat for diagnostics.
LOG_OUT="$ANDROID_DIR/output/logcat-$(date +%Y%m%d-%H%M%S).log"
mkdir -p "$ANDROID_DIR/output"
"$ADB" logcat -d > "$LOG_OUT"
echo "Full logcat saved to $LOG_OUT"
