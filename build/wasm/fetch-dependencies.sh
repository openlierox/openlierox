#!/usr/bin/env bash
# Fetch third-party native dependencies the Wasm build needs to compile
# from source. SDL2/SDL_image/SDL_mixer/libpng/libjpeg/libogg/libvorbis/
# zlib all come from Emscripten's built-in ports, so we only clone what
# Emscripten doesn't ship.
#
# Clones land under build/wasm/deps/<name> at pinned tags. The directory
# is .gitignored.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DEPS_DIR="$SCRIPT_DIR/deps"
mkdir -p "$DEPS_DIR"
cd "$DEPS_DIR"

DEPS=(
    "libxml2|https://github.com/GNOME/libxml2.git|v2.13.5"
    "libgd|https://github.com/libgd/libgd.git|gd-2.3.3"
    "curl|https://github.com/curl/curl.git|curl-8_10_1"
    # 0.59's TouchControls parses share/gamedir/touchscreen/*.yaml at
    # runtime. Same dep + pin as the Android port.
    "yaml-cpp|https://github.com/jbeder/yaml-cpp.git|0.8.0"
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

# Patch libgd's gdkanji.c so its iconv stubs don't fight any system
# <iconv.h>. Same patch as the Android port; emcc's musl sysroot also
# defines iconv_t.
if [ -f libgd/src/gdkanji.c ] && ! grep -q "OLX Wasm iconv guard" libgd/src/gdkanji.c; then
    python3 - <<'PYEOF'
import pathlib
p = pathlib.Path("libgd/src/gdkanji.c")
src = p.read_text()
src = src.replace(
    "#if defined(HAVE_ICONV_H)\n#include <iconv.h>\n#endif",
    "/* OLX Wasm iconv guard */\n"
    "#if defined(HAVE_ICONV_H) && !defined(__EMSCRIPTEN__)\n"
    "#include <iconv.h>\n"
    "#endif")
src = src.replace(
    "#ifndef HAVE_ICONV_T_DEF\ntypedef void *iconv_t;\n#endif",
    "#if !defined(HAVE_ICONV_T_DEF) && !defined(__EMSCRIPTEN__)\n"
    "typedef void *iconv_t;\n"
    "#elif defined(__EMSCRIPTEN__)\n"
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
        echo "WARNING: /usr/include/boost not found. Install libboost-dev." >&2
    fi
fi

echo "All dependencies are in place under $DEPS_DIR"
