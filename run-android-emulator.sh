#!/usr/bin/env bash
# run-android-emulator.sh — Launch an Android emulator for testing the
# OpenLieroX APK built by build-android.sh.
#
# Expects an AVD to already exist (default name: "olx"). Create one with:
#   sdkmanager --install "emulator" "system-images;android-34;google_apis;x86_64"
#   avdmanager create avd --name olx --package "system-images;android-34;google_apis;x86_64" --device pixel_5
#
# Usage: ./run-android-emulator.sh [--headless] [--wipe] [--avd NAME] [--install APK] [--run]
#
# Options:
#   --avd NAME     Use AVD NAME (default: olx)
#   --headless     Launch with -no-window -no-audio (good for CI / SSH)
#   --wipe         Wipe user data on start (fresh boot)
#   --install APK  adb install the given APK after boot completes
#   --run          After --install, launch the OpenLieroX MainActivity
#   --wait         Wait for the emulator to finish booting, then detach
#                  (default: tail the emulator process in foreground)
#   -h             Show this help
#
# Env:
#   ANDROID_SDK_ROOT / ANDROID_HOME   Android SDK path (default: ~/android-sdk)
#   EMULATOR_EXTRA_ARGS               Extra args forwarded to `emulator`

set -euo pipefail

AVD="olx"
HEADLESS=0
WIPE=0
INSTALL_APK=""
RUN_APP=0
WAIT_ONLY=0

while [ $# -gt 0 ]; do
    case "$1" in
        --avd)        AVD="$2"; shift ;;
        --headless)   HEADLESS=1 ;;
        --wipe)       WIPE=1 ;;
        --install)    INSTALL_APK="$2"; shift ;;
        --run)        RUN_APP=1 ;;
        --wait)       WAIT_ONLY=1 ;;
        -h|--help)    sed -n '2,22p' "$0" | sed 's/^# \?//'; exit 0 ;;
        *)            echo "Unknown arg: $1" >&2; exit 2 ;;
    esac
    shift
done

export ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-${ANDROID_HOME:-$HOME/android-sdk}}"
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export PATH="$ANDROID_SDK_ROOT/emulator:$ANDROID_SDK_ROOT/platform-tools:$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$PATH"

for tool in emulator adb; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "ERROR: $tool not found. Install it:" >&2
        echo "  sdkmanager --install emulator platform-tools" >&2
        exit 1
    }
done

if ! emulator -list-avds 2>/dev/null | grep -qx "$AVD"; then
    echo "ERROR: AVD '$AVD' not found. Available AVDs:" >&2
    emulator -list-avds >&2 || true
    echo >&2
    echo "Create one with:" >&2
    echo "  avdmanager create avd --name $AVD \\" >&2
    echo "      --package 'system-images;android-34;google_apis;x86_64' \\" >&2
    echo "      --device pixel_5" >&2
    exit 1
fi

if [ ! -e /dev/kvm ]; then
    echo "WARN: /dev/kvm missing — emulator will fall back to software and be very slow." >&2
elif ! [ -r /dev/kvm ] || ! [ -w /dev/kvm ]; then
    echo "WARN: /dev/kvm not readable/writable by this user — add yourself to the kvm group:" >&2
    echo "      sudo usermod -aG kvm \$USER  (log out/in to take effect)" >&2
fi

EMU_ARGS=(-avd "$AVD" -no-boot-anim)
[ "$WIPE" = 1 ]     && EMU_ARGS+=(-wipe-data)
[ "$HEADLESS" = 1 ] && EMU_ARGS+=(-no-window -no-audio -gpu swiftshader_indirect)
# shellcheck disable=SC2206
EMU_ARGS+=(${EMULATOR_EXTRA_ARGS:-})

# Start adb server before the emulator so it registers cleanly.
adb start-server >/dev/null 2>&1 || true

if [ -n "$INSTALL_APK" ] || [ "$RUN_APP" = 1 ] || [ "$WAIT_ONLY" = 1 ]; then
    LOG="$(mktemp -t olx-emulator.XXXXXX.log)"
    echo ">>> starting emulator in background (log: $LOG)"
    nohup emulator "${EMU_ARGS[@]}" >"$LOG" 2>&1 &
    EMU_PID=$!
    echo ">>> emulator pid=$EMU_PID"

    echo ">>> waiting for boot (up to 5 min)…"
    # wait-for-device blocks until adbd is up; boot_completed signals full boot.
    for _ in $(seq 1 60); do
        if adb wait-for-device shell getprop sys.boot_completed 2>/dev/null | tr -d '\r' | grep -q '^1$'; then
            break
        fi
        sleep 5
        if ! kill -0 "$EMU_PID" 2>/dev/null; then
            echo "ERROR: emulator exited before boot. Log tail:" >&2
            tail -40 "$LOG" >&2
            exit 1
        fi
    done
    adb shell input keyevent 82 >/dev/null 2>&1 || true  # unlock screen
    echo ">>> boot complete"

    if [ -n "$INSTALL_APK" ]; then
        if [ ! -f "$INSTALL_APK" ]; then
            echo "ERROR: APK not found: $INSTALL_APK" >&2
            exit 1
        fi
        echo ">>> adb install -r $INSTALL_APK"
        adb install -r "$INSTALL_APK"
    fi

    if [ "$RUN_APP" = 1 ]; then
        PKG="openlierox.net"
        ACT="$PKG/.MainActivity"
        echo ">>> launching $ACT"
        adb shell am start -n "$ACT"
    fi

    if [ "$WAIT_ONLY" = 1 ]; then
        echo ">>> detaching (emulator pid=$EMU_PID still running; stop with: adb emu kill)"
        exit 0
    fi
    echo ">>> emulator pid=$EMU_PID (ctrl-c detaches; stop with: adb emu kill)"
    wait "$EMU_PID"
else
    exec emulator "${EMU_ARGS[@]}"
fi
