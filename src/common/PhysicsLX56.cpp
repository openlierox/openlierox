/*
	OpenLieroX

	the LX56 physics

	code under LGPL
	created on 9/2/2008
*/

/*
 some references:
 
 LX56 projectile simulation:
 http://openlierox.svn.sourceforge.net/viewvc/openlierox/src/client/CClient_Game.cpp?revision=1&view=markup&pathrev=1
 
 LX56 Ninjarope simulation:
 http://openlierox.svn.sf.net/viewvc/openlierox/src/client/CNinjaRope.cpp?revision=1&view=markup&pathrev=1
 
 */


#include "LieroX.h"
#include "ProfileSystem.h"
#include "Physics.h"
#include "CGameScript.h"
#include "MathLib.h"
#include "game/CWorm.h"
#include "Entity.h"
#include "CBonus.h"
#include "CClient.h"
#include "OLXConsole.h"
#include "Debug.h"
#include "CServer.h"
#include "FlagInfo.h"
#include "ProjectileDesc.h"
#include "WeaponDesc.h"
#include "PhysicsLX56.h"
#include "sound/SoundsBase.h"
#include "game/Sounds.h"
#include "game/Game.h"


// defined in PhysicsLX56_Projectiles
void LX56_simulateProjectiles(Iterator<CProjectile*>::Ref projs);

// Default LX56PhysicsFPS is 84.
static int _getCurrentLX56PhysicsFPS() {
	if(game.isServer())
		return gameSettings[FT_LX56PhysicsFPS];
	else if(cClient->getGameLobby()[FT_ForceSameLX56PhysicsFPS])
		return cClient->getGameLobby()[FT_LX56PhysicsFPS];
	else // each client can have its own custom FPS
		return gameSettings[FT_LX56PhysicsFPS]; // this is the client side config
	return 84;
}

int getCurrentLX56PhysicsFPS() {
	return MAX(_getCurrentLX56PhysicsFPS(), 1);
}

static float getNinjaropePrecision() {
	if(cClient->getServerVersion() >= OLXBetaVersion(0,57,4))
		return (float)cClient->getGameLobby()[FT_NinjaropePrecision];
	else
		return 0; // like LX56
}


static bool m_inited = false;

// ---------

std::string PhysicsEngine::name() { return "LX56 physics"; }

void PhysicsEngine::initGame() { m_inited = true; }
void PhysicsEngine::uninitGame() { m_inited = false; }
bool PhysicsEngine::isInitialised() { return m_inited; }


static void simulateNinjarope(float dt, CWorm* owner);
static void simulateWormWeapon(float dt, CWorm* worm);

// -----------------------------
// ------ worm -----------------

// Check collisions with the level
// HINT: it directly manipulates vPos!
static bool moveAndCheckWormCollision(AbsTime currentTime, float dt, CWorm* worm, CVec pos, CVec *vel, CVec vOldPos, bool jump ) {
	static const int maxspeed2 = 10; // this should not be too high as we could run out of the game.gameMap() without checking else

	// Can happen when starting a game
	if (!game.gameMap())
		return false;
	CMap* map = game.gameMap();

	// check if the vel is really too high (or infinity), in this case just ignore
	if( (*vel*dt*worm->speedFactor()).GetLength2() > (float)map->GetWidth() * (float)map->GetHeight() )
		return true;

	// If the worm is going too fast, divide the speed by 2 and perform 2 collision checks
	// TODO: is this still needed? we call this function with a fixed dt
	// though perhaps it is as with higher speed the way we have to check is longer
	if( (*vel * dt * worm->speedFactor()).GetLength2() > maxspeed2 && dt > 0.001f ) {
		dt /= 2;
		if(moveAndCheckWormCollision(currentTime, dt,worm,pos,vel,vOldPos,jump)) return true;
		return moveAndCheckWormCollision(currentTime, dt,worm,worm->getPos(),vel,vOldPos,jump);
	}

	bool wrapAround = cClient->getGameLobby()[FT_InfiniteMap];
	pos += *vel * dt * worm->speedFactor();
	if(wrapAround) {
		FMOD(pos.x, (float)map->GetWidth());
		FMOD(pos.y, (float)map->GetHeight());
	}
	worm->pos() = pos;


	int x = (int)pos.x;
	int y = (int)pos.y;
	short clip = 0; // 0x1=left, 0x2=right, 0x4=top, 0x8=bottom
	bool coll = false;

	if(y >= 0 && (uint)y < map->GetHeight()) {
		for(x=-3;x<4;x++) {
			// Optimize: pixelflag++

			// Left side clipping
			if(!wrapAround && (pos.x+x <= 2)) {
				clip |= 0x01;
				worm->pos().write().x = 5;
				coll = true;
				if(fabs(vel->x) > 40)
					vel->x *=  -0.4f;
				else
					vel->x=(0);
				continue; // Note: This was break in LX56, but continue is really better here
			}

			// Right side clipping
			if(!wrapAround && (pos.x+x >= map->GetWidth())) {
				worm->pos().write().x = (float)map->GetWidth() - 5;
				coll = true;
				clip |= 0x02;
				if(fabs(vel->x) > 40)
					vel->x *= -0.4f;
				else
					vel->x=(0);
				continue; // Note: This was break in LX56, but continue is really better here
			}

			int posx = (int)pos.x + x; if(wrapAround) { posx %= (int)map->GetWidth(); if(posx < 0) posx += map->GetWidth(); }
			if(!(map->GetPixelFlag(posx,y) & PX_EMPTY)) {
				coll = true;

				// NOTE: Be carefull that you don't do any float->int->float conversions here.
				// This has some *huge* effect. People reported it as high ceil friction.
				if(x<0) {
					clip |= 0x01;
					worm->pos().write().x = pos.x + x + 4;
				}
				else {
					clip |= 0x02;
					worm->pos().write().x = pos.x + x - 4;
				}

				// Bounce
				if(fabs(vel->x) > 40)
					vel->x *= -0.4f;
				else
					vel->x=(0);
			}
		}
	}

	// In case of this, it could be that we need to do a FMOD. Just do it to be sure.
	if(wrapAround) {
		FMOD(pos.x, (float)map->GetWidth());
	}

	worm->setOnGround( false );

	bool hit = false;
	x = (int)pos.x;

	if(x >= 0 && (uint)x < map->GetWidth()) {
		for(y=5;y>-5;y--) {
			// Optimize: pixelflag + Width

			// Top side clipping
			if(!wrapAround && (pos.y+y <= 1)) {
				worm->pos().write().y = 6;
				coll = true;
				clip |= 0x04;
				if(fabs(vel->y) > 40)
					vel->y *= -0.4f;
				else
					vel->y = (0);

				// it was also break in LX56. this makes a huge difference when bouncing against the bottom.
				break;
			}

			// Bottom side clipping
			if(!wrapAround && (pos.y+y >= map->GetHeight())) {
				worm->pos().write().y = (float)map->GetHeight() - 5;
				clip |= 0x08;
				coll = true;
				worm->setOnGround( true );
				if(fabs(vel->y) > 40)
					vel->y *= -0.4f;
				else
					vel->y=(0);
				continue; // Note: This was break in LX56, but continue is really better here
			}

			int posy = (int)pos.y + y; if(wrapAround) { posy %= (int)map->GetHeight(); if(posy < 0) posy += map->GetHeight(); }
			if(!(map->GetPixelFlag(x,posy) & PX_EMPTY)) {
				coll = true;

				if(!hit && !jump) {
					if(fabs(vel->y) > 40 && ((vel->y > 0 && y>0) || (vel->y < 0 && y<0)))
						vel->y *= -0.4f;
					else
						vel->y=(0);
				}

				hit = true;
				worm->setOnGround( true );

				// NOTE: Be carefull that you don't do any float->int->float conversions here.
				// This has some *huge* effect. People reported it as high ceil friction.
				if(y<0) {
					clip |= 0x04;
					worm->pos().write().y = pos.y + y + 5;
				}
				else {
					clip |= 0x08;
					worm->pos().write().y = pos.y + y - 5;
				}
			}
		}
	}

	// In case of this, it could be that we need to do a FMOD. Just do it to be sure.
	if(wrapAround) {
		FMOD(pos.y, (float)map->GetHeight());
	}

	// If we are stuck in left & right or top & bottom, just don't move in that direction
	if ((clip & 0x01) && (clip & 0x02))
		worm->pos().write().x = vOldPos.x;

	// HINT: when stucked horizontal we move slower - it's more like original LX
	if ((clip & 0x04) && (clip & 0x08))  {
		worm->pos().write().y = vOldPos.y;
		if (!worm->tState.get().bJump)  // HINT: this is almost exact as old LX
			worm->pos().write().x = vOldPos.x;
	}

	// If we collided with the ground and we were going pretty fast, make a bump sound
	if(coll) {
		if( fabs(vel->x) > 30 && (clip & 0x01 || clip & 0x02) )
			StartSound( sfxGame.smpBump, worm->pos(), worm->getLocal(), -1 );
		else if( fabs(vel->y) > 30 && (clip & 0x04 || clip & 0x08) )
			StartSound( sfxGame.smpBump, worm->pos(), worm->getLocal(), -1 );
	}

	return coll;
}


void PhysicsEngine::simulateWorm(CWorm* worm, bool local) {
	if(game.gameScript()->gusEngineUsed()) return;

	// TODO: Later, we should have a message bus for input-events which is filled
	// by goleft/goright/stopleft/stopright/shoot/etc signals. These signals are handled in here.
	// Though the key/mouse event handling (what CWorm::getInput() is doing atm)
	// is then handled in the InputEngine and not called from here because
	// the PhysicEngine should not care about when the key/mouse events are handled.

	// TODO: count the amount of shootings
	// The problem is that we eventually count one shoot-press twice or also the
	// other way around, twice shoot-presses only once.
	// To solve this in a clean way, we really need this message bus. A dirty way
	// to solve it would be to send wormstate-updates more often if needed
	// with some checks here. (In the end, also the clean way would result in more
	// worm updates.)

	// get input max once a frame (and not at all if we don't simulate this frame)
	// TODO: This is not the case anymore. Is this bad?
	
		/*
			Only get input for this worm on certain conditions:
			1) This worm is a local worm (ie, owned by me)
			2) We're not in a game menu
			3) We're not typing a message
			4) weapons selected
		*/

	if(cClient && local && !cClient->isGameMenu() && !cClient->isChatTyping() && !game.gameOver && !Con_IsVisible() && worm->bWeaponsReady) {
		int old_weapon = worm->getCurrentWeapon();

		worm->getInput();

		if (worm->isShooting() || old_weapon != worm->getCurrentWeapon())  // The weapon bar is changing
			cClient->shouldRepaintInfo() = true;
	}

	const worm_state_t *ws = &worm->tState.get();

	if(!worm->getAlive()) return;
	
	// We expect that this gets called with this fixed FPS.
	// Gamespeed multiplicators or other things are handled from the outside.
	float dt = LX56PhysicsDT.seconds();

	// If we're seriously injured (below 15% health) and visible, bleed
	// HINT: We have to check the visibility for everybody as we don't have entities for specific teams/worms.
	// If you want to make that better, you would have to give the CViewport to simulateWorm (but that would be really stupid).
	if(worm->getHealth() < 15 && worm->isVisibleForEverybody()) {
		if(GetPhysicsTime() > worm->getLastBlood() + 2.0f) {
			worm->setLastBlood( GetPhysicsTime() );

			const float amount = ((float)tLXOptions->iBloodAmount / 100.0f) * 20;
			for(short i=0;i<amount;i++) {
				SpawnEntity(ENT_BLOOD,0,worm->getPos(),GetRandomVec()*100,Color(200,0,0),NULL);
				SpawnEntity(ENT_BLOOD,0,worm->getPos(),GetRandomVec()*100,Color(180,0,0),NULL);
			}
		}
	}


	// Process the carving
	if(ws->bCarve) {
		const CVec dir = worm->getMoveDirection();
		worm->incrementDirtCount( CarveHole(worm->getPos() + dir*4) );
		//cClient->SendCarve(vPos + dir*4);
	}

	static const float	fFrameRate = 7.5f;
	if(ws->bMove)
		worm->frame() += fFrameRate * dt;

	if(worm->frame() >= 3.0f || !ws->bMove)
		worm->frame() = 0.0f;
	if(worm->frame() < 0)
		worm->frame() = 2.99f;

	// Process the ninja rope
	if(worm->getNinjaRope()->isReleased()) {
		simulateNinjarope( dt, worm );
		worm->velocity() += worm->getNinjaRope()->GetForce() * dt;
	}

	if(!(bool)cClient->getGameLobby()[FT_GusanosWormPhysics]) {
		// Process the moving
		if(ws->bMove) {
			const bool onGround = worm->isOnGround();
			const float speed = onGround ? (float)cClient->getGameLobby()[FT_WormGroundSpeed] : (float)cClient->getGameLobby()[FT_WormAirSpeed];
			const float maxspeed = onGround ? (float)cClient->getGameLobby()[FT_WormMaxGroundMoveSpeed] : (float)cClient->getGameLobby()[FT_WormMaxAirMoveSpeed];

			if(worm->getMoveDirectionSide() == DIR_RIGHT) {
				// Right
				if(worm->velocity().get().x < maxspeed)
					worm->velocity().write().x += speed * dt * 90.0f;
			} else {
				// Left
				if(worm->velocity().get().x > -maxspeed)
					worm->velocity().write().x -= speed * dt * 90.0f;
			}
		}


		// Process the jump
		bool jumped = false;
		{
			const bool onGround = worm->CheckOnGround();
			const float jumpForce = cClient->getGameLobby()[FT_WormJumpForce];

			if( onGround )
				worm->setLastAirJumpTime(AbsTime());
			if(ws->bJump && ( onGround || worm->canAirJump() ||
				( bool(cClient->getGameLobby()[FT_RelativeAirJump]) && GetPhysicsTime() >
					worm->getLastAirJumpTime() + float( cClient->getGameLobby()[FT_RelativeAirJumpDelay] ) ) ))
			{
				if( onGround )
					worm->velocity().write().y = jumpForce;
				else {
					// GFX effect, as in TeeWorlds (we'll change velocity after that)
					SpawnEntity(ENT_SPARKLE, 10, worm->getPos() + CVec( 0, 4 ), worm->velocity() + CVec( 0, 40 ), Color(), NULL );
					SpawnEntity(ENT_SPARKLE, 10, worm->getPos() + CVec( 2, 4 ), worm->velocity() + CVec( 20, 40 ), Color(), NULL );
					SpawnEntity(ENT_SPARKLE, 10, worm->getPos() + CVec( -2, 4 ), worm->velocity() + CVec( -20, 40 ), Color(), NULL );

					if( !(bool)cClient->getGameLobby()[FT_JumpToAimDir] && worm->canAirJump() && worm->velocity().get().y > jumpForce ) // Negative Y coord = moving up
						worm->velocity().write().y = jumpForce; // Absolute velocity - instant air jump
					else {
						CVec dir = (bool)cClient->getGameLobby()[FT_JumpToAimDir] ? -worm->getFaceDirection() : CVec(0.0f,1.0f);
						worm->velocity() += dir * jumpForce; // Relative velocity - relative air jump
					}
				}
				worm->setLastAirJumpTime(GetPhysicsTime());
				worm->setOnGround( false );
				jumped = true;
			}
		}

		{
			// Air drag (Mainly to dampen the ninja rope)
			const float Drag = cClient->getGameLobby()[FT_WormAirFriction];

			if(!worm->isOnGround())	{
				worm->velocity().write().x -= SQR(worm->velocity().get().x) * SIGN(worm->velocity().get().x) * Drag * dt;
				worm->velocity().write().y += -SQR(worm->velocity().get().y) * SIGN(worm->velocity().get().y) * Drag * dt;
			}
		}

		// Gravity
		worm->velocity().write().y += (float)cClient->getGameLobby()[FT_WormGravity] * dt;

		{
			const float friction = cClient->getGameLobby()[FT_WormFriction];
			if(friction > 0) {
				static const float wormSize = 5.0f;
				static const float wormMass = (wormSize/2) * (wormSize/2) * (float)PI;
				static const float wormDragCoeff = 0.1f; // Note: Never ever change this! (Or we have to make this configureable)
				applyFriction(worm->velocity().write(), dt, wormSize, wormMass, wormDragCoeff, friction);
			}
		}


		// Check collisions and move
		moveAndCheckWormCollision( GetPhysicsTime(), dt, worm, worm->getPos(), &worm->velocity().write(), worm->getPos(), jumped );


		// Ultimate in friction
		if(worm->isOnGround()) {
			worm->velocity().write().x *= 1.0f - float(cClient->getGameLobby()[FT_WormGroundFriction]);

			// Too slow, just stop
			if(fabs(worm->velocity().get().x) < (float)cClient->getGameLobby()[FT_WormGroundStopSpeed] && !ws->bMove)
				worm->velocity().write().x = 0;
		}
	}

	// Gusanos worm physics
	else {
		worm->jumping = ws->bJump;
		worm->movingLeft = ws->bMove && worm->getMoveDirectionSide() == DIR_LEFT;
		worm->movingRight = ws->bMove && worm->getMoveDirectionSide() == DIR_RIGHT;

		CGameObject::ScopedGusCompatibleSpeed scopedSpeed(*worm);

		VectorD2<float> next = worm->pos() + worm->velocity();

		VectorD2<long> inext(static_cast<long>(next.x), static_cast<long>(next.y));

		worm->calculateAllReactionForces(next, inext);

		worm->processJumpingAndNinjaropeControls();
		worm->processPhysics();
		worm->processMoveAndDig();

		worm->tState.write().bJump = worm->jumping; // we may have overwritten this
	}

	simulateWormWeapon(dt, worm);

	worm->posRecordings.push_back(worm->getPos());
}

static void simulateWormWeapon(float dt, CWorm* worm) {
	if(worm->tWeapons.size() == 0) return;

	// Without FT_GameSpeedOnlyForProjs, we apply the gamespeed multiplicator logic outside.
	if((bool)cClient->getGameLobby()[FT_GameSpeedOnlyForProjs])
		dt *= (float)cClient->getGameLobby()[FT_GameSpeed];
	
	wpnslot_t *Slot = worm->writeCurWeapon();
	if(!Slot->weapon()) return;
	
	// Slot should still do the reloading even without enabled wpn, thus uncommented this check.
	//if(!Slot->Enabled) return;

	if(Slot->LastFire > 0)
		Slot->LastFire -= dt;

	if(Slot->Reloading) {
		if((int)cClient->getGameLobby()[FT_LoadingTime] == 0)
			Slot->Charge = 1.f;
		else
			Slot->Charge += dt * (Slot->weapon()->Recharge * (1.0f/((int)cClient->getGameLobby()[FT_LoadingTime] * 0.01f)));

		if(Slot->Charge >= 1.f) {
			Slot->Charge = 1.f;
			Slot->Reloading = false;
		}
	}
}

void PhysicsEngine::simulateProjectiles(Iterator<CProjectile*>::Ref projs) {
	if(game.gameScript()->gusEngineUsed()) return;
	LX56_simulateProjectiles(projs);
}

static void simulateNinjarope(float dt, CWorm* owner) {
	CNinjaRope* rope = &owner->cNinjaRope.write();
	CVec playerpos = owner->getPos();

	const bool wrapAround = cClient->getGameLobby()[FT_InfiniteMap];

	if(!rope->isReleased())
		return;

	const bool firsthit = !rope->isAttached();
	CVec force = rope->isShooting() ?
		CVec(0.0f, (float)cClient->getGameLobby()[FT_RopeGravity]) :
		CVec(0.0f, (float)cClient->getGameLobby()[FT_RopeFallingGravity]);

	// dt is fixed, but for very high speed, this could be inaccurate.
	// We use the limit 5 here to have it very unpropable to shoot through a wall.
	// In most cases, dt his halfed once, so this simulateNinjarope is
	// like in LX56 with 200FPS.
	if((rope->hookVelocity() + force*dt).GetLength2() * dt * dt * getNinjaropePrecision() > 5) {
		simulateNinjarope( dt/2, owner );
		simulateNinjarope( dt/2, owner );
		return;
	}

	// Still flying in the air
	if(rope->isShooting()) {

		// Gravity
		rope->hookVelocity() += force*dt;
		rope->hookPos() += rope->hookVelocity() * dt;

		const float length2 = (playerpos - rope->hookPos()) . GetLength2();

		// Check if it's too long
		const float ropeMaxLength = (float)(int)cClient->getGameLobby()[FT_RopeMaxLength];
		if(length2 > ropeMaxLength * ropeMaxLength) {
			rope->hookVelocity() = CVec(0,0);
			rope->setShooting( false );
		}
	}
	// Falling
	else if(!rope->isShooting() && !rope->isAttached()) {

		// Going towards the player
		const float length2 = (playerpos - rope->hookPos()) . GetLength2();
		const float ropeRestLength = (float)(int)cClient->getGameLobby()[FT_RopeRestLength];
		if(length2 > ropeRestLength * ropeRestLength) {

			// Pull the hook back towards the player
			CVec d = playerpos - rope->hookPos();
			if(length2) d *= (float)(1.0f/sqrt(length2)); // normalize

			force += (d*10000)*dt;
		}

		rope->hookVelocity() += force * dt;
		rope->hookPos() += rope->hookVelocity() * dt;
	}


	bool outsideMap = false;

	// Hack to see if the hook went out of the game.gameMap()
	if(!rope->isPlayerAttached() && !wrapAround)
		if(
			rope->hookPos().get().x <= 0 || rope->hookPos().get().y <= 0 ||
			rope->hookPos().get().x >= game.gameMap()->GetWidth()-1 ||
			rope->hookPos().get().y >= game.gameMap()->GetHeight()-1) {
		rope->Attach();

		// Make the hook stay at an edge
		rope->hookPos().write().x = ( MAX((float)0, rope->hookPos().get().x) );
		rope->hookPos().write().y = ( MAX((float)0, rope->hookPos().get().y) );

		rope->hookPos().write().x = ( MIN(game.gameMap()->GetWidth()-(float)1, rope->hookPos().get().x) );
		rope->hookPos().write().y = ( MIN(game.gameMap()->GetHeight()-(float)1, rope->hookPos().get().y) );

		outsideMap = true;
	}


	// Check if the hook has hit anything on the game.gameMap()
	if(!rope->isPlayerAttached()) {
		rope->setAttached( false );

		VectorD2<long> wrappedHookPos = rope->hookPos().get();
		MOD(wrappedHookPos.x, (long)game.gameMap()->GetWidth());
		MOD(wrappedHookPos.y, (long)game.gameMap()->GetHeight());

		const Material& px = game.gameMap()->getMaterial((uint)wrappedHookPos.x, (uint)wrappedHookPos.y);
		if(!px.particle_pass || outsideMap) {
			if(!outsideMap && !px.can_hook) {
				rope->hookVelocity() = CVec(0,0);
				rope->setShooting( false );
			}
			else {
				rope->Attach();

				if(px.destroyable && firsthit) {
					Color col = game.gameMap()->getColorAt(wrappedHookPos.x, wrappedHookPos.y);
					for( short i=0; i<5; i++ )
						SpawnEntity(ENT_PARTICLE,0, rope->hookPos() + CVec(0,2), GetRandomVec()*40,col,NULL);
				}
			}
		}
	}

	rope->checkForWormAttachment();
}


// --------------------
// ---- Bonus ---------

static void colideBonus(CBonus* bonus, int x, int y) {
	bonus->velocity() = CVec(0,0);
	bonus->pos().y = (float)y - 2;
}

static void simulateBonus(CBonus* bonus) {
	// We expect that this gets called with this fixed FPS.
	// Gamespeed multiplicators or other things are handled from the outside.
	float dt = LX56PhysicsDT.seconds();

	int x,  y;
	int mw, mh;
	int px, py;


	bonus->life() += dt;
	if(bonus->life() > BONUS_LIFETIME-3) {
		bonus->flashTime() += dt;

		if(bonus->flashTime() > 0.3f)
			bonus->flashTime() = 0;
	}

	//
	// Position & Velocity
	//
	bonus->velocity().y += 80 * dt;
	bonus->pos() += bonus->velocity() * dt;


	//
	// Check if we are hitting the ground
	//


	px = (int)bonus->pos().x;
	py = (int)bonus->pos().y;

	// Initialize
	x = px-2;
	y = py-2;


	mw = game.gameMap()->GetWidth();
	mh = game.gameMap()->GetHeight();

	for(y=py-2; y<=py+2; y++) {

		// Clipping
		if(y<0)
			continue;
		if(y>=mh) {
			if( cClient->getGameLobby()[FT_InfiniteMap] )
				bonus->pos().y -= (float)mh;
			else
				colideBonus(bonus, x,y);
			return;
		}

		// TODO: do we need to lock pixel flags here?

		for(x=px-2; x<=px+2; x++) {

			// Clipping
			if(x<0) {
				continue;
			}
			if(x>=mw) {
				colideBonus(bonus, x,y); // TODO: should not happen, remove that (bonus doesn't move horizontally). And why checking only right map border, not left one?
				return;
			}

			if(game.gameMap()->GetPixelFlag(x,y) & (PX_DIRT|PX_ROCK)) {
				colideBonus(bonus, x,y);
				return;
			}
		}
	}
}

void PhysicsEngine::simulateBonuses(CBonus* bonuses, size_t count) {
	if(game.gameScript()->gusEngineUsed()) return;

	if(!cClient->getGameLobby()[FT_Bonuses])
		return;

	if(!game.gameMap()) return;

	CBonus *b = bonuses;

	for(size_t i=0; i < count; i++,b++) {
		if(!b->getUsed())
			continue;

		simulateBonus(b);
	}
}


PhysicsEngine* PhysicsEngine::Get() {
	static PhysicsEngine engine;
	return &engine;
}
