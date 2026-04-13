# CMake file for development of OLX

cmake_minimum_required(VERSION 2.4)

# Note: This must be a relative path because AUX_SOURCE_DIRECTORY would not work otherwise with CMake 2.4
SET(OLXROOTDIR ${Project_SOURCE_DIR})
include(${OLXROOTDIR}/CMakeOlxCommon.cmake)

ADD_EXECUTABLE(openlierox ${OLXROOTDIR}/src/main.cpp ${ALL_SRCS})

IF(WIN32 AND MSVC)
	SET_TARGET_PROPERTIES(openlierox PROPERTIES OUTPUT_NAME "../../../bin/OpenLieroX")

	# Add debug info to release build in linker flags
	SET_TARGET_PROPERTIES(openlierox PROPERTIES LINK_FLAGS "${LINK_FLAGS}
		/LTCG
		/DEBUG
		/MAP:\"${CMAKE_SOURCE_DIR}/bin/OpenLieroX.map\"
		/MAPINFO:EXPORTS
		/NODEFAULTLIB:\"LIBC\"
		/INCREMENTAL:NO")
ELSE()
	SET_TARGET_PROPERTIES(openlierox PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		OUTPUT_NAME openlierox)
ENDIF()

TARGET_LINK_LIBRARIES(openlierox ${LIBS})

IF(PCH)
	EXEC_PROGRAM(./${OLXROOTDIR}/update_precompiled_header.sh OUTPUT_VARIABLE NULL)
	ADD_PRECOMPILED_HEADER(openlierox ${OLXROOTDIR}/include/PrecompiledHeader.hpp 1)
ENDIF(PCH)

