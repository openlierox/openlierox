/*
 *  Minimal unit-test helpers for the olx_tests target.
 *
 *  A test is a plain `void test_Foo();` function;
 *  call it from tests/unit/test_main.cpp and use CHECK(cond) for assertions.
 *  A failing CHECK is recorded and printed but does not abort the run,
 *  so one test failing still lets the others run.
 */

#ifndef OLX_UNITTEST_H
#define OLX_UNITTEST_H

extern int g_olxTestFailures;
void olxCheckFailed(const char* expr, const char* file, int line);

#define CHECK(cond) do { if(!(cond)) olxCheckFailed(#cond, __FILE__, __LINE__); } while(0)

#endif // OLX_UNITTEST_H
