cmake_minimum_required(VERSION 3.10)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/configure.ac" _configure_ac)
string(REGEX MATCH "V_LIB_CURRENT=([0-9]+)" tremor_major "${_configure_ac}")
if(NOT tremor_major)
    message(FATAL_ERROR "Failed to extract major tremor version")
endif()
set(tremor_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "V_LIB_REVISION=([0-9]+)" tremor_minor "${_configure_ac}")
if(NOT tremor_minor)
    message(FATAL_ERROR "Failed to extract minor tremor version")
endif()
set(tremor_minor "${CMAKE_MATCH_1}")
string(REGEX MATCH "V_LIB_AGE=([0-9]+)" tremor_patch "${_configure_ac}")
if(NOT tremor_patch)
    message(FATAL_ERROR "Failed to extract patch tremor version")
endif()
set(tremor_patch "${CMAKE_MATCH_1}")
project(tremor VERSION "${tremor_major}.${tremor_minor}.${tremor_patch}" LANGUAGES C)
if(NOT TARGET Ogg::ogg)
    find_package(Ogg REQUIRED)
endif()

include(CheckCSourceCompiles)
include(CheckSymbolExists)
include(CheckIncludeFile)

check_include_file(alloca.h HAVE_ALLOCA_H)
if(HAVE_ALLOCA_H)
    add_definitions(-DHAVE_ALLOCA_H)
endif()

if(WIN32)
    check_symbol_exists(_alloca "malloc.h" HAVE_ALLOCA)
    add_definitions(-DHAVE_ALLOCA)
else()
    check_symbol_exists(alloca "alloca.h" HAVE_ALLOCA)
    check_symbol_exists(alloca "stdlib.h" HAVE_ALLOCA)
    if(HAVE_ALLOCA)
        add_definitions(-DHAVE_ALLOCA)
    endif()
endif()

check_c_source_compiles(
    "static int x; int main() { char a[++x]; a[sizeof a - 1] = 0; int N; return a[0]; }"
    HAVE_VAR_ARRAYS
)
if(HAVE_VAR_ARRAYS)
    add_definitions(-DVAR_ARRAYS)
endif()

add_library(vorbisidec
    mdct.c
    block.c
    window.c
    synthesis.c
    info.c
    floor1.c
    floor0.c
    vorbisfile.c
    res012.c
    mapping0.c
    registry.c
    codebook.c
    sharedbook.c
    codebook.h
    misc.h
    mdct_lookup.h
    os.h
    mdct.h
    block.h
    ivorbisfile.h
    lsp_lookup.h
    registry.h
    window.h
    window_lookup.h
    codec_internal.h
    backends.h
    asm_arm.h
    ivorbiscodec.h
)
target_link_libraries(vorbisidec PRIVATE Ogg::ogg)
set_target_properties(vorbisidec PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS TRUE
    SOVERSION "${PROJECT_VERSION_MAJOR}"
    VERSION "${PROJECT_VERSION}"
)
