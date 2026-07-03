/*
	OpenLieroX

	Network-protocol version — the single source of truth for which wire
	protocol the server speaks.

	This is deliberately SEPARATE from the software version (GetGameVersion() /
	GetFullGameName()). The software version identifies the build (shown in the
	server browser, used for updates, crash reports, etc.). This value is used
	ONLY to decide the network protocol.

	Why it isn't just the software version:
	Unmodified clients derive their wire protocol from the version the server
	advertises in the connect handshake. The 0.59.10 sync protocol delivers the
	mod to a client only via lobby updates, which a client ignores while not in
	the lobby — so a player joining a game already in progress never learns the
	mod and gets rejected ("invalid mod name") / kicked ("selected weapons too
	long"). The older protocol sends the mod inside PrepareGame, so mid-game
	joins work. This is exactly why an unmodified master client can already join
	a 0.58 server.

	So the server advertises this pre-0.59.10 protocol version (and treats
	clients accordingly, see CServerConnection::setClientVersion) to keep every
	connection on the join-capable protocol, while still reporting its real
	software version everywhere else.

	It is RT_RC (a release candidate), which newer clients do NOT version-ban
	(only RT_ALPHA/RT_BETA versions older than the client are banned).
*/

#ifndef __NETPROTOCOLVERSION_H__
#define __NETPROTOCOLVERSION_H__

#include "Version.h"

// The network-protocol version the server speaks / advertises.
INLINE Version GetServerNetProtocolVersion() { return OLXRcVersion(0, 58, 5); }

// Same value as the "GAMENAME/version" string put on the wire in the handshake.
INLINE const char* GetServerNetProtocolName() { return "OpenLieroX/0.58_rc5"; }

#endif
