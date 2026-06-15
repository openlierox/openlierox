/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Entity routines
// Created 23/7/02
// Jason Boettcher


#include "LieroX.h"
#include "Options.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/Graphics.h"
#include "Entity.h"
#include "MathLib.h"
#include "FastVector.h"
#include "CViewport.h"
#include "game/CMap.h"
#include "game/CWorm.h"
#include "Consts.h"
#include "Geometry.h"
#include "game/Game.h"

typedef FastVector<entity_t,MAX_ENTITIES> Entities;
Entities tEntities;
Color doomsday[4];

struct DrawBeamInfo {
	int frame;
	bool isUsed;
	Color col;
	VectorD2<int> startPos, endPos;
	Polygon2D p;
	bool hasWidth;
	
	DrawBeamInfo() : frame(0), isUsed(false) {}
	
	void simulate() {
		if(!isUsed) return;
		if(frame >= 1) { isUsed = false; return; }
		frame++;
	}
	
	void draw(SDL_Surface* dst, CViewport* v) {
		if(!isUsed) return;
				
		if(!hasWidth) {
			int wx = v->GetWorldX();
			int wy = v->GetWorldY();
			int l = v->GetLeft();
			int t = v->GetTop();

			int x = (startPos.x - wx) * 2 + l;
			int y = (startPos.y - wy) * 2 + t;
			int x2 = (endPos.x - wx) * 2 + l;
			int y2 = (endPos.y - wy) * 2 + t;
			DrawBeam(dst, x, y, x2, y2, col);
		}
		else {
			p.drawFilled(dst, 0, 0, v, col);
		}
	}
};

DrawBeamInfo drawBeamInfos[MAX_WORMS];


///////////////////
// Initialzie the entity system
int InitializeEntities()
{
	tEntities.clear();

    // Pre-calculate the doomsday colour
    doomsday[0] = Color(244,244,112);
    doomsday[1] = Color(248,192,36);
	doomsday[2] = Color(248,108,20);
    doomsday[3] = Color(248,108,20);

	return true;
}


///////////////////
// Shutdown the entity system
void ShutdownEntities()
{
	tEntities.clear();
}


///////////////////
// Clear all the entities
void ClearEntities()
{
	tEntities.clear();
	
	for(unsigned short i = 0; i < sizeof(drawBeamInfos) / sizeof(DrawBeamInfo); ++i) {
		drawBeamInfos[i].isUsed = false;
	}
}


void SetWormBeamEntity(int worm, Color col, const Line& startLine, const Line& endLine) {
	if(worm < 0 || worm >= MAX_WORMS) {
		errors << "SetWormBeamEntity: invalid worm ID: " << worm << endl;
		return;
	}

	drawBeamInfos[worm].frame = 0;
	drawBeamInfos[worm].isUsed = true;
	drawBeamInfos[worm].col = col;
	drawBeamInfos[worm].hasWidth = true;
	drawBeamInfos[worm].p.clear();
	drawBeamInfos[worm].p.startPointAdding();
	drawBeamInfos[worm].p.addPoint( startLine.start );
	if(startLine.start != startLine.end)
		drawBeamInfos[worm].p.addPoint( startLine.end );
	drawBeamInfos[worm].p.addPoint( endLine.start );
	if(endLine.start != endLine.end)
		drawBeamInfos[worm].p.addPoint( endLine.end );
	drawBeamInfos[worm].p.addPoint( startLine.start );
	drawBeamInfos[worm].p.endPointAdding();
}

void SetWormBeamEntity(int worm, Color col, VectorD2<int> startPos, VectorD2<int> endPos) {
	if(worm < 0 || worm >= MAX_WORMS) {
		errors << "SetWormBeamEntity: invalid worm ID: " << worm << endl;
		return;
	}
	
	drawBeamInfos[worm].frame = 0;
	drawBeamInfos[worm].isUsed = true;
	drawBeamInfos[worm].col = col;
	drawBeamInfos[worm].hasWidth = false;
	drawBeamInfos[worm].startPos = startPos;
	drawBeamInfos[worm].endPos = endPos;
}

static void simulateDrawBeams() {
	for(unsigned short i = 0; i < sizeof(drawBeamInfos) / sizeof(DrawBeamInfo); ++i) {
		drawBeamInfos[i].simulate();
	}
}

static void drawBeams(SDL_Surface * bmpDest, CViewport *v) {
	for(unsigned short i = 0; i < sizeof(drawBeamInfos) / sizeof(DrawBeamInfo); ++i) {
		drawBeamInfos[i].draw(bmpDest, v);
	}
}

///////////////////
// Spawn an entity
void SpawnEntity(int type, int type2, CVec pos, CVec vel, Color colour, SmartPointer<SDL_Surface> img)
{
	// If this is a particle type entity, and particles are switched off, just leave
	if(!tLXOptions->bParticles) {
		if(type == ENT_PARTICLE ||
		   type == ENT_BLOOD)
			return;
	}

	if(type == ENT_GIB && img.get() == NULL) {
		errors << "SpawnEntity: giblet without surface" << endl;
		return;
	}

	entity_t *ent = tEntities.getNewObj();

	// Are there any free entitiy slots?
	if (!ent)
		return;
	
	ent->Spawn();
	ent->iType = type;
	ent->iType2 = type2;
	ent->vPos = pos;
	ent->vVel = vel;
	ent->iColour = colour;
	ent->bmpSurf = img;

	ent->fExtra = 0;
	ent->fFrame = 0;
	ent->fLife = 0;
	ent->iAngle = 0;	

	switch(ent->iType) {
	case ENT_EXPLOSION:
		ent->fFrame = (float)(15-type2);
		break;
	case ENT_GIB:
		ent->fAnglVel = (float)fabs(GetRandomNum())*20;
		ent->iRotation = (int)(fabs(GetRandomNum())*3);
		break;
	case ENT_JETPACKSPRAY:
		ent->fLife = 3;
	}
}


///////////////////
// Draw the entities
void DrawEntities(SDL_Surface * bmpDest, CViewport *v)
{
	CVec end;

	int wx = v->GetWorldX();
	int wy = v->GetWorldY();
	int l = v->GetLeft();
	int t = v->GetTop();


	int x,y;
	int x2,y2;
		
	for (Entities::Iterator::Ref e = tEntities.begin(); e->isValid(); e->next()) {

		entity_t *ent = e->get();

		x= int((ent->vPos.x - (float)wx)*2.0) + l;
		y= int((ent->vPos.y - (float)wy)*2.0) + t;

		// Clipping
		if(ent->iType != ENT_BEAM && ent->iType != ENT_LASERSIGHT) {
			if((x<l || x>l+v->GetVirtW()))
				continue;
			if((y<t || y>t+v->GetVirtH()))
				continue;
		}

		switch(ent->iType) {

			// Particle & Blood
			case ENT_PARTICLE:
			case ENT_BLOOD:				// Fallthrough
			case ENT_BLOODDROPPER:		// Fallthrough
				DrawRectFill2x2(bmpDest, x - 1, y - 1, ent->iColour);
				break;

			// Explosion
			case ENT_EXPLOSION:
				DrawImageAdv(bmpDest, DeprecatedGUI::gfxGame.bmpExplosion,int(ent->fFrame)*32,0,x-16,y-16,32,32);
				break;

			// Smoke
			case ENT_SMOKE:
				DrawImageAdv(bmpDest, DeprecatedGUI::gfxGame.bmpSmoke, int(ent->fFrame)*14,0,x-7,y-7,14,14);
				break;

			// Chemical smoke
			case ENT_CHEMSMOKE:
				DrawImageAdv(bmpDest, DeprecatedGUI::gfxGame.bmpChemSmoke, int(ent->fFrame)*10,0,x-5,y-5,10,10);
				break;

			// Spawn
			case ENT_SPAWN:
				DrawImageAdv(bmpDest, DeprecatedGUI::gfxGame.bmpSpawn, int(ent->fFrame)*32,0,x-16,y-16,32,32);
				break;

			// Giblet
			case ENT_GIB:
				DrawImageAdv(bmpDest,ent->bmpSurf,int(ent->iRotation)*8,0,x-2,y-2,8,8);
				break;

			// Sparkle
			case ENT_SPARKLE:
				if(ent->iColour != Color()) {
					// Well, I admit, not very creative but it's ok for now
					DrawRectFill2x2(bmpDest, x, y - 1, ent->iColour);
					DrawRectFill2x2(bmpDest, x - 1, y, ent->iColour);
				} else
					DrawImageAdv(bmpDest, DeprecatedGUI::gfxGame.bmpSparkle, int(ent->fFrame)*10,0, x-5,y-5,10,10);
				break;

			// Doomsday
			case ENT_DOOMSDAY:
				DrawRectFill2x2(bmpDest, x - 1, y - 1, doomsday[(int)ent->fFrame]);
				break;

			// Jetpack spray
			case ENT_JETPACKSPRAY:
				Uint8 r,g,b;
				r = (Uint8)((float)MIN(0.314f * (255-ent->fFrame),255.0f));
				g = (Uint8)((float)MIN(0.588f * (255-ent->fFrame),255.0f));
				b = (Uint8)((float)MIN(0.784f * (255-ent->fFrame),255.0f));
				DrawRectFill2x2(bmpDest, x - 1, y - 1, Color(r, g, b));
				break;

			// Beam
			case ENT_BEAM:
				end = ent->vPos + ent->vVel*(float)ent->iType2;
				x2=((int)end.x-wx)*2+l;
				y2=((int)end.y-wy)*2+t;
				DrawBeam(bmpDest, x,y, x2,y2, ent->iColour);
				break;

			// Laser Sight
			case ENT_LASERSIGHT:
				end = ent->vPos + ent->vVel*(float)ent->iType2;
				x2=((int)end.x-wx)*2+l;
				y2=((int)end.y-wy)*2+t;
				DrawLaserSight(bmpDest, x,y, x2,y2, ent->iColour);
				break;
		}
	}
	
	drawBeams(bmpDest, v);
}


///////////////////
// Simulate the entities
void SimulateEntities(TimeDiff dt)
{
	CMap* map = game.gameMap();
	if (!map) { // Weird
		hints << "SimulateEntities gots no map" << endl;
		return;
	}

	TimeDiff realdt = tLX->fRealDeltaTime;

	for (Entities::Iterator::Ref e = tEntities.begin(); e->isValid(); e->next()) {

		entity_t *ent = e->get();

		// Collisions and stuff
		switch(ent->iType) {

			case ENT_GIB:			
				ent->iRotation += (int) (ent->fAnglVel * realdt.seconds());
				ent->iRotation %= 5;
			
			// Fallthrough
			case ENT_JETPACKSPRAY:
			case ENT_PARTICLE:
			case ENT_BLOOD:
			case ENT_BLOODDROPPER:
				ent->vVel.y += 100*dt.seconds();
				ent->vPos += ent->vVel * dt.seconds();
	
				// Clipping
				if(ent->vPos.x < 0 || ent->vPos.y < 0 || (uint)ent->vPos.x >= (uint)map->GetWidth() || (uint)ent->vPos.y >= (uint)map->GetHeight()) {
					ent->setUnused();
					break;
				}

				// Check if the particle has hit the map
				uchar pf = map->GetPixelFlag((uint)ent->vPos.x, (uint)ent->vPos.y);

				if((pf & (PX_ROCK|PX_DIRT))) {

					switch(ent->iType) {

						// Blood
						case ENT_BLOOD: {
							int x = (int)ent->vPos.x;
							int y = (int)ent->vPos.y;
							map->putColorTo(x, y, ent->iColour);
							ent->setUnused();
							
							break;
						}
							
						// Giblet
						case ENT_GIB:
							if((int)ent->vVel.GetLength2() > 25600)  {
								EntityBounce(ent);
								// Still alive
							}
							else {

								// Add the gib to the map
								int x = (int)ent->vPos.x-1;
								int y = (int)ent->vPos.y-1;

								// Safety
								if (ent->bmpSurf.get())
									map->putSurfaceTo(x,y,ent->bmpSurf.get(), (int)ent->iRotation*4, 8, 4, 4);

								ent->setUnused();
							}
							break;

						default:
							ent->setUnused();
					}
				}
				break;
		}


		// Other stuff
		switch(ent->iType) {

			// Explosion
			case ENT_EXPLOSION:
				ent->fFrame += realdt.seconds() * 40;
				if(ent->fFrame > 15) ent->setUnused();
				break;

			// Smoke
			case ENT_SMOKE:
				// Fallthrough
			case ENT_CHEMSMOKE:
				ent->fFrame += realdt.seconds() * 15;
				if((int)ent->fFrame > 4) ent->setUnused();
				break;

			// Doomsday
			case ENT_DOOMSDAY:
				ent->fFrame += realdt.seconds() * 15;
				if((int)ent->fFrame > 3) ent->setUnused();
				break;

			// Spawn
			case ENT_SPAWN:
				ent->fFrame += realdt.seconds() * 15;
				if((int)ent->fFrame > 5) ent->setUnused();
				break;

			// Sparkle
			case ENT_SPARKLE:
				{
					ent->vPos = ent->vPos + ent->vVel * dt.seconds(); // CVec(0, 5.0f * dt.seconds());
					int fadeSpeed = 5;
					if(ent->iType2 != 0)
						fadeSpeed = ent->iType2;
					ent->fFrame += realdt.seconds() * fadeSpeed;
					if((int)ent->fFrame > 2) ent->setUnused();
				}
				break;

			// Jetpack Spray
			case ENT_JETPACKSPRAY:
				ent->fFrame += realdt.seconds() * 200;
				if((int)ent->fFrame > 150) ent->setUnused();
				break;

			// Beam & Laser Sight
			case ENT_BEAM:
			case ENT_LASERSIGHT:
				if((int)ent->fFrame == 1) ent->setUnused();
				ent->fFrame++;
				break;

			// Blood dropper
			case ENT_BLOODDROPPER:
				if(ent->fExtra > 0.1f) {
					int col = GetRandomInt(1);
					static const int colour[] = {128,200};
					SpawnEntity(ENT_BLOOD,0,ent->vPos,GetRandomVec(),Color(colour[col],0,0),NULL);
					ent->fExtra = 0;
				}
				ent->fExtra += dt;
				break;
		}
	}
	
	simulateDrawBeams();
}


///////////////////
// Bounce an entity
void EntityBounce(entity_t *ent)
{
	ent->vVel.x *= -0.4f;
	ent->vVel.y *= -0.4f;
	ent->fAnglVel *= 0.8f;
}

void EntityEffect::Process()
{
	_lastTime -= tLX->fDeltaTime.seconds();
	if( _lastTime > 0 )
		return;
	_lastTime = _delay;
	
	if( _parent->getTagIT() )
		Set( ENTE_SPARKLE_RANDOM, 1, 0.15f, 5, 10 );
	else if( _parent->damageFactor() > 1 )
		Set( ENTE_SPARKLE_CIRCLE_ROTATING, CLAMP((int)_parent->damageFactor(), 0, 10), 0.05f, 
										5.0f + (_parent->damageFactor() - int(_parent->damageFactor()))*5.0f, 9, 10 );
	else if( _parent->shieldFactor() > 1 )
		Set( ENTE_SPARKLE_CIRCLE_ROTATING, CLAMP((int)_parent->shieldFactor(), 0, 10), 0.05f, 
			5.0f + (_parent->shieldFactor() - int(_parent->shieldFactor()))*5.0f, 9, 10 );
	else
		Set();

	CVec pos( _parent->getPos() ), vel( _parent->getVelocity() );
	switch( _type )
	{
		case ENTE_NONE:
			break;

		case ENTE_SPARKLE_DOT:
			for( int i = 0; i < _amount; i++ )
			{
				CVec spread = GetRandomVec() * _speed;
				// _radius here is gravitation
				SpawnEntity(ENT_SPARKLE, _fade, pos, vel + spread + CVec(0, _radius), Color(), NULL);
			}
			break;

		case ENTE_SPARKLE_RANDOM:
			for( int i = 0; i < _amount; i++ )
			{
				const CVec spread = GetRandomVec() * _speed;
				const CVec randPos = GetRandomVec() * _radius;
				SpawnEntity(ENT_SPARKLE, _fade, pos + randPos, vel + spread, Color(), NULL);
			}
			break;

		case ENTE_SPARKLE_SPREAD:
			{
				_lastAngle += _speed;
				while( _lastAngle > 360.0f )
					_lastAngle -= 360.0f;
				float angle = _lastAngle / 180.0f * (float)PI;
				const float angleDiv = ( 360.0f / (float)_amount ) / 180.0f * (float)PI;
				for( int i = 0; i < _amount; i++, angle += angleDiv )
				{
					const CVec spread = CVec( sinf(angle), cosf(angle) ) * _radius;
					SpawnEntity(ENT_SPARKLE, _fade, pos, vel + spread, Color(), NULL);
				}
			}
			break;

		case ENTE_SPARKLE_CIRCLE:
			{
				_lastAngle += _speed;
				while( _lastAngle > 360.0f )
					_lastAngle -= 360.0f;
				float angle = _lastAngle / 180.0f * (float)PI;
				const float angleDiv = ( 360.0f / (float)_amount ) / 180.0f * (float)PI;
				for( int i = 0; i < _amount; i++, angle += angleDiv )
				{
					const CVec spread = CVec( sinf(angle) * _radius , cosf(angle) * _radius );
					SpawnEntity(ENT_SPARKLE, _fade, pos + spread, vel, Color(), NULL);
				}
			}
			break;

		case ENTE_SPARKLE_CIRCLE_ROTATING:
			{
				_lastAngle += _speed;
				while( _lastAngle > 360.0f )
					_lastAngle -= 360.0f;
				float angle = _lastAngle / 180.0f * (float)PI;
				const float angleDiv = ( 360.0f / (float)_amount ) / 180.0f * (float)PI;
				Color c;
				{
					const float shield = _parent->shieldFactor() - 1.0f;
					const float damage = _parent->damageFactor() - 1.0f;
					float sum = shield + damage; if(fabs(sum) < 0.01) sum = 1;
					c = Color(120,120,255) * (damage / sum) + Color(255,30,30) * (shield / sum);
				}
				for( int i = 0; i < _amount; i++, angle += angleDiv )
				{
					const CVec spread = CVec( sinf(angle), cosf(angle) ) * _radius;
					const CVec addVel = CVec( sinf(angle - (float)PI/1.5f), cosf(angle - (float)PI/1.5f) ) * _radius * _speed * _delay * 2.0f;
					SpawnEntity(ENT_SPARKLE, _fade, pos + spread, vel + addVel, c, NULL);
				}
			}
			break;

	}
}


