# CMake common file for OpenLieroX
# sets up the source lists and vars

cmake_minimum_required(VERSION 2.4)
IF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.4)
	# These CMP000{3,5,11} OLD settings were needed for the 2.4->2.6 transition
	# but the OLD behaviors were removed in CMake 4.0. Only apply them on
	# CMake < 3.0 where they still exist.
	if(COMMAND CMAKE_POLICY AND CMAKE_VERSION VERSION_LESS 3.0)
		cmake_policy(VERSION 2.4)
		cmake_policy(SET CMP0005 OLD)
		cmake_policy(SET CMP0003 OLD)
		if(${CMAKE_MAJOR_VERSION} GREATER 2 OR ${CMAKE_MINOR_VERSION} GREATER 6 OR ${CMAKE_PATCH_VERSION} GREATER 2)
			cmake_policy(SET CMP0011 OLD)
		endif()
	endif()
	include(${OLXROOTDIR}/PCHSupport_26.cmake)
ENDIF (${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 2.4)


SET(SYSTEM_DATA_DIR "/usr/share/games" CACHE STRING "system data dir")
OPTION(DEBUG "enable debug build" Yes)
OPTION(DEDICATED_ONLY "dedicated_only - without gfx and sound" No)
OPTION(G15 "G15 support" No)
OPTION(X11 "X11 clipboard / notify" Yes)
OPTION(HAWKNL_BUILTIN "HawkNL builtin support" Yes)
OPTION(LIBZIP_BUILTIN "LibZIP builtin support" No)
OPTION(LIBLUA_BUILTIN "LibLua builtin support" Yes)
OPTION(STLPORT "STLport support" No)
OPTION(GCOREDUMPER "Google Coredumper support" No)
OPTION(PCH "Precompiled header (CMake 2.6 only)" No)
OPTION(ADVASSERT "Advanced assert" No)
OPTION(PYTHON_DED_EMBEDDED "Python embedded in dedicated server"  No)
OPTION(OPTIM_PROJECTILES "Enable optimisations for projectiles" Yes)
OPTION(MEMSTATS "Enable memory statistics and debugging" No)
OPTION(HASBFD "Use libbfd for extended stack traces" Yes)
OPTION(BREAKPAD "Google Breakpad support" No)
OPTION(LINENOISE "builtin Linenose support (readline/libedit replacement)" Yes)
OPTION(DISABLE_JOYSTICK "Disable joystick support" No)
OPTION(BOOST_LINK_STATIC "Link boost-libs statically" No)
OPTION(MINGW_CROSS_COMPILE "Cross-compile Windows .EXE using i586-mingw32msvc-cc compiler" No)

IF (DEBUG)
	SET(CMAKE_BUILD_TYPE Debug)
ELSE (DEBUG)
	SET(CMAKE_BUILD_TYPE Release)
ENDIF (DEBUG)

IF (DEDICATED_ONLY)
	SET(X11 No)
	SET(WITH_G15 No)
ENDIF (DEDICATED_ONLY)

# Platform specific things can be put here
# Compilers and other specific variables can be found here:
# http://www.cmake.org/Wiki/CMake_Useful_Variables
IF(UNIX)
	IF(APPLE)
		SET(HAWKNL_BUILTIN ON)
		SET(LIBZIP_BUILTIN ON)
		SET(LIBLUA_BUILTIN ON)
		SET(X11 OFF)
		#SET(BOOST_LINK_STATIC ON)
		# libbfd isn't available by default on macOS.
		SET(HASBFD OFF)
		# Use the plain int main() from src/main.cpp — the legacy
		# src/MacMain.m SDL1 entry point is not part of the cmake build.
		ADD_DEFINITIONS(-DOLX_USE_STD_MAIN)
	ENDIF(APPLE)

	IF (CYGWIN)
		SET(WITH_G15 OFF) # Linux library as of now
	ELSE (CYGWIN)
	ENDIF (CYGWIN)
	IF(CMAKE_C_COMPILER MATCHES i586-mingw32msvc-cc)
		SET(MINGW_CROSS_COMPILE ON)
	ENDIF(CMAKE_C_COMPILER MATCHES i586-mingw32msvc-cc)
	IF (MINGW_CROSS_COMPILE)
		SET(G15 OFF)
		SET(HAWKNL_BUILTIN ON) # We already have prebuilt HawkNL library
		SET(LIBZIP_BUILTIN ON)
		SET(LIBLUA_BUILTIN ON)
		SET(X11 OFF)
		SET(BOOST_LINK_STATIC ON)
	ENDIF (MINGW_CROSS_COMPILE)
ELSE(UNIX)
	IF(WIN32)
		SET(G15 OFF)
		IF(MSVC)
			SET(HAWKNL_BUILTIN OFF) # We already have prebuilt HawkNL library
		ENDIF()
		SET(X11 OFF)
	ELSE(WIN32)
	ENDIF(WIN32)
ENDIF(UNIX)


MESSAGE( "SYSTEM_DATA_DIR = ${SYSTEM_DATA_DIR}" )
MESSAGE( "DEBUG = ${DEBUG}" )
MESSAGE( "DEDICATED_ONLY = ${DEDICATED_ONLY}" )
MESSAGE( "G15 = ${G15}" )
MESSAGE( "X11 = ${X11}" )
MESSAGE( "HAWKNL_BUILTIN = ${HAWKNL_BUILTIN}" )
MESSAGE( "LIBZIP_BUILTIN = ${LIBZIP_BUILTIN}" )
MESSAGE( "LIBLUA_BUILTIN = ${LIBLUA_BUILTIN}" )
MESSAGE( "STLPORT = ${STLPORT}" )
MESSAGE( "GCOREDUMPER = ${GCOREDUMPER}" )
MESSAGE( "HASBFD = ${HASBFD}" )
MESSAGE( "BREAKPAD = ${BREAKPAD}" )
MESSAGE( "LINENOISE = ${LINENOISE}" )
MESSAGE( "CMAKE_C_COMPILER = ${CMAKE_C_COMPILER}" )
MESSAGE( "CMAKE_C_FLAGS = ${CMAKE_C_FLAGS}" )
MESSAGE( "CMAKE_CXX_COMPILER = ${CMAKE_CXX_COMPILER}" )
MESSAGE( "CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}" )
# commented out because only devs need these options anyway
#MESSAGE( "PCH = ${PCH} (Precompiled header, CMake 2.6 only)" )
#MESSAGE( "ADVASSERT = ${ADVASSERT}" )
#MESSAGE( "PYTHON_DED_EMBEDDED = ${PYTHON_DED_EMBEDDED}" )
MESSAGE( "MINGW_CROSS_COMPILE = ${MINGW_CROSS_COMPILE}" )


PROJECT(openlierox)

EXEC_PROGRAM(mkdir ARGS -p bin OUTPUT_VARIABLE DUMMY)

# main includes
INCLUDE_DIRECTORIES(${OLXROOTDIR}/optional-includes/generated)
IF(WIN32 AND NOT MSVC)
	# On Windows NTFS is case-insensitive, so adding include/ as a
	# regular -I dir causes project headers like Process.h and
	# CChannel.h to shadow Windows system headers (process.h,
	# cchannel.h). Use -iquote so the dir is only searched for
	# quote-form includes, which keeps project "#include \"Foo.h\""
	# working while letting system <process.h> resolve correctly.
	add_compile_options(-iquote "${OLXROOTDIR}/include")
ELSE()
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/include)
ENDIF()
INCLUDE_DIRECTORIES(${OLXROOTDIR}/src)
INCLUDE_DIRECTORIES(${OLXROOTDIR}/libs/pstreams)

IF(ADVASSERT)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/optional-includes/advanced-assert)
ENDIF(ADVASSERT)

# TODO: don't hardcode path here
IF(NOT WIN32 AND NOT MINGW_CROSS_COMPILE)
	INCLUDE_DIRECTORIES(/usr/include/libxml2)
	INCLUDE_DIRECTORIES(/usr/local/include/libxml2)
ENDIF(NOT WIN32 AND NOT MINGW_CROSS_COMPILE)


file(GLOB_RECURSE ALL_SRCS ${OLXROOTDIR}/src/*.c*)

# The legacy src/MacMain.m is SDL 1.x specific and only used by the
# old build/Xcode project. The cmake build uses SDL2 and provides its
# own main() via OLX_USE_STD_MAIN, plus a trimmed Cocoa helper file
# (src/MacHelpers.m) for clipboard and user-attention shims.
IF(APPLE)
	enable_language(OBJC)
	SET(ALL_SRCS ${OLXROOTDIR}/src/MacHelpers.m ${ALL_SRCS})
ENDIF(APPLE)

IF (BREAKPAD)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/libs/breakpad/src)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/optional-includes/breakpad)
	ADD_DEFINITIONS("'-DBP_LOGGING_INCLUDE=\"breakpad_logging.h\"'")

	IF(MINGW_CROSS_COMPILE OR WIN32)

	ELSE(MINGW_CROSS_COMPILE OR WIN32)
		EXEC_PROGRAM(python ARGS ${CMAKE_CURRENT_SOURCE_DIR}/libs/breakpad_getallsources.py OUTPUT_VARIABLE BREAKPAD_SRCS)
		SET(ALL_SRCS ${BREAKPAD_SRCS} ${ALL_SRCS})		
	ENDIF(MINGW_CROSS_COMPILE OR WIN32)
ELSE (BREAKPAD)
	ADD_DEFINITIONS(-DNBREAKPAD)
ENDIF (BREAKPAD)

IF (LINENOISE AND NOT WIN32)
	ADD_DEFINITIONS(-DHAVE_LINENOISE)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/libs/linenoise)
	SET(ALL_SRCS ${OLXROOTDIR}/libs/linenoise/linenoise.cpp ${ALL_SRCS})
ENDIF ()

IF (DISABLE_JOYSTICK)
	ADD_DEFINITIONS(-DDISABLE_JOYSTICK)
ENDIF (DISABLE_JOYSTICK)

IF (GCOREDUMPER)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/libs/coredumper/src)
	ADD_DEFINITIONS(-DGCOREDUMPER)
	AUX_SOURCE_DIRECTORY(${OLXROOTDIR}/libs/coredumper/src COREDUMPER_SRCS)
	SET(ALL_SRCS ${ALL_SRCS} ${COREDUMPER_SRCS})
ENDIF (GCOREDUMPER)

IF (HAWKNL_BUILTIN)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/libs/hawknl/include)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/crc.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/errorstr.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/nl.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/sock.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/group.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/loopback.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/err.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/thread.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/mutex.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/condition.c)
	SET(ALL_SRCS ${ALL_SRCS} ${OLXROOTDIR}/libs/hawknl/src/nltime.c)
ELSE (HAWKNL_BUILTIN)
	INCLUDE_DIRECTORIES(/usr/include/hawknl)
ENDIF (HAWKNL_BUILTIN)

IF (LIBZIP_BUILTIN)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/libs/libzip)
	AUX_SOURCE_DIRECTORY(${OLXROOTDIR}/libs/libzip LIBZIP_SRCS)
	SET(ALL_SRCS ${ALL_SRCS} ${LIBZIP_SRCS})
ENDIF (LIBZIP_BUILTIN)

IF (LIBLUA_BUILTIN)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/libs/lua)
	AUX_SOURCE_DIRECTORY(${OLXROOTDIR}/libs/lua LIBLUA_SRCS)
	SET(ALL_SRCS ${ALL_SRCS} ${LIBLUA_SRCS})
ELSE (LIBLUA_BUILTIN)
	# TODO: Make this even more dynamic, search for the directory somehow? Like you can search for linking options with pkg-config?
	#Search for lua5.1 first
	IF(EXISTS /usr/include/lua5.1)
		SET(LUA_SEARCHDIR /usr/include/lua5.1)
	ENDIF(EXISTS /usr/include/lua5.1)
	
	#On some systems, e.g. Fedora, it may be lua-5.1
	IF(NOT LUA_SEARCHDIR)
		IF(EXISTS /usr/include/lua-5.1)
			SET(LUA_SEARCHDIR /usr/include/lua-5.1)
		ENDIF(EXISTS /usr/include/lua-5.1)
	ENDIF(NOT LUA_SEARCHDIR)

	#If not found, try "lua", but give a warning
	IF(NOT LUA_SEARCHDIR)
		MESSAGE(WARNING "No Lua 5.1 header directory found. Trying default lua but it may not work. You may have to install Lua 5.1 development packages, or use the built-in library")
		IF(EXISTS /usr/include/lua)
			SET(LUA_SEARCHDIR /usr/include/lua)
		ENDIF(EXISTS /usr/include/lua)
	ENDIF(NOT LUA_SEARCHDIR)

	#Set the header path, or display a warning if it doesn't exist
	IF(LUA_SEARCHDIR)
		INCLUDE_DIRECTORIES(${LUA_SEARCHDIR})
	ELSE(LUA_SEARCHDIR)
		MESSAGE(WARNING "No Lua header directory found. Make sure that Lua 5.1 development packages are installed, or use the built-in library")
	ENDIF(LUA_SEARCHDIR)
ENDIF (LIBLUA_BUILTIN)

IF (STLPORT)
	INCLUDE_DIRECTORIES(/usr/include/stlport)
	ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64) # hm, don't know, at least it works for me (ppc32/amd32)
# debugging stuff for STLport
	ADD_DEFINITIONS(-D_STLP_DEBUG=1)
	ADD_DEFINITIONS(-D_STLP_DEBUG_LEVEL=_STLP_STANDARD_DBG_LEVEL)
	ADD_DEFINITIONS(-D_STLP_SHRED_BYTE=0xA3)
	ADD_DEFINITIONS(-D_STLP_DEBUG_UNINITIALIZED=1)
	ADD_DEFINITIONS(-D_STLP_DEBUG_ALLOC=1)
ENDIF (STLPORT)


IF(DEBUG)
	ADD_DEFINITIONS(-DDEBUG=1)
ENDIF(DEBUG)

IF(MEMSTATS)
	ADD_DEFINITIONS(-include ${OLXROOTDIR}/optional-includes/memdebug/memstats.h)
ENDIF(MEMSTATS)


# Generic defines
IF(WIN32)
	IF(MSVC)
		ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE -DHAVE_BOOST -DZLIB_WIN32_NODLL)
		SET(OPTIMIZE_COMPILER_FLAG /Ox /Ob2 /Oi /Ot /GL)
		IF(DEBUG)
			ADD_DEFINITIONS(-DUSE_DEFAULT_MSC_DELEAKER)
		ELSE(DEBUG)
			ADD_DEFINITIONS(${OPTIMIZE_COMPILER_FLAG})
		ENDIF(DEBUG)
	ELSE()
		# MinGW on Windows (MSYS2)
		ADD_DEFINITIONS(-DHAVE_BOOST -DZLIB_WIN32_NODLL)
		ADD_DEFINITIONS(-D_WIN32_WINNT=0x0601)
		SET(OPTIMIZE_COMPILER_FLAG -O3)
		# The legacy widget code has several "DWORD from pointer"
		# casts that trip GCC on 64-bit builds. Demote to warnings
		# until the widget API is ported to uintptr_t.
		add_compile_options(-fpermissive)
	ENDIF()
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/libs/hawknl/include
				${OLXROOTDIR}/libs/hawknl/src
				${OLXROOTDIR}/libs/libzip
				${OLXROOTDIR}/libs/boost_process)
ELSE(WIN32)
	ADD_DEFINITIONS(-Wall)
	# -std= belongs to CXX only — AppleClang rejects it for C sources.
	add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-std=c++0x>")

	EXEC_PROGRAM(sh ARGS ${CMAKE_CURRENT_SOURCE_DIR}/get_version.sh OUTPUT_VARIABLE OLXVER)
	string(REGEX REPLACE "[\r\n]" " " OLXVER "${OLXVER}")
	MESSAGE( "OLX_VERSION = ${OLXVER}" )

	IF(MINGW_CROSS_COMPILE)
		ADD_DEFINITIONS(-DHAVE_BOOST -DZLIB_WIN32_NODLL -DLIBXML_STATIC -DNONDLL -DCURL_STATICLIB -D_XBOX # _XBOX to link OpenAL statically
							-D_WIN32_WINNT=0x0500 -D_WIN32_WINDOWS=0x0500 -DWINVER=0x0500)
		INCLUDE_DIRECTORIES(
					${OLXROOTDIR}/build/mingw/include
					${OLXROOTDIR}/libs/hawknl/include
					${OLXROOTDIR}/libs/hawknl/src
					${OLXROOTDIR}/libs/libzip
					${OLXROOTDIR}/libs/lua
					${OLXROOTDIR}/libs/boost_process)
		# as long as we dont have breakpad, this doesnt make sense
		ADD_DEFINITIONS(-gdwarf-2 -g1) # By default GDB uses STABS and produces 300Mb exe - DWARF will produce 40Mb and no line numbers, -g2 will give 170Mb
	ELSE(MINGW_CROSS_COMPILE)
		ADD_DEFINITIONS("-pthread")
	ENDIF(MINGW_CROSS_COMPILE)

	SET(OPTIMIZE_COMPILER_FLAG -O3)
ENDIF(WIN32)

IF(OPTIM_PROJECTILES)
	#Always optimize these files - they make debug build unusable otherwise
	SET_SOURCE_FILES_PROPERTIES(	${OLXROOTDIR}/src/common/PhysicsLX56_Projectiles.cpp
						PROPERTIES COMPILE_FLAGS ${OPTIMIZE_COMPILER_FLAG})
ENDIF(OPTIM_PROJECTILES)

# SDL libs
IF(WIN32 AND MSVC)
ELSEIF(APPLE)
	# Modern macOS cmake build: rely on Homebrew-installed SDL2 and
	# friends via pkg-config. The old SDL 1.x Framework layout is kept
	# alive only by the build/Xcode project, not by cmake.
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2 REQUIRED sdl2)
	pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
	pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
	pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
	pkg_check_modules(LIBZIP REQUIRED libzip)
	pkg_check_modules(LIBGD REQUIRED gdlib)
	pkg_check_modules(VORBISFILE REQUIRED vorbisfile)
	pkg_check_modules(OPENAL REQUIRED openal)
	pkg_check_modules(FREEALUT REQUIRED freealut)
	INCLUDE_DIRECTORIES(
		${SDL2_INCLUDE_DIRS}
		${SDL2_IMAGE_INCLUDE_DIRS}
		${SDL2_MIXER_INCLUDE_DIRS}
		${LIBXML2_INCLUDE_DIRS}
		${LIBZIP_INCLUDE_DIRS}
		${LIBGD_INCLUDE_DIRS}
		${VORBISFILE_INCLUDE_DIRS}
		${OPENAL_INCLUDE_DIRS}
		${FREEALUT_INCLUDE_DIRS})
	link_directories(
		${SDL2_LIBRARY_DIRS}
		${SDL2_IMAGE_LIBRARY_DIRS}
		${SDL2_MIXER_LIBRARY_DIRS}
		${LIBXML2_LIBRARY_DIRS}
		${LIBZIP_LIBRARY_DIRS}
		${LIBGD_LIBRARY_DIRS}
		${VORBISFILE_LIBRARY_DIRS}
		${OPENAL_LIBRARY_DIRS}
		${FREEALUT_LIBRARY_DIRS})
ELSEIF(MINGW_CROSS_COMPILE)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/build/mingw/include/SDL)
ELSEIF(WIN32)
	# MSYS2 MinGW: use pkg-config (sdl2-config isn't shipped)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(SDL2 REQUIRED sdl2)
	pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
	pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
	pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
	INCLUDE_DIRECTORIES(${SDL2_INCLUDE_DIRS} ${SDL2_IMAGE_INCLUDE_DIRS}
				${SDL2_MIXER_INCLUDE_DIRS} ${LIBXML2_INCLUDE_DIRS})
ELSE()
	EXEC_PROGRAM(sdl2-config ARGS --cflags OUTPUT_VARIABLE SDLCFLAGS)
	string(REGEX REPLACE "[\r\n]" " " SDLCFLAGS "${SDLCFLAGS}")
	ADD_DEFINITIONS(${SDLCFLAGS})
endif()


IF(X11)
	ADD_DEFINITIONS("-DX11")
ENDIF(X11)
IF(DEDICATED_ONLY)
	ADD_DEFINITIONS("-DDEDICATED_ONLY")
ENDIF(DEDICATED_ONLY)


IF(G15)
	ADD_DEFINITIONS("-DWITH_G15")
ENDIF(G15)

IF(HASBFD)
	ADD_DEFINITIONS("-DHASBFD")
	SET(LIBS ${LIBS} dl bfd opcodes iberty)
	INCLUDE_DIRECTORIES(/usr/include/libiberty)
ENDIF(HASBFD)


IF(BOOST_LINK_STATIC)
	# seems this is the way for Debian:
	SET(LIBS ${LIBS} boost_signals.a)
	IF(MINGW_CROSS_COMPILE)
		SET(LIBS ${LIBS} boost_system.a)
	ENDIF(MINGW_CROSS_COMPILE)
	# and this on newer CMake (>=2.6?)
	SET(LIBS ${LIBS} /usr/lib/x86_64-linux-gnu/libboost_signals.a /usr/lib/x86_64-linux-gnu/libboost_system.a)
ELSE(BOOST_LINK_STATIC)
	FIND_PACKAGE(Boost REQUIRED)
	SET(LIBS ${LIBS} ${Boost_LIBRARIES})
ENDIF(BOOST_LINK_STATIC)

SET(LIBS ${LIBS} alut openal vorbisfile)

SET(LIBS ${LIBS} curl)

SET(LIBS ${LIBS} SDL2 SDL2_image)
if(APPLE)
	# Needed by src/MacHelpers.m (clipboard + user-attention shims).
	SET(LIBS ${LIBS} "-framework Cocoa")
endif(APPLE)

IF(WIN32 AND MSVC)
	SET(LIBS ${LIBS} SDL_mixer wsock32 wininet dbghelp
				"${OLXROOTDIR}/build/msvc/libs/SDLmain.lib"
				"${OLXROOTDIR}/build/msvc/libs/libxml2.lib"
				"${OLXROOTDIR}/build/msvc/libs/NLstatic.lib"
				"${OLXROOTDIR}/build/msvc/libs/libzip.lib"
				"${OLXROOTDIR}/build/msvc/libs/zlib.lib"
				"${OLXROOTDIR}/build/msvc/libs/bgd.lib")
ELSEIF(WIN32)
	# MinGW on Windows (MSYS2 packages).
	# mingw32 and SDL2main must come before libmingw32's default main()
	# in the link order. Wrap SDL2main in --whole-archive so the
	# linker doesn't drop it before its main() overrides the default.
	SET(LIBS mingw32
				-Wl,--whole-archive SDL2main -Wl,--no-whole-archive
				${LIBS}
				SDL2_mixer xml2 zip gd z
				wsock32 ws2_32 wininet dbghelp iphlpapi
				version
				pthread)
ELSEIF(APPLE)
	SET(LIBS ${LIBS} SDL2_mixer xml2 zip gd z)
ELSEIF(MINGW_CROSS_COMPILE)

ELSE()
	EXEC_PROGRAM(sdl2-config ARGS --libs OUTPUT_VARIABLE SDLLIBS)
	STRING(REGEX REPLACE "[\r\n]" " " SDLLIBS "${SDLLIBS}")
ENDIF(WIN32)

if(UNIX)
	IF (NOT HAWKNL_BUILTIN)
		SET(LIBS ${LIBS} NL)
	ENDIF (NOT HAWKNL_BUILTIN)
	IF (NOT LIBZIP_BUILTIN)
		SET(LIBS ${LIBS} zip)
	ENDIF (NOT LIBZIP_BUILTIN)
	IF (NOT LIBLUA_BUILTIN)
		EXEC_PROGRAM(pkg-config ARGS lua5.1 --libs --silence-errors OUTPUT_VARIABLE LIBLUA_NAME) #Search for lua5.1 first because newer versions seem to be incompatible
		IF(NOT LIBLUA_NAME)
			EXEC_PROGRAM(pkg-config ARGS lua-5.1 --libs --silence-errors OUTPUT_VARIABLE LIBLUA_NAME) #On some systems, e.g. Fedora, it may be lua-5.1
		ENDIF(NOT LIBLUA_NAME)
		IF(NOT LIBLUA_NAME)
			EXEC_PROGRAM(pkg-config ARGS lua51 --libs --silence-errors OUTPUT_VARIABLE LIBLUA_NAME) #On Arch Linux pkg-config wants lua51 even though the result is still -llua5.1 -lm...
		ENDIF(NOT LIBLUA_NAME)
		IF(NOT LIBLUA_NAME)
			MESSAGE(WARNING "No Lua 5.1 found - searching for default Lua, but it may not work. You may have to install Lua 5.1 packages or use the built-in library")
			EXEC_PROGRAM(pkg-config ARGS lua --libs --silence-errors OUTPUT_VARIABLE LIBLUA_NAME) #Search for lua if neither found
		ENDIF(NOT LIBLUA_NAME)
		IF(NOT LIBLUA_NAME)
			MESSAGE(WARNING "No Lua found by pkg-config - trying default Lua, but it may not work. Make sure that Lua 5.1 packages are installed, or use the built-in library") 
			SET(LIBLUA_NAME lua) #Set default if nothing works, although this will likely lead to errors
		ENDIF(NOT LIBLUA_NAME)
		SET(LIBS ${LIBS} ${LIBLUA_NAME})
	ENDIF (NOT LIBLUA_BUILTIN)
	IF(X11)
		SET(LIBS ${LIBS} X11)
	ENDIF(X11)
	IF (STLPORT)
		SET(LIBS ${LIBS} stlportstlg)
	ENDIF (STLPORT)
	IF (G15)
		SET(LIBS ${LIBS} g15daemon_client g15render)
	ENDIF (G15)

	SET(LIBS ${LIBS} ${SDLLIBS} xml2 z)
	IF(NOT MINGW_CROSS_COMPILE)
		SET(LIBS ${LIBS} ${SDLLIBS} pthread)
	ENDIF(NOT MINGW_CROSS_COMPILE)

	ADD_DEFINITIONS("-ftabstop=4") # Avoid some GCC and clang warnings due to messy indentation
endif(UNIX)

IF (NOT DEDICATED_ONLY)
	if(APPLE)
                SET(LIBS ${LIBS} gd)
		#SET(LIBS ${LIBS} "-framework GD")
	elseif(UNIX)
                SET(LIBS ${LIBS} gd)
	endif(APPLE)
ENDIF (NOT DEDICATED_ONLY)

IF(MINGW_CROSS_COMPILE)
	SET(LIBS ${LIBS} SDLmain SDL boost_system jpeg png vorbisenc vorbis ogg dbghelp dsound dxguid wsock32 wininet wldap32 user32 gdi32 winmm version kernel32)
ENDIF(MINGW_CROSS_COMPILE)

add_compile_definitions(SYSTEM_DATA_DIR="${SYSTEM_DATA_DIR}")
