# OpenLieroX WebAssembly build.
#
# Driven by build-wasm.sh, which sources emsdk_env.sh and runs us under
# emcmake. Produces openlierox.{html,js,wasm,data} in the build dir.
#
# Strategy: lean on Emscripten ports for SDL2 family + image/audio
# codecs + zlib, build libxml2/libgd/libcurl from source as
# subprojects, ship a tiny alut shim, link OpenLieroX as a single wasm
# module.

cmake_minimum_required(VERSION 3.22)
project(openlierox_wasm C CXX)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This build must be configured with emcmake.")
endif()

if(NOT DEFINED OLX_ROOT)
    message(FATAL_ERROR "OLX_ROOT must be set (path to OpenLieroX repo root).")
endif()

set(WASM_DIR  "${CMAKE_CURRENT_SOURCE_DIR}")
set(DEPS_DIR  "${WASM_DIR}/deps")

# Our stub Find*.cmake shims live here. Prepend so they win over CMake
# stock modules when subprojects (libxml2, libgd, libcurl, libpng) call
# find_package(ZLIB|PNG|JPEG).
list(PREPEND CMAKE_MODULE_PATH "${WASM_DIR}/cmake")
# libgd resets CMAKE_MODULE_PATH inside its own CMakeLists, so drop a
# copy of the shims into its module dir too.
file(COPY "${WASM_DIR}/cmake/FindZLIB.cmake"
          "${WASM_DIR}/cmake/FindPNG.cmake"
          "${WASM_DIR}/cmake/FindJPEG.cmake"
     DESTINATION "${DEPS_DIR}/libgd/cmake/modules")

# ---------------------------------------------------------------------
# Global flags
# ---------------------------------------------------------------------
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Emscripten port flags shared across compile + link. SDL2 headers come
# from `-sUSE_SDL=2`; without it the compiler picks up its (older) bundled
# SDL2 instead, which mismatches at link time.
set(EMSCRIPTEN_PORTS
    "-sUSE_SDL=2"
    "-sUSE_SDL_IMAGE=2"
    "-sSDL2_IMAGE_FORMATS=[\"bmp\",\"gif\",\"jpg\",\"png\",\"tga\"]"
    "-sUSE_LIBPNG=1"
    "-sUSE_LIBJPEG=1"
    "-sUSE_OGG=1"
    "-sUSE_VORBIS=1"
    "-sUSE_ZLIB=1"
    # SDL2_mixer is not used by OpenLieroX (it uses OpenAL directly).
    # The bundled SDL2_mixer port also lacks an -mt variant in this
    # Emscripten version, so pulling it in breaks --shared-memory.
    #
    # Need real pthreads for OLX's heavily threaded game loop; without
    # them SDL_CreateThread is a stub. Both compile and link must agree
    # on -pthread.
    "-pthread")

add_compile_options(${EMSCRIPTEN_PORTS}
    -Wno-deprecated-declarations
    -Wno-narrowing
    -Wno-deprecated-non-prototype
    -Wno-implicit-function-declaration
    -U_FORTIFY_SOURCE)
add_link_options(${EMSCRIPTEN_PORTS})

# Pre-load our stub Find modules so the imported targets exist before
# any subproject's find_package() call.
include(FindZLIB)
include(FindPNG)
include(FindJPEG)

# ---------------------------------------------------------------------
# libxml2 (no Python, no programs, no http/ftp, with zlib).
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
add_subdirectory("${DEPS_DIR}/libxml2" "${CMAKE_BINARY_DIR}/libxml2" EXCLUDE_FROM_ALL)
unset(BUILD_SHARED_LIBS CACHE)

# ---------------------------------------------------------------------
# libgd. Depends on libpng + libjpeg + zlib (provided via the imported
# targets above). Skip CPack/install/tests/freetype/etc.
# libgd's CMakeLists references "${CMAKE_SOURCE_DIR}/COPYING" for CPack
# even when CPack is unused — point it at our stub via configure_file
# so the path always resolves.
# ---------------------------------------------------------------------
# libgd's CPack module reads ${CMAKE_SOURCE_DIR}/COPYING — that's our
# top-level source dir. Ship a stub COPYING alongside this
# CMakeLists.txt so the configure step doesn't error out (we never
# invoke cpack).

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
add_subdirectory("${DEPS_DIR}/libgd" "${CMAKE_BINARY_DIR}/libgd" EXCLUDE_FROM_ALL)
unset(BUILD_SHARED_LIBS CACHE)

# ---------------------------------------------------------------------
# libcurl — HTTP only. The browser fetch backend isn't applicable; we
# build the regular socket transport on top of Emscripten's POSIX
# sockets (which become WebSockets at runtime). Network features
# beyond HTTP GET aren't expected to work in browser anyway.
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
set(CURL_USE_MBEDTLS OFF CACHE BOOL "")
set(CURL_DISABLE_LDAP ON CACHE BOOL "")
set(CURL_DISABLE_LDAPS ON CACHE BOOL "")
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
set(HTTP_ONLY ON CACHE BOOL "")
set(USE_LIBIDN2 OFF CACHE BOOL "")
set(USE_NGHTTP2 OFF CACHE BOOL "")
set(CURL_ZLIB ON CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/curl" "${CMAKE_BINARY_DIR}/curl" EXCLUDE_FROM_ALL)
unset(BUILD_SHARED_LIBS CACHE)

# ---------------------------------------------------------------------
# yaml-cpp — 0.59's TouchControls parses share/gamedir/touchscreen/*.yaml
# at runtime. Built static from source (no Emscripten port exists), same
# as the Android sibling port.
# ---------------------------------------------------------------------
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "")
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "")
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "")
set(YAML_CPP_INSTALL OFF CACHE BOOL "")
set(YAML_BUILD_SHARED_LIBS OFF CACHE BOOL "")
add_subdirectory("${DEPS_DIR}/yaml-cpp" "${CMAKE_BINARY_DIR}/yaml-cpp" EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------
# OpenLieroX itself
# ---------------------------------------------------------------------
file(GLOB_RECURSE OLX_SRCS
    "${OLX_ROOT}/src/*.cpp"
    "${OLX_ROOT}/src/*.c")

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

add_executable(openlierox
    ${OLX_SRCS}
    ${OLX_HAWKNL_SRCS}
    ${OLX_LIBZIP_SRCS}
    ${OLX_LUA_SRCS}
    "${WASM_DIR}/jni/alut_stub.c")

set_target_properties(openlierox PROPERTIES SUFFIX ".html")

# Generate Version_generated.h (#define LX_VERSION) from get_version_full.sh and
# put it ahead of ${OLX_ROOT}/include so it wins over the empty stub there.
# Without this Version.cpp has no LX_VERSION and the build fails (#error).
include("${OLX_ROOT}/CMakeOlxVersion.cmake")
olx_generate_version_header("${OLX_ROOT}" OLX_VERSION_INCLUDE_DIR)
target_include_directories(openlierox BEFORE PRIVATE "${OLX_VERSION_INCLUDE_DIR}")

target_include_directories(openlierox PRIVATE
    "${OLX_ROOT}/include"
    "${OLX_ROOT}/src"
    "${OLX_ROOT}/libs/pstreams"
    "${OLX_ROOT}/libs/hawknl/include"
    "${OLX_ROOT}/libs/libzip"
    "${OLX_ROOT}/libs/lua"
    "${OLX_ROOT}/libs/boost_process"
    "${DEPS_DIR}/boost-hdr"
    "${DEPS_DIR}/libgd/src"
    "${CMAKE_BINARY_DIR}/libgd"
    "${CMAKE_BINARY_DIR}/libgd/src"
    "${WASM_DIR}/jni/include")

# Resolve the build's git revision so it can be logged at startup.
# Best-effort: works on a normal checkout, falls back to "unknown" for
# tarballs / shallow archives without a git history.
# Used by redistributed builds: when a user reports an error we can map
# it to the exact commit. We deliberately don't tag dirty worktrees —
# only released builds should ship, and those are clean.
execute_process(
    COMMAND git -C "${OLX_ROOT}" rev-parse HEAD
    OUTPUT_VARIABLE OLX_GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE OLX_GIT_RC)
if(NOT OLX_GIT_RC EQUAL 0 OR OLX_GIT_HASH STREQUAL "")
    set(OLX_GIT_HASH "unknown")
endif()
message(STATUS "OLX git hash: ${OLX_GIT_HASH}")

target_compile_definitions(openlierox PRIVATE
    NBREAKPAD
    HAVE_BOOST
    OLX_GIT_HASH="${OLX_GIT_HASH}"
    SYSTEM_DATA_DIR="/")

target_link_libraries(openlierox PRIVATE
    LibXml2::LibXml2
    gd_static
    libcurl_static
    yaml-cpp)

# Emscripten-specific link flags.
#   - INITIAL_MEMORY: 256 MB up front; OLX easily allocates that loading
#     mods, and growing memory mid-game freezes WebGL.
#   - ALLOW_MEMORY_GROWTH: still allow grow if needed.
#   - ASYNCIFY: lets blocking SDL_WaitEvent / SDL_Delay yield to the
#     browser without rewriting the main loop. Pays a code-size +
#     speed cost but keeps the source diff tiny.
#   - openal: built-in JS implementation, satisfies the AL/al.h symbols
#     used by sound_sample_openal / sfxdriver_openal.
#   - --preload-file: bake share/gamedir into a .data sidecar that
#     SDL2/stdio can open at "/gamedir/...".
#   - --shell-file: drop the default Emscripten boilerplate; use ours.
target_link_options(openlierox PRIVATE
    "-lopenal"
    # Run main() (and therefore every blocking SDL_WaitEvent /
    # threadPool->wait) on a Web Worker. The browser's main thread
    # forwards canvas/keyboard/mouse events automatically. Requires
    # COOP/COEP — see serve.py.
    "-sPROXY_TO_PTHREAD=1"
    # The main thread spawns several pthreads; pre-allocate a pool so
    # we don't pay the worker-creation latency mid-game.
    "-sPTHREAD_POOL_SIZE=12"
    # OLX's call chains (especially the gusanos blitters and the
    # ConfigHandler / ScriptableVars setup) easily blow past
    # Emscripten's 64 KB default stack per thread. 8 MB main stack
    # and 4 MB per pthread mirror what the desktop build runs with
    # and remove a whole class of "stack overflow → memory access
    # out of bounds" crashes.
    "-sSTACK_SIZE=8388608"
    "-sDEFAULT_PTHREAD_STACK_SIZE=4194304"
    "-sSTACK_OVERFLOW_CHECK=2"
    "-sINITIAL_MEMORY=536870912"
    "-sALLOW_MEMORY_GROWTH=1"
    "-sASSERTIONS=1"
    "-sEXIT_RUNTIME=0"
    "-sFORCE_FILESYSTEM=1"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','callMain']"
    "--preload-file" "${OLX_PRELOAD_DIR}/gamedir@/gamedir"
    "--shell-file" "${WASM_DIR}/shell/shell.html")

# Generate the .html alongside .js + .wasm + .data.
set_target_properties(openlierox PROPERTIES OUTPUT_NAME "openlierox")
