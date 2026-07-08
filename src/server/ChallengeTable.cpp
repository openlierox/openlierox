/*
 *  Connection-challenge table. See ChallengeTable.h.
 */

#include "ChallengeTable.h"

#include <cstdlib>


int ChallengeTable_issue(challenge_t* slots, int count,
						 const NetworkAddr& addr, AbsTime now,
						 const std::string& clientVersion) {
	// Reuse this address's live challenge if it has one,
	// so a repeated getchallenge (e.g. a duplicated packet) returns the same number.
	for(int i = 0; i < count; i++) {
		if(IsNetAddrValid(slots[i].Address) && AreNetAddrEqual(addr, slots[i].Address)) {
			slots[i].fTime = now;
			slots[i].sClientVersion = clientVersion;
			return slots[i].iNum;
		}
	}

	// Otherwise take a free slot, or overwrite the oldest.
	int slot = -1;
	AbsTime oldest = AbsTime::Max();
	for(int i = 0; i < count; i++) {
		if(!IsNetAddrValid(slots[i].Address)) { slot = i; break; }
		if(slot < 0 || slots[i].fTime < oldest) { oldest = slots[i].fTime; slot = i; }
	}
	if(slot < 0) return 0;  // count <= 0

	slots[slot].iNum = (rand() << 16) ^ rand();
	slots[slot].Address = addr;
	slots[slot].fTime = now;
	slots[slot].sClientVersion = clientVersion;
	return slots[slot].iNum;
}


bool ChallengeTable_consume(challenge_t* slots, int count,
							const NetworkAddr& addr, int challId,
							std::string& clientVersion) {
	// There is at most one live challenge per address (see ChallengeTable_issue).
	for(int i = 0; i < count; i++) {
		if(IsNetAddrValid(slots[i].Address) && AreNetAddrEqual(addr, slots[i].Address)) {
			if(challId != slots[i].iNum)
				// Stale or duplicate connect: keep the challenge for the correct one.
				return false;
			SetNetAddrValid(slots[i].Address, false);  // consume; blocks a replayed connect
			slots[i].iNum = 0;
			clientVersion = slots[i].sClientVersion;
			return true;
		}
	}
	return false;
}
