/*
 *  Entry point for the olx_tests unit-test executable.
 *
 *  This target links the whole engine (with its own main() disabled via
 *  OLX_UNITTEST) plus the tests/unit sources, so tests can exercise any
 *  engine class directly. Add a new test by writing a `void test_Foo();`
 *  function in a test_*.cpp file and calling it from main() below.
 */

#include "unittest.h"
#include <cstdio>

int g_olxTestFailures = 0;

void olxCheckFailed(const char* expr, const char* file, int line) {
	++g_olxTestFailures;
	printf("  CHECK failed: %s  (%s:%d)\n", expr, file, line);
}

// Test entry points, defined in the test_*.cpp files.
void test_CBytestream();
void test_Version();

int main() {
	printf("Running OLX unit tests...\n");

	test_CBytestream();
	test_Version();

	if(g_olxTestFailures) {
		printf("FAILED: %d check(s)\n", g_olxTestFailures);
		return 1;
	}
	printf("OK: all unit tests passed\n");
	return 0;
}
