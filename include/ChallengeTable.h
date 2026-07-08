/*
 *  The connection challenges the game server hands out:
 *  a client must echo its number back in the connect packet.
 */

#ifndef OLX_CHALLENGETABLE_H
#define OLX_CHALLENGETABLE_H

#include <string>
#include "Networking.h"
#include "Timer.h"

class challenge_t { public:
	NetworkAddr	Address;
	AbsTime		fTime;
	int			iNum;
	std::string	sClientVersion;
};

// Issue a challenge for addr in a table of count slots; return its number.
int ChallengeTable_issue(challenge_t* slots, int count,
						 const NetworkAddr& addr, AbsTime now,
						 const std::string& clientVersion);

// Consume addr's challenge if challId matches it, filling clientVersion;
// return whether it matched.
bool ChallengeTable_consume(challenge_t* slots, int count,
							const NetworkAddr& addr, int challId,
							std::string& clientVersion);

#endif // OLX_CHALLENGETABLE_H
