/*
 *  TouchScreenInput.cpp
 *  OpenLieroX
 *
 *  See TouchScreenInput.h for the design overview.
 *
 *  code under LGPL
 */

#include "TouchScreenInput.h"

#include <deque>

namespace TouchScreenInput {

namespace {

// Touch currently only drives player 1; size the arrays to leave room
// for a second local player without a layout change later.
constexpr int kMaxPlayers     = 2;
constexpr int kActionCount    = (int)Action::__Count;

// Producer pushes here, consumer drains in processFrame(). Same-thread
// communication (gameloop thread for both sides), so a plain deque is
// enough — no lockfree primitives needed.
std::deque<Event> gQueue;

// Held state, indexed [playerNumber-1][action]. Updated by processFrame.
bool gIsDown[kMaxPlayers][kActionCount] = {{false}};

// Pending leading-edge taps, indexed [playerNumber-1][action].
// processFrame() increments this on each not-down -> down transition and
// never clears it; the count is consumed (reset to 0) by tapCount() /
// wasActionTapped(). Keeping a running count -- rather than a single
// per-frame bool -- means rapid taps that land in one queue drain are
// not coalesced into one, and a tap is not lost on a gameloop tick where
// the consumer (e.g. the weapon-select draw path) didn't run.
int gPendingTaps[kMaxPlayers][kActionCount] = {{0}};

inline bool playerInRange(int p) {
	return p >= 1 && p <= kMaxPlayers;
}

inline int actionIdx(Action a) {
	return (int)a;
}

} // namespace


void pushEvent(int playerNumber, Action action, StateChange stateChange) {
	if(!playerInRange(playerNumber)) return;
	gQueue.push_back(Event{playerNumber, action, stateChange});
}

void processFrame() {
	while(!gQueue.empty()) {
		const Event ev = gQueue.front();
		gQueue.pop_front();
		if(!playerInRange(ev.playerNumber)) continue;

		const int p = ev.playerNumber - 1;
		const int a = actionIdx(ev.action);
		if(ev.stateChange == StateChange::Down) {
			// Leading-edge detection: a Down that transitions from
			// not-pressed to pressed is one tap. Accumulate rather than
			// overwrite -- the consumer may not run every tick, and two
			// quick taps can land in a single queue drain.
			if(!gIsDown[p][a])
				gPendingTaps[p][a]++;
			gIsDown[p][a] = true;
		} else {
			gIsDown[p][a] = false;
		}
	}
}

bool isActionDown(int playerNumber, Action action) {
	if(!playerInRange(playerNumber)) return false;
	return gIsDown[playerNumber - 1][actionIdx(action)];
}

int tapCount(int playerNumber, Action action) {
	if(!playerInRange(playerNumber)) return 0;
	int& slot = gPendingTaps[playerNumber - 1][actionIdx(action)];
	const int n = slot;
	slot = 0; // consume: each tap is reported exactly once
	return n;
}

bool wasActionTapped(int playerNumber, Action action) {
	return tapCount(playerNumber, action) > 0;
}

void releaseAll() {
	gQueue.clear();
	for(int p = 0; p < kMaxPlayers; p++)
		for(int a = 0; a < kActionCount; a++) {
			gIsDown[p][a]      = false;
			gPendingTaps[p][a] = 0;
		}
}

} // namespace TouchScreenInput
