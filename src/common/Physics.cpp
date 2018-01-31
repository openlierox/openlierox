/*
	OpenLieroX
	
	physic simulation interface
	
	code under LGPL
	created on 9/2/2008
*/

#include "Physics.h"
#include "PhysicsLX56.h"
#include "Debug.h"
#include "LieroX.h"
#include "game/CWorm.h"
#include "CClient.h"
#include "CProjectile.h"
#include "CBonus.h"
#include "game/Game.h"
#include "CGameScript.h"


void PhysicsEngine::Init() {
	notes << "PhysicsEngine " << PhysicsEngine::Get()->name() << " loaded" << endl;
	PhysicsEngine::Get()->initGame();
}

void PhysicsEngine::UnInit() {
	PhysicsEngine::Get()->uninitGame();
	notes << "unloading PhysicsEngine " << PhysicsEngine::Get()->name() << " .." << endl;
}

AbsTime GetPhysicsTime() {
	return game.simulationAbsTime();
}

void PhysicsEngine::skipProjectiles(Iterator<CProjectile*>::Ref projs) {
	cClient->fLastSimulationTime += tLX->fRealDeltaTime;
	
	for(Iterator<CProjectile*>::Ref i = projs; i->isValid(); i->next()) {
		CProjectile* p = i->get();
		p->fLastSimulationTime += tLX->fRealDeltaTime;
	}
}
