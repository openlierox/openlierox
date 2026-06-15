/*
	LX56 with extensions - Projectile simulation

	merged code of ProjAction and CProjectile

	code under LGPL
	created by Albert Zeyer on 22-05-2009
*/


/*
 
 This file includes the main code of LX56 projectile simulation - much cleaned up and extended.
 
 For a reference, look here:
 http://openlierox.svn.sourceforge.net/viewvc/openlierox/src/common/CProjectile.cpp?revision=1&view=markup&pathrev=1
 SimulateProjectiles in:
 http://openlierox.svn.sourceforge.net/viewvc/openlierox/src/client/CClient_Game.cpp?revision=1&view=markup&pathrev=1
 
 */

#include <cmath>
#include <typeinfo>

#include "CodeAttributes.h"
#include "ProjAction.h"
#include "CGameScript.h"
#include "game/CWorm.h"
#include "CProjectile.h"
#include "CClient.h"
#include "ProjectileDesc.h"
#include "Physics.h"
#include "PhysicsLX56.h"
#include "ConfigHandler.h"
#include "EndianSwap.h"
#include "Geometry.h"
#include "ThreadPool.h" // for struct Action
#include "Timer.h"
#include "WeaponDesc.h"
#include "sound/SoundsBase.h"
#include "game/Game.h"

#ifdef __MINGW32_VERSION
// TODO: ugly hack, fix it - mingw stdlib seems to be broken
#define powf(x,y) ((float)pow((double)x,(double)y))
#endif


///////////////////
// Lower level projectile-worm collision test
INLINE int CProjectile::ProjWormColl(CVec pos)
{
	Shape<int> s; s.pos = pos; s.radius = radius;
	if(getProjInfo()->Type == PRJ_CIRCLE)
		s.type = Shape<int>::ST_CIRCLE;
	else {
		// that's LX56 behaviour...
		if(s.radius.x <= 2) s.radius.x = 0;
		if(s.radius.y <= 2) s.radius.y = 0;
	}
	
	CWorm* ownerWorm = NULL;
	if(hasOwner())
		ownerWorm = game.wormById(GetOwner(), false);
	
	bool preventSelfShooting =
		(this->getIgnoreWormCollBeforeTime() > this->fLastSimulationTime); // if the simulation is too early, ignore this worm col

	for_each_iterator(CWorm*, w_, game.aliveWorms()) {
		CWorm* w = w_->get();
		
		if(preventSelfShooting && w == ownerWorm)
			continue;

		if(ownerWorm && cClient->isTeamGame() && !cClient->getGameLobby()[FT_TeamHit] && w != ownerWorm && w->getTeam() == ownerWorm->getTeam())
			continue;
		
		if(ownerWorm && !cClient->getGameLobby()[FT_SelfHit] && w == ownerWorm)
			continue;
		
		const static int wsize = 4;
		Shape<int> worm;
		worm.pos = w->posRecordings.getBest((size_t)LX56PhysicsDT.milliseconds(), (size_t)(tLX->currentTime - this->fLastSimulationTime).milliseconds());
		worm.radius = VectorD2<int>(wsize, wsize);
		
		if(s.CollisionWith(worm)) {
			
			CollisionSide = 0;
			
			// Calculate the side of the collision (obsolete??)
			if(s.pos.x < worm.pos.x-2)
				CollisionSide |= COL_LEFT;
			else if(s.pos.x > worm.pos.x+2)
				CollisionSide |= COL_RIGHT;
			if(s.pos.y < worm.pos.y-2)
				CollisionSide |= COL_TOP;
			else if(s.pos.y > worm.pos.y+2)
				CollisionSide |= COL_BOTTOM;
			
			return w->getID();
		}
	}
	
	// No worm was hit
	return -1;
}


static INLINE bool CProjectile_CollisionWith(const CProjectile* src, const CProjectile* target, int src_rx, int src_ry) {
	Shape<int> s1; s1.pos = src->getPos(); s1.radius.x = src_rx; s1.radius.y = src_ry;
	Shape<int> s2; s2.pos = target->getPos(); s2.radius = target->getRadius();
	if(src->getProjInfo()->Type == PRJ_CIRCLE) s1.type = Shape<int>::ST_CIRCLE;
	if(target->getProjInfo()->Type == PRJ_CIRCLE) s2.type = Shape<int>::ST_CIRCLE;
	
	return s1.CollisionWith(s2);
}

static INLINE bool CProjectile_CollisionWith(const CProjectile* src, const CProjectile* target) {
	return CProjectile_CollisionWith(src, target, src->getRadius().x, src->getRadius().y);
}




INLINE ProjCollisionType FinalWormCollisionCheck(CProjectile* proj, const CVec& vFrameOldPos, const CVec& vFrameOldVel, TimeDiff dt, ProjCollisionType curResult) {
	CMap* map = game.gameMap();
	
	// do we get any worm?
	if(proj->GetProjInfo()->PlyHit.Type != PJ_NOTHING) {
		CVec dif = proj->getPos() - vFrameOldPos;
		float len = NormalizeVector( &dif );
		len = MIN(len, float(game.gameMap()->GetWidth()) + float(game.gameMap()->GetHeight()));
		
		// the worm has a size of 4*4 in ProjWormColl, so it's save to check every second pixel here
		for (float p = 0.0f; p <= len; p += 2.0f) {
			CVec curpos = vFrameOldPos + dif * p;
			
			int ret = proj->ProjWormColl(curpos);
			if (ret >= 0)  {
				if(proj->GetProjInfo()->PlyHit.Type != PJ_GOTHROUGH) {
					proj->setPos( curpos ); // save the new position at the first collision
					proj->vOldPos = curpos;
					proj->setVelocity( vFrameOldVel ); // don't get faster
				}

				if(cClient->getGameLobby()[FT_InfiniteMap]) {
					FMOD(proj->vPos.write().x, (float)map->GetWidth());
					FMOD(proj->vPos.write().y, (float)map->GetHeight());
					FMOD(proj->vOldPos.x, (float)map->GetWidth());
					FMOD(proj->vOldPos.y, (float)map->GetWidth());		
				}
				
				return ProjCollisionType::Worm(ret);
			}
		}
	}
	
	if(cClient->getGameLobby()[FT_InfiniteMap]) {
		FMOD(proj->vPos.write().x, (float)map->GetWidth());
		FMOD(proj->vPos.write().y, (float)map->GetHeight());
		FMOD(proj->vOldPos.x, (float)map->GetWidth());
		FMOD(proj->vOldPos.y, (float)map->GetHeight());		
	}

	return curResult;
}


///////////////////////
// Checks for collision with the level border
INLINE bool CProjectile::MapBoundsCollision(int px, int py)
{
	CollisionSide = 0;
	if(cClient->getGameLobby()[FT_InfiniteMap]) return false;
	
	CMap* map = game.gameMap();
	
	if (px < 0 || px - radius.x < 0)
		CollisionSide |= COL_LEFT;
	
	if (px >= (int)map->GetWidth() || px + radius.x >= (int)map->GetWidth())
		CollisionSide |= COL_RIGHT;
	
	if (py < 0 || py - radius.y < 0)
		CollisionSide |= COL_TOP;
	
	if (py >= (int)map->GetHeight() || py + radius.y >= (int)map->GetHeight())
		CollisionSide |= COL_BOTTOM;
	
	return CollisionSide != 0;
}


INLINE static void handlePixelFlag(CProjectile::ColInfo& res, const Material& m, int x, int y, int cx, int cy) {
	// Solid pixel
	if(!m.particle_pass) {
		if (y < cy)
			++res.top;
		else if (y > cy)
			++res.bottom;
		if (x < cx)
			++res.left;
		else if (x > cx)
			++res.right;
		
		if (!m.destroyable)
			res.onlyDirt = false;
		res.collided = true;
	}	
}

////////////////////////////
// Checks for collision with the terrain
// WARNING: assumed to be called only from SimulateFrame
INLINE CProjectile::ColInfo CProjectile::TerrainCollision(int px, int py)
{
	CMap* map = game.gameMap();
	
	ColInfo res = { 0, 0, 0, 0, false, true };

	const bool wrapAround = cClient->getGameLobby()[FT_InfiniteMap];

	// check for most common case - we do this because compiler can probably optimise this case very good
	if(!wrapAround && tProjInfo->Type != PRJ_CIRCLE) {	
		// Check for the collision
		for(int y = py - radius.y; y <= py + radius.y; ++y) {
			// this is safe because in SimulateFrame, we do map bound checks
			uchar *pf = &map->material->line[y][px - radius.x];
			
			for(int x = px - radius.x; x <= px + radius.x; ++x, ++pf) {				
				handlePixelFlag(res, map->materialForIndex(*pf), x, y, px, py);
			}
		}
	}
	else if(wrapAround) {
		px %= map->GetWidth();
		py %= map->GetHeight();
		if(px < 0) px += map->GetWidth();
		if(py < 0) py += map->GetHeight();
		
		// Check for the collision
		for(int _y = - radius.y; _y <= radius.y; ++_y) {
			int y = (map->GetHeight() + _y + py) % (long)map->GetHeight();
			uchar *_pf = map->material->line[y];
			
			for(int _x = - radius.x; _x <= radius.x; ++_x) {
				if(tProjInfo->Type == PRJ_CIRCLE && VectorD2<int>(_x,_y).GetLength2() > radius.GetLength2())
					// outside the range, skip this
					continue;
				
				int x = (map->GetWidth() + _x + px) % (long)map->GetWidth();
				uchar* pf = _pf + x;
				
				handlePixelFlag(res, map->materialForIndex(*pf), x, y, px, py);
			}
		}
	}
	else { // circle
		// Check for the collision
		for(int y = py - radius.y; y <= py + radius.y; ++y) {
			// this is safe because in SimulateFrame, we do map bound checks
			uchar *pf = &map->material->line[y][px - radius.x];
			
			for(int x = px - radius.x; x <= px + radius.x; ++x, ++pf) {				
				if(VectorD2<int>(x - px,y - py).GetLength2() > radius.GetLength2())
					// outside the range, skip this
					continue;

				handlePixelFlag(res, map->materialForIndex(*pf), x, y, px, py);
			}
		}		
	}
	
	return res;
}

////////////////////////
// Handle the terrain collsion (helper function)
// returns false if collision should be ignored
bool CProjectile::HandleCollision(const CProjectile::ColInfo &c, const CVec& oldpos, const CVec& oldvel, TimeDiff dt)
{
	
	if(tProjInfo->Hit.Type == PJ_EXPLODE && c.onlyDirt) {
		// HINT: don't reset vPos here, because we want
		//		the explosion near (inside) the object
		//		this behavior is the same as in original LX
		return true;
	}
	
	bool bounce = false;
	
	// Bit of a hack
	switch (tProjInfo->Hit.Type)  {
	case PJ_BOUNCE:
			// HINT: don't reset vPos here; it will be reset,
			//		depending on the collisionside
		bounce = true;
		break;
	case PJ_NOTHING:  // PJ_NOTHING projectiles go through walls (but a bit slower)
		vPos = oldpos + (vVelocity * dt.seconds()) * 0.5f;
		vOldPos = vPos; // TODO: this is a hack; we do it to not go back to real old position because of collision
			// HINT: The above velocity reduction is not exact. SimulateFrame is also executed only for one checkstep because of the collision.
		break;
	case PJ_GOTHROUGH:
		vPos = oldpos + (vVelocity * dt.seconds()) * tProjInfo->Hit.GoThroughSpeed;
		return false; // ignore collision
	default:
		vPos = vOldPos;
		vVelocity = oldvel;
		return true;
	}
	
	int vx = (int)vVelocity.get().x;
	int vy = (int)vVelocity.get().y;
	
	// Find the collision side
	if ((c.left > c.right || c.left > 2) && c.left > 1 && vx <= 0) {
		if(bounce)
			vPos.write().x = oldpos.x;
		if (vx)
			CollisionSide |= COL_LEFT;
	}
	
	if ((c.right > c.left || c.right > 2) && c.right > 1 && vx >= 0) {
		if(bounce)
			vPos.write().x = oldpos.x;
		if (vx)
			CollisionSide |= COL_RIGHT;
	}
	
	if (c.top > 1 && vy <= 0) {
		if(bounce)
			vPos.write().y = oldpos.y;
		if (vy)
			CollisionSide |= COL_TOP;
	}
	
	if (c.bottom > 1 && vy >= 0) {
		if(bounce)
			vPos.write().y = oldpos.y;
		if (vy)
			CollisionSide |= COL_BOTTOM;
	}
	
	// If the velocity is too low, just stop me
	if (abs(vx) < 2)
		vVelocity.write().x = 0;
	if (abs(vy) < 2)
		vVelocity.write().y = 0;
	
	return true;
}

///////////////////
// Check for a collision, updates velocity and position
// TODO: move to physicsengine
// TODO: we need one single CheckCollision which is used everywhere in the code
// atm we have two CProj::CC, Map:CC and ProjWormColl and fastTraceLine
// we should complete the function in CMap.cpp in a general way by using fastTraceLine
// also dt shouldn't be a parameter, you should specify a start- and an endpoint
// (for example CWorm_AI also uses this to check some possible cases)
INLINE ProjCollisionType LX56Projectile_checkCollAndMove_Frame(CProjectile* const prj, TimeDiff dt, CMap *map)
{
	// Gravity
	float fGravity = 100.0f; // Default
	if (prj->getProjInfo()->UseCustomGravity)
		fGravity = (float)prj->getProjInfo()->Gravity;
	prj->vVelocity.write().y += (float)cClient->getGameLobby()[FT_ProjGravityFactor] * fGravity * dt.seconds();

	{
		const float friction = cClient->getGameLobby()[FT_ProjFriction];
		if(friction > 0) {
			const float projSize = (prj->getRadius().x + prj->getRadius().y) * 0.5f;
			const float projMass = prj->getRadius().GetLength();
			// A bit lower drag coefficient as for worms because normally, projectiles have better shape for less dragging.
			static const float projDragCoeff = 0.02f; // Note: Never ever change this! (Or we have to make this configureable)
			applyFriction(prj->vVelocity.write(), dt.seconds(), projSize, projMass, projDragCoeff, friction);
		}
	}
	
	CVec vOldVel = prj->getVelocity();

	CVec vFrameOldPos = prj->vPos;
	prj->vPos += prj->vVelocity * dt.seconds();
	
	// if distance is to short to last check, just return here without a check
	if ((int)(prj->vOldPos - prj->vPos).GetLength2() < prj->MIN_CHECKSTEP2) {
/*		printf("pos dif = %f , ", (vOldPos - vPos).GetLength());
		printf("len = %f , ", sqrt(len));
		printf("vel = %f , ", vVelocity.GetLength());
		printf("mincheckstep = %i\n", MIN_CHECKSTEP);	*/
		return FinalWormCollisionCheck(prj, vFrameOldPos, vOldVel, dt, ProjCollisionType::NoCol());
	}
	
	int px = (int)(prj->vPos.get().x);
	int py = (int)(prj->vPos.get().y);
	
	// Hit edges
	if (prj->MapBoundsCollision(px, py))  {
		prj->vPos = prj->vOldPos;
		prj->vVelocity = vOldVel;
		
		return FinalWormCollisionCheck(prj, vFrameOldPos, vOldVel, dt, ProjCollisionType::Terrain(PJC_TERRAIN|PJC_MAPBORDER));
	}
	
	// Make wallshooting possible
	// NOTE: wallshooting is a bug in old LX physics that many players got used to
	if (prj->fLastSimulationTime <= prj->fSpawnTime + TimeDiff(prj->fWallshootTime))
		return FinalWormCollisionCheck(prj, vFrameOldPos, vOldVel, dt, ProjCollisionType::NoCol());
	
	// Check collision with the terrain
	CProjectile::ColInfo c = prj->TerrainCollision(px, py);
	
	// Check for a collision
	if(c.collided && prj->HandleCollision(c, vFrameOldPos, vOldVel, dt)) {
		int colmask = PJC_TERRAIN;
		if(c.onlyDirt) colmask |= PJC_DIRT;
		return FinalWormCollisionCheck(prj, vFrameOldPos, vOldVel, dt, ProjCollisionType::Terrain(colmask));
	}
	
	// the move was safe, save the position
	prj->vOldPos = prj->vPos;
	
	return FinalWormCollisionCheck(prj, vFrameOldPos, vOldVel, dt, ProjCollisionType::NoCol());
}


INLINE ProjCollisionType LX56Projectile_checkCollAndMove(CProjectile* const prj, TimeDiff dt, CMap *map) {
	// Check if we need to recalculate the checksteps (projectile changed its velocity too much)
	if (prj->bChangesSpeed)  {
		const int len = (int)prj->vVelocity.get().GetLength2();
		if (abs(len - prj->iCheckSpeedLen) > 50000)
			prj->CalculateCheckSteps();
	}
	
	/*
	 In LX56, we skipped the simulation into half if dt >= 0.015f.
	 The dt we have here is usually lower (at least LX56PhysicsDT is), so it's like
	 LX56 to do the damping just once here.
	 */
	
	{
		// Dampening
		const float dmp = prj->getProjInfo()->Dampening;
		if(dmp != 1.0f) {
			// dt is not fixed (because of possible gamespeed factor)
			if(dt == LX56PhysicsDT)
				prj->vVelocity *= dmp;
			else if(dt > 0)
				prj->vVelocity *= powf(dmp, dt.seconds() / LX56PhysicsDT.seconds());
			else if(dt < 0)
				// doesn't make sense to do negative dampening...
				prj->vVelocity *= powf(dmp, -dt.seconds() / LX56PhysicsDT.seconds());
		}
	}

	TimeDiff cstep;
	{
		float checkstep = prj->vVelocity.get().GetLength2(); // |v|^2
		
		if((int)(checkstep * dt.seconds() * dt.seconds()) <= prj->MAX_CHECKSTEP2) // |dp|^2=|v*dt|^2
			return LX56Projectile_checkCollAndMove_Frame(prj, dt, map);
		
		// calc new dt, so that we have |v*dt|=AVG_CHECKSTEP
		// checkstep is new dt
		checkstep = (float)prj->AVG_CHECKSTEP / sqrt(checkstep);
		checkstep = MAX(checkstep, 0.001f);
		cstep = TimeDiff(checkstep);
	}
	
	// In some bad cases (float accurance problems mainly),
	// it's possible that checkstep >= dt .
	// If we would not check this case, we get in an infinie
	// recursive loop.
	// Therefore if this is the case, we don't do multiple checksteps.
	if(cstep < dt) {
		for(TimeDiff time; time < dt; time += cstep) {
			ProjCollisionType ret = LX56Projectile_checkCollAndMove_Frame(prj, (time + cstep > dt) ? dt - time : cstep, map);
			if(ret) return ret;
		}
		
		return ProjCollisionType::NoCol();
	}
	
	return LX56Projectile_checkCollAndMove_Frame(prj, dt, map);
}



int Proj_SpawnParent::ownerWorm() const {
	switch(type) {
		case PSPT_NOTHING: return -1;
		case PSPT_SHOT: return shot->nWormID;
		case PSPT_PROJ: return proj->GetOwner();
	}
	return -1;
}

int Proj_SpawnParent::fixedRandomIndex() const {
	switch(type) {
		case PSPT_NOTHING: return -1;
		case PSPT_SHOT: return shot->nRandom;
		case PSPT_PROJ: return proj->getRandomIndex() + 1;
	}
	return -1;
}

float Proj_SpawnParent::fixedRandomFloat() const {
	switch(type) {
		case PSPT_NOTHING: return -1;
		case PSPT_SHOT: return GetFixedRandomNum(shot->nRandom);
		case PSPT_PROJ: return proj->getRandomFloat();
	}
	return -1;
}

CVec Proj_SpawnParent::position() const {
	switch(type) {
		case PSPT_NOTHING: return CVec(0,0);
		case PSPT_SHOT: return shot->cPos + GetVecFromAngle((float)shot->nAngle) * (float)cClient->getGameLobby()[FT_ShootSpawnDistance];
		case PSPT_PROJ: return proj->getPos();
	}
	return CVec(0,0);
}

CVec Proj_SpawnParent::velocity() const {
	switch(type) {
		case PSPT_NOTHING: return CVec(0,0);
		case PSPT_SHOT: return shot->cWormVel;
		case PSPT_PROJ: return proj->getVelocity();
	}
	return CVec(0,0);
}

float Proj_SpawnParent::angle() const {
	switch(type) {
		case PSPT_NOTHING: return 0;
		case PSPT_SHOT: return (float)shot->nAngle;
		case PSPT_PROJ: {
			CVec v = velocity();
			NormalizeVector(&v);
			float heading = (float)( -atan2(v.x,v.y) * (180.0f/PI) );
			heading+=90;
			FMOD(heading, 360.0f);
			return heading;
		}
	}
	return 0;
}


void Proj_SpawnInfo::dump() const {
	if(Proj)
		notes << "spawn: " << Amount << " of " << Proj->filename << endl;
}

void Proj_SpawnInfo::apply(Proj_SpawnParent parent, AbsTime spawnTime) const {
	// Calculate the angle of the direction the projectile is heading
	const float heading = Useangle ? parent.angle() : 0;
	
	// We figure out the weapon that created this projectile
	if(parent.type == Proj_SpawnParent::PSPT_SHOT) {
		// The projectile came straight from a weapon(shot)
		const weapon_t *wpn = game.gameScript()->GetWeapons() + parent.shot->nWeapon;
		Proj->WeaponName = wpn->Name;
	} else if(parent.type == Proj_SpawnParent::PSPT_PROJ) {
		// Another projectile caused this projectile so the weapon that created that
		// projectile is responsible for this one as well.
		Proj->WeaponName = parent.proj->getProjInfo()->WeaponName;
	}

	for(int i = 0; i < Amount; i++) {
		const CVec sprd = UseParentVelocityForSpread ?
			(parent.velocity() * ParentVelSpreadFactor)
		:
			// NOTE: It was a float -> int -> float conversion before (in LX56). this changed now, we just keep float!
			GetVecFromAngle(
							(float)Angle +
							heading +
							parent.fixedRandomFloat() * (float)Spread );
		
		int rot = 0;
		if(UseRandomRot) {
			// Calculate a random starting angle for the projectile rotation (if used)
			if(Proj->Rotating) {
				// Prevent div by zero
				if(Proj->RotIncrement == 0)
					Proj->RotIncrement = 1;
				rot = GetRandomInt( abs( 360 / Proj->RotIncrement ) ) * Proj->RotIncrement;
			}
		}
		
		if(parent.type == Proj_SpawnParent::PSPT_SHOT) {
			parent.shot->nRandom++;
			parent.shot->nRandom %= 255;
		}
		
		const CVec& speedVarVec = UseSpecial11VecForSpeedVar ? CVec(1,1) : sprd;
		const CVec v =
			sprd * (float)Speed +
			speedVarVec * (float)SpeedVar * parent.fixedRandomFloat() +
			( AddParentVel
			  ? ParentVelFactor * parent.velocity() : CVec(0,0));
		
		if(parent.type == Proj_SpawnParent::PSPT_SHOT) {
			parent.shot->nRandom *= 5;
			parent.shot->nRandom %= 255;
		}
		
		const AbsTime ignoreWormCollBeforeTime =
			(parent.type == Proj_SpawnParent::PSPT_PROJ) ?
				parent.proj->getIgnoreWormCollBeforeTime()
			:
				// we set the ignoreWormCollBeforeTime to the current time to let the physics engine
				// first emulate the projectiles to the curtime and ignore earlier colls as the worm-pos
				// is probably outdated at this time
				(GetPhysicsTime() + 0.1f); // HINT: we add 100ms (it was dt before) because the projectile is spawned -> worms are simulated (pos change) -> projectiles are simulated
		
		const int random = parent.fixedRandomIndex();
		
		CVec pos = parent.position() + PosDiff;
		if(SnapToGrid.x >= 1 && SnapToGrid.y >= 1) {
			pos.x -= (int)pos.x % SnapToGrid.x; pos.x += SnapToGrid.x / 2;
			pos.y -= (int)pos.y % SnapToGrid.y; pos.y += SnapToGrid.y / 2;
		}
		
		cClient->SpawnProjectile(pos, v, rot, parent.ownerWorm(), Proj, random, spawnTime, ignoreWormCollBeforeTime);
		
		if(parent.type == Proj_SpawnParent::PSPT_SHOT) {
			parent.shot->nRandom++;
			parent.shot->nRandom %= 255;
		}
	}
	
}

Proj_Action& Proj_Action::operator=(const Proj_Action& a) {
	if(additionalAction) delete additionalAction;
	additionalAction = NULL;
	Type = a.Type;
	Damage = a.Damage;
	Projectiles = a.Projectiles;
	Shake = a.Shake;
	UseSound = a.UseSound;
	SndFilename = a.SndFilename;
	BounceCoeff = a.BounceCoeff;
	BounceExplode = a.BounceExplode;
	Proj = a.Proj;
	Sound = a.Sound;
	GoThroughSpeed = a.GoThroughSpeed;
	ChangeRadius = a.ChangeRadius;
	Speed = a.Speed;
	SpeedMult = a.SpeedMult;
	if(a.additionalAction) additionalAction = new Proj_Action(*a.additionalAction);
	
	return *this;
}


static VectorD2<float> objectAngleDiff(CGameObject* w, CGameObject* prj) {
	CVec diff = w->getPos() - prj->getPos(); NormalizeVector(&diff);
	return MatrixD2<float>::Rotation(diff.x, -diff.y) * prj->getVelocity();
}

static CWorm* nearestWorm(CVec pos) {
	CWorm* best = NULL;
	for_each_iterator(CWorm*, w, game.aliveWorms()) {
		if(best == NULL) { best = w->get(); continue; }
		if((best->getPos() - pos).GetLength2() > (w->get()->getPos() - pos).GetLength2())
			best = w->get();
	}
	return best;
}

static CWorm* nearestOtherWorm(CVec pos, int worm) {
	CWorm* best = NULL;
	for_each_iterator(CWorm*, w, game.aliveWorms()) {
		if(w->get()->getID() == worm) continue;
		if(best == NULL) { best = w->get(); continue; }
		if((best->getPos() - pos).GetLength2() > (w->get()->getPos() - pos).GetLength2())
			best = w->get();
	}
	return best;
}

static CWorm* nearestEnemyWorm(CVec pos, int worm) {
	const int team = game.ifWorm<int>(worm, &CWorm::getTeam, -1);
	CWorm* best = NULL;
	for_each_iterator(CWorm*, w, game.aliveWorms()) {
		if(w->get()->getID() == worm) continue;
		if(cClient->isTeamGame() && team == w->get()->getTeam()) continue;
		if(best == NULL) { best = w->get(); continue; }
		if((best->getPos() - pos).GetLength2() > (w->get()->getPos() - pos).GetLength2())
			best = w->get();
	}
	return best;
}

static CWorm* nearestTeamMate(CVec pos, int worm) {
	if(!cClient->isTeamGame()) return NULL;
	const int team = game.ifWorm<int>(worm, &CWorm::getTeam, -1);
	CWorm* best = NULL;
	for_each_iterator(CWorm*, w, game.aliveWorms()) {
		if(w->get()->getID() == worm) continue;
		if(team != w->get()->getTeam()) continue;
		if(best == NULL) { best = w->get(); continue; }
		if((best->getPos() - pos).GetLength2() > (w->get()->getPos() - pos).GetLength2())
			best = w->get();
	}
	return best;
}

static VectorD2<float> getHeadingVelTo(CGameObject* target, CGameObject* src) {
	if(target == NULL) return VectorD2<float>();

	VectorD2<float> ret = src->getVelocity().orthogonal();
	NormalizeVector(&ret);
	
	VectorD2<float> angleDiffV = objectAngleDiff(target, src);
	float angleDiff = atan2f(angleDiffV.y, angleDiffV.x);
	if(angleDiff >= float(PI)) angleDiff -= float(PI) * 2;
	ret *= angleDiff / float(PI);
	
	return ret;
}

void Proj_Action::applyTo(const Proj_EventOccurInfo& eventInfo, CProjectile* prj, Proj_DoActionInfo* info) const {
	/*
	 * Well, some behaviour here seems strange, but it's all *100% exact* LX56 behaviour.
	 * Please, before touching anything, be very sure that it stays exactly the same!
	 * If you think something is wrong here, check the revision log from this file and
	 * from PhysicsLX56.cpp *before* changing it here!
	 * See also CGameScript.cpp because we enforce some specific values in some cases.
	 */
	
	bool push_worm = true;
	bool spawnprojs = Projectiles;
	
	switch (Type)  {
		// Explosion
		case PJ_EXPLODE:
			info->explode = true;
			info->damage = Damage;
			
			if(eventInfo.timerHit)
				info->timer = true;
			
			if(Shake > info->shake)
				info->shake = Shake;
			
			// Play the hit sound
			if(UseSound && Sound)
				info->sound = Sound;
			break;
			
		// Bounce
		case PJ_BOUNCE:
			if(eventInfo.timerHit) { spawnprojs = false; break; }
			push_worm = false;
			prj->Bounce(BounceCoeff);
			
			// Do we do a bounce-explosion (bouncy larpa uses this)
			if(BounceExplode > 0)
				cClient->Explosion(prj->fLastSimulationTime, prj->getPos(), (float)BounceExplode, false, prj->GetOwner(), prj->GetProjInfo()->WeaponName);
			break;
			
		// Carve
		case PJ_CARVE:
			if(eventInfo.timerHit || (eventInfo.colType && !eventInfo.colType->withWorm)) {
				int d = game.gameMap()->CarveHole(Damage, prj->getPos(), cClient->getGameLobby()[FT_InfiniteMap]);
				info->deleteAfter = true;
				
				// Increment the dirt count
				if(prj->hasOwner()) {
					CWorm* w = game.wormById(prj->GetOwner(), false);
					if(w) w->incrementDirtCount( d );
				}
			}
			break;
			
		// Dirt
		case PJ_DIRT:
			info->dirt = true;
			if(eventInfo.timerHit && Shake > info->shake)
				info->shake = Shake;
			break;
			
		// Green Dirt
		case PJ_GREENDIRT:
			info->grndirt = true;
			if(eventInfo.timerHit && Shake > info->shake)
				info->shake = Shake;
			break;
			
		case PJ_DISAPPEAR2:
			info->deleteAfter = true;
			break;
		
		case PJ_INJUREWORM:
			if(eventInfo.colType && eventInfo.colType->withWorm) {
				CWorm* w = game.wormById(eventInfo.colType->wormId, false);
				if(w) cClient->InjureWorm(w, (float)Damage, prj->GetOwner(), prj->GetProjInfo()->WeaponName);
			}			
			break;
				
		case PJ_INJURE:
			if(eventInfo.colType && eventInfo.colType->withWorm) {
				info->deleteAfter = true;
				CWorm* w = game.wormById(eventInfo.colType->wormId, false);
				if(w) cClient->InjureWorm(w, (float)Damage, prj->GetOwner(), prj->GetProjInfo()->WeaponName);
				break;
			}
			
		case PJ_DISAPPEAR:
			// TODO: do something special?
			if(eventInfo.colType && eventInfo.colType->withWorm) break;
			
		case PJ_GOTHROUGH:
		case PJ_NOTHING:
			// if Hit_Type == PJ_NOTHING, it means that this projectile goes through all walls
			if(eventInfo.colType && !eventInfo.colType->withWorm && (eventInfo.colType->colMask & PJC_MAPBORDER)) {
				// HINT: This is new since Beta9. I hope it doesn't change any serious behaviour.
				// It means, if such a projectile which goes through everything hits the mapborder, it will
				// get deleted - otherwise it would be stuck there at the border.
				info->deleteAfter = true;
			}
			push_worm = false;
			if(eventInfo.timerHit) spawnprojs = false;
			break;
			
		case PJ_INJUREPROJ:
			for(Proj_EventOccurInfo::Targets::const_iterator p = eventInfo.targets.begin(); p != eventInfo.targets.end(); ++p) {
				if(typeid(**p) == typeid(CProjectile))
					(*p)->injure((float)Damage);
			}
			break;
			
		case PJ_PLAYSOUND:
			if(UseSound && Sound)
				info->sound = Sound;
			break;
		
		case PJ_ChangeRadius:
			prj->radius += ChangeRadius;
			if(prj->radius.x < 0) prj->radius.x = 0;
			if(prj->radius.y < 0) prj->radius.y = 0;
			break;
			
		case PJ_OverwriteOwnSpeed:
			prj->setVelocity(Speed);
			break;
			
		case PJ_MultiplyOwnSpeed:
			prj->setVelocity( SpeedMult * prj->getVelocity() );
			break;
			
		case PJ_DiffOwnSpeed:
			prj->setVelocity( prj->getVelocity() + Speed );
			break;
			
		case PJ_OverwriteTargetSpeed:
			for(Proj_EventOccurInfo::Targets::const_iterator p = eventInfo.targets.begin(); p != eventInfo.targets.end(); ++p) {
				(*p)->setVelocity(Speed);
			}
			break;
			
		case PJ_MultiplyTargetSpeed:
			for(Proj_EventOccurInfo::Targets::const_iterator p = eventInfo.targets.begin(); p != eventInfo.targets.end(); ++p) {
				(*p)->setVelocity( SpeedMult * (*p)->getVelocity() );
			}
			break;
			
		case PJ_DiffTargetSpeed:
			for(Proj_EventOccurInfo::Targets::const_iterator p = eventInfo.targets.begin(); p != eventInfo.targets.end(); ++p) {
				(*p)->setVelocity( (*p)->getVelocity() + Speed );
			}
			break;
			
		case PJ_HeadingToNextWorm:
			prj->velocity() += SpeedMult * getHeadingVelTo(nearestWorm(prj->getPos()), prj);
			break;
			
		case PJ_HeadingToOwner:
			if(prj->GetOwner() >= 0 && prj->GetOwner() < MAX_WORMS) {
				CWorm* w = game.wormById(prj->GetOwner(), false);
				if(w)
					prj->velocity() += SpeedMult * getHeadingVelTo(w, prj);
			}
			break;
			
		case PJ_HeadingToNextOtherWorm:
			prj->velocity() += SpeedMult * getHeadingVelTo(nearestOtherWorm(prj->getPos(), prj->GetOwner()), prj);
			break;
			
		case PJ_HeadingToNextEnemyWorm:
			prj->velocity() += SpeedMult * getHeadingVelTo(nearestEnemyWorm(prj->getPos(), prj->GetOwner()), prj);
			break;
			
		case PJ_HeadingToNextTeamMate:
			prj->velocity() += SpeedMult * getHeadingVelTo(nearestTeamMate(prj->getPos(), prj->GetOwner()), prj);
			break;
			
		case PJ_HeadTargetToUs:
			for(Proj_EventOccurInfo::Targets::const_iterator obj = eventInfo.targets.begin(); obj != eventInfo.targets.end(); ++obj) {
				(*obj)->velocity() += SpeedMult * getHeadingVelTo(prj, *obj);
			}
			break;
			
		case __PJ_LBOUND: case __PJ_UBOUND:
			errors << "Proj_Action::applyTo: hit __PJ_BOUND" << endl;
			break;
	}
	
	// Push the worm back
	if(push_worm && eventInfo.colType && eventInfo.colType->withWorm) {
		CVec d = prj->getVelocity();
		NormalizeVector(&d);
		CWorm* w = game.wormById(eventInfo.colType->wormId, false);
		if(w)
			w->velocity() += (d * 100) * eventInfo.dt.seconds();
	}
	
	if(spawnprojs) {
		if(Proj.isSet())
			info->otherSpawns.push_back(&Proj);
		else
			info->spawnprojectiles = true;
	}
	
	if(additionalAction)
		additionalAction->applyTo(eventInfo, prj, info);
}



bool Proj_LX56Timer::checkEvent(Proj_EventOccurInfo& eventInfo, CProjectile* prj, Proj_DoActionInfo*) const {
	float f = prj->getTimeVarRandom();
	if(Time > 0 && (Time + TimeVar * f) < prj->getLife()) {
		eventInfo.timerHit = true;
		return true;
	}
	return false;
}

bool Proj_TimerEvent::checkEvent(Proj_EventOccurInfo& eventInfo, CProjectile* prj, Proj_DoActionInfo*) const {
	ProjTimerState& state = prj->timerInfo[this];
	if(state.c > 0 && !Repeat) return PermanentMode == 1;
	
	if(UseGlobalTime) {
		float cur = eventInfo.serverTime.seconds() * (float)cClient->getGameLobby()[FT_GameSpeed];
		if(state.c == 0) {
			float startTime = cur - prj->getLife();
			float mstart = startTime; FMOD(mstart, Delay);
			float next = startTime - mstart + Delay;
			if(cur >= next) {
				state.c++;
				state.last = cur;
				if(PermanentMode == 0) return true;
			}
		}
		else {
			float mcur = state.last; FMOD(mcur, Delay);
			float next = state.last - mcur + Delay;
			if(cur >= next) {
				state.c++;
				state.last = cur;
				if(PermanentMode == 0) return true;
			}
		}
	}
	else { // not global time
		if(state.last + Delay <= prj->getLife()) {
			state.c++;
			state.last = prj->getLife();
			if(PermanentMode == 0) return true;
		}
	}
	
	if(PermanentMode > 0)
		return state.c % (PermanentMode + 1) != 0;
	else if(PermanentMode < 0)
		return state.c % (-PermanentMode + 1) == 0;
	return false;
}


static INLINE bool checkProjHit(const Proj_ProjHitEvent& info, std::set<CGameObject*>& projs, CProjectile* prj, CProjectile* p) {
	if(p == prj) return true;
	if(info.Target && p->getProjInfo() != info.Target) return true;
	if(!info.ownerWorm.match(prj->GetOwner(), p)) return true;
	if(info.TargetHealthIsMore && p->getHealth() <= prj->getHealth()) return true;
	if(info.TargetHealthIsLess && p->getHealth() >= prj->getHealth()) return true;
	if(info.TargetTimeIsMore && p->getLife() <= prj->getLife()) return true;
	if(info.TargetTimeIsLess && p->getLife() >= prj->getLife()) return true;
	
	if(info.Width >= 0 && info.Height >= 0) { if(!CProjectile_CollisionWith(prj, p, info.Width/2, info.Height/2)) return true; }
	else { if(!CProjectile_CollisionWith(prj, p)) return true; }
	
	projs.insert(p);
	
	if(info.MaxHitCount < 0 && projs.size() >= (size_t)info.MinHitCount) return false; // no need to check further
	if(projs.size() > (size_t)info.MaxHitCount) return false; // no need to check further
	
	return true;
}

template<bool TOP, bool LEFT>
static CClient::MapPosIndex MPI(const VectorD2<int>& p, const VectorD2<int>& r) {
	return CClient::MapPosIndex( p + VectorD2<int>(LEFT ? -r.x : r.x, TOP ? -r.y : r.y) );
}

bool Proj_ProjHitEvent::checkEvent(Proj_EventOccurInfo& ev, CProjectile* prj, Proj_DoActionInfo*) const {
	const VectorD2<int> vPos = prj->getPos();
	const VectorD2<int> radius = prj->getRadius();
	for(int x = MPI<true,true>(vPos,radius).x; x <= MPI<true,false>(vPos,radius).x; ++x)
		for(int y = MPI<true,true>(vPos,radius).y; y <= MPI<false,true>(vPos,radius).y; ++y) {
			CClient::ProjectileSet* projs = cClient->projPosMap[CClient::MapPosIndex(x,y).index(game.gameMap())];
			if(projs == NULL) continue;
			for(CClient::ProjectileSet::const_iterator p = projs->begin(); p != projs->end(); ++p)
				if(!checkProjHit(*this, ev.targets, prj, *p)) goto finalChecks;
		}
	
finalChecks:
	if(ev.targets.size() >= (size_t)MinHitCount && (MaxHitCount < 0 || ev.targets.size() <= (size_t)MaxHitCount))
		return true;
	return false;
}



bool Proj_WormHitEvent::canMatch() const {
	if(SameWormAsProjOwner) {
		if(DiffWormAsProjOwner || DiffTeamAsProjOwner || EnemyOfProjOwner) return false;
		return true;
	}
	
	if(SameTeamAsProjOwner) {
		if(DiffTeamAsProjOwner) return false;
		if(EnemyOfProjOwner) return false;
		return true;
	}
	
	if(TeamMateOfProjOwner) return !EnemyOfProjOwner;
	
	return true;
}

bool Proj_WormHitEvent::match(int worm, CProjectile* prj) const {
	if(SameWormAsProjOwner && prj->GetOwner() != worm) return false;
	if(DiffWormAsProjOwner && prj->GetOwner() == worm) return false;
	
	const int team = game.ifWorm<int>(worm, &CWorm::getTeam, -1);
	const int projTeam = game.ifWorm<int>(prj->GetOwner(), &CWorm::getTeam, -1);
	if(SameTeamAsProjOwner && projTeam != team) return false;
	if(DiffTeamAsProjOwner && projTeam == team) return false;
	
	if(TeamMateOfProjOwner && !cClient->isTeamGame() && prj->GetOwner() != worm) return false;
	if(TeamMateOfProjOwner && cClient->isTeamGame() && projTeam != team) return false;
	if(EnemyOfProjOwner && prj->GetOwner() == worm) return false;
	if(EnemyOfProjOwner && cClient->isTeamGame() && projTeam == team) return false;
	
	return true;
}

bool Proj_WormHitEvent::checkEvent(Proj_EventOccurInfo& eventInfo, CProjectile* prj, Proj_DoActionInfo*) const {
	if(eventInfo.colType == NULL || !eventInfo.colType->withWorm) return false;
	if(match(eventInfo.colType->wormId, prj)) {
		CWorm* w = game.wormById(eventInfo.colType->wormId, false);
		if(w)
			eventInfo.targets.insert(w);
		return true;
	}
	return false;
}



bool Proj_TerrainHitEvent::checkEvent(Proj_EventOccurInfo& eventInfo, CProjectile* prj, Proj_DoActionInfo*) const {
	if(eventInfo.colType == NULL || eventInfo.colType->withWorm) return false;
	
	const int colMask = eventInfo.colType->colMask;
	if(MapBound && (colMask & PJC_MAPBORDER) == PJC_NONE) return false;
	if(Dirt && (colMask & PJC_DIRT) == PJC_NONE) return false;
	if(Rock && colMask != PJC_TERRAIN) return false;
	
	return true;
}


bool Proj_DeathEvent::checkEvent(Proj_EventOccurInfo& eventInfo, CProjectile* prj, Proj_DoActionInfo*) const {
	return prj->getHealth() < 0;
}

bool Proj_FallbackEvent::checkEvent(Proj_EventOccurInfo& eventInfo, CProjectile* prj, Proj_DoActionInfo* info) const {
	return !info->hasAnyEffect();
}





static void projectile_doExplode(CProjectile* const prj, int damage, int shake) {
	// Explosion
	if(damage != -1) // TODO: why only with -1?
		cClient->Explosion(prj->fLastSimulationTime, prj->getPos(), (float)damage, shake, prj->GetOwner(), prj->GetProjInfo()->WeaponName);
}

static void projectile_doTimerExplode(CProjectile* const prj, int shake) {
	const proj_t *pi = prj->GetProjInfo();
	// Explosion
	int damage = pi->Timer.Damage;
	if(pi->PlyHit.Type == PJ_EXPLODE)
		damage = pi->PlyHit.Damage;
	
	if(damage != -1) // TODO: why only with -1?
		cClient->Explosion(prj->fLastSimulationTime, prj->getPos(), (float)damage, shake, prj->GetOwner(), prj->GetProjInfo()->WeaponName);
}

static void projectile_doMakeDirt(CProjectile* const prj) {
	const int damage = 5;
	int d = 0;
	d += game.gameMap()->PlaceDirt(damage,prj->getPos()-CVec(6,6));
	d += game.gameMap()->PlaceDirt(damage,prj->getPos()+CVec(6,-6));
	d += game.gameMap()->PlaceDirt(damage,prj->getPos()+CVec(0,6));
	
	// Remove the dirt count on the worm
	if(prj->hasOwner()) {
		CWorm* w = game.wormById(prj->GetOwner(), false);
		if(w) w->incrementDirtCount( -d );
	}
}

static void projectile_doMakeGreenDirt(CProjectile* const prj) {
	int d = game.gameMap()->PlaceGreenDirt(prj->getPos());
	
	// Remove the dirt count on the worm
	if(prj->hasOwner()) {
		CWorm* w = game.wormById(prj->GetOwner(), false);
		if(w) w->incrementDirtCount( -d );
	}
}


bool Proj_DoActionInfo::hasAnyEffect() const {
	if(explode) return true;
	if(dirt || grndirt) return true;
	if(trailprojspawn || spawnprojectiles || otherSpawns.size() > 0) return true;
	if(deleteAfter) return true;
	return false;
}

void Proj_DoActionInfo::execute(CProjectile* const prj, const AbsTime currentTime) {
	const proj_t *pi = prj->GetProjInfo();
	
	// Explode?
	if(explode) {
		if(!timer)
			projectile_doExplode(prj, damage, shake);
		else
			projectile_doTimerExplode(prj, shake);
		deleteAfter = true;
	}
	
	// Dirt
	if(dirt) {
		projectile_doMakeDirt(prj);
		deleteAfter = true;
	}
	
	// Green dirt
	if(grndirt) {
		projectile_doMakeGreenDirt(prj);
		deleteAfter = true;
	}
		
	if(trailprojspawn) {
		// we use prj->fLastSimulationTime here to simulate the spawing at the current simulation time of this projectile
		pi->Trail.Proj.apply(prj, prj->fLastSimulationTime);
	}
	
	// Spawn any projectiles?
	if(spawnprojectiles) {
		// Even the spawing is caused probably by conditions of the environment like collision with worm/game.gameMap(),
		// we use the last simulation time to have it more accurate.
		// Also, in case of worms, we have fixed the physics in a way that it also uses the old position from the past.
		pi->GeneralSpawnInfo.apply(prj, prj->fLastSimulationTime);
	}
	
	for(std::list<const Proj_SpawnInfo*>::iterator i = otherSpawns.begin(); i != otherSpawns.end(); ++i) {
		// Even the spawing is caused probably by conditions of the environment like collision with worm/game.gameMap(),
		// we use the last simulation time to have it more accurate.
		// Also, in case of worms, we have fixed the physics in a way that it also uses the old position from the past.
		(*i)->apply(prj, prj->fLastSimulationTime);
	}
	
	if(sound) {
		StartSound(sound, prj->pos());
	}
	
	// HINT: delete "junk projectiles" - projectiles that have no action assigned and are therefore never destroyed
	// Some bad-written mods contain those projectiles and they make the game more and more laggy (because new and new
	// projectiles are spawned and never destroyed) and prevent more important projectiles from spawning.
	// These conditions test for those projectiles and remove them
	bool hasAnyAction = pi->Hit.hasAction() || pi->PlyHit.hasAction() || pi->Timer.hasAction();
	for(size_t i = 0; i < pi->actions.size(); ++i) {
		if(hasAnyAction) break;
		hasAnyAction |= pi->actions[i].hasAction();
	}
	if (!hasAnyAction) // Isn't destroyed by any event
		if (!pi->Animating || (pi->Animating && (pi->AnimType != ANI_ONCE || pi->bmpImage == NULL))) // Isn't destroyed after animation ends
			deleteAfter = true;
	
	if(deleteAfter) {
		prj->setUnused();
	}
	
}



static INLINE ProjCollisionType LX56_simulateProjectile_LowLevel(AbsTime currentTime, TimeDiff dt, CProjectile* proj, bool* projspawn, bool* deleteAfter) {
	// If this is a remote projectile, we have already set the correct fLastSimulationTime
	//proj->setRemote( false );

	// Check for collisions and move
	ProjCollisionType res = LX56Projectile_checkCollAndMove(proj, dt, game.gameMap());
	
	proj->life() += dt.seconds();
	proj->extra() += dt.seconds();
	
	
	
	/*
		vOldPos = vPos;
	*/
	
	const proj_t *pi = proj->GetProjInfo();
	
	if(pi->Rotating)  {
		proj->rotation() += (float)pi->RotSpeed * dt.seconds();
		FMOD(proj->rotation(), 360.0f);
	}
	
		// Animation
	if(pi->Animating) {
		if(proj->getFrameDelta())
			proj->frame() += (float)pi->AnimRate * dt.seconds();
		else
			proj->frame() -= (float)pi->AnimRate * dt.seconds();
		
		if(pi->bmpImage) {
			const int NumFrames = pi->bmpImage->w / pi->bmpImage->h;
			if(proj->frame() >= NumFrames) {
				switch(pi->AnimType) {
				case ANI_ONCE:
					*deleteAfter = true;
					break;
				case ANI_LOOP:
					proj->frame() = 0;
					break;
				case ANI_PINGPONG:
					proj->setFrameDelta( ! proj->getFrameDelta() );
					proj->frame() = (float)NumFrames - 1;
					break;
				case __ANI_LBOUND: case __ANI_UBOUND: errors << "simulateProjectile_LowLevel: hit __ANI_BOUND" << endl;
				}
			}
			else if(proj->frame() < 0) {
				if(proj->getProjInfo()->AnimType == ANI_PINGPONG) {
					proj->setFrameDelta( ! proj->getFrameDelta() );
					proj->frame() = 0.0f;
				}
			}
		}
	}
	
		// Trails
	switch(pi->Trail.Type) {
	case TRL_NONE: break;
	case TRL_SMOKE:
		if(proj->extra() >= 0.075f) {
			proj->extra() = 0.0f;
			SpawnEntity(ENT_SMOKE,0,proj->getPos(),CVec(0,0),Color(),NULL);
		}
		break;
	case TRL_CHEMSMOKE:
		if(proj->extra() >= 0.075f) {
			proj->extra() = 0.0;
			SpawnEntity(ENT_CHEMSMOKE,0,proj->getPos(),CVec(0,0),Color(),NULL);
		}
		break;
	case TRL_DOOMSDAY:
		if(proj->extra() >= 0.05f) {
			proj->extra() = 0.0;
			SpawnEntity(ENT_DOOMSDAY,0,proj->getPos(),proj->getVelocity(),Color(),NULL);
		}
		break;
	case TRL_EXPLOSIVE:
		if(proj->extra() >= 0.05f) {
			proj->extra() = 0.0;
			SpawnEntity(ENT_EXPLOSION,10,proj->getPos(),CVec(0,0),Color(),NULL);
		}
		break;
	case TRL_PROJECTILE: // Projectile trail
		if(currentTime > proj->lastTrailProj()) {
			proj->lastTrailProj() = currentTime + pi->Trail.Delay / (float)cClient->getGameLobby()[FT_GameSpeed];
			
			*projspawn = true;
		}
		break;
	case __TRL_LBOUND: case __TRL_UBOUND: errors << "simulateProjectile_LowLevel: hit __TRL_BOUND" << endl;
	}
	return res;
}


static INLINE bool LX56ProjectileHandler_doFrame(const AbsTime currentTime, TimeDiff dt, CProjectile* const prj) {
	const proj_t& projInfo = *prj->GetProjInfo();
	
	Proj_DoActionInfo doActionInfo;
	TimeDiff serverTime = game.serverTime();
	{
		TimeDiff timeDiff = currentTime - prj->fLastSimulationTime;
		if(timeDiff >= serverTime)
			serverTime = TimeDiff(0); // strange case
		else
			serverTime -= timeDiff;
	}
	
	// Check if the timer is up
	projInfo.Timer.checkAndApply(Proj_EventOccurInfo::Unspec(serverTime, dt), prj, &doActionInfo);
	
	// Simulate the projectile
	ProjCollisionType result = LX56_simulateProjectile_LowLevel( prj->fLastSimulationTime, dt, prj, &doActionInfo.trailprojspawn, &doActionInfo.deleteAfter );
	
	const Proj_EventOccurInfo eventInfo = Proj_EventOccurInfo::Col(serverTime, dt, &result);
	
	/*
	===================
	Terrain Collision
	===================
	*/
	if( !result.withWorm && (result.colMask & PJC_TERRAIN) ) {
		projInfo.Hit.applyTo(eventInfo, prj, &doActionInfo);
	}
	
	/*
	===================
	Worm Collision
	===================
	*/
	if( result.withWorm && !doActionInfo.explode) {
		projInfo.PlyHit.applyTo(eventInfo, prj, &doActionInfo);
	}
			
	for(size_t i = 0; i < projInfo.actions.size(); ++i) {
		projInfo.actions[i].checkAndApply(eventInfo, prj, &doActionInfo);
	}
	
	doActionInfo.execute(prj, currentTime);
	return !doActionInfo.deleteAfter;
}


static void LX56_simulateProjectile(const AbsTime currentTime, CProjectile* const prj) {
	// The FPS callrate doesn't matter that much because we do our own FPS handling here.
	
	const TimeDiff orig_dt = LX56PhysicsDT;
	TimeDiff frame_dt = orig_dt * (1.0f/CLAMP((float)cClient->getGameLobby()[FT_GameSpeed],0.05f,10.0f));
	if(frame_dt <= TimeDiff(0))
		frame_dt = TimeDiff(1);
		
	VectorD2<int> oldPos(prj->getPos());
	VectorD2<int> oldRadius(prj->getRadius());
	
simulateProjectileStart:
	if(prj->fLastSimulationTime + frame_dt > currentTime) goto finalMapPosIndexUpdate;
	prj->fLastSimulationTime += frame_dt;
	if(LX56ProjectileHandler_doFrame(currentTime, orig_dt, prj))
		goto simulateProjectileStart;

finalMapPosIndexUpdate:
	prj->updateCollMapInfo(&oldPos, &oldRadius);
}


void LX56_simulateProjectiles(Iterator<CProjectile*>::Ref projs) {
	// Note: This function can be called with any FPS -
	// LX56_simulateProjectile will handle its own internal FPS
	// via CProjectile::fLastSimulationTime.

	AbsTime currentTime = GetPhysicsTime();
		
	for(Iterator<CProjectile*>::Ref i = projs; i->isValid(); i->next()) {
		CProjectile* const p = i->get();
		LX56_simulateProjectile( currentTime, p );
	}	
}

