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
void test_LoopbackDeliversDatagramExactlyOnce();
void test_LoopbackReplyReachesSender();
void test_LoopbackSecondClientReconnects();
void test_ChallengeIssueThenConsume();
void test_ChallengeSurvivesStaleConnect();
void test_ChallengeReissueIsIdempotent();
void test_ChallengeConsumeRepeated();
void test_KeyboardDownOnceFiresOncePerPress();
void test_GuiLayoutArrowKeysStayInTextbox();
void test_OmfgScriptDeepExpression();
void test_OmfgScriptDeepPropertyBlocks();
void test_OmfgScriptShallowOk();
void test_OmfgScriptHugeIntegerLiteral();
void test_OmfgScriptHugeNumberFirstToken();
void test_LinenoiseRefreshLineZeroCols();
void test_CShootListSpeedRoundtrip();

int main() {
	printf("Running OLX unit tests...\n");

	test_CBytestream();
	test_Version();
	test_LoopbackDeliversDatagramExactlyOnce();
	test_LoopbackReplyReachesSender();
	test_LoopbackSecondClientReconnects();
	test_ChallengeIssueThenConsume();
	test_ChallengeSurvivesStaleConnect();
	test_ChallengeReissueIsIdempotent();
	test_ChallengeConsumeRepeated();
	test_KeyboardDownOnceFiresOncePerPress();
	test_GuiLayoutArrowKeysStayInTextbox();
	test_OmfgScriptDeepExpression();
	test_OmfgScriptDeepPropertyBlocks();
	test_OmfgScriptShallowOk();
	test_OmfgScriptHugeIntegerLiteral();
	test_OmfgScriptHugeNumberFirstToken();
	test_LinenoiseRefreshLineZeroCols();
	test_CShootListSpeedRoundtrip();

	if(g_olxTestFailures) {
		printf("FAILED: %d check(s)\n", g_olxTestFailures);
		return 1;
	}
	printf("OK: all unit tests passed\n");
	return 0;
}
