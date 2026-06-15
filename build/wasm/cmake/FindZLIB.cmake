# Stub FindZLIB.cmake for the OpenLieroX Wasm build.
#
# The actual symbols come from -sUSE_ZLIB=1 added globally in our top-
# level CMakeLists. Subprojects (libxml2, libpng, libcurl, libgd) call
# find_package(ZLIB) and expect:
#   - ZLIB::ZLIB imported target (libxml2/libgd link via that)
#   - ZLIB_LIBRARIES populated as a plain string (curl validates that
#     the entries don't look like target names like "::")
# So we expose both: the imported target carries the flags for cmake
# consumers, and ZLIB_LIBRARIES is the plain link spec "z" — which
# emcc resolves through its zlib port at link time.

if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB INTERFACE IMPORTED)
    target_compile_options(ZLIB::ZLIB INTERFACE "-sUSE_ZLIB=1")
    target_link_options(ZLIB::ZLIB INTERFACE "-sUSE_ZLIB=1")
endif()

set(ZLIB_FOUND        TRUE)
set(ZLIB_INCLUDE_DIR  "${CMAKE_BINARY_DIR}")
set(ZLIB_INCLUDE_DIRS "${CMAKE_BINARY_DIR}")
# Empty so subprojects don't add `-lz` directly — the actual zlib
# symbols come from `-sUSE_ZLIB=1` (already in our global link
# options), which under `-pthread` resolves to libz-mt.a. Adding `-lz`
# would also pull in the non-mt libz.a, breaking --shared-memory.
set(ZLIB_LIBRARY      "")
set(ZLIB_LIBRARIES    "")
set(ZLIB_VERSION_STRING "1.3.1")
set(ZLIB_VERSION       "1.3.1")
