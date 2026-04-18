# CMake file for development of OLX

cmake_minimum_required(VERSION 2.4)

# Note: This must be a relative path because AUX_SOURCE_DIRECTORY would not work otherwise with CMake 2.4
SET(OLXROOTDIR ${Project_SOURCE_DIR})
include(${OLXROOTDIR}/CMakeOlxCommon.cmake)

ADD_EXECUTABLE(openlierox ${OLXROOTDIR}/src/main.cpp ${ALL_SRCS})

IF(WIN32)
	# Embedding a path in OUTPUT_NAME confuses the Ninja generator
	# (Ninja emits both the implicit target rule and the OUTPUT_NAME
	# rule, which collide). Use RUNTIME_OUTPUT_DIRECTORY for the
	# location and keep OUTPUT_NAME as just the executable name.
	SET_TARGET_PROPERTIES(openlierox PROPERTIES
		OUTPUT_NAME "OpenLieroX"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin")

	# Add debug info to release build in linker flags. /LTCG is dropped
	# along with /GL on the compile side - link-time-codegen with vcpkg
	# objects produced without /GL emits LNK4075 spam.
	SET_TARGET_PROPERTIES(openlierox PROPERTIES LINK_FLAGS "${LINK_FLAGS}
		/DEBUG
		/MAP:\"${CMAKE_BINARY_DIR}/bin/OpenLieroX.map\"
		/MAPINFO:EXPORTS
		/NODEFAULTLIB:\"LIBC\"
		/INCREMENTAL:NO")
ELSE(WIN32)
	IF(MINGW_CROSS_COMPILE)
		SET_TARGET_PROPERTIES(openlierox PROPERTIES OUTPUT_NAME bin/openlierox.exe)
	ELSE(MINGW_CROSS_COMPILE)
		SET_TARGET_PROPERTIES(openlierox PROPERTIES OUTPUT_NAME bin/openlierox)
	ENDIF(MINGW_CROSS_COMPILE)
ENDIF(WIN32)

TARGET_LINK_LIBRARIES(openlierox ${LIBS})

IF(PCH)
	EXEC_PROGRAM(./${OLXROOTDIR}/update_precompiled_header.sh OUTPUT_VARIABLE NULL)
	ADD_PRECOMPILED_HEADER(openlierox ${OLXROOTDIR}/include/PrecompiledHeader.hpp 1)
ENDIF(PCH)

