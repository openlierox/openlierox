# Stub FindJPEG.cmake — Emscripten provides libjpeg via -sUSE_LIBJPEG=1.

if(NOT TARGET JPEG::JPEG)
    add_library(JPEG::JPEG INTERFACE IMPORTED)
    target_compile_options(JPEG::JPEG INTERFACE "-sUSE_LIBJPEG=1")
    target_link_options(JPEG::JPEG INTERFACE "-sUSE_LIBJPEG=1")
endif()

set(JPEG_FOUND       TRUE)
# Empty — actual jpeg comes from -sUSE_LIBJPEG=1 in our global link
# options. See note in FindZLIB.cmake.
set(JPEG_LIBRARY     "")
set(JPEG_LIBRARIES   "")
set(JPEG_INCLUDE_DIR "${CMAKE_BINARY_DIR}")
set(JPEG_INCLUDE_DIRS "${CMAKE_BINARY_DIR}")
set(JPEG_VERSION     "8.0.0")
