# OpenLieroX Android native build.
#
# Builds SDL2 + image/mixer, supporting native libs, and OpenLieroX
# itself as a single shared library "main" (libmain.so) that
# SDLActivity loads via android.app.lib_name=main in the manifest.
#
# Third-party deps live under build/android/deps and are added with
# add_subdirectory(). They are pulled by build/android/deps/fetch.sh.

cmake_minimum_required(VERSION 3.22.1)
project(openlierox_android C CXX)

if(NOT DEFINED OLX_ROOT)
    message(FATAL_ERROR "OLX_ROOT must be set (path to OpenLieroX repo root)")
endif()

set(DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../deps")
get_filename_component(DEPS_DIR "${DEPS_DIR}" ABSOLUTE)

# Override CMake's stock FindPNG / FindJPEG so libgd can find the
# libpng / libjpeg-turbo targets we add as subprojects below.
# libgd resets CMAKE_MODULE_PATH inside its CMakeLists, so we drop our
# overrides into libgd's own cmake/modules/ dir to make sure they win.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules/FindPNG.cmake"
          "${CMAKE_CURRENT_SOURCE_DIR}/cmake-modules/FindJPEG.cmake"
     DESTINATION "${DEPS_DIR}/libgd/cmake/modules")

# ---------------------------------------------------------------------
# Global flags
# ---------------------------------------------------------------------
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wno-deprecated-declarations -Wno-narrowing -U_FORTIFY_SOURCE)

# ---------------------------------------------------------------------
# zlib (NDK ships libz.so since API 21). Subprojects (libpng,
# libxml2, libcurl…) expect a ZLIB::ZLIB imported target. CMake's
# stock FindZLIB doesn't auto-discover the NDK-provided libz, so we
# wire one up explicitly.
# ---------------------------------------------------------------------
find_library(ANDROID_ZLIB z REQUIRED)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_LOCATION "${ANDROID_ZLIB}")
endif()
set(ZLIB_FOUND TRUE)
set(ZLIB_LIBRARY "${ANDROID_ZLIB}")
set(ZLIB_LIBRARIES "${ANDROID_ZLIB}")
set(ZLIB_INCLUDE_DIR "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include")
set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIR}")

# ---------------------------------------------------------------------
# SDL2 + SDL2_image + SDL2_mixer
# Vendored deps inside SDL_image/SDL_mixer pull in libpng, libjpeg, libogg
# and libvorbis as transitive subprojects, so we don't need to wire them
# manually.
# ---------------------------------------------------------------------
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "")
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "")
set(SDL_TEST OFF CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/SDL"          "${CMAKE_BINARY_DIR}/SDL")

set(SDL2IMAGE_INSTALL OFF CACHE BOOL "")
set(SDL2IMAGE_DEPS_SHARED OFF CACHE BOOL "")
set(SDL2IMAGE_VENDORED ON CACHE BOOL "")
set(SDL2IMAGE_BACKEND_IMAGEIO OFF CACHE BOOL "")
set(SDL2IMAGE_BACKEND_STB OFF CACHE BOOL "")
set(SDL2IMAGE_BACKEND_WIC OFF CACHE BOOL "")
set(SDL2IMAGE_AVIF OFF CACHE BOOL "")
set(SDL2IMAGE_BMP ON  CACHE BOOL "")
set(SDL2IMAGE_GIF ON  CACHE BOOL "")
set(SDL2IMAGE_JPG ON  CACHE BOOL "")
set(SDL2IMAGE_LBM OFF CACHE BOOL "")
set(SDL2IMAGE_PCX OFF CACHE BOOL "")
set(SDL2IMAGE_PNG ON  CACHE BOOL "")
set(SDL2IMAGE_PNM OFF CACHE BOOL "")
set(SDL2IMAGE_QOI OFF CACHE BOOL "")
set(SDL2IMAGE_SVG OFF CACHE BOOL "")
set(SDL2IMAGE_TGA ON  CACHE BOOL "")
set(SDL2IMAGE_TIF OFF CACHE BOOL "")
set(SDL2IMAGE_WEBP OFF CACHE BOOL "")
set(SDL2IMAGE_XCF OFF CACHE BOOL "")
set(SDL2IMAGE_XPM OFF CACHE BOOL "")
set(SDL2IMAGE_XV  OFF CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/SDL_image"    "${CMAKE_BINARY_DIR}/SDL_image")

# SDL_image's vendored libpng exposes only ${CMAKE_CURRENT_SOURCE_DIR}
# as PUBLIC; the generated pnglibconf.h sits in the build tree though,
# so consumers (including SDL_image's own IMG_png.c) can't find it.
# Forward the build dir as a PUBLIC include dir so the header resolves.
if(TARGET png_static)
    target_include_directories(png_static PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/SDL_image/external/libpng>)
endif()

set(SDL2MIXER_INSTALL OFF CACHE BOOL "")
set(SDL2MIXER_DEPS_SHARED OFF CACHE BOOL "")
set(SDL2MIXER_VENDORED ON CACHE BOOL "")
set(SDL2MIXER_VORBIS "VORBISFILE" CACHE STRING "")
set(SDL2MIXER_FLAC OFF CACHE BOOL "")
set(SDL2MIXER_MOD OFF CACHE BOOL "")
set(SDL2MIXER_MP3 OFF CACHE BOOL "")
set(SDL2MIXER_MIDI OFF CACHE BOOL "")
set(SDL2MIXER_OPUS OFF CACHE BOOL "")
set(SDL2MIXER_WAVPACK OFF CACHE BOOL "")
set(SDL2MIXER_WAVE ON  CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/SDL_mixer"    "${CMAKE_BINARY_DIR}/SDL_mixer")

# ---------------------------------------------------------------------
# libxml2
# ---------------------------------------------------------------------
set(LIBXML2_WITH_PYTHON OFF CACHE BOOL "")
set(LIBXML2_WITH_TESTS OFF CACHE BOOL "")
set(LIBXML2_WITH_PROGRAMS OFF CACHE BOOL "")
set(LIBXML2_WITH_LZMA OFF CACHE BOOL "")
set(LIBXML2_WITH_ZLIB ON  CACHE BOOL "")
set(LIBXML2_WITH_ICONV OFF CACHE BOOL "")
set(LIBXML2_WITH_HTTP OFF CACHE BOOL "")
set(LIBXML2_WITH_FTP OFF CACHE BOOL "")
set(LIBXML2_WITH_THREADS ON  CACHE BOOL "")
set(LIBXML2_WITH_HTML ON  CACHE BOOL "")
set(LIBXML2_WITH_TREE ON  CACHE BOOL "")
set(LIBXML2_WITH_SAX1 ON  CACHE BOOL "")
set(LIBXML2_WITH_OUTPUT ON CACHE BOOL "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPS_DIR}/libxml2"      "${CMAKE_BINARY_DIR}/libxml2")
unset(BUILD_SHARED_LIBS CACHE)

# ---------------------------------------------------------------------
# libpng (built via SDL_image vendored, but libgd needs the target too)
# We expose the SDL_image-vendored libpng if available; otherwise build
# our own copy.
# ---------------------------------------------------------------------
if(NOT TARGET png_static AND NOT TARGET png)
    set(PNG_SHARED OFF CACHE BOOL "")
    set(PNG_STATIC ON  CACHE BOOL "")
    set(PNG_TESTS  OFF CACHE BOOL "")
    set(PNG_TOOLS  OFF CACHE BOOL "")
    add_subdirectory("${DEPS_DIR}/libpng"   "${CMAKE_BINARY_DIR}/libpng")
endif()

# ---------------------------------------------------------------------
# libjpeg-turbo (libgd needs JPEG support)
# ---------------------------------------------------------------------
if(NOT TARGET jpeg-static AND NOT TARGET jpeg)
    set(ENABLE_SHARED OFF CACHE BOOL "")
    set(ENABLE_STATIC ON  CACHE BOOL "")
    set(WITH_TURBOJPEG OFF CACHE BOOL "")
    add_subdirectory("${DEPS_DIR}/libjpeg-turbo" "${CMAKE_BINARY_DIR}/libjpeg-turbo")
endif()

# ---------------------------------------------------------------------
# libogg + libvorbis + libvorbisfile
# (SDL_mixer vendored may build them too — guard against double inclusion.)
# ---------------------------------------------------------------------
if(NOT TARGET ogg)
    set(BUILD_TESTING OFF CACHE BOOL "")
    add_subdirectory("${DEPS_DIR}/libogg"   "${CMAKE_BINARY_DIR}/libogg")
endif()
if(NOT TARGET vorbis)
    add_subdirectory("${DEPS_DIR}/libvorbis" "${CMAKE_BINARY_DIR}/libvorbis")
endif()

# ---------------------------------------------------------------------
# OpenAL Soft (OpenSL ES backend on Android)
# ---------------------------------------------------------------------
set(LIBTYPE STATIC CACHE STRING "")
set(ALSOFT_UTILS OFF CACHE BOOL "")
set(ALSOFT_NO_CONFIG_UTIL ON CACHE BOOL "")
set(ALSOFT_EXAMPLES OFF CACHE BOOL "")
set(ALSOFT_TESTS OFF CACHE BOOL "")
set(ALSOFT_INSTALL OFF CACHE BOOL "")
set(ALSOFT_INSTALL_CONFIG OFF CACHE BOOL "")
set(ALSOFT_INSTALL_HRTF_DATA OFF CACHE BOOL "")
set(ALSOFT_INSTALL_AMBDEC_PRESETS OFF CACHE BOOL "")
set(ALSOFT_INSTALL_EXAMPLES OFF CACHE BOOL "")
set(ALSOFT_INSTALL_UTILS OFF CACHE BOOL "")
set(ALSOFT_BACKEND_OPENSL ON CACHE BOOL "")
set(ALSOFT_BACKEND_OBOE OFF CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/openal-soft"  "${CMAKE_BINARY_DIR}/openal-soft")

# ---------------------------------------------------------------------
# alut: freealut's CMake build doesn't work with modern CMake (it
# references undefined macros from its own admin/ scripts that aren't
# discoverable when embedded). OpenLieroX only needs alutInit/alutExit
# and the two error helpers, so we ship a tiny in-tree replacement
# (alut_stub.c) that opens an OpenAL device directly.
# ---------------------------------------------------------------------

# ---------------------------------------------------------------------
# libgd (depends on libpng + libjpeg + libz). libgd unconditionally
# pulls in CPack and references "${CMAKE_SOURCE_DIR}/COPYING"; we ship
# a stub COPYING file alongside this CMakeLists.txt so the configure
# step succeeds (we never invoke cpack).
# ---------------------------------------------------------------------
set(BUILD_TEST OFF CACHE BOOL "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON  CACHE BOOL "" FORCE)
set(ENABLE_PNG ON  CACHE BOOL "")
set(ENABLE_JPEG ON CACHE BOOL "")
set(ENABLE_FREETYPE OFF CACHE BOOL "")
set(ENABLE_FONTCONFIG OFF CACHE BOOL "")
set(ENABLE_TIFF OFF CACHE BOOL "")
set(ENABLE_WEBP OFF CACHE BOOL "")
set(ENABLE_HEIF OFF CACHE BOOL "")
set(ENABLE_AVIF OFF CACHE BOOL "")
set(ENABLE_LIQ OFF CACHE BOOL "")
set(ENABLE_RAQM OFF CACHE BOOL "")
set(ENABLE_XPM OFF CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/libgd"        "${CMAKE_BINARY_DIR}/libgd")
unset(BUILD_SHARED_LIBS CACHE)

# ---------------------------------------------------------------------
# mbedTLS — provides the HTTPS backend for libcurl below. Android has no
# usable system curl, and curl's other TLS backends (OpenSSL, Schannel,
# Secure Transport) aren't available here, so we link a small static
# mbedTLS into the curl build.
# ---------------------------------------------------------------------
set(ENABLE_PROGRAMS OFF CACHE BOOL "")
set(ENABLE_TESTING OFF CACHE BOOL "")
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "")
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "")
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/mbedtls"      "${CMAKE_BINARY_DIR}/mbedtls")

# Pre-fill the variables curl's FindMbedTLS.cmake looks up so it picks up
# the in-tree subproject above instead of searching the NDK sysroot. The
# library names resolve to the add_subdirectory targets at link time.
set(MBEDTLS_INCLUDE_DIRS "${DEPS_DIR}/mbedtls/include" CACHE PATH    "" FORCE)
set(MBEDTLS_LIBRARY      mbedtls            CACHE FILEPATH "" FORCE)
set(MBEDX509_LIBRARY     mbedx509           CACHE FILEPATH "" FORCE)
set(MBEDCRYPTO_LIBRARY   mbedcrypto         CACHE FILEPATH "" FORCE)

# ---------------------------------------------------------------------
# libcurl — HTTP + HTTPS via mbedTLS.
# ---------------------------------------------------------------------
set(BUILD_CURL_EXE OFF CACHE BOOL "")
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "")
set(BUILD_MISC_DOCS OFF CACHE BOOL "")
set(BUILD_TESTING OFF CACHE BOOL "")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "")
set(CURL_USE_LIBPSL OFF CACHE BOOL "")
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "")
set(CURL_USE_OPENSSL OFF CACHE BOOL "")
set(CURL_USE_GSSAPI OFF CACHE BOOL "")
set(CURL_USE_MBEDTLS ON CACHE BOOL "")
# We embed libcurl as a subproject and never install it. Skipping the
# CMake export target avoids "export() / install(EXPORT CURLTargets ...)
# requires target mbedtls that is not in any export set" - mbedTLS'
# subproject targets aren't in any export set, and we have no use for
# the exported curl targets anyway.
set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "")
set(CURL_DISABLE_LDAP ON CACHE BOOL "")
set(CURL_DISABLE_LDAPS ON CACHE BOOL "")
set(CURL_DISABLE_PROXY OFF CACHE BOOL "")
set(CURL_DISABLE_SMTP ON CACHE BOOL "")
set(CURL_DISABLE_IMAP ON CACHE BOOL "")
set(CURL_DISABLE_POP3 ON CACHE BOOL "")
set(CURL_DISABLE_TFTP ON CACHE BOOL "")
set(CURL_DISABLE_RTSP ON CACHE BOOL "")
set(CURL_DISABLE_TELNET ON CACHE BOOL "")
set(CURL_DISABLE_DICT ON CACHE BOOL "")
set(CURL_DISABLE_FILE ON CACHE BOOL "")
set(CURL_DISABLE_FTP ON CACHE BOOL "")
set(CURL_DISABLE_GOPHER ON CACHE BOOL "")
set(CURL_DISABLE_MQTT ON CACHE BOOL "")
set(CURL_DISABLE_NETRC ON CACHE BOOL "")
set(CURL_DISABLE_COOKIES ON CACHE BOOL "")
set(CURL_DISABLE_ALTSVC ON CACHE BOOL "")
set(HTTP_ONLY ON CACHE BOOL "")
set(USE_LIBIDN2 OFF CACHE BOOL "")
set(USE_NGHTTP2 OFF CACHE BOOL "")
set(CURL_ZLIB ON CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/curl"         "${CMAKE_BINARY_DIR}/curl")
unset(BUILD_SHARED_LIBS CACHE)

# ---------------------------------------------------------------------
# yaml-cpp — parses share/gamedir/touchscreen/*.yaml at runtime.
# ---------------------------------------------------------------------
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "")
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "")
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "")
set(YAML_CPP_INSTALL OFF CACHE BOOL "")
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/yaml-cpp"     "${CMAKE_BINARY_DIR}/yaml-cpp")

# ---------------------------------------------------------------------
# OpenLieroX itself
# ---------------------------------------------------------------------
file(GLOB_RECURSE OLX_SRCS
    "${OLX_ROOT}/src/*.cpp"
    "${OLX_ROOT}/src/*.c")

# Exclude bits that don't apply on Android.
list(FILTER OLX_SRCS EXCLUDE REGEX "/breakpad/")
list(FILTER OLX_SRCS EXCLUDE REGEX "/MacMain")
list(FILTER OLX_SRCS EXCLUDE REGEX "/MacHelpers")

# Vendored libraries (HAWKNL_BUILTIN, LIBZIP_BUILTIN, LIBLUA_BUILTIN).
file(GLOB OLX_HAWKNL_SRCS
    "${OLX_ROOT}/libs/hawknl/src/crc.c"
    "${OLX_ROOT}/libs/hawknl/src/errorstr.c"
    "${OLX_ROOT}/libs/hawknl/src/nl.c"
    "${OLX_ROOT}/libs/hawknl/src/sock.c"
    "${OLX_ROOT}/libs/hawknl/src/group.c"
    "${OLX_ROOT}/libs/hawknl/src/loopback.c"
    "${OLX_ROOT}/libs/hawknl/src/err.c"
    "${OLX_ROOT}/libs/hawknl/src/thread.c"
    "${OLX_ROOT}/libs/hawknl/src/mutex.c"
    "${OLX_ROOT}/libs/hawknl/src/condition.c"
    "${OLX_ROOT}/libs/hawknl/src/nltime.c")

file(GLOB OLX_LIBZIP_SRCS "${OLX_ROOT}/libs/libzip/*.c")
file(GLOB OLX_LUA_SRCS    "${OLX_ROOT}/libs/lua/*.c")

add_library(main SHARED
    ${OLX_SRCS}
    ${OLX_HAWKNL_SRCS}
    ${OLX_LIBZIP_SRCS}
    ${OLX_LUA_SRCS}
    "${CMAKE_CURRENT_SOURCE_DIR}/alut_stub.c")

# Generate Version_generated.h (#define LX_VERSION) from get_version_full.sh and
# put it ahead of ${OLX_ROOT}/include so it wins over the empty stub there.
# Without this Version.cpp has no LX_VERSION and the build fails (#error).
include("${OLX_ROOT}/CMakeOlxVersion.cmake")
olx_generate_version_header("${OLX_ROOT}" OLX_VERSION_INCLUDE_DIR)
target_include_directories(main BEFORE PRIVATE "${OLX_VERSION_INCLUDE_DIR}")

target_include_directories(main PRIVATE
    "${OLX_ROOT}/include"
    "${OLX_ROOT}/src"
    "${OLX_ROOT}/libs/pstreams"
    "${OLX_ROOT}/libs/hawknl/include"
    "${OLX_ROOT}/libs/libzip"
    "${OLX_ROOT}/libs/lua"
    "${OLX_ROOT}/libs/boost_process"
    "${DEPS_DIR}/boost-hdr"
    # libgd's gd.h lives in its source dir; we link against gd_static
    # but CMake doesn't propagate include paths for it from libgd's
    # install target (since we never install).
    "${DEPS_DIR}/libgd/src"
    "${CMAKE_BINARY_DIR}/libgd"
    "${CMAKE_BINARY_DIR}/libgd/src"
    # Stub AL/alut.h that pairs with alut_stub.c.
    "${CMAKE_CURRENT_SOURCE_DIR}/include")

target_compile_definitions(main PRIVATE
    NBREAKPAD
    HAVE_BOOST
    SYSTEM_DATA_DIR="/data/data/openlierox.net/files")

# DEDICATED_ONLY is checked with `#ifdef`, not by value; defining it to
# 0 still flips the bDedicated default to true, so just leave the macro
# undefined for the client build.

target_link_libraries(main PRIVATE
    SDL2::SDL2
    SDL2_image::SDL2_image
    SDL2_mixer::SDL2_mixer
    LibXml2::LibXml2
    OpenAL::OpenAL
    ogg
    vorbis
    vorbisfile
    gd_static
    libcurl_static
    yaml-cpp
    log
    android
    GLESv1_CM
    OpenSLES
    z
    dl
    m)
