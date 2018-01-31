/*
	OpenLieroX

	physic simulation interface

	code under LGPL
	created on 9/2/2008
*/

#ifndef __PHYSICS_H__
#define __PHYSICS_H__

#include <string>
#include "CVec.h"
#include "Iter.h"
#include "olx-types.h"
#include "CodeAttributes.h"


class CClient;
class CWorm;
class CProjectile;
class CBonus;
class CNinjaRope;
class CMap;

// HINT: I call this class PhysicsEngine, though it doesn't matches the meaning
// of engine completely as I intented it for Hirudo. But it can probably easily be
// reused later.

class PhysicsEngine {
public:
	static void Init(); // init general stuff and set default engine
	static void UnInit(); // deletes standard engine etc.

	// get/set the current instance of the PhysicsEngine (for example PhysicsLX56)
	static PhysicsEngine* Get();

// ------------
	std::string name(); // get a name of the implementation

	void initGame(); // init a new game
	void uninitGame(); // gives just a hint to the engine that the game isn't runnign anymore
	bool isInitialised(); // tells if the engine is inited

	// TODO: later, we should have a class World and all objects and the map are included there
	// in the end, I want to have one single simulate(CWorld* world);
	void simulateWorm(CWorm* worm, bool local);
	void simulateProjectiles(Iterator<CProjectile*>::Ref projs);
	void simulateBonuses(CBonus* bonuses, size_t count);
	
	// skips simulation for one frame (but increments simulationtime)
	void skipProjectiles(Iterator<CProjectile*>::Ref projs);
};

AbsTime GetPhysicsTime(); // Returns tLX->currentTime


INLINE void warpSimulationTimeForDeltaTimeCap(AbsTime& simulationTime, TimeDiff deltaTime, TimeDiff realDeltaTime) {
	simulationTime += realDeltaTime - deltaTime;
}

INLINE void applyFriction(CVec& vel, float dt, float area, float mass, float dragCoeff, float airCoeff, CVec airVel = CVec(0,0)) {
	const CVec relAirVel = airVel - vel;
	vel += relAirVel * relAirVel.GetLength() * area * dragCoeff * airCoeff * 0.5f * dt / mass;
}


#endif
