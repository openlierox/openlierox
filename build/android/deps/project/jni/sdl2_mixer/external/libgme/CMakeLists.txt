# CMake project definition file.
cmake_minimum_required(VERSION 3.3...3.10)
project(libgme)

include(CMakePushCheckState)
include(CheckCXXSourceCompiles)
include(TestBigEndian)
include(GNUInstallDirs)
include(CTest)

file(READ "gme/gme.h" GME_GME_H)
string(REGEX MATCH "/\\* Game_Music_Emu ([0-9]+)\\.([0-9]+).([0-9]+) \\*/" RE_GME_VERSION "${GME_GME_H}")
if(NOT RE_GME_VERSION)
    message(FATAL_ERROR "Unable to extract gme version from gme/gme.h")
endif()
set(GME_VERSION_MAJOR "${CMAKE_MATCH_1}")
set(GME_VERSION_MINOR "${CMAKE_MATCH_2}")
set(GME_VERSION_PATCH "${CMAKE_MATCH_3}")
set(GME_VERSION "${GME_VERSION_MAJOR}.${GME_VERSION_MINOR}.${GME_VERSION_PATCH}")

# Default emulators to build (all of them! ;)
option(USE_GME_AY "Enable support for Spectrum ZX music emulation" ON)
option(USE_GME_GBS "Enable support for Game Boy music emulation" ON)
option(USE_GME_GYM "Enable Sega MegaDrive/Genesis music emulation" ON)
option(USE_GME_HES "Enable PC Engine/TurboGrafx-16 music emulation" ON)
option(USE_GME_KSS "Enable MSX or other Z80 systems music emulation" ON)
option(USE_GME_NSF "Enable NES NSF music emulation" ON)
option(USE_GME_NSFE "Enable NES NSFE and NSF music emulation" ON)
option(USE_GME_SAP "Enable Atari SAP music emulation" ON)
option(USE_GME_SPC "Enable SNES SPC music emulation" ON)
option(USE_GME_VGM "Enable Sega VGM/VGZ music emulation" ON)

option(GME_SPC_ISOLATED_ECHO_BUFFER "Enable isolated echo buffer on SPC emulator to allow correct playing of \"dodgy\" SPC files made for various ROM hacks ran on ZSNES" OFF)
option(GME_ZLIB "Enable GME to support compressed sound formats" ON)

set(GME_YM2612_EMU "Nuked" CACHE STRING "Which YM2612 emulator to use: \"Nuked\" (LGPLv2.1+), \"MAME\" (GPLv2+), or \"GENS\" (LGPLv2.1+)")
set(GME_YM2612_EMU_CHOICES "Nuked;MAME;GENS")
set_property(CACHE GME_YM2612_EMU PROPERTY STRINGS "${GME_YM2612_EMU_CHOICES}")

if(USE_GME_NSFE AND NOT USE_GME_NSF)
    message(STATUS "NSFE support requires NSF, enabling NSF support.")
    set(USE_GME_NSF 1 CACHE BOOL "Enable NES NSF music emulation" FORCE)
endif()

option(BUILD_SHARED_LIBS "Build shared library (set to OFF for static library)" ON)
option(BUILD_FRAMEWORK "Build framework instead of dylib on macOS" OFF)

option(ENABLE_UBSAN "Enable Undefined Behavior Sanitizer error-checking" OFF)


set (CMAKE_CXX_STANDARD  11)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

test_big_endian(WORDS_BIGENDIAN)

if(MSVC)
    string(REGEX REPLACE "/W[3|4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
    string(REGEX REPLACE "/W[3|4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options(/W2)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -W -Wextra)
endif()

# Check for GCC/Clang "visibility" support.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND ENABLE_UBSAN)
    # GCC needs -static-libubsan
    if(NOT BUILD_SHARED_LIBS AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -static-libubsan")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
    endif()
endif()

# Shared library defined here
add_subdirectory(gme)

# EXCLUDE_FROM_ALL adds build rules but keeps it out of default build
add_subdirectory(player EXCLUDE_FROM_ALL)
add_subdirectory(demo EXCLUDE_FROM_ALL)
