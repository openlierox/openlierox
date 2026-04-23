#!/usr/bin/env bash
# Installs the toolchain required to build the Android apps in this repo
# (openlierox in particular).
#
# What this installs:
#   - System packages: OpenJDK 21, build-essential, unzip, wget, curl, git,
#     ant, zip, xz-utils (needed for .zip.xz data archives).
#   - Android command-line tools, platform-tools, build-tools;36.0.0,
#     platforms;android-36 and NDK 29.0.14206865 (as required by
#     project/app/build-template.gradle) under $HOME/android-sdk.
#
# Safe to re-run: each step is idempotent.

set -euo pipefail

SUDO=""
if [ "$(id -u)" -ne 0 ]; then
    SUDO="sudo"
fi

echo "==> Installing system packages via apt"
export DEBIAN_FRONTEND=noninteractive
$SUDO apt-get update
$SUDO apt-get install -y --no-install-recommends \
    openjdk-21-jdk-headless \
    build-essential \
    ant \
    unzip \
    zip \
    xz-utils \
    wget \
    curl \
    ca-certificates \
    git \
    python3 \
    file

# ---------------------------------------------------------------------------
# Android SDK + NDK (user-local, no sudo required)
# ---------------------------------------------------------------------------
ANDROID_SDK_ROOT="${ANDROID_SDK_ROOT:-$HOME/android-sdk}"
CMDLINE_TOOLS_VERSION="13114758"   # commandlinetools-linux-13114758 (Android Studio Koala Feature Drop)
NDK_VERSION="29.0.14206865"
BUILD_TOOLS_VERSION="36.0.0"
PLATFORM_VERSION="android-36"

mkdir -p "$ANDROID_SDK_ROOT"

# cmdline-tools -- only install if missing.
if [ ! -x "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" ]; then
    echo "==> Installing Android command-line tools to $ANDROID_SDK_ROOT"
    TMP_ZIP="$(mktemp -u /tmp/cmdline-tools.XXXXXX.zip)"
    wget -q -O "$TMP_ZIP" \
        "https://dl.google.com/android/repository/commandlinetools-linux-${CMDLINE_TOOLS_VERSION}_latest.zip"
    rm -rf "$ANDROID_SDK_ROOT/cmdline-tools"
    mkdir -p "$ANDROID_SDK_ROOT/cmdline-tools"
    unzip -q "$TMP_ZIP" -d "$ANDROID_SDK_ROOT/cmdline-tools"
    # The archive unpacks into cmdline-tools/, we need cmdline-tools/latest/
    mv "$ANDROID_SDK_ROOT/cmdline-tools/cmdline-tools" "$ANDROID_SDK_ROOT/cmdline-tools/latest"
    rm -f "$TMP_ZIP"
fi

export PATH="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:$ANDROID_SDK_ROOT/platform-tools:$PATH"
export ANDROID_SDK_ROOT
export ANDROID_HOME="$ANDROID_SDK_ROOT"

# Make sure sdkmanager can find a JDK.
if [ -z "${JAVA_HOME:-}" ] && [ -d /usr/lib/jvm/java-21-openjdk-amd64 ]; then
    export JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
fi

echo "==> Accepting SDK licenses"
yes | sdkmanager --licenses >/dev/null || true

echo "==> Installing SDK packages (platform-tools, build-tools, platform, NDK)"
sdkmanager \
    "platform-tools" \
    "build-tools;${BUILD_TOOLS_VERSION}" \
    "platforms;${PLATFORM_VERSION}" \
    "ndk;${NDK_VERSION}"

# Convenient symlinks so build.sh's `which ndk-build` logic works.
mkdir -p "$HOME/.local/bin"
ln -sf "$ANDROID_SDK_ROOT/ndk/${NDK_VERSION}/ndk-build" "$HOME/.local/bin/ndk-build"
ln -sf "$ANDROID_SDK_ROOT/build-tools/${BUILD_TOOLS_VERSION}/zipalign" "$HOME/.local/bin/zipalign"
ln -sf "$ANDROID_SDK_ROOT/build-tools/${BUILD_TOOLS_VERSION}/apksigner" "$HOME/.local/bin/apksigner"

cat <<EOF

==> Done.

Add the following to your shell rc (or the invoking environment) before running ./build.sh:
    export ANDROID_SDK_ROOT="$ANDROID_SDK_ROOT"
    export ANDROID_HOME="$ANDROID_SDK_ROOT"
    export ANDROID_NDK_HOME="$ANDROID_SDK_ROOT/ndk/${NDK_VERSION}"
    export JAVA_HOME=${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}
    export PATH="\$ANDROID_SDK_ROOT/cmdline-tools/latest/bin:\$ANDROID_SDK_ROOT/platform-tools:\$ANDROID_SDK_ROOT/ndk/${NDK_VERSION}:\$ANDROID_SDK_ROOT/build-tools/${BUILD_TOOLS_VERSION}:\$HOME/.local/bin:\$PATH"
EOF
