/*
 *  Unit test for the linenoise line editor.
 */

#include "unittest.h"

#ifdef HAVE_LINENOISE
#include "linenoise.h"

// refreshLine() used to spin forever when the terminal reported 0 columns
// (a pty with no window size set):
// prompt.size() + pos >= 0 is always true for the unsigned pos,
// so pos and len underflowed instead of the loop ending,
// and a dedicated server with the stdin CLI could hang at startup.
// It must now terminate for any cols value.
void test_LinenoiseRefreshLineZeroCols() {
	LinenoiseEnv env;
	env.fd = -1;               // send its writes nowhere
	env.cols = 0;              // a pty with no window size
	env.prompt = "OLX> ";
	env.buf = "abcdefghij";
	env.pos = env.buf.size();
	env.refreshLine();         // before the fix: an infinite loop
	CHECK(true);               // reaching here means it terminated
}

#else

void test_LinenoiseRefreshLineZeroCols() { CHECK(true); }

#endif
