/*
	OpenLieroX

	some constants

	created 2008-10-03
	code under LGPL
*/

#ifndef __OLX_CONSTS_H__
#define __OLX_CONSTS_H__

#include "CodeAttributes.h"

// Net status
enum ClientNetState {
	NET_DISCONNECTED=0,
	NET_CONNECTING,			// Server doesn't use this state, only client side
	NET_CONNECTED,			// Server usage: when client connected or playing, client usage: only when in server lobby
	NET_ZOMBIE				// Server side only state - server won't accept any client packets and will send disconnect packets to client
};

INLINE const char* NetStateString(ClientNetState state) {
	switch (state) {
		case NET_DISCONNECTED: return "NET_DISCONNECTED";
		case NET_CONNECTING: return "NET_CONNECTING";
		case NET_CONNECTED: return "NET_CONNECTED";
		case NET_ZOMBIE: return "NET_ZOMBIE";
	}
	return "INVALID NETSTATE";
}

// Misc constants
static constexpr int MAX_WORMS = 32;
static constexpr int LX_PORT = 23400;
static constexpr int SPAWN_HOLESIZE = 4;
static constexpr int MAX_CLIENTS = 32;
static constexpr int MAX_PLAYERS = 32;
static constexpr int MAX_TEAMS = 4;
static constexpr int MAX_CHATLINES = 8;
// Maximum number of local (split-screen) human players sharing one screen.
// Each local player gets their own viewport and their own gamepad (player N ->
// pad N). Bump this if the split-screen layout is extended to more players.
static constexpr int MAX_LOCAL_PLAYERS = 4;
// One viewport per local split-screen player.
static constexpr int NUM_VIEWPORTS = MAX_LOCAL_PLAYERS;
static constexpr int GAMEOVER_WAIT = 3;


static const float LX_ENDWAIT = 9.0f;

// Game modes
enum GameModeIndex {
	GM_DEATHMATCH = 0,
	GM_TEAMDEATH,
	GM_TAG,
	GM_DEMOLITIONS,
	GM_HIDEANDSEEK,
	GM_CTF,
	GM_RACE,
	GM_TEAMRACE,
};

// Game mode types
// HINT: do not change the order, this is used over the network
enum {
	GMT_NORMAL = 0,  // Worms appear as normal
	GMT_TEAMS,   // Worms appear in teams
	GMT_TIME,    // One worm appears special, worms have a time attribute
	GMT_DIRT,     // There is a dirt counter
	GMT_MAX = GMT_DIRT
};

enum AFK_TYPE {
	AFK_BACK_ONLINE = 0,
	AFK_TYPING_CHAT,
	AFK_AWAY,
	AFK_SELECTING_WPNS,
	AFK_CONSOLE,
	AFK_MENU,
};


#endif

