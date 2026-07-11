/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Projectile Class
// Created 11/2/02
// Jason Boettcher


#include "LieroX.h"
#include "CGameScript.h" // for all PRJ_* and PJ_* constants only
#include "GfxPrimitives.h"
#include "CProjectile.h"
#include "Protocol.h"
#include "game/CWorm.h"
#include "Entity.h"
#include "MathLib.h"
#include "CClient.h"
#include "ProfileSystem.h"
#include "Debug.h"
#include "ProjectileDesc.h"
#include "Physics.h"
#include "Geometry.h"
#include "game/Game.h"


void CProjectile::setUnused() {
	bUsed = false;
	timerInfo.clear();

	onInvalidation.occurred( EventData(this) );
}

///////////////////
// Spawn the projectile
// this function is called by CClient::SpawnProjectile()
void CProjectile::Spawn(proj_t *_proj, CVec _pos, CVec _vel, int _rot, int _owner, int _random, AbsTime time, AbsTime ignoreWormCollBeforeTime)
{
	if (_owner < 0 || _owner >= MAX_WORMS)
		iOwner = -1;
	else
		iOwner = _owner;
	
	bUsed = true;
	tProjInfo = _proj;
	fLife = 0;
	fExtra = 0;
	vOldPos = _pos;
	vPos = _pos;
	vVelocity = _vel;
	fRotation = (float)_rot;
	radius.x = tProjInfo->Width / 2;
	radius.y = tProjInfo->Height / 2;
	health = 100.f;
	
	fLastTrailProj = AbsTime();
	iRandom = _random;
    iFrameX = 0;
	fIgnoreWormCollBeforeTime = ignoreWormCollBeforeTime;

    fTimeVarRandom = GetFixedRandomNum(iRandom);
	fLastSimulationTime = time;
	fSpawnTime = time;
	timerInfo.clear();

	fSpeed = _vel.GetLength();
	CalculateCheckSteps();

	fFrame = 0;
	bFrameDelta = true;

	firstbounce = true;

	switch(tProjInfo->Type) {
		case PRJ_RECT:
		case PRJ_PIXEL:
		case PRJ_CIRCLE:
		case PRJ_POLYGON: {
			// Choose a colour
			if(tProjInfo->Colour.size() > 0) {
				int c = GetRandomInt((int)tProjInfo->Colour.size()-1);
				iColour = tProjInfo->Colour[c];
			}
			else {
				// don't give the warning here, give it while loading GS
				iColour = Color();
			}
			break;
		}
		case PRJ_IMAGE: break;
		case __PRJ_LBOUND: case __PRJ_UBOUND: errors << "CProjectile::DrawShadow: hit __PRJ_BOUND" << endl;
	}

	if((bool)cClient->getGameLobby()[FT_LX56WallShooting])
		fWallshootTime = 0.011f + getRandomFloat() / 1000; // Support wallshooting - ignore collisions before this time
	else
		fWallshootTime = 0.0f;

	// TODO: the check was tProjInfo->Type != PJ_BOUNCE before, which didn't make sense. is it correct now?
	// TODO: fGravity != 0 => bChangesSpeed=false was here before. why?
	bChangesSpeed = ((int)tProjInfo->Dampening == 1)
		&& (tProjInfo->Hit.Type != PJ_BOUNCE || (int)tProjInfo->Hit.BounceCoeff == 1);  // Changes speed on bounce

	updateCollMapInfo();
}


///////////////////
// Gets a random float from a special list
// TODO: how does this belong to projectiles? move it perhaps out here
float CProjectile::getRandomFloat()
{
	float r = GetFixedRandomNum(iRandom++);

	iRandom %= 255;

	return r;
}


//////////////////////
// Pre-calculates the check steps for collisions
void CProjectile::CalculateCheckSteps()
{
	MIN_CHECKSTEP = 4;
	MAX_CHECKSTEP = 6;
	AVG_CHECKSTEP = 4;

	iCheckSpeedLen = (int)vVelocity.get().GetLength2();
	if (iCheckSpeedLen < 14000)  {
		MIN_CHECKSTEP = 0;
		MAX_CHECKSTEP = 3;
		AVG_CHECKSTEP = 2;
	} else if (iCheckSpeedLen < 75000)  {
		MIN_CHECKSTEP = 0;
		MAX_CHECKSTEP = 4;
		AVG_CHECKSTEP = 2;
	} else if (iCheckSpeedLen < 250000)  {
		MIN_CHECKSTEP = 1;
		MAX_CHECKSTEP = 5;
		AVG_CHECKSTEP = 2;

	// HINT: add or substract some small random number for high speeds, it behaves more like original LX
	} else {
		int rnd = (getRandomIndex() % 3);
		rnd *= SIGN(getRandomFloat());
		MIN_CHECKSTEP = 6;
		if (tProjInfo->Hit.Type == PJ_BOUNCE)  { // HINT: this avoids fast bouncing projectiles to stay in a wall too often (for example zimm)
			MAX_CHECKSTEP = 2;
			AVG_CHECKSTEP = 2;
		} else {
			MAX_CHECKSTEP = 9 + rnd;
			AVG_CHECKSTEP = 6 + rnd;
		}
	}

	MAX_CHECKSTEP2 = MAX_CHECKSTEP * MAX_CHECKSTEP;
	MIN_CHECKSTEP2 = MIN_CHECKSTEP * MIN_CHECKSTEP;
}


///////////////////
// Check for a collision (static version; doesnt do anything else then checking)
// Returns true if there was a collision, otherwise false is returned
// This is not used anywhere within physics, it's just for AI (and also not very correct; it ignored radius)
int CProjectile::CheckCollision(proj_t* tProjInfo, float dt, CVec pos, CVec vel)
{
	// Check if it hit the terrain
	CMap* map = game.gameMap();
	int mw = map->GetWidth();
	int mh = map->GetHeight();
	int w,h;

	if(tProjInfo->Type == PRJ_PIXEL)
		w=h=1;
	else
		w=h=2;

	float maxspeed2 = (float)(4*w*w+4*w+1); // (2w+1)^2
	if( (vel*dt).GetLength2() > maxspeed2) {
		dt *= 0.5f;

		int col = CheckCollision(tProjInfo,dt,pos,vel);
		if(col) return col;

		pos += vel*dt;

		return CheckCollision(tProjInfo,dt,pos,vel);
	}

	pos += vel*dt;

	int px = (int)pos.x;
	int py = (int)pos.y;

	// Hit edges
	if(px-w<0 || py-h<0 || px+w>=mw || py+h>=mh)
		return PJC_TERRAIN|PJC_MAPBORDER;
	
	for(int y=py-h;y<=py+h;y++) {

		uchar *pf = &map->material->line[y][px-w];

		for(int x=px-w;x<=px+w;x++) {

			if(!map->materialForIndex(*pf).particle_pass)
				return PJC_TERRAIN;

			pf++;
		}
	}


	// No collision
	return 0;
}

///////////////////
// Draw the projectile
void CProjectile::Draw(SDL_Surface * bmpDest, CViewport *view)
{
	CMap* map = game.gameMap();
	VectorD2<int> p = view->physicToReal(vPos.get(), cClient->getGameLobby()[FT_InfiniteMap], map->GetWidth(), map->GetHeight());
	
    switch (tProjInfo->Type) {
		case PRJ_PIXEL:
			if(view->posInside(p))
				DrawRectFill2x2(bmpDest, p.x - 1, p.y - 1,iColour);
			return;
	
		case PRJ_IMAGE:  {
	
			if(tProjInfo->bmpImage == NULL)
				return;
	
			float framestep = 0;
			
			if (view->posInside(p))  {  // HINT: this is how it was in old LX, the projectile is not animated/destroyed when out of the screen
				if(tProjInfo->Animating)
					framestep = fFrame;
				
				// Special angle
				// Basically another way of organising the angles in images
				// Straight up is in the middle, rotating left goes left, rotating right goes right in terms
				// of image index's from the centre
				else if(tProjInfo->UseSpecAngle) {
					CVec dir = vVelocity;
					float angle = (float)( -atan2(dir.x,dir.y) * (180.0f/PI) );
					int direct = 0;
		
					if(angle > 0)
						angle=180-angle;
					if(angle < 0) {
						angle=180+angle;
						direct = 1;
					}
					if(angle == 0)
						direct = 0;
		
		
					int num = (tProjInfo->AngleImages - 1) / 2;
					if(direct == 0)
						// Left side
						framestep = (float)(151-angle) / 151.0f * (float)num;
					else {
						// Right side
						framestep = (float)angle / 151.0f * (float)num;
						framestep += num+1;
					}
				}
		
				// Directed in the direction the projectile is travelling
				else if(tProjInfo->UseAngle) {
					CVec dir = vVelocity;
					float angle = (float)( -atan2(dir.x,dir.y) * (180.0f/PI) );
					float offset = 360.0f / (float)tProjInfo->AngleImages;
		
					FMOD(angle, 360.0f);
		
					framestep = angle / offset;
				}
				
				// Spinning projectile only when moving
				else if(tProjInfo->RotIncrement != 0 && tProjInfo->Rotating && (vVelocity.get().x != 0 || vVelocity.get().y != 0))
					framestep = fRotation / (float)tProjInfo->RotIncrement;
				
			}

			const int size = tProjInfo->bmpImage->h;
			const int half = size/2;
			iFrameX = (int)framestep*size;
			MOD(iFrameX, tProjInfo->bmpImage->w);
	
			DrawImageAdv(bmpDest, tProjInfo->bmpImage, iFrameX, 0, p.x-half, p.y-half, size,size);
		
			return;
		}
		
		case PRJ_CIRCLE:
			DrawCircleFilled(bmpDest, p.x, p.y, radius.x*2, radius.y*2, iColour);
			return;
			
		case PRJ_RECT:
			DrawRectFill(bmpDest, p.x - radius.x*2, p.y - radius.y*2, p.x + radius.x*2, p.y + radius.y*2, iColour);
			return;
			
		case PRJ_POLYGON:
			getProjInfo()->polygon.drawFilled(bmpDest, (int)vPos.get().x, (int)vPos.get().y, view, iColour);
			return;
		
		case __PRJ_LBOUND: case __PRJ_UBOUND: errors << "CProjectile::Draw: hit __PRJ_BOUND" << endl;
	}
}


///////////////////
// Draw the projectiles shadow
void CProjectile::DrawShadow(SDL_Surface * bmpDest, CViewport *view)
{
	if (tLX->fDeltaTime >= 0.1f) // Don't draw projectile shadows with FPS <= 10 to get a little better performance
		return;

	CMap* map = game.gameMap();

	// TODO: DrawObjectShadow is a bit complicated to fix for shadows&tiling, so I just leave all shadows away for now...
	if(!view->physicsInside(vPos.get() /*, cClient->getGameLobby()[FT_InfiniteMap], map->GetWidth(), map->GetHeight() */))
		return;
	   
	switch (tProjInfo->Type)  {
	
		// Pixel
		case PRJ_PIXEL:
			map->DrawPixelShadow(bmpDest, view, (int)vPos.get().x, (int)vPos.get().y);
			break;
	
		// Image
		case PRJ_IMAGE:  {
	
			if(tProjInfo->bmpImage == NULL)
				return;
			/*if (tProjInfo->bmpImage.get()->w <= 2 && tProjInfo->bmpImage.get()->h <= 2)  {
				map->DrawPixelShadow(bmpDest, view, (int)vPos.x, (int)vPos.y);
				return;
			}*/

			int size = tProjInfo->bmpImage->h;
			int half = size / 2;
			map->DrawObjectShadow(bmpDest, tProjInfo->bmpImage, tProjInfo->bmpShadow.get(), iFrameX, 0, size,size, view, (int)vPos.get().x-(half>>1), (int)vPos.get().y-(half>>1));
		
			break;	
		}
		
		case PRJ_CIRCLE:
		case PRJ_POLYGON:
		case PRJ_RECT:
			// TODO ...
			break;
		
		case __PRJ_LBOUND: case __PRJ_UBOUND: errors << "CProjectile::DrawShadow: hit __PRJ_BOUND" << endl;
    }
}


///////////////////
// Bounce
// HINT: this is not exactly the way the original LX did it,
//		but this way is way more correct and it seems to work OK
//		(original LX resets the bounce-direction on each checked side)
void CProjectile::Bounce(float fCoeff)
{
	float x,y;
	x=y=1;

	// This code is right, it should be done like that
	// However, we want to keep compatibility with .56 and when on each client would be another simulation,
	// we couldn't call that compatibility at all

	// For now we just keep the old, wrong code, so noone will call OLX players as cheaters
/*	if(CollisionSide & (COL_TOP|COL_BOTTOM)) {
		y=-y;
	}
	if(CollisionSide & (COL_LEFT|COL_RIGHT)) {
		x=-x;
	}

	if(CollisionSide & COL_TOP) {
		y*=fCoeff;
	}
	if(CollisionSide & COL_BOTTOM) {
		y*=fCoeff;
	}
	if(CollisionSide & COL_LEFT) {
		x*=fCoeff;
	}
	if(CollisionSide & COL_RIGHT) {
		x*=fCoeff;
	}*/


	// WARNING: this code should not be used; it is simply wrong
	//	(this was the way the original LX did it)

	if (CollisionSide & COL_TOP)  {
		x = fCoeff;
		y = -fCoeff;
	}
	if (CollisionSide & COL_BOTTOM)  {
		x = fCoeff;
		y = -fCoeff;
	}
	if (CollisionSide & COL_LEFT)  {
		x = -fCoeff;
		y = fCoeff;
	}
	if (CollisionSide & COL_RIGHT)  {
		x = -fCoeff;
		y = fCoeff;
	}


	vVelocity.write().multPairwiseInplace(x, y);
}





template<bool TOP, bool LEFT>
static CClient::MapPosIndex MPI(const VectorD2<int>& p, const VectorD2<int>& r) {
	return CClient::MapPosIndex( p + VectorD2<int>(LEFT ? -r.x : r.x, TOP ? -r.y : r.y) );
}

template<bool INSERT>
static void updateMap(CProjectile* prj, const VectorD2<int>& p, const VectorD2<int>& r) {
	for(int x = MPI<true,true>(p,r).x; x <= MPI<true,false>(p,r).x; ++x)
		for(int y = MPI<true,true>(p,r).y; y <= MPI<false,true>(p,r).y; ++y) {
			CClient::ProjectileSet* projs = cClient->projPosMap[CClient::MapPosIndex(x,y).index(game.gameMap())];
			if(projs == NULL) continue;
			if(INSERT)
				projs->insert(prj);
			else
				projs->erase(prj);
		}
}

void CProjectile::updateCollMapInfo(const VectorD2<int>* oldPos, const VectorD2<int>* oldRadius) {
	if( !game.gameScript()->getNeedCollisionInfo() && 
		!bool(cClient->getGameLobby()[FT_CollideProjectiles]) ) 
		return;
	
	if(!isUsed()) { // not used anymore
		if(oldPos && oldRadius)
			updateMap<false>(this, *oldPos, *oldRadius);
		return;
	}
	
	if(oldPos && oldRadius) {
		if(
		(MPI<true,true>(*oldPos,*oldRadius) == MPI<true,true>(vPos.get(),radius)) &&
		(MPI<true,false>(*oldPos,*oldRadius) == MPI<true,false>(vPos.get(),radius)) &&
		(MPI<false,true>(*oldPos,*oldRadius) == MPI<false,true>(vPos.get(),radius)) &&
		(MPI<false,false>(*oldPos,*oldRadius) == MPI<false,false>(vPos.get(),radius))) {
			return; // nothing has changed
		}
		
		// delete from all
		updateMap<false>(this, *oldPos, *oldRadius);
	}
	
	// add to all
	updateMap<true>(this, vPos.get(), radius);
}
