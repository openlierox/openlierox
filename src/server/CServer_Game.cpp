/////////////////////////////////////////
//
//			 OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Server class - Game routines
// Created 11/7/02
// Jason Boettcher

#include "LieroX.h"
#include "CServer.h"
#include "Debug.h"
#include "CServerConnection.h"
#include "CServerNetEngine.h"
#include "StringUtils.h"
#include "Protocol.h"
#include "game/CWorm.h"
#include "MathLib.h"
#include "DedicatedControl.h"
#include "Physics.h"
#include "DeprecatedGUI/Menu.h"
#include "CGameMode.h"
#include "FlagInfo.h"
#include "WeaponDesc.h"
#include "game/Game.h"
#include "CGameScript.h"



///////////////////
// Spawn a worm
void GameServer::SpawnWorm(CWorm *Worm, const std::string& reason, CVec * _pos, CServerConnection * client)
{
	if (game.gameOver || Worm->isSpectating())
		return;

	CVec pos;

	//notes << "spawn worm " << Worm->getID() << ":" << Worm->getName() << ": " << reason << endl;

	if( _pos )
		pos = *_pos;
	else
	{
		if( game.gameMap()->getPredefinedSpawnLocation(Worm, &pos) ) {
			// ok
		}
		else if( gameSettings[FT_GameMode].as<GameModeInfo>()->actualIndex() == GM_CTF ) {
			Flag* flag = m_flagInfo->getFlag(Worm->getTeam());
			if(!flag) // no flag yet, choose like in respawngroupteams
				pos = game.gameMap()->FindSpotCloseToTeam(Worm->getTeam(), Worm, true);
			else
				pos = game.gameMap()->FindSpotCloseToPos(flag->spawnPoint.pos);
		}
		// Spawn worm closer to it's own team and away from other teams
		else if( gameSettings[FT_RespawnGroupTeams] && ( game.gameMode()->GameTeams() > 1 ) )
		{
			pos = game.gameMap()->FindSpotCloseToTeam(Worm->getTeam(), Worm);
		}
		else
			pos = game.gameMap()->FindSpot();			
	}

	if(pos.x == -1 && pos.y == -1)
		pos = game.gameMap()->FindSpot();

	// Allow the game mode to override spawns
	if(!game.gameMode()->Spawn(Worm, pos))
		return;

	Worm->pos().ext.S2CupdateNeeded = true;
	Worm->health.ext.S2CupdateNeeded = true;
	Worm->Spawn(pos);
	bool sendWormUpdate = true;
	if(Worm->getLives() == WRM_OUT) {
		if((int)gameSettings[FT_Lives] < 0)
			Worm->setLives(WRM_UNLIM);
		else
			Worm->setLives(0);
		sendWormUpdate = true;
	}
	
	if( client ) { // Spawn all playing worms only for new client for connect-during-game
		client->getNetEngine()->SendSpawnWorm(Worm, Worm->getPos());
		if(sendWormUpdate) client->getNetEngine()->SendWormScore(Worm);		
	}
	else {
		if( DedicatedControl::Get() )
			DedicatedControl::Get()->WormSpawned_Signal(Worm);
		for( int i = 0; i < MAX_CLIENTS; i++ ) {
			cClients[i].getNetEngine()->SendSpawnWorm(Worm, Worm->getPos());
			if(sendWormUpdate) cClients[i].getNetEngine()->SendWormScore(Worm);
		}

		if( gameSettings[FT_EmptyWeaponsOnRespawn] )
			cServer->SendEmptyWeaponsOnRespawn( Worm );
	}
}



void GameServer::killWorm( int victim, int killer, int suicidesCount )
{
	// If the game is already over, ignore this
	if (game.gameOver)  {
		warnings << "GameServer::killWorm: Game is over, ignoring." << endl;
		return;
	}
	// Safety check
	if (victim < 0 || victim >= MAX_WORMS)  {
		errors << "GameServer::killWorm: victim ID out of bounds." << endl;
		return;
	}

	if(victim != killer )
		suicidesCount = 0;
	if(victim == killer && suicidesCount == 0 )
		suicidesCount = 1;

	CWorm *vict = game.wormById(victim);
	CWorm *kill = game.wormById(killer, false);
	
	// Cheat prevention, game behaves weird if this happens
	if (vict->getLives() < 0 && (int)gameSettings[FT_Lives] >= 0)  {
		vict->setLives(WRM_OUT);  // Safety
		warnings("GameServer::killWorm: victim is already out of the game.\n");
		return;
	}

	// Adjust the score if there were multiple suicides
	if (suicidesCount > 1)  {
		if ((int)gameSettings[FT_Lives] != WRM_UNLIM) // Substracting from infinite makes no sense
			vict->setLives(MAX<int>(0, vict->getLives() - suicidesCount + 1)); // HINT: +1 because one life is substracted in vict->Kill()
	}

	game.gameMode()->Kill(vict, kill);
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(!cClients[i].isConnected()) continue;		
		cClients[i].getNetEngine()->SendWormScore(vict);
		if (kill && killer != victim)
			cClients[i].getNetEngine()->SendWormScore(kill);
	}
	// Let everyone know that the worm is now dead
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(!cClients[i].isConnected()) continue;		
		cClients[i].getNetEngine()->SendWormDied(vict);
	}
	
	if(DedicatedControl::Get())
		DedicatedControl::Get()->WormDied_Signal(vict,kill);
	
	SendTeamScoreUpdate();
	
	RecheckGame();
	return;
};


///////////////////
// Simulate the game stuff
void GameServer::SimulateGame()
{
	if(recheckGame) {
		if(game.state == Game::S_Preparing)
			// Check if all the clients are ready
			CheckReadyClient();

		if(!game.gameOver && game.state == Game::S_Playing)
			if(game.gameMode()->CheckGameOver())
				GameOver();

		for(int t = 0; t < game.gameMode()->GameTeams(); ++t) {
			game.writeTeamScore(t) = game.gameMode()->TeamScores(t);
		}
	}

	if(game.state != Game::S_Playing)
		return;

	// If this is a remote game, and game over,
	// and we've seen the scoreboard for a certain amount of time, go back to the lobby
	if(game.gameOver
	&& (game.gameOverTime() > LX_ENDWAIT || (bDedicated && game.localWorms()->size() <= 1)) // dedicated server should go to lobby immediatly if alone
	&& game.state != Game::S_Lobby
	&& (game.isServer() && !game.isLocalGame())) {
		game.gotoLobby("timeout for gameover scoreboard");
		return;
	}

	// Don't process if the game is paused (local play only)
	if (cClient->getGamePaused())
		return;

	// Process worms
	for_each_iterator(CWorm*, w_, game.worms()) {
		CWorm* w = w_->get();

		if(!w->getAlive() && w->getLives() != WRM_OUT && w->bWeaponsReady) {

			// Check to see if they have been dead for longer than fRespawnTime (originally 2.5 seconds)
			if(!w->haveSpawnedOnce() || (tLX->currentTime > w->getTimeofDeath() + TimeDiff((float)gameSettings[FT_RespawnTime])) )
			{
				// with MaxRespawnTime set, we automatically spawn if that time has been hit
				if((float)gameSettings[FT_MaxRespawnTime] >= 0.f &&
				   (tLX->currentTime > w->getTimeofDeath() + TimeDiff((float)gameSettings[FT_MaxRespawnTime]))) {
					SpawnWorm(w, "max respawn-time reached");
				}
				else { // MaxRespawnTime disabled or not yet reached
					// only spawn if the client cannot. otherwise wait for clientside respawn request
					if(w->bRespawnRequested)
						SpawnWorm(w, "respawn requested");
					else if(!CanWormHandleClientSideRespawn(w))
						SpawnWorm(w, "worm cannot handle respawn-request");
					else
						w->bCanRespawnNow = true;
				}
			}
		}
	}

	// Check if any bonuses have been in for too long and need to be destroyed
	if (gameSettings[FT_Bonuses])  {
		for(short i=0; i<MAX_BONUSES; i++) {
			if(!cBonuses[i].getUsed())
				continue;

			// If it's been here too long, destroy it
			if( tLX->currentTime - cBonuses[i].getSpawnTime() > gameSettings[FT_BonusLife] ) {
				cBonuses[i].setUsed(false);

				// TODO: move this out here
				CBytestream bs;
				bs.writeByte(S2C_DESTROYBONUS);
				bs.writeByte((byte)i);
				SendGlobalPacket(&bs);
			}
		}
	}

	// Check if we need to spawn a bonus
	if(tLX->currentTime - fLastBonusTime > gameSettings[FT_BonusFreq] && gameSettings[FT_Bonuses] && !game.gameOver) {
		SpawnBonus();
		fLastBonusTime = tLX->currentTime;
	}

	// check for flag
	for_each_iterator(CWorm*, w, game.worms())
		flagInfo()->checkWorm(w->get());
	
	// Simulate anything needed by the game mode
	game.gameMode()->Simulate();

	if( game.gameMode()->TimeLimit() > 0 && game.serverTime() > game.gameMode()->TimeLimit() )
		RecheckGame();
}


///////////////////
// Spawn a bonus
void GameServer::SpawnBonus()
{
	// Find an empty spot for the bonus
	CVec pos = game.gameMap()->FindSpot();

	// Carve a hole for it
	//game.gameMap()->CarveHole(SPAWN_HOLESIZE,pos);

	// NOTE: Increase to 2 when we want to use the fullcharge bonus
	int type = (GetRandomInt(999) >= (float)gameSettings[FT_BonusHealthToWeaponChance] * 1000.0f) ? BNS_HEALTH : BNS_WEAPON;

	// Find a free bonus spot
	CBonus *b = cBonuses;
	int spot = -1;
	for(short i=0;i<MAX_BONUSES;i++,b++) {
		if(b->getUsed())
			continue;

		spot = i;
		break;
	}

	// No spots
	if(spot == -1)
		return;

	std::vector<int> goodWpns;
	for(int i = 0; i < game.gameScript()->GetNumWeapons(); ++i)
		if(game.weaponRestrictions()->getWeaponState( (game.gameScript()->GetWeapons()+i)->Name ) != wpr_banned)
			goodWpns.push_back(i);
	
	int wpn = (goodWpns.size() > 0) ? randomChoiceFrom(goodWpns) : -1;
	if(type == BNS_WEAPON && wpn < 0) return; // no weapon found
	
	b->Spawn(pos, type, wpn, game.gameScript());


	// Send the spawn the everyone
	CBytestream bs;

	bs.writeByte(S2C_SPAWNBONUS);
	bs.writeInt(type,1);
	if(type == BNS_WEAPON)
		bs.writeInt(wpn,1);
	bs.writeInt(spot,1);

	bs.writeInt((int)pos.x, 2);
	bs.writeInt((int)pos.y, 2);

	SendGlobalPacket(&bs);
}


void GameServer::WormShootEnd(CWorm* w, const weapon_t* wpn) {
	// Don't shoot when the game is over
	if (game.gameOver)
		return;
	
	if(!game.gameMode()->Shoot(w))
		return;
	
	if(!wpn)
		return;
	
	if(!wpn->FinalProj.isSet())
		return;

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
	
	CVec vel = w->getVelocity();
	speed = NormalizeVector( &vel );
	
	TimeDiff time = game.serverTime();
	if( w->hasOwnServerTime() )
		time = w->serverTime();
	
	// Add the shot to ALL the connected clients shootlist
	CServerConnection *cl = getClients();
	for(short i=0; i<MAX_CLIENTS; i++,cl++) {
		if(cl->getStatus() == NET_DISCONNECTED)
			continue;
		
		cl->getShootList()->addShoot(wpn->ID, time, speed, (int)Angle, w, true);
	}
}

///////////////////
// Worm is shooting
void GameServer::WormShoot(CWorm *w)
{
	if(game.gameScript()->gusEngineUsed()) return; // right now, this only handles LX weapons

	// Don't shoot when the game is over
	if (game.gameOver)
		return;

	if(!game.gameMode()->Shoot(w))
		return;

	if(w->tWeapons.size() == 0) return;
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


	// Must be a projectile or beam.
	// We send also beam via the shootlist. See CClient::ProcessShot which is calling CClient::ProcessShot_Beam in that case.
	if(Slot->weapon()->Type != WPN_PROJECTILE && Slot->weapon()->Type != WPN_BEAM) {
		// It's not a projectile, so the client is handling this, but we take track of charge to disable weapon if needed.
		
		// Drain the Weapon charge
		Slot->Charge -= Slot->weapon()->Drain / 100;
		if(Slot->Charge <= 0.f) {
			Slot->Charge = 0.f;
			
			if(gameSettings[FT_DisableWpnsWhenEmpty]) {
				Slot->WeaponId = -1;
				SendWeapons(NULL, w);
			} else
				Slot->Reloading = true;
		}
		return;
	}
	
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
	
	TimeDiff time = game.serverTime();
	if( game.isClient() && w->hasOwnServerTime() )
		time = w->serverTime();
	
	// Add the shot to ALL the connected clients shootlist
	CServerConnection *cl = getClients();
	for(short i=0; i<MAX_CLIENTS; i++,cl++) {
		if(cl->getStatus() == NET_DISCONNECTED)
			continue;

		cl->getShootList()->addShoot(Slot->weapon()->ID, time, speed, (int)Angle, w, false);
	}

	

	//
	// Note: Drain does NOT have to use a delta time, because shoot timing is controlled by the ROF
	// (ROF = Rate of Fire)
	//

	// Drain the Weapon charge
	Slot->Charge -= Slot->weapon()->Drain / 100;
	if(Slot->Charge <= 0.f) {
		Slot->Charge = 0.f;
		
		if(gameSettings[FT_DisableWpnsWhenEmpty]) {
			Slot->WeaponId = -1;
			SendWeapons(NULL, w);
		} else
			Slot->Reloading = true;
	}
}




///////////////////
// Go back to the lobby
void GameServer::gotoLobby()
{
	if(game.state != Game::S_Lobby) {
		errors << "server gotolobby handling: not in lobby, current state: " << game.state << endl;
		return;
	}
	
	// Tell all the clients
	CBytestream bs;
	bs.writeByte(S2C_GOTOLOBBY);
	SendGlobalPacket(&bs);

	UpdateGameLobby();

	for(short i=0; i<MAX_CLIENTS; i++) {
		cClients[i].setGameReady(false);
	}

	fLastUpdateSent = AbsTime();

	SendWormLobbyUpdate();

	DeprecatedGUI::Menu_Net_GotoHostLobby();
}




///////////////////
// Recheck the game status
// Called iff game state has changed (e.g. player left etc.)
void GameServer::RecheckGame()
{
	recheckGame = true;
}


///////////////////
// Checks if all the clients are ready to play
void GameServer::CheckReadyClient()
{
	if(game.state != Game::S_Preparing)
		return;

	bool allready = true;
	CServerConnection *client = cClients;
	for(short c=0; c<MAX_CLIENTS; c++, client++) {
		if(client->getStatus() == NET_DISCONNECTED || client->getStatus() == NET_ZOMBIE || game.wormsOfClient(client)->size() == 0)
			continue;

		if(!client->getGameReady()) {
			notes << "CheckReadyClient: " << client->debugName() << " is not ready yet" << endl;
			allready = false;
			break;
		}
	}

	// All ready to go?
	if(allready)
		BeginMatch();
}
