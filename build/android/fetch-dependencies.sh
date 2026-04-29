#!/usr/bin/env bash
# Fetch third-party native dependencies for the OpenLieroX Android build.
#
# All clones land in build/android/deps/<name> at a pinned tag. The
# CMake build under build/android/app/src/main/jni/CMakeLists.txt
# consumes them via add_subdirectory(). build/android/deps/ is
# .gitignored and may be a real directory or a symlink to an external
# location — this script doesn't care either way.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="$SCRIPT_DIR/deps"
mkdir -p "$DEPS_DIR"
cd "$DEPS_DIR"

# name | git url | tag/branch
DEPS=(
    "SDL|https://github.com/libsdl-org/SDL.git|release-2.32.2"
    "SDL_image|https://github.com/libsdl-org/SDL_image.git|release-2.8.4"
    "SDL_mixer|https://github.com/libsdl-org/SDL_mixer.git|release-2.8.0"
    "libxml2|https://github.com/GNOME/libxml2.git|v2.13.5"
    "libgd|https://github.com/libgd/libgd.git|gd-2.3.3"
    "libpng|https://github.com/pnggroup/libpng.git|v1.6.43"
    "libjpeg-turbo|https://github.com/libjpeg-turbo/libjpeg-turbo.git|3.0.4"
    "libogg|https://github.com/xiph/ogg.git|v1.3.5"
    "libvorbis|https://github.com/xiph/vorbis.git|v1.3.7"
    "openal-soft|https://github.com/kcat/openal-soft.git|1.23.1"
    "freealut|https://github.com/vancegroup/freealut.git|freealut_1_1_0"
    "curl|https://github.com/curl/curl.git|curl-8_10_1"
)

clone_dep() {
    local name="$1"
    local url="$2"
    local tag="$3"

    if [ -d "$name/.git" ]; then
        local current
        current=$(git -C "$name" describe --tags --always 2>/dev/null || echo "")
        if [ "$current" = "$tag" ]; then
            echo "$name @ $tag (already present)"
            return
        fi
        echo "$name: updating to $tag"
        git -C "$name" fetch --tags --depth 1 origin "$tag" 2>/dev/null || \
            git -C "$name" fetch --tags origin
        git -C "$name" checkout "$tag"
        return
    fi

    echo "Cloning $name @ $tag"
    git clone --depth 1 --branch "$tag" --recurse-submodules --shallow-submodules \
        "$url" "$name" 2>&1 | tail -5
}

for entry in "${DEPS[@]}"; do
    IFS='|' read -r name url tag <<< "$entry"
    clone_dep "$name" "$url" "$tag"
done

# Patch libjpeg-turbo so it can be add_subdirectory()'d. Upstream
# explicitly forbids it; we accept the consequences. We also stub out
# every `install(...)` call (multi-line) with a no-op header — install
# DESTINATIONs depend on GNUInstallDirs vars that aren't set when
# embedded as a subproject and would error out otherwise.
if [ -f libjpeg-turbo/CMakeLists.txt ]; then
    sed -i 's|message(FATAL_ERROR "The libjpeg-turbo build system cannot|message(WARNING "The libjpeg-turbo build system cannot|' \
        libjpeg-turbo/CMakeLists.txt
    # Wrap "install(...)" calls (multi-line, well-formatted) in if(FALSE).
    python3 - <<'PYEOF'
import re, pathlib
p = pathlib.Path("libjpeg-turbo/CMakeLists.txt")
src = p.read_text()
# Match install(...) including nested parens, line by line.
out = []
i = 0
lines = src.splitlines(keepends=True)
while i < len(lines):
    line = lines[i]
    stripped = line.lstrip()
    if stripped.startswith("install("):
        # consume until matching closing paren on its own line / inline.
        depth = line.count("(") - line.count(")")
        block = [line]
        i += 1
        while depth > 0 and i < len(lines):
            block.append(lines[i])
            depth += lines[i].count("(") - lines[i].count(")")
            i += 1
        indent = line[:len(line) - len(line.lstrip())]
        out.append(f"{indent}if(FALSE) # disabled by OpenLieroX Android build\n")
        out.extend(block)
        out.append(f"{indent}endif()\n")
    else:
        out.append(line)
        i += 1
p.write_text("".join(out))
PYEOF
fi

# Patch libgd's gdkanji.c so its local iconv_t typedef and stub
# declarations don't fight Bionic's <iconv.h> on Android. We can't
# easily prevent libgd's autoheader from setting HAVE_ICONV_H, so
# guard the conflicting blocks at the source level instead.
if [ -f libgd/src/gdkanji.c ] && ! grep -q "OLX Android iconv guard" libgd/src/gdkanji.c; then
    python3 - <<'PYEOF'
import pathlib
p = pathlib.Path("libgd/src/gdkanji.c")
src = p.read_text()
src = src.replace(
    "#if defined(HAVE_ICONV_H)\n#include <iconv.h>\n#endif",
    "/* OLX Android iconv guard: Bionic's <iconv.h> defines the same\n"
    " * symbols this file then re-declares as stubs. Skip both on Android. */\n"
    "#if defined(HAVE_ICONV_H) && !defined(__ANDROID__)\n"
    "#include <iconv.h>\n"
    "#endif")
src = src.replace(
    "#ifndef HAVE_ICONV_T_DEF\ntypedef void *iconv_t;\n#endif",
    "#if !defined(HAVE_ICONV_T_DEF) && !defined(__ANDROID__)\n"
    "typedef void *iconv_t;\n"
    "#elif defined(__ANDROID__)\n"
    "typedef void *iconv_t;\n"
    "#endif")
p.write_text(src)
PYEOF
fi

# boost-hdr: symlink to whatever boost headers the host system has.
if [ ! -e boost-hdr/boost ]; then
    mkdir -p boost-hdr
    if [ -d /usr/include/boost ]; then
        ln -sfn /usr/include/boost boost-hdr/boost
        echo "boost-hdr -> /usr/include/boost"
    else
        echo "WARNING: /usr/include/boost not found. Install libboost-dev or set up boost-hdr/boost manually." >&2
    fi
fi

echo "All dependencies are in place under $DEPS_DIR"
