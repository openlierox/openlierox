/*
 *  Connection-challenge table. See ChallengeTable.h.
 */

#include "ChallengeTable.h"

#include <cstdlib>


int ChallengeTable_issue(challenge_t* slots, int count,
						 const NetworkAddr& addr, AbsTime now,
						 const std::string& clientVersion) {
	AbsTime oldest = AbsTime::Max();
	int slot = -1;

	// Reuse this address's slot if it has one, else a free slot or the oldest.
	for(int i = 0; i < count; i++) {
		if(IsNetAddrValid(slots[i].Address)) {
			if(AreNetAddrEqual(addr, slots[i].Address)) { slot = i; break; }
			if(slot < 0 || slots[i].fTime < oldest) { oldest = slots[i].fTime; slot = i; }
		} else { slot = i; break; }
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
	for(int i = 0; i < count; i++) {
		if(IsNetAddrValid(slots[i].Address) && AreNetAddrEqual(addr, slots[i].Address)) {
			if(challId == slots[i].iNum) {
				SetNetAddrValid(slots[i].Address, false);
				slots[i].iNum = 0;
				clientVersion = slots[i].sClientVersion;
				return true;
			}
			// Mismatch: discard this address's live challenge.
			SetNetAddrValid(slots[i].Address, false);
			slots[i].iNum = 0;
		}
	}
	return false;
}
