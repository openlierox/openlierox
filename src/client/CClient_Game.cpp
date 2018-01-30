////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Client class - Game routines
// Created 21/7/02
// Jason Boettcher

#include <algorithm>

#include "LieroX.h"
#include "Clipboard.h"
#include "AuxLib.h"
#include "MathLib.h"
#include "CClient.h"
#include "CServer.h"
#include "CServerConnection.h"
#include "CBonus.h"
#include "OLXConsole.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/Graphics.h"
#include "StringUtils.h"
#include "game/CWorm.h"
#include "Entity.h"
#include "Protocol.h"
#include "Physics.h"
#include "CClient.h"
#include "CClientNetEngine.h"
#include "ProfileSystem.h"
#include "Debug.h"
#include "NewNetEngine.h"
#include "CGameMode.h"
#include "CHideAndSeek.h"
#include "ProjectileDesc.h"
#include "WeaponDesc.h"
#include "game/Game.h"
#include "sound/SoundsBase.h"
#include "game/Sounds.h"
#include "CGameScript.h"
#include "util/Random.h"


CClient		*cClient = NULL;


bool CClient::shouldDoProjectileSimulation() {
	if(bDedicated && game.isServer() && !tLXOptions->doProjectileSimulationInDedicated) return false;
	return true;
}


///////////////////
// Simulation
void CClient::Simulation()
{
	AbsTime currentTime = GetPhysicsTime();
	const TimeDiff orig_dt = LX56PhysicsDT;
	TimeDiff frame_dt = orig_dt;
	if(!(bool)cClient->getGameLobby()[FT_GameSpeedOnlyForProjs]) {
		frame_dt *= 1.0f/CLAMP((float)cClient->getGameLobby()[FT_GameSpeed], 0.05f, 10.0f);
		if(frame_dt <= TimeDiff(0))
			frame_dt = TimeDiff(1);
	}

	// Don't simulate if the physics engine is not ready
	if (!PhysicsEngine::Get() || !PhysicsEngine::Get()->isInitialised())  {
		errors << "WARNING: trying to simulate with non-initialized physics engine!" << endl;
		return;
	}

	if(game.gameOver) {
		// Clear the input of the local worms
		clearLocalWormInputs();
	}

	// Note that this is slightly something different than gameOver.
	if(!game.shouldDoPhysicsFrame()) {
		// Clear the input of the local worms
		clearLocalWormInputs();

		// skip simulations for this frame
		PhysicsEngine::Get()->skipProjectiles(cProjectiles.begin());
		
		cClient->fLastSimulationTime = currentTime;
		return;		
	}
	
simulateStart:
	if(cClient->fLastSimulationTime + frame_dt > currentTime) return;

	// TODO: does it work also, if we
	// 1. simulate all worms
	// 2. check collisions with bonuses
	// (at the moment, we are doing these 2 things at once in the loop, I want to have 2 loops)
	// TODO: make it working
	// TODO: create a function simulateWorms() in PhysicsEngine which does all worms-simulation

	// Player simulation
	for_each_iterator(CWorm*, _w, game.worms()) {
		CWorm* w = _w->get();

		bool wasShootingBefore = w->tState.get().bShoot;
		const weapon_t* oldWeapon = w->getCurWeapon()->weapon();

		// Simulate the worm. In case the worm is dead, it (the inputhandler) might do some thinking or
		// request for respawn or so.
		// TODO: move this to a simulateWorms() in PhysicsEngine
		PhysicsEngine::Get()->simulateWorm( w, w->getLocal() );

		if(game.gameOver)
			continue;

		if(w->getAlive()) {

			// Check for damaging areas
			const Material& mat = game.gameMap()->getMaterial(
						(unsigned int)CLAMP(w->getPos().x, 0.0f, (float)game.gameMap()->GetWidth() - 1),
						(unsigned int)CLAMP(w->getPos().y, 0.0f, (float)game.gameMap()->GetHeight() - 1));
			if(mat.damage)
				InjureWorm(w, (float)mat.damage, -1);

			// Check if this worm picked up a bonus

			CBonus *b = cBonuses;
			if (getGameLobby()[FT_Bonuses])  {
				for(short n = 0; n < MAX_BONUSES; n++, b++) {
					if(!b->getUsed())
						continue;

					if(w->CheckBonusCollision(b)) {
						if(w->getLocal() || (game.isServer() && tLXOptions->bServerSideHealth)) {

							if( w->GiveBonus(b) ) {

								// Pickup noise
								PlaySoundSample(sfxGame.smpPickup);

								DestroyBonus(n, w->getLocal(), w->getID());

								bShouldRepaintInfo = true;
							}
						}
					}
				}
			}
		}

		// from CServerNetEngine::ParseUpdate. we don't send updates of local worms anymore, so we have to handle this here
		if(game.isServer()) {
			// If the worm is shooting, handle it
			if (w->tState.get().bShoot && w->getAlive())
				cServer->WormShoot(w); // handle shot and add to shootlist to send it later to the clients
			
			// handle FinalProj for weapon
			if(oldWeapon && ((wasShootingBefore && !w->tState.get().bShoot) || (wasShootingBefore && oldWeapon != w->getCurWeapon()->weapon())))
				cServer->WormShootEnd(w, oldWeapon);
		}

		// TODO: move this to physics as it should also be FPS independent
		// Is this worm shooting?
		if(w->getAlive()) {

			// Shoot
			if(w->tState.get().bShoot) {
				// This handles only client-side weapons, like jetpack and for beam drawing
				// It doesn't process the shot itself.
				// The shot-info will be sent to the server which sends it back and
				// we handle it in CClient::ProcessShot in the end.
				PlayerShoot(w);
			}

			// If the worm is using a weapon with a laser sight, spawn a laser sight
			if (w->getCurWeapon()->weapon())
				if(w->getCurWeapon()->weapon()->LaserSight && !w->getCurWeapon()->Reloading)
					LaserSight(w, w->getAngle());
			
			// Show vision cone of seeker worm
			if( getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode == GameMode(GM_HIDEANDSEEK) &&
				w->getTeam() == HIDEANDSEEK_SEEKER  )
			{
				int Angle = gameSettings[FT_HS_SeekerVisionAngle];
				if( Angle < 360 )
				{
					LaserSight(w, w->getAngle() + Angle/2, false );
					LaserSight(w, w->getAngle() - Angle/2, false );
				}
			}
		}


		// In a timed game increment the tagged worms time
		if(getGeneralGameType() == GMT_TIME && w->getTagIT())  {
			w->incrementTagTime(frame_dt);

			// Log
			log_worm_t *l = GetLogWorm(w->getID());
			if (l)
				l->fTagTime += frame_dt;
		}
	}

	// Entities
	// only some gfx effects, therefore it doesn't belong to PhysicsEngine
	if(!bDedicated)
		SimulateEntities(orig_dt);

	// Projectiles
	if(shouldDoProjectileSimulation())
		PhysicsEngine::Get()->simulateProjectiles(cProjectiles.begin());

	// Bonuses
	PhysicsEngine::Get()->simulateBonuses(cBonuses, MAX_BONUSES);
	
	cClient->fLastSimulationTime += frame_dt;
	goto simulateStart;
}


void CClient::NewNet_Simulation() // Simulates one frame, delta time always set to 10 ms, ignores current time
{

	// Don't simulate if the physics engine is not ready
	if (!PhysicsEngine::Get()->isInitialised())  {
		errors << "WARNING: trying to simulate with non-initialized physics engine!" << endl;
		return;
	}

	// Local game always uses old net engine, so NewNet_Simulation() should never be called on local game.
	if (game.isLocalGame())  {
		errors << "WARNING: new net engine is used for local game!" << endl;
		return;
	}

    // We stop a few seconds after the actual game over
	if(game.gameOver && game.gameOverTime().seconds() > GAMEOVER_WAIT)
        return;


	// Player simulation
	for_each_iterator(CWorm*, w_, game.worms()) {
		CWorm* w = w_->get();

		if(w->getAlive()) 
		{
			// Remote worm -> do not get input for worm - worm input is simulated beforew this function called
			PhysicsEngine::Get()->simulateWorm( w, false ); 
		}

		if(w->getAlive()) 
		{
			// Shoot
			if(w->tState.get().bShoot) {
				// This handles only client-side weapons, like jetpack and for beam drawing
				// It doesn't process the shot itself.
				// The shot-info will be sent to the server which sends it back and
				// we handle it in CClient::ProcessShot in the end.
				NewNet_DoLocalShot( w );
			}

			// TODO: we should move this stuff to drawing, we may skip it for physics calculation
			// If the worm is using a weapon with a laser sight, spawn a laser sight
			if (w->getCurWeapon()->weapon())
				if(w->getCurWeapon()->weapon()->LaserSight && !w->getCurWeapon()->Reloading)
					LaserSight(w, w->getAngle());
		}
	}

	// Entities
	// only some gfx effects, therefore it doesn't belong to PhysicsEngine
	// TODO: we should move this stuff to drawing, we may skip it for physics calculation
	if(!bDedicated)
		SimulateEntities(TimeDiff(NewNet::TICK_TIME));

	// Projectiles
	PhysicsEngine::Get()->simulateProjectiles(cProjectiles.begin());
}


///////////////////
// Explosion
void CClient::Explosion(AbsTime time, CVec pos, float damage, int shake, int owner, std::string weapon)
{	
	bool    gotDirt = false;
	Color	DirtEntityColour;
	
	{
		int		x,y,px;
		if(game.gameMap()->GetTheme())
			DirtEntityColour = game.gameMap()->GetTheme()->iDefaultColour;

		// Go through until we find dirt to throw around
		y = MIN((uint)pos.y,game.gameMap()->GetHeight()-1);

		px = (uint)pos.x;

		for(x=px-2; x<px+2; x++) {
			// Clipping
			if(x < 0)	continue;
			if(x >= (int)game.gameMap()->GetWidth())	break;

			if(game.gameMap()->GetPixelFlag(x,y) & PX_DIRT) {
				DirtEntityColour = game.gameMap()->getColorAt(x,y);
				gotDirt = true;
				break;
			}
		}
	}
	
	// Go through bonuses. If any were next to an explosion, destroy the bonus explosivly
	if (getGameLobby()[FT_Bonuses])  {
		CBonus *b = cBonuses;
		for(int i=0; i < MAX_BONUSES; i++,b++) {
			if(!b->getUsed())
				continue;

			if( fabs(b->getPosition().x - pos.x) > 15 )
				continue;
			if( fabs(b->getPosition().y - pos.y) > 15 )
				continue;

			b->setUsed(false);
			Explosion(time, pos,15,5,owner);
		}
	}


    // Go through projectiles. If any were next to an explosion, set the projectile's explode event to true
	/*  CProjectile *prj = cProjectiles;
    for( i=0; i<MAX_PROJECTILES; i++, prj++ ) {
        if( !prj->isUsed() )
            continue;

        if( fabs(prj->GetPosition().x - pos.x) > 15 )
            continue;
        if( fabs(prj->GetPosition().y - pos.y) > 15 )
            continue;

        prj->setExplode( tLX->currentTime + CalculateDistance(prj->GetPosition(),pos) / 500.0f, true);
    }*/


	// Particles
    if(gotDirt) {
	    for(short x=0;x<2;x++)
		    SpawnEntity(ENT_PARTICLE,0,pos,GetRandomVec().multPairwise(30,10),DirtEntityColour,NULL);
    }


	int expsize = 8;
	if(damage > 5)
		expsize = (int)damage;

	// Explosion
	SpawnEntity(ENT_EXPLOSION, expsize, pos, CVec(0,0),Color(),NULL);

	int d = game.gameMap()->CarveHole((int)damage,pos, getGameLobby()[FT_InfiniteMap]);

    // Increment the dirt count
	if(CWorm* ownerW = game.wormById(owner, false))
		ownerW->incrementDirtCount( d );

	// If this is within a viewport, shake the viewport
	for(int i=0; i<NUM_VIEWPORTS; i++) {
		if(!cViewports[i].getUsed())
            continue;

		if(shake) {
			if(cViewports[i].inView(pos))
				cViewports[i].Shake(shake);
		}
	}


	// Check if any worm is near the explosion, for both my worms and remote worms
	for_each_iterator(CWorm*, w_, game.aliveWorms()) {
		CWorm* w = w_->get();

		CVec wPos = w->posRecordings.getBest((size_t)LX56PhysicsDT.milliseconds(), (size_t)(tLX->currentTime - time).milliseconds());		
		if((pos - wPos).GetLength2() <= 25) {
			// Injure him
			InjureWorm(w, damage, owner, weapon);
		}
	}
}


///////////////////
// Injure a worm
void CClient::InjureWorm(CWorm *w, float damage, int owner, std::string weapon)
{
	if (!w->getAlive())  // Injuring a dead worm makes no sense
		return;

	CWorm* ownerWorm = NULL;
	if(owner >= 0 && owner < MAX_WORMS)
		ownerWorm = game.wormById(owner, false);

	if(damage > 0)
		w->setLastWeaponHitBy(weapon);

	bool me = ownerWorm && ownerWorm->getID() == w->getID();
	if(ownerWorm)
		damage *= ownerWorm->damageFactor();

	if(w->shieldFactor() == 0) {
		if(damage > 0)
			damage = w->getHealth() + 1;
		else if(damage < 0)
			damage = MIN(w->getHealth() - 100, 0.0f);
	}
	else
		damage /= w->shieldFactor();
	
	ushort i;
	
	if(damage > 0 && ownerWorm && this->isTeamGame() && !this->getGameLobby()[FT_TeamInjure] && !me && w->getTeam() == ownerWorm->getTeam())
		return;
	
	if(damage > 0 && ownerWorm && !this->getGameLobby()[FT_SelfInjure] && me)
		return;
	
	if (w->getLocal())  // Health change
		bShouldRepaintInfo = true;

	bool someOwnWorm = w->getLocal();

	CServerConnection *client = NULL;
	if(game.isServer())
		client = cServer->getClient(w->getID());
	Version clientver;
	if (client)
		clientver = client->getClientVersion();

	float realdamage = MIN(w->getHealth(), damage);

	// Send REPORTDAMAGE to server (also calculate & send it for pre-Beta9 clients, when we're hosting)
	if( realdamage > 0 && getServerVersion() >= OLXBetaVersion(0,58,1) && 
		( someOwnWorm || 
		( game.isServer() && clientver < OLXBetaVersion(0,58,1) ) ) )
		getNetEngine()->QueueReportDamage( w->getID(), realdamage, owner );

	// Set damage report for local worm for Beta9 server - server won't send it back to us
	// Set it also for remote worms on pre-Beta9 server
	if( getServerVersion() < OLXBetaVersion(0,58,1) || 
		( getServerVersion() >= OLXBetaVersion(0,58,1) && someOwnWorm ) ||
		( NewNet::Active() && NewNet::CanUpdateGameState() ) )
	{
		// TODO: fix this
		if(ownerWorm) {
			w->getDamageReport()[owner].damage += realdamage;
			w->getDamageReport()[owner].lastTime = GetPhysicsTime();
		}
	}

	// Update our scoreboard for local worms
	if( game.isServer() || getServerVersion() >= OLXBetaVersion(0,58,1) || NewNet::Active() ) { // Do not update scoreboard for pre-Beta9 servers
		// TODO: fix this
		if(ownerWorm)
			ownerWorm->addDamage( realdamage, w, false ); // Update client scoreboard
	}
	else if( game.isServer() && NewNet::Active() && NewNet::CanUpdateGameState() ) {
		// TODO: fix this
		if(ownerWorm)
			ownerWorm->addDamage( realdamage, w, false ); // Update server scoreboard
	}
	
	// Do not injure remote worms when playing on Beta9 - server will report us their correct health with REPORTDAMAGE packets
	if( getServerVersion() < OLXBetaVersion(0,58,1) || 
		( getServerVersion() >= OLXBetaVersion(0,58,1) && someOwnWorm ) ||
		( game.isServer() && clientver < OLXBetaVersion(0,58,1) ) || // We're hosting, calculate health for pre-Beta9 clients
		NewNet::Active() ) 
	{
		if(w->injure(damage)) {
			// His dead Jim

			// Kill someOwnWorm
			if(someOwnWorm || NewNet::Active() ||
				(game.isServer() && tLXOptions->bServerSideHealth) ) {

				if(game.isServer()) {
					cServer->killWorm(w->getID(), owner);

					// TODO: merge this part of code with cClient::ParseWormDown()?
					// Make a death sound
					int s = GetRandomInt(2);
					if( NewNet::CanPlaySound(w->getID()) )
						StartSound( sfxGame.smpDeath[s], w->getPos(), w->getLocal(), -1 );

					// Spawn some giblets
					for(int n=0;n<7;n++)
						SpawnEntity(ENT_GIB,0,w->getPos(),GetRandomVec()*80,Color(),w->getGibimg());

					// Blood
					int amount = (int) (50.0f * ((float)tLXOptions->iBloodAmount / 100.0f));
					for(int i=0;i<amount;i++) {
						float sp = GetRandomNum()*100+50;
						SpawnEntity(ENT_BLOODDROPPER,0,w->getPos(),GetRandomVec()*sp,Color(128,0,0),NULL);
						SpawnEntity(ENT_BLOOD,0,w->getPos(),GetRandomVec()*sp,Color(200,0,0),NULL);
						SpawnEntity(ENT_BLOOD,0,w->getPos(),GetRandomVec()*sp,Color(128,0,0),NULL);
					}
				}
				else { // game.isClient()
					w->Kill(false);
					if( !NewNet::Active() )
						w->clearInput();

					cNetEngine->SendDeath(w->getID(), owner); // Let the server know that i am dead
				}
			}
		}
	}

	// Spawn some blood
	if( damage > 0 )	// Don't spawn blood if we hit with healing weapon (breaks old behavior)
	{
		float amount = CLAMP(damage, 10.f, 100.f) * ((float)tLXOptions->iBloodAmount / 100.0f);
		for(i=0;i<amount;i++) {
			float sp = GetRandomNum() * (50 + damage * 2);
			SpawnEntity(ENT_BLOODDROPPER,0,w->getPos(),GetRandomVec().multPairwise(sp,sp*4),Color(128,0,0),NULL);
			SpawnEntity(ENT_BLOOD,0,w->getPos(),GetRandomVec()*sp,Color(128,0,0),NULL);
			SpawnEntity(ENT_BLOOD,0,w->getPos(),GetRandomVec()*sp,Color(200,0,0),NULL);
		}
	}
}

///////////////////
// Send a carve to the server
void CClient::SendCarve(CVec pos)
{
	int x,y,n,px;

	// Go through until we find dirt to throw around
	y = (int) MIN((int)pos.y, game.gameMap()->GetHeight()-1);
	y = MAX(y,0);
	px = (int)pos.x;

	for(x=px-2; x<=px+2; x++) {
		// Clipping
		if(x<0)	continue;
		if((uint)x>=game.gameMap()->GetWidth())	break;

		if(game.gameMap()->GetPixelFlag(x,y) & PX_DIRT) {
			Color c = game.gameMap()->getColorAt(x,y);
			for(n=0;n<3;n++)
				SpawnEntity(ENT_PARTICLE,0,pos,GetRandomVec().multPairwise(30,10),c,NULL);
			break;
		}
	}


	// Just carve a hole for the moment
	game.gameMap()->CarveHole(4,pos, (bool)getGameLobby()[FT_InfiniteMap]);
}


///////////////////
// Player shoot
// This is the function which is called very first. It does not much as most shoots aren't handled yet
// but later by the server. The server itself checks for w->tState.bShoot, so we don't care about that here.
void CClient::PlayerShoot(CWorm *w)
{
	if(w->tWeapons.size() == 0) return;
	if(game.gameScript()->gusEngineUsed()) return; // right now, this only handles LX weapons
	wpnslot_t *Slot = w->writeCurWeapon();

	if(Slot->Reloading)
		return;

	if(Slot->LastFire>0)
		return;

	if(!Slot->weapon())
		return;

	// Safe the ROF time (Rate of Fire). That's the pause time after each shot.
	// In simulateWormWeapon, we decrease this by deltatime and
	// the next shot is allowed if lastfire<=0.
	Slot->LastFire = Slot->weapon()->ROF;

	// Special weapons get processed differently
	if(Slot->weapon()->Type == WPN_SPECIAL) {
		ShootSpecial(w);
		return;
	}

	// Beam weapons get processed differently
	if(Slot->weapon()->Type == WPN_BEAM) {
		DrawBeam(w);
		return;
	}


	// Shots are now handled by the server. Server directly checks the bShoot state
	// and server handles it in GameServer::WormShoot.
	// Server sends us later a shootlist. We are handling each shot in this shootlist in
	// CClient::ProcessShot().
	return;

}


///////////////////
// Shoot a special weapon
void CClient::ShootSpecial(CWorm *w)
{
	if(w->tWeapons.size() == 0) return;
	wpnslot_t *Slot = w->writeCurWeapon();
	TimeDiff dt = tLX->fDeltaTime;

	// Safety
	if(!Slot->weapon())
	{
		errors << "ShootSpecial: Slot->Weapon was not set! Guilty worm: " << itoa(w->getID()) << " with name " << w->getName() << endl;
		return;
	}

	switch(Slot->weapon()->Special) {
		case SPC_NONE: break;

		// Jetpack
		case SPC_JETPACK: {
			w->velocity().write().y += -50.0f * ((float)Slot->weapon()->tSpecial.Thrust * (float)dt.seconds());

			Color blue = Color(80,150,200);
			CVec s = CVec(15,0) * GetRandomNum();
			SpawnEntity(ENT_JETPACKSPRAY, 0, w->getPos(), s + CVec(0,1) * (float)Slot->weapon()->tSpecial.Thrust, blue, NULL);
			break;
		}
			
		case __SPC_LBOUND: case __SPC_UBOUND: errors << "ShootSpecial: hit __SPC_BOUND" << endl;
	}


	// Drain the Weapon charge
	Slot->Charge -= Slot->weapon()->Drain / 100;
	if(Slot->Charge < 0.f) {
		Slot->Charge = 0.f;
		Slot->Reloading = true;
	}
}


///////////////////
// Shoot a beam
void CClient::DrawBeam(CWorm *w)
{
	//return;

	const wpnslot_t *Slot = w->getCurWeapon();

	// Safety
	if(!Slot->weapon())
	{
		errors << "DrawBeam: Slot->Weapon was not set! Guilty worm: " << itoa(w->getID()) << " with name " << w->getName() << endl;
		return;
	}

	if(Slot->Reloading)
		return;

	// Get the direction angle
	float Angle = w->getAngle();
	if(w->getFaceDirectionSide() == DIR_LEFT)
		Angle=180-Angle;


	// Trace a line from the worm to length or until it hits something
	CVec pos = w->getPos();
	CVec dir;
	GetVecsFromAngle(Angle,&dir,NULL);

	CVec orth_dir = dir.orthogonal();
	
	int width = 0;
	std::vector<bool> goodWidthParts;
	bool drawBeam = false;
	// Don't draw the beam if it is 255,0,255
	Color col = Slot->weapon()->Bm.Colour;
	// HINT: We have to check the visibility for everybody as we don't have entities for specific teams/worms.
	// If you want to make that better, you would have to give the CViewport to DrawBeam.
	if(col != tLX->clPink && w->isVisibleForEverybody())
		drawBeam = true;

	std::list<CWorm*> worms;
	for_each_iterator(CWorm*, w2, game.aliveWorms()) {
		// Don't check against someOwnWorm
		if(w2->get()->getID() == w->getID())
			continue;
		
		worms.push_back(w2->get());
	}
	
	for(int i=0; i<Slot->weapon()->Bm.Length; ++i) {
		{
			int newWidth = int(float(Slot->weapon()->Bm.InitWidth) + Slot->weapon()->Bm.WidthIncrease * i);
			newWidth = (int) MIN( newWidth, MIN( game.gameMap()->GetWidth(), game.gameMap()->GetHeight() ) );
			if(newWidth != width) {
				goodWidthParts.resize(newWidth);
				for(int j = width; j < newWidth; ++j) {
					int old = MAX(j - 2, 0);
					goodWidthParts[j] = (width > 0) ? goodWidthParts[old] : true;
				}
				width = newWidth;
			}
			if(width <= 0) break;
		}
		
		{
			bool stopbeam = true;
			for(uint j = 0; j < goodWidthParts.size(); ++j) {
				if(!goodWidthParts[j]) continue;

				int o = (j % 2 == 0) ? int(j/2) : - int((j + 1)/2);
				CVec p = pos + orth_dir * o;
				uchar px = (p.x <= 0 || p.y <= 0) ? PX_ROCK : game.gameMap()->GetPixelFlag( (int)p.x, (int)p.y );

				if((px & PX_DIRT) || (px & PX_ROCK)) {
					// Don't draw explosion when damage is -1
					if (Slot->weapon()->Bm.Damage != -1) {
						if(width <= 2) SpawnEntity(ENT_EXPLOSION, 5, p, CVec(0,0), Color(), NULL);
						int damage = Slot->weapon()->Bm.Damage;
						if(Slot->weapon()->Bm.DistributeDamageOverWidth) { damage /= width; if(damage == 0) damage = SIGN(Slot->weapon()->Bm.Damage); }
						int d = game.gameMap()->CarveHole(damage, p, getGameLobby()[FT_InfiniteMap]);
						w->incrementDirtCount(d);
					}
					
					goodWidthParts[j] = false;
					
					continue;
				}

				for(std::list<CWorm*>::iterator w2 = worms.begin(); w2 != worms.end(); ++w2) {					
					static const float wormsize = 5;
					if((p - (*w2)->getPos()).GetLength2() < wormsize*wormsize) {
						if (Slot->weapon()->Bm.Damage != -1)
							SpawnEntity(ENT_EXPLOSION, 3, p+CVec(1,1), CVec(0,0), Color(), NULL);

						goodWidthParts[j] = false;
						worms.erase(w2);
						break;
					}
				}
								
				stopbeam = false;								
			}
			
			if(stopbeam) break;
		}
		
		pos += dir;
	}

	// Spawn a beam entity
	if(drawBeam) {
		if(Slot->weapon()->Bm.InitWidth != 1 || Slot->weapon()->Bm.WidthIncrease != 0) {
			Line startLine;
			startLine.start = w->getPos() - orth_dir * Slot->weapon()->Bm.InitWidth / 2;
			startLine.end = w->getPos() + orth_dir * Slot->weapon()->Bm.InitWidth / 2;
			Line endLine;
			endLine.start = pos + orth_dir * width / 2;
			endLine.end = pos - orth_dir * width / 2;			
			SetWormBeamEntity(w->getID(), col, startLine, endLine);
		} else
			SetWormBeamEntity(w->getID(), col, w->getPos(), pos);
	}
}


///////////////////
// Spawn a projectile
void CClient::SpawnProjectile(CVec pos, CVec vel, int rot, int owner, proj_t *_proj, int _random, AbsTime remotetime, AbsTime ignoreWormCollBeforeTime)
{
	CProjectile* proj = cProjectiles.getNewObj();

	if(proj == NULL) {
		// Warning: Out of space for a projectile
		//warnings << "Warning: Out of space for SpawnProjectile" << endl;
		return;
	}

    // Safety
    _random %= 255;

	proj->Spawn(_proj,pos,vel,rot,owner,_random,remotetime, ignoreWormCollBeforeTime);
}



///////////////////
// Destroy a bonus (not explosively)
void CClient::DestroyBonus(int id, bool local, int wormid)
{
	cBonuses[id].setUsed(false);


	// Tell the server the bonus is gone if we grabbed it
	if(local) {
		cNetEngine->SendGrabBonus(id, wormid);
	}
}



///////////////////
// Show a laser sight
void CClient::LaserSight(CWorm *w, float Angle, bool highlightCrosshair)
{

	// Get the direction angle
	if(w->getFaceDirectionSide() == DIR_LEFT)
		Angle=180-Angle;

	while( Angle > 180.0f ) // Normalize it
		Angle -= 360.0f;
	while( Angle < -180.0f )
		Angle += 360.0f;

	// Trace a line from the worm to length or until it hits something
	CVec pos = w->getPos();
	CVec dir;
	GetVecsFromAngle(Angle,&dir,NULL);

	static const int divisions = 3;			// How many pixels we go through each check (less = slower)
	bool stopbeam = false;

	short i = 0;
	for(; i<9999; i+=divisions) {
		const Material& px = game.gameMap()->getMaterialWrapped( (int)pos.x, (int)pos.y );

		if(!px.particle_pass)
			break;

		// Check if it has hit any of the worms
		for_each_iterator(CWorm*, w2_, game.aliveWorms()) {
			CWorm* w2 = w2_->get();

			// Don't check against me
			if(w2->getID() == w->getID())
				continue;

			static const float wormsize = 5;
			if((pos - w2->getPos()).GetLength2() < wormsize*wormsize) {
				stopbeam = true;

				// We have a target
				if( highlightCrosshair )
					w->setTarget(true);
				break;
			}
		}

		if(stopbeam)
			break;

		pos = pos + dir*(float)divisions;
	}

	// Spawn a laser sight entity (only if the worm is visible though, else we'd tell the worm position)
	// HINT: We have to check the visibility for everybody as we don't have entities for specific teams/worms.
	// If you want to make that better, you would have to give the CViewport to LaserSight.
	if (w->isVisibleForEverybody())
		SpawnEntity(ENT_LASERSIGHT, i, w->getPos(), dir, Color(), NULL);
}


///////////////////
// Process the shots
void CClient::ProcessServerShotList()
{
	int num = cShootList.getNumShots();

	for(int i=0; i<num; i++) {
		shoot_t *sh = cShootList.getShot(i);
		
		// handle all shots not given by me
		if(sh)
			ProcessShot(sh, sh->spawnTime());
	}
}

// IMPORTANT HINT: this function is currently not used
// It was intended for client side shooting but we
// decided for good reasons to not use it.
// The main reason is to keep the code clean and
// to have better forward-compatibility support.
// See discussion in OLX mailinglist for further details.
void CClient::DoLocalShot( float fTime, float fSpeed, int nAngle, CWorm *pcWorm ) {
	assert(false); // to ensure you not use this if you are not really sure

	shoot_t shot;

	shot.cPos = pcWorm->getPos();
	shot.cWormVel = pcWorm->getVelocity();
	shot.fTime = fTime;
	shot.nAngle = nAngle;
	shot.nRandom = GetRandomInt(255);
	shot.nSpeed = (int)( fSpeed*100 );
	shot.nWeapon = pcWorm->getCurWeapon()->weapon()->ID;
	shot.nWormID = pcWorm->getID();

	ProcessShot( &shot, tLX->currentTime );
}

void CClient::NewNet_DoLocalShot( CWorm *w ) 
{
	if(w->tWeapons.size() == 0) return;
	wpnslot_t *Slot = w->writeCurWeapon();

	if(Slot->Reloading)
		return;

	if(Slot->LastFire>0)
		return;

	if(!Slot->weapon())
		return;

	Slot->LastFire = Slot->weapon()->ROF;

	// Special weapons get processed differently
	if(Slot->weapon()->Type == WPN_SPECIAL) {
		ShootSpecial(w);
		return;
	}

	// Beam weapons get processed differently
	if(Slot->weapon()->Type == WPN_BEAM) {
		DrawBeam(w);
		return;
	}

	// Must be a projectile
	if(Slot->weapon()->Type != WPN_PROJECTILE)
		return;

	shoot_t shot;

	shot.cPos = w->getPos();
	shot.cWormVel = w->getVelocity();
	shot.fTime = GetPhysicsTime().seconds();
	shot.nRandom = w->NewNet_random.getInt(255);
	shot.nWeapon = w->getCurWeapon()->weapon()->ID;
	shot.nWormID = w->getID();


	// Get the direction angle
	float Angle = w->getAngle();
	if(w->getFaceDirectionSide() == DIR_LEFT)
		Angle=180-Angle;

	if(Angle < 0)
		Angle+=360;
	if(Angle > 360)
		Angle-=360;
	if(Angle == 360)
		Angle=0;

	float speed = 0.0f;

	// only projectile wpns have speed; Beam weapons have no speed
	if(Slot->weapon()->Type == WPN_PROJECTILE) {
		// Add the shot to the shooting list
		CVec vel = w->getVelocity();
		speed = NormalizeVector( &vel );
	}
	
	shot.nAngle = (int)Angle;
	shot.nSpeed = (int)( speed*100 );
	

	//
	// Note: Drain does NOT have to use a delta time, because shoot timing is controlled by the ROF
	// (ROF = Rate of Fire)
	//

	// Drain the Weapon charge
	Slot->Charge -= Slot->weapon()->Drain / 100;
	if(Slot->Charge <= 0.f) {
		Slot->Charge = 0.f;
		Slot->Reloading = true;
	}

	ProcessShot( &shot, GetPhysicsTime() );
}

///////////////////
// Process a shot
// this is called by CClient::ProcessServerShotList
void CClient::ProcessShot(shoot_t *shot, AbsTime fSpawnTime)
{
	if(shot->nWormID >= 0 && shot->nWormID < MAX_WORMS) {
		CWorm *w = game.wormById(shot->nWormID, false);

		if(!w)  {
			notes << "ProcessShot: unused worm was shooting" << endl;
			return;
		}
		
		// If this worm is dead, ignore the shot (original LX56 behaviour)
		if(!w->getAlive()) {
			//warnings << "WARNING: dead worm was shooting" << endl; // Occurs pretty often, don't spam console
			return;
		}
	}

	// Weird
	if (!game.gameScript()->GetWeapons()) {
		warnings << "ProcessShot: weapons not loaded while a client was shooting" << endl;
		return;
	}

	// Safety check
	if (shot->nWeapon < 0 || shot->nWeapon >= game.gameScript()->GetNumWeapons())  {
		warnings << "ProcessShot: weapon ID invalid. Guilty worm: " << shot->nWormID << endl;
		return;
	}
	
	const weapon_t *wpn = game.gameScript()->GetWeapons() + shot->nWeapon;

	if(shot->release) { // the shot key was released
		wpn->FinalProj.apply(shot, fSpawnTime);
		return;
	}
	
	// Process beam weapons differently
	if(wpn->Type == WPN_BEAM) {
		ProcessShot_Beam(shot);
		return;
	}

	if(wpn->Proj.Proj == NULL) {
		errors << "ProcessShot: weapon " << wpn->Name << " has no projectile set" << endl;
		return;
	}
	

	// Play the weapon's sound
	if(wpn->UseSound) {
		if(shot->nWormID >= 0 && shot->nWormID < MAX_WORMS) {
			if(NewNet::CanPlaySound(shot->nWormID))
				StartSound(wpn->smpSample, shot->cPos, game.wormById(shot->nWormID)->getLocal(), 100);
		} else
			StartSound(wpn->smpSample, shot->cPos, false, 100);
	}
	
	if(shot->nWormID >= 0 && shot->nWormID < MAX_WORMS) {
		CWorm *w = game.wormById(shot->nWormID);

		// Add the recoil
		const CVec dir = GetVecFromAngle(float(shot->nAngle));
		w->velocity() -= dir*(float)wpn->Recoil;

		// Draw the muzzle flash
		w->setDrawMuzzle(true);
	}

	
	wpn->Proj.apply(shot, fSpawnTime);
}


///////////////////
// Process a shot - Beam
void CClient::ProcessShot_Beam(shoot_t *shot)
{
	if(shot->nWormID >= 0 && shot->nWormID < MAX_WORMS) {
		CWorm *w = game.wormById(shot->nWormID, false);
		if(!w) return;
	}
	const weapon_t *wpn = game.gameScript()->GetWeapons() + shot->nWeapon;

	// Trace a line from the worm to length or until it hits something
	CVec dir;
	GetVecsFromAngle((float)shot->nAngle,&dir,NULL);
	CVec pos = shot->cPos;

	CVec orth_dir = dir.orthogonal();
	std::vector<bool> goodWidthParts;
	int width = 0;

	std::list<CWorm*> worms;
	for_each_iterator(CWorm*, w2, game.aliveWorms()) {
		// Don't check against someOwnWorm
		if(w2->get()->getID() == shot->nWormID)
			continue;
		
		worms.push_back(w2->get());
	}
	
	for(int i=0; i<wpn->Bm.Length; ++i) {
		{
			int newWidth = int(float(wpn->Bm.InitWidth) + wpn->Bm.WidthIncrease * i);
			newWidth = (int) MIN( newWidth, MIN( game.gameMap()->GetWidth(), game.gameMap()->GetHeight() ) );
			if(newWidth != width) {
				goodWidthParts.resize(newWidth);
				for(int j = width; j < newWidth; ++j) {
					int old = MAX(j - 2, 0);
					goodWidthParts[j] = (width > 0) ? goodWidthParts[old] : true;
				}
				width = newWidth;
			}
			if(width <= 0) break;
		}
		
		{
			bool stopbeam = true;
			for(uint j = 0; j < goodWidthParts.size(); ++j) {
				if(!goodWidthParts[j]) continue;
				
				int o = (j % 2 == 0) ? int(j/2) : - int((j + 1)/2);
				CVec p = pos + orth_dir * o;
				uchar px = (p.x <= 0 || p.y <= 0) ? PX_ROCK : game.gameMap()->GetPixelFlag( (int)p.x, (int)p.y );
			
				// Check bonus colision and destroy the bonus, if damage isn't -1
				if (wpn->Bm.Damage != -1)  {
					CBonus *b = cBonuses;
					for(int n=0;n<MAX_BONUSES;n++,b++) {
						if(!b->getUsed())
							continue;

						const float bonussize = 3;
						if((p - b->getPosition()).GetLength() < bonussize) {
							Explosion(shot->spawnTime(), p,0,5,shot->nWormID); // Destroy the bonus by an explosion
							goodWidthParts[j] = false;
							break;
						}
					}
					
					if(!goodWidthParts[j]) continue;
				}

				if((px & (PX_DIRT|PX_ROCK))) {
					// No explosion if damage is -1
					if(wpn->Bm.Damage != -1) {
						//SpawnEntity(ENT_EXPLOSION, 5, pos, CVec(0,0), 0, NULL);
						int damage = wpn->Bm.Damage;
						if(wpn->Bm.DistributeDamageOverWidth) { damage /= width; if(damage == 0) damage = SIGN(wpn->Bm.Damage); }
						int d = game.gameMap()->CarveHole(damage, p, getGameLobby()[FT_InfiniteMap]);
						if(shot->nWormID >= 0 && shot->nWormID < MAX_WORMS)
							game.wormById(shot->nWormID)->incrementDirtCount(d);						
					}

					// Stop the beam regardless of explosion being shown
					goodWidthParts[j] = false;
					continue;
				}

				// Check if it has hit any of the worms
				for(std::list<CWorm*>::iterator w2 = worms.begin(); w2 != worms.end(); ++w2) {						
					static const float wormsize = 5;
					if((p - (*w2)->getPos()).GetLength2() < wormsize*wormsize) {
						int damage = wpn->Bm.PlyDamage;
						if(wpn->Bm.DistributeDamageOverWidth) { damage /= width; if(damage == 0) damage = SIGN(wpn->Bm.PlyDamage); }
						InjureWorm(*w2, (float)damage, shot->nWormID, wpn->Name);

						goodWidthParts[j] = false;
						worms.erase(w2);
						break;
					}
				}
				
				if(!goodWidthParts[j]) continue;
				
				stopbeam = false;
			}
			
			if(stopbeam) break;
		}
		
		pos += dir;
	}

	// Spawn a beam entity and don't draw 255,0,255 (pink) beams
	/*if(wpn->Bm.Colour[0] != 255 || wpn->Bm.Colour[1] != 0 || wpn->Bm.Colour[2] != 255) {
		Color col = Color(wpn->Bm.Colour[0], wpn->Bm.Colour[1], wpn->Bm.Colour[2]);
		SpawnEntity(ENT_BEAM, i, w->getPos(), dir, col, NULL);
	}*/
}


void CClient::clearLocalWormInputs() {
	for_each_iterator(CWorm*, w, game.localWorms())
		w->get()->clearInput();
}

void CClient::clearHumanWormInputs() {
	for_each_iterator(CWorm*, w, game.localWorms())
		if (w->get()->getType() == PRF_HUMAN)
			w->get()->clearInput();
}


///////////////////
// Process any chatter
void CClient::processChatter()
{
	if (game.isLocalGame())
		return;

    keyboard_t *kb = GetKeyboard();

	// Process taunt keys
	for(short i=0; i<kb->queueLength; i++) {
    	const KeyboardEvent& input = kb->keyQueue[i];

		if( input.down && input.state.bCtrl && taunts->getTauntForKey( input.sym ) != "" ) {
			CWorm* firstLocalWorm = game.localWorms()->tryGet();
			if(!firstLocalWorm) {
				warnings << "processChatter: cannot send text: no local worms" << endl;
				return;
			}
			sChat_Text += taunts->getTauntForKey( input.sym );
			if( bChat_Typing && bTeamChat )
				cNetEngine->SendText("/teamchat \"" + sChat_Text + "\"", firstLocalWorm->getName() );
			else
				cNetEngine->SendText(sChat_Text, firstLocalWorm->getName() );
			sChat_Text = "";
			bChat_Typing = false;
			clearHumanWormInputs();
			return; // TODO: we may lose some chat keys if user typing very fast ;)
		}
	}

	// If we're currently typing a message, add any keys to it
	if(bChat_Typing) {

		fChat_BlinkTime += tLX->fDeltaTime;
		if (fChat_BlinkTime.seconds() > 0.5f)  {
			bChat_CursorVisible = !bChat_CursorVisible;
			fChat_BlinkTime = 0;
		}

        // Go through the keyboard queue
        for(short i=0; i<kb->queueLength; i++) {

			if(kb->keyQueue[i].down && kb->keyQueue[i].sym == SDLK_ESCAPE) {
				// Stop typing
				bChat_Typing = false;
				sChat_Text = "";
				clearHumanWormInputs();
				break;
			}

			bChat_CursorVisible = true;
            processChatCharacter(kb->keyQueue[i]);
        }

        return;
    }


	// Check if we have hit the chat key and we're in a network game
	if( cChat_Input.wasDown() || cTeamChat_Input.wasDown() ) {

		// Initialize the chatter
		fChat_BlinkTime = 0;
		bChat_CursorVisible = true;
		bChat_Typing = true;
		iChat_Pos = 0;
		sChat_Text = "";
		iChat_Lastchar = 0;
		bChat_Holding = false;
		fChat_TimePushed = AbsTime();
		bTeamChat = false;
		if( cTeamChat_Input.wasDown() )
			bTeamChat = true;

		// Clear the input
		clearHumanWormInputs();

		return;
	}

    // Start typing on any keypress, if we can
	if (!tLXOptions->bAutoTyping)
		return;

	bTeamChat = false;

    for(short i=0; i<kb->queueLength; i++) {
    	const KeyboardEvent& input = kb->keyQueue[i];

        // Was it an up keypress?
        if(!input.down) continue;

		if (!bChat_Typing)  {

			if (input.ch<32) continue;

			// Check, if we can start typing
			bool controls =
				cChat_Input.isDown() ||
				cShowScore.isDown() ||
				cShowHealth.isDown() ||
				cShowSettings.isDown() ||
				cToggleTopBar.isDown() ||
				(input.sym == SDLK_BACKQUOTE) ||
				tLX->isAnyControlKeyDown();

			if(controls) continue;

			for_each_iterator(CWorm*, w, game.localWorms()) {
				if (w->get()->getType() == PRF_HUMAN)  {

					// Can we type?
					if (w->get()->getLives() == WRM_OUT && !w->get()->getAlive())  { // We're spectating
						if (cSpectatorViewportKeys.Down.isDown() ||
							cSpectatorViewportKeys.Left.isDown() ||
							cSpectatorViewportKeys.Right.isDown() ||
							cSpectatorViewportKeys.Up.isDown() ||
							cSpectatorViewportKeys.V1Type.isDown() ||
							cSpectatorViewportKeys.V2Toggle.isDown() ||
							cSpectatorViewportKeys.V2Type.isDown())
							return;
					} else if (!w->get()->CanType()) // Playing
						return;
				}
			}

			// Clear the input
			clearHumanWormInputs();

			// Initialize the chatter
			fChat_BlinkTime = 0;
			bChat_CursorVisible = true;
			bChat_Typing = true;
			iChat_Pos = 0;
			sChat_Text = "";
			iChat_Lastchar = 0;
			bChat_Holding = false;
			fChat_TimePushed = AbsTime();
		}

        processChatCharacter(input);
    }
}




std::string CClient::getChatterCommand() {
	if(strSeemsLikeChatCommand(sChat_Text))
		return sChat_Text.substr(1);
	else
		return "";
}



///////////////////
// Process a single character chat
void CClient::processChatCharacter(const KeyboardEvent& input)
{

    // Up?
    if(!input.down) {
        bChat_Holding = false;
        return;
    }

    // Must be down

    if(bChat_Holding) {
        if(iChat_Lastchar != input.ch)
            bChat_Holding = false;
        else {
            if((tLX->currentTime - fChat_TimePushed).seconds() < 0.4f)
                return;
        }
    }

    if(!bChat_Holding) {
        bChat_Holding = true;
        fChat_TimePushed = tLX->currentTime;
    }

    iChat_Lastchar = input.ch;

    // Backspace
    if(input.sym == SDLK_BACKSPACE) {
		if(iChat_Pos > 0)  {
			Utf8Erase(sChat_Text, --iChat_Pos, 1);
		}
        return;
    }

	// Delete
	if (input.sym == SDLK_DELETE)  {
		Utf8Erase(sChat_Text, iChat_Pos, 1);
		return;
	}

	// Home
	if (input.sym == SDLK_HOME)  {
		iChat_Pos = 0;
		return;
	}

	// End
	if (input.sym == SDLK_END)  {
		iChat_Pos = Utf8StringSize(sChat_Text);
		return;
	}

	// Left arrow
	if (input.sym == SDLK_LEFT) {
		if (iChat_Pos > 0)
			iChat_Pos--;
		else
			iChat_Pos = 0;
		return;
	}

	// Right arrow
	if (input.sym == SDLK_RIGHT) {
		if (iChat_Pos < Utf8StringSize(sChat_Text))
			iChat_Pos++;
		else
			iChat_Pos = Utf8StringSize(sChat_Text);
		return;
	}

    // Enter
    if(input.ch == '\r') {
        bChat_Typing = false;
		clearHumanWormInputs();

		CWorm* firstLocalWorm = game.localWorms()->tryGet();
		if(!firstLocalWorm) return;
		
        // Send chat message to the server
		if(sChat_Text != "") {
			if( bTeamChat )	// No "/me" macro in teamchat - server won't recognize such command
				cNetEngine->SendText("/teamchat \"" + sChat_Text + "\"", firstLocalWorm->getName() );
			else
				cNetEngine->SendText(sChat_Text, firstLocalWorm->getName() );
		}
		sChat_Text = "";
        return;
    }

	// Paste
	if (input.ch == 22 ||
		( input.sym == SDLK_v && ( input.state.bCtrl || input.state.bGui ) ) ||
		( input.sym == SDLK_INSERT && input.state.bShift )) {
		
		size_t text_len = Utf8StringSize(sChat_Text);

		// Safety
		if (iChat_Pos > text_len)
			iChat_Pos = text_len;

		// Get the text
		std::string buf = copy_from_clipboard();
		replace(buf, "\r", " ");
		replace(buf, "\n", " ");
		replace(buf, "\t", " ");

		// Paste
		Utf8Insert(sChat_Text, iChat_Pos, buf);
		iChat_Pos += Utf8StringSize(buf);
		return;
	}

	// Tab key
	if(input.ch == SDLK_TAB) {
		if(strSeemsLikeChatCommand(sChat_Text)) {
			cNetEngine->SendChatCommandCompletionRequest(getChatterCommand());
			return;
		}
		// handle the key like a normal key
	}

    // Normal key
    if(tLX->cFont.CanDisplayCharacter(input.ch) ) {
    	InsertUnicodeChar(sChat_Text, iChat_Pos, input.ch);
		iChat_Pos++;
    }
}

