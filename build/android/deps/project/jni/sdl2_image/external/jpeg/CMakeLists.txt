cmake_minimum_required(VERSION 3.1...3.5)

# extract version out of configure.ac
file(READ "configure.ac" CONFIGURE_AC)
string(REGEX MATCH "AC_INIT\\(\\[libjpeg\\], \\[([0-9.]+)\\]\\)" ver_found "${CONFIGURE_AC}")
if(NOT ver_found)
   message(FATAL_ERROR "Could not extract version from configure.ac")
endif()
set(LIBJPEG_VERSION "${CMAKE_MATCH_1}")

project(jpeg LANGUAGES C VERSION ${LIBJPEG_VERSION})

option(BUILD_SHARED_LIBS "Build shared jpeg library" ON)

include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(GNUInstallDirs)

check_symbol_exists(setmode io.h USE_SETMODE)
check_c_source_compiles("
  int x[(char)0xff]; /* x[-1] if signed */
  int main(void) {return 0;}" CHAR_IS_UNSIGNED
)
check_c_source_compiles("
  int x[((signed char)0xff) >> 1]; /* x[-1] if signed */
  int main(void) {return 0;}" RIGHT_SHIFT_IS_UNSIGNED
)

set(jpeg_headers
    jerror.h
    jmorecfg.h
    jpeglib.h
)
set(jpeg_sources
    jaricom.c jcapimin.c jcapistd.c jcarith.c jccoefct.c jccolor.c
    jcdctmgr.c jchuff.c jcinit.c jcmainct.c jcmarker.c jcmaster.c
    jcomapi.c jcparam.c jcprepct.c jcsample.c jctrans.c jdapimin.c
    jdapistd.c jdarith.c jdatadst.c jdatasrc.c jdcoefct.c jdcolor.c
    jddctmgr.c jdhuff.c jdinput.c jdmainct.c jdmarker.c jdmaster.c
    jdmerge.c jdpostct.c jdsample.c jdtrans.c jerror.c jfdctflt.c
    jfdctfst.c jfdctint.c jidctflt.c jquant1.c
    jquant2.c jutils.c jmemmgr.c
)

if(BUILD_SHARED_LIBS AND WIN32)
  set(CMAKE_SHARED_LIBRARY_NAME_WITH_VERSION 1)
endif()

add_library(jpeg ${jpeg_sources})
add_library(JPEG::JPEG ALIAS jpeg)

if(ANDROID)
  target_sources(jpeg PRIVATE jmem-android.c)
  # Android.mk defines this:
  target_compile_definitions(jpeg PRIVATE AVOID_TABLES)
else()
  target_sources(jpeg PRIVATE jmemansi.c)
endif()
target_sources(jpeg PRIVATE jidctint.c jidctfst.c)
# FIXME : include asm for ARM
# target_sources(jpeg PRIVATE jidctint.c jidctfst.S)

set_property(TARGET jpeg PROPERTY DEFINE_SYMBOL JPEG_SHARED)
set_target_properties(jpeg PROPERTIES
  SOVERSION "${PROJECT_VERSION_MAJOR}"
  VERSION "${PROJECT_VERSION}"
)
if(BUILD_SHARED_LIBS AND MSVC)
  set_target_properties(jpeg PROPERTIES
    PREFIX "lib"
  )
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/jconfig.h.in ${CMAKE_CURRENT_BINARY_DIR}/jconfig.h)
list(APPEND jpeg_headers ${CMAKE_CURRENT_BINARY_DIR}/jconfig.h)
target_include_directories(jpeg PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

install(TARGETS jpeg
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(FILES ${jpeg_headers}
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)
