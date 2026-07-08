/*
 *  Unit tests for HawkNL's loopback driver (NL_LOOP_BACK),
 *  which only the wasm build uses (browsers can't open raw UDP sockets).
 *  Checks the UDP-emulation OLX depends on: a datagram reaches a bound port
 *  exactly once, the receiver can reply, and a second client still works.
 */

#include "unittest.h"

#include <nl.h>

static const NLushort kServerPort = 27100;

// HawkNL defaults to non-blocking IO,
// so a read with no data returns 0 rather than blocking.
// Drain a socket and return how many datagrams it held.
static int drain(NLsocket s) {
	char buf[512];
	int count = 0;
	while(nlRead(s, buf, sizeof(buf)) > 0)
		++count;
	return count;
}

// Set up loopback for one test; torn down when it leaves scope.
namespace {
struct LoopbackNet {
	LoopbackNet() {
		CHECK(nlInit());
		CHECK(nlSelectNetwork(NL_LOOP_BACK));
	}
	~LoopbackNet() { nlShutdown(); }
};
}

// A single datagram to a bound port must arrive there exactly once.
void test_LoopbackDeliversDatagramExactlyOnce() {
	LoopbackNet net;
	NLsocket server = nlOpen(kServerPort, NL_UNRELIABLE);
	NLsocket client = nlOpen(0, NL_UNRELIABLE);
	CHECK(server != NL_INVALID);
	CHECK(client != NL_INVALID);

	NLaddress srvAddr;
	CHECK(nlGetLocalAddr(server, &srvAddr));
	CHECK(nlSetRemoteAddr(client, &srvAddr));
	CHECK(nlWrite(client, "hello", 5) == 5);

	char buf[512];
	CHECK(nlRead(server, buf, sizeof(buf)) == 5);
	// Nothing else queued: the datagram was delivered exactly once.
	CHECK(drain(server) == 0);

	nlClose(client);
	nlClose(server);
}

// The receiver must learn the sender's address and be able to reply,
// which is what the server does to answer a getchallenge.
void test_LoopbackReplyReachesSender() {
	LoopbackNet net;
	NLsocket server = nlOpen(kServerPort, NL_UNRELIABLE);
	NLsocket client = nlOpen(0, NL_UNRELIABLE);
	CHECK(server != NL_INVALID);
	CHECK(client != NL_INVALID);

	NLaddress srvAddr;
	CHECK(nlGetLocalAddr(server, &srvAddr));
	CHECK(nlSetRemoteAddr(client, &srvAddr));
	CHECK(nlWrite(client, "getchallenge", 12) == 12);

	char buf[512];
	CHECK(nlRead(server, buf, sizeof(buf)) == 12);
	NLaddress from;
	CHECK(nlGetRemoteAddr(server, &from));

	CHECK(nlSetRemoteAddr(server, &from));
	CHECK(nlWrite(server, "challenge", 9) == 9);
	CHECK(nlRead(client, buf, sizeof(buf)) == 9);
	CHECK(drain(client) == 0);

	nlClose(client);
	nlClose(server);
}

// A second local game opens a fresh client socket against the same server.
// The new datagram must reach the server exactly once,
// with no stale or duplicated delivery left over from the first client.
void test_LoopbackSecondClientReconnects() {
	LoopbackNet net;
	NLsocket server = nlOpen(kServerPort, NL_UNRELIABLE);
	CHECK(server != NL_INVALID);

	NLaddress srvAddr;
	CHECK(nlGetLocalAddr(server, &srvAddr));
	char buf[512];

	{	// game 1
		NLsocket c1 = nlOpen(0, NL_UNRELIABLE);
		CHECK(c1 != NL_INVALID);
		CHECK(nlSetRemoteAddr(c1, &srvAddr));
		CHECK(nlWrite(c1, "g1", 2) == 2);
		CHECK(nlRead(server, buf, sizeof(buf)) == 2);
		CHECK(drain(server) == 0);
		nlClose(c1);
	}

	{	// game 2: a new client socket
		NLsocket c2 = nlOpen(0, NL_UNRELIABLE);
		CHECK(c2 != NL_INVALID);
		CHECK(nlSetRemoteAddr(c2, &srvAddr));
		CHECK(nlWrite(c2, "g2", 2) == 2);
		CHECK(nlRead(server, buf, sizeof(buf)) == 2);
		CHECK(drain(server) == 0);
		nlClose(c2);
	}

	nlClose(server);
}
