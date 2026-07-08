/*
 *  Unit tests for the server's connection-challenge table (ChallengeTable.h).
 */

#include "unittest.h"
#include "ChallengeTable.h"

#include <nl.h>
#include <string>

namespace {

// StringToNetAddr needs a network driver selected; loopback is enough.
struct NetDriver {
	NetDriver() { nlInit(); nlSelectNetwork(NL_LOOP_BACK); }
	~NetDriver() { nlShutdown(); }
};

void clearTable(challenge_t* t, int n) {
	for(int i = 0; i < n; i++)
		SetNetAddrValid(t[i].Address, false);
}

}

// Issue a challenge, echo it back to connect, and it can't be replayed.
void test_ChallengeIssueThenConsume() {
	NetDriver net;
	challenge_t table[8];
	clearTable(table, 8);

	NetworkAddr addr = StringToNetAddr("127.0.0.1:5555");
	CHECK(IsNetAddrValid(addr));

	int num = ChallengeTable_issue(table, 8, addr, AbsTime(), "beta9");
	std::string ver;
	CHECK(ChallengeTable_consume(table, 8, addr, num, ver));
	CHECK(ver == "beta9");
	CHECK(!ChallengeTable_consume(table, 8, addr, num, ver));  // consumed already
}

// A connect with the wrong challenge must not discard the live one,
// so the correct connect right after still succeeds (the wasm 2nd-game bug).
void test_ChallengeSurvivesStaleConnect() {
	NetDriver net;
	challenge_t table[8];
	clearTable(table, 8);

	NetworkAddr addr = StringToNetAddr("127.0.0.1:5555");
	int cur = ChallengeTable_issue(table, 8, addr, AbsTime(), "beta9");

	std::string ver;
	CHECK(!ChallengeTable_consume(table, 8, addr, cur ^ 0x5a5a5a5a, ver));
	CHECK(ChallengeTable_consume(table, 8, addr, cur, ver));
	CHECK(ver == "beta9");
}

// A repeated getchallenge for the same address returns the same number
// (idempotent), so a duplicated packet can't invalidate the first reply.
void test_ChallengeReissueIsIdempotent() {
	NetDriver net;
	challenge_t table[8];
	clearTable(table, 8);

	NetworkAddr addr = StringToNetAddr("127.0.0.1:5555");
	int a = ChallengeTable_issue(table, 8, addr, AbsTime(), "beta9");
	int b = ChallengeTable_issue(table, 8, addr, AbsTime(), "beta9");
	CHECK(a == b);

	std::string ver;
	CHECK(ChallengeTable_consume(table, 8, addr, a, ver));
	// After it is consumed, the next getchallenge issues a fresh one.
	int c = ChallengeTable_issue(table, 8, addr, AbsTime(), "beta9");
	CHECK(ChallengeTable_consume(table, 8, addr, c, ver));
}

// A repeated consume with the same number:
// the second one currently fails, because consume resets iNum on the first.
// (Whether a duplicate connect should be re-accepted here,
// or left to reconnectFrom in ParseConnect, is the open question.)
void test_ChallengeConsumeRepeated() {
	NetDriver net;
	challenge_t table[8];
	clearTable(table, 8);

	NetworkAddr addr = StringToNetAddr("127.0.0.1:5555");
	int num = ChallengeTable_issue(table, 8, addr, AbsTime(), "beta9");

	std::string ver;
	CHECK(ChallengeTable_consume(table, 8, addr, num, ver));
	CHECK(!ChallengeTable_consume(table, 8, addr, num, ver));  // second: currently fails
}
