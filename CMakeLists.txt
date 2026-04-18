# CMake file for development of OLX

cmake_minimum_required(VERSION 2.4)

# Note: This must be a relative path because AUX_SOURCE_DIRECTORY would not work otherwise with CMake 2.4
SET(OLXROOTDIR ${Project_SOURCE_DIR})
include(${OLXROOTDIR}/CMakeOlxCommon.cmake)

ADD_EXECUTABLE(openlierox ${OLXROOTDIR}/src/main.cpp ${ALL_SRCS})

IF(WIN32)
	# OUTPUT_NAME with "../" path components confuses the Ninja generator
	# ("multiple rules generate" error), so just emit the binary into
	# build/bin/ like the Linux/Mac branches do and let the package step
	# pick it up from there.
	SET_TARGET_PROPERTIES(openlierox PROPERTIES OUTPUT_NAME "bin/OpenLieroX")

	# Add debug info to release build in linker flags
	SET_TARGET_PROPERTIES(openlierox PROPERTIES LINK_FLAGS "${LINK_FLAGS}
		/LTCG
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

