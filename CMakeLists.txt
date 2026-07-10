# CMake file for development of OLX

cmake_minimum_required(VERSION 2.4)

# Note: This must be a relative path because AUX_SOURCE_DIRECTORY would not work otherwise with CMake 2.4
SET(OLXROOTDIR ${Project_SOURCE_DIR})
include(${OLXROOTDIR}/CMakeOlxCommon.cmake)

IF(WIN32)
	SET(OLX_ICON_PATH "${OLXROOTDIR}/share/OpenLieroX.ico")
	SET(OLX_MANIFEST_PATH "${OLXROOTDIR}/share/OpenLieroX-windows.manifest")
	configure_file(
		"${OLXROOTDIR}/OpenLieroX.rc.in"
		"${CMAKE_BINARY_DIR}/OpenLieroX.rc"
		@ONLY)
	SET(OLX_WIN_RESOURCES "${CMAKE_BINARY_DIR}/OpenLieroX.rc")
ENDIF()

ADD_EXECUTABLE(openlierox ${OLXROOTDIR}/src/main.cpp ${ALL_SRCS} ${OLX_WIN_RESOURCES})

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

# Dedicated unit-test executable, built only with -DOLX_TESTS=Yes.
# It links the whole engine (its main() disabled via the OLX_UNITTEST
# define) plus the tests/unit sources, which provide their own main(),
# so a test can exercise any engine class directly.
IF(OLX_TESTS)
	INCLUDE_DIRECTORIES(${OLXROOTDIR}/tests/unit)
	ADD_EXECUTABLE(olx_tests
		${OLXROOTDIR}/tests/unit/test_main.cpp
		${OLXROOTDIR}/tests/unit/test_CBytestream.cpp
		${OLXROOTDIR}/tests/unit/test_Version.cpp
		${OLXROOTDIR}/tests/unit/test_loopback.cpp
		${OLXROOTDIR}/tests/unit/test_challenge_table.cpp
		${OLXROOTDIR}/tests/unit/test_CInput.cpp
		${OLXROOTDIR}/tests/unit/test_omfgscript_parser.cpp
		${OLXROOTDIR}/tests/unit/test_linenoise.cpp
		${OLXROOTDIR}/tests/unit/test_CShootList.cpp
		${ALL_SRCS} ${OLX_WIN_RESOURCES})
	SET_TARGET_PROPERTIES(olx_tests PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
		OUTPUT_NAME olx_tests
		COMPILE_DEFINITIONS "OLX_UNITTEST")
	TARGET_LINK_LIBRARIES(olx_tests ${LIBS})
ENDIF(OLX_TESTS)

