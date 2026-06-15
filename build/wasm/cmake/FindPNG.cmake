# Stub FindPNG.cmake — Emscripten provides libpng via -sUSE_LIBPNG=1.

if(NOT TARGET PNG::PNG)
    add_library(PNG::PNG INTERFACE IMPORTED)
    target_compile_options(PNG::PNG INTERFACE "-sUSE_LIBPNG=1")
    target_link_options(PNG::PNG INTERFACE "-sUSE_LIBPNG=1")
endif()

set(PNG_FOUND       TRUE)
# Empty — actual png comes from -sUSE_LIBPNG=1 in our global link
# options. See note in FindZLIB.cmake.
set(PNG_LIBRARY     "")
set(PNG_LIBRARIES   "")
set(PNG_INCLUDE_DIR "${CMAKE_BINARY_DIR}")
set(PNG_INCLUDE_DIRS "${CMAKE_BINARY_DIR}")
set(PNG_PNG_INCLUDE_DIR "${CMAKE_BINARY_DIR}")
set(PNG_VERSION_STRING "1.6.43")
