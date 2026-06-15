/*
 *  Game.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 09.12.09.
 *  code under LGPL
 *
 */

#include "Game.h"
#include "AuxLib.h"
#include "Debug.h"
#include "LieroX.h"
#include "GfxPrimitives.h"
#include "Entity.h"
#include "EventQueue.h"
#include "InputEvents.h"
#include "DedicatedControl.h"
#include "CrashHandler.h"
#include "Timer.h"
#include "OLXCommand.h"
#include "IRC.h"
#include "CClient.h"
#include "CServer.h"
#include "Physics.h"
#include "DeprecatedGUI/Menu.h"
#include "Cache.h"
#include "gusanos/gusanos.h"
#include "gusanos/gusgame.h"
#include "game/WormInputHandler.h"
#include "CWormHuman.h"
#include "gusanos/luaapi/context.h"
#ifndef DEDICATED_ONLY
#include "sound/sfx.h"
#endif
#include "OLXConsole.h"
#include "game/SinglePlayer.h"
#include "game/SettingsPreset.h"
#include "CGameScript.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include "WeaponDesc.h"
#include "ProfileSystem.h"
#include "Attr.h"
#include "gusanos/luaapi/classes.h"
#include "gusanos/network.h"
#include "FlagInfo.h"
#include "CWpnRest.h"
#include "CChannel.h"
#include "CServerConnection.h"
#include "CServerNetEngine.h"
#include "GameState.h"
#include "DeprecatedGUI/CBrowser.h"
#include "gusanos/LuaCallbacks.h"

#include <boost/shared_ptr.hpp>
#include <boost/lambda/lambda.hpp>
#include <boost/bind.hpp>

Game game;

static bool inMainGameLoop = false;

static bool DbgSimulateSlow = false;

static bool bRegisteredDebugVars = CScriptableVars::RegisterVars("Debug.Game")
( DbgSimulateSlow, "SimulateSlow" );

bool gameWasPrepared = false;
ScriptVar_t preparedMap;
ScriptVar_t preparedMod;
ScriptVar_t preparedMode;

Game::Game() {
	menuFrame = 0;
	thisRef.classId = LuaID<Game>::value;
	thisRef.objId = 1;
	m_isServer = false;
	m_isLocalGame = false;
	m_wpnRest = new CWpnRest();
	gameStateUpdates = new GameStateUpdates;
}

void Game::init() {
	state = S_Inactive;
	prepareMenu();
}

void Game::startServer(bool localGame) {
	m_isServer = true;
	m_isLocalGame = localGame;
	state = S_Lobby;
	LUACALLBACK(serverStart).call()();
}

void Game::startClient() {
	m_isServer = false;
	m_isLocalGame = false;
	state = S_Connecting;
}

void Game::startGame() {
	if(!isServer()) {
		errors << "startGame as client" << endl;
		return;
	}

	if(state != Game::S_Lobby)
		warnings << "startGame: expected to be in lobby but game state is " << game.state << endl;

	state = Game::S_Preparing;
	gameOver = false;
	serverFrame = 0;
}

void Game::restartGame() {
	if(!isServer()) {
		errors << "restartGame as client" << endl;
		return;
	}

	if(state >= Game::S_Preparing) {
		state = Game::S_Lobby;
		game.cleanupAfterGameloopEnd(); // We need a re-init. This will do it.
	}

	state = Game::S_Preparing;
	gameOver = false;
	serverFrame = 0;
}

void Game::gotoLobby(const std::string &reason) {
	if(!isServer()) {
		errors << "gotoLobby as client" << endl;
		return;
	}

	if(state == Game::S_Inactive) {
		errors << "gotoLobby: server not started, we are inactive" << endl;
		return;
	}

	if(isLocalGame()) {
		errors << "gotoLobby in local game" << endl;
		return;
	}

	notes << "gotoLobby: " << reason << endl;
	state = S_Lobby;
}

void Game::stop() {
	if(game.state >= Game::S_Lobby) {
		if(game.isServer())
			LUACALLBACK(serverStop).call()();
		else
			LUACALLBACK(serverLeft).call()();
	}
	m_isServer = false;
	m_isLocalGame = false;
	state = S_Inactive;
}

void checkCurrentGameState() {
	if(!cClient || cClient->getStatus() == NET_DISCONNECTED) {
		// S_INACTIVE;
		if(game.state != Game::S_Inactive)
			errors << "client is disconnected but game state is " << game.state << endl;
		return;
	}
	if(game.isClient()) {
		if(cClient->getStatus() == NET_CONNECTING) {
			// S_CLICONNECTING;
			if(game.state != Game::S_Connecting)
				errors << "client is connecting but game state is " << game.state << endl;
			return;
		}
		if(game.state < Game::S_Preparing) {
			// S_CLILOBBY;
			if(game.state != Game::S_Lobby)
				errors << "client is connected and game is not ready but game state is " << game.state << endl;
			return;
		}
		// S_CLIWEAPONS;
		if(game.state != Game::S_Preparing)
			errors << "client is connected and should be preparing game but game state is " << game.state << endl;
		return;
	}
	if(!cServer->isServerRunning()) {
		// S_INACTIVE;
		if(game.state != Game::S_Inactive)
			errors << "server is not running but game state is " << game.state << endl;
		return;
	}
	//if(!DeprecatedGUI::tMenu || DeprecatedGUI::tMenu->bMenuRunning);
}


void Game::onSettingsUpdate(BaseObject* settingsObj, const AttrDesc* attrDesc, ScriptVar_t /* oldValue */) {
	assert(attrDesc->objTypeId == LuaID<Settings>::value);
	FeatureIndex featureIndex = Settings::getAttrDescs().getIndex(attrDesc);
	ScriptVar_t curValue = attrDesc->get(settingsObj);

	bool needReinit = false;
	switch(featureIndex) {
	case FT_WeaponSlotsNum:
		for_each_iterator(CWorm*, w, game.localWorms()) {
			w->get()->weaponSlots.write().resize((int)curValue);
		}
		break;

	case FT_Map:
		if(cClient)
			cClient->bHaveMap = infoForLevel(curValue.as<LevelInfo>()->path).valid;
		if(gameWasPrepared && curValue != preparedMap) needReinit = true;
		break;
	case FT_Mod:
		if(cClient)
			cClient->bHaveMod = infoForMod(curValue.as<ModInfo>()->path).valid;
		if(gameWasPrepared && curValue != preparedMod) needReinit = true;
		break;
	case FT_GameMode:
		if(gameWasPrepared && curValue != preparedMode) needReinit = true;
		break;
	default: break; // nop
	}

	if(needReinit) {
		notes << featureArray[featureIndex].name << " changed -> reinit" << endl;
		game.cleanupAfterGameloopEnd(); // We need a re-init. This will do it.
	}

	// whenever we need that...
	DeprecatedGUI::bJoin_Update = true;
	DeprecatedGUI::bHost_Update = true;
}

static void cleanupAfterDisconnect();

static void stateUpdate_connectingMenu() {
	assert(game.isClient());
	if(!bDedicated) {
		// goto the joining dialog
		DeprecatedGUI::Menu_Current_Shutdown();
		DeprecatedGUI::Menu_SetSkipStart(true);
		DeprecatedGUI::Menu_NetInitialize(false);
		DeprecatedGUI::Menu_Net_JoinInitialize();
	}
}

static void stateUpdate_gotoNetState() {
	gameSettings.pushUpdateHintAll();

	if(bDedicated) return;
	if(game.isLocalGame()) return;

	// when we leave the net-state (return to S_Inactive)
	DeprecatedGUI::tMenu->iReturnTo = DeprecatedGUI::iNetMode;
}

static void stateUpdate_leaveNetState() {
	if(bDedicated) return;

	DeprecatedGUI::Menu_Current_Shutdown();
	DeprecatedGUI::Menu_SetSkipStart(true);
	if(game.isLocalGame())
		DeprecatedGUI::Menu_LocalInitialize();
	else
		DeprecatedGUI::Menu_NetInitialize(true);
}

void Game::onStateUpdate(BaseObject* oPt, const AttrDesc* attrDesc, ScriptVar_t oldValue) {
	assert(oPt == &game);
	assert(attrDesc == game.state.attrDesc());

#ifdef __EMSCRIPTEN__
	// Tell the JS shell when the engine fully QUITS (transition into
	// S_Quit), so it can drop browser fullscreen. We deliberately do NOT
	// signal on a return to the menu after a match (active state ->
	// S_Inactive): "Quit game" there usually means the player wants to
	// start another match, so staying in fullscreen is the expected
	// behaviour — only leaving the whole application exits fullscreen.
	// PROXY_TO_PTHREAD puts main() on a worker; route through the main
	// browser thread so `window` is reachable.
	if(game.state == Game::S_Quit && (int)oldValue != Game::S_Quit) {
		MAIN_THREAD_ASYNC_EM_ASM({
			if (typeof window !== 'undefined' &&
			    typeof window.olxOnEngineExit === 'function') {
				window.olxOnEngineExit($0);
			}
		}, (int)game.state);
	}
#endif

	if((int)oldValue >= Game::S_Preparing && game.state <= Game::S_Lobby)
		game.cleanupAfterGameloopEnd();
	if((int)oldValue >= Game::S_Lobby && game.state < Game::S_Lobby)
		cleanupAfterDisconnect();
	if((int)oldValue >= Game::S_Preparing && game.state <= Game::S_Lobby)
		game.prepareMenu();
	if((int)oldValue <= Game::S_Inactive && game.state >= Game::S_Connecting)
		stateUpdate_gotoNetState();
	if((int)oldValue > Game::S_Inactive && game.state == Game::S_Inactive)
		stateUpdate_leaveNetState();
	if(game.state == Game::S_Connecting && game.isClient())
		stateUpdate_connectingMenu();

	if((int)oldValue <= Game::S_Connecting && game.state >= Game::S_Lobby && game.isClient())
		LUACALLBACK(serverJoined).call()();
	if((int)oldValue <= Game::S_Lobby && game.state >= Game::S_Preparing)
		LUACALLBACK(gamePrepare).call()();
	if((int)oldValue <= Game::S_Preparing && game.state >= Game::S_Playing) {
		LUACALLBACK(gameBegin).call()();
		cClient->InitializeIngameScore(); // reinit to be sure
	}
	if(game.state == Game::S_Lobby)
		LUACALLBACK(gotoLobby).call()();
}

void Game::onGameOverUpdate(BaseObject* oPt, const AttrDesc* attrDesc, ScriptVar_t oldValue) {
	assert(oPt == &game);
	assert(attrDesc == game.gameOver.attrDesc());

	if(game.gameOver)
		LUACALLBACK(gameOver).call()();
}

void Game::prepareMenu() {
	menuFrame = 0;

	cClient->SetSocketWithEvents(true);
	cServer->SetSocketWithEvents(true);

	if(!bDedicated) {
		if(!DeprecatedGUI::bSkipStart) {
			notes << "Loading main menu" << endl;
			DeprecatedGUI::tMenu->iMenuType = DeprecatedGUI::MNU_MAIN;
			DeprecatedGUI::Menu_MainInitialize();
		} else
			DeprecatedGUI::Menu_RedrawMouse(true);
	}

	DeprecatedGUI::bSkipStart = false;

	tLX->currentTime = menuStartTime = GetTime();
}

Result Game::prepareGameloop() {
	notes << "prepare game loop" << endl;

	// Pre-game initialization
	if(!bDedicated) FillSurface(VideoPostProcessor::videoSurface().get(), tLX->clBlack);

	// TODO: remove that as soon as we do the gamescript/map loading in a seperate thread
	ScopedBackgroundLoadingAni backgroundLoadingAni(320, 280, 50, 50, Color(128,128,128), Color(64,64,64));

	// Note: This is probably the most hacky way of doing this but it should work fine.
	// In the future, it probably will be exactly the other way around:
	// Map/mod loading will be in an extra thread and the main game thread will keep us alive in the meanwhile.
	// WARNING: This code assumes that we don't access cl->cNetChan in the meanwhile. Even when doing this
	// in a clean way, we would need that. In case we need this at some time, the only solution is to make CChannel
	// threadsafe or to add another way to keep us alive (maybe a connection-less ping package with the same effect or so).
	struct TimeoutAvoider {
		struct TimeoutAvoiderAction : Action {
			CChannel* chan;
			SmartPointer< ThreadVar<bool> > scope;

			Result handle() {
				int c = 0;
				while(true) {
					{
						ThreadVar<bool>::Reader s(*scope.get());
						if(!s.get()) break;

						if(c == 1) {
							// This will kind of keep us alive.
							CBytestream emptyUnreliable;
							chan->Transmit(&emptyUnreliable);
						}
					}
					c++; c %= 10;
					SDL_Delay(100);
				}
				return true;
			}
		};

		SmartPointer< ThreadVar<bool> > scope;
		TimeoutAvoider() {
			if(game.isServer()) return;
			scope = new ThreadVar<bool>(true);
			TimeoutAvoiderAction* a = new TimeoutAvoiderAction();
			a->chan = cClient->cNetChan;
			a->scope = scope;
			threadPool->start(a, "prepareGameloop timeout avoider keep-me-alive", true);
		}
		~TimeoutAvoider() { if(scope.get()) scope->write() = false; }
	};
	TimeoutAvoider timeoutAvoider;

	// If we go out of the menu, it means the user has selected something.
	// This indicates that everything is fine, so we should restart in case of a crash.
	// Note that we will set this again to false later on in case the user quitted.
	CrashHandler::restartAfterCrash = true;

	cClient->SetSocketWithEvents(false);
	cServer->SetSocketWithEvents(false);

	if(isClient()) {
		// check if we have level
		std::string sMapFilename = "levels/" + cClient->getGameLobby()[FT_Map].as<LevelInfo>()->path.get();
		if(CMap::GetLevelName(GetBaseFilename(sMapFilename)) == "") {
			cClient->DownloadMap(GetBaseFilename(sMapFilename));  // Download the map
			// we have bDownloadingMap = true when this was successfull
		}

		if(cClient->bDownloadingMod)
			cClient->bWaitingForMod = true;
		if(cClient->bDownloadingMap)
			cClient->bWaitingForMap = true;
	}

	if(NegResult r = game.loadMod())
		return "Error while loading mod: " + r.res.humanErrorMsg;
	preparedMod = cClient->getGameLobby()[FT_Mod];

	if(NegResult r = game.loadMap())
		return "Error while loading map: " + r.res.humanErrorMsg;
	preparedMap = cClient->getGameLobby()[FT_Map];

	cClient->bMapGrabbed = true;

	// always also load Gusanos engine
	// even with LX-stuff-only, we may access/need it (for network stuff and later probably more)
	if( !game.gameMap()->gusIsLoaded() && (game.isServer() || cClient->getServerVersion() >= OLXBetaVersion(0,59,1) ) ) {
		// WARNING: This may be temporary
		// Right now, we load the gus mod in the map loader (gusGame.changeLevel).
		// Thus, when we don't load a gus level, we must load the mod manually.

		if(!game.gameScript()->gusEngineUsed())
			gusGame.setMod(gusGame.getDefaultPath());
		gusGame.loadModWithoutMap();
	}

	if(gusGame.isEngineNeeded()) {
		gusGame.runInitScripts();
	}

	if(game.isServer()) {
		// Check that gamespeed != 0
		if (-0.05f <= (float)gameSettings[FT_GameSpeed] && (float)gameSettings[FT_GameSpeed] <= 0.05f) {
			warnings << "WARNING: gamespeed was set to " << gameSettings[FT_GameSpeed].toString() << "; resetting it to 1" << endl;
			gameSettings.overwrite[FT_GameSpeed] = 1;
		}

		// Note: this code must be after we loaded the mod!
		// TODO: this must be moved to the menu so that we can see it also there while editing custom settings
		if(!gameScript()->gusEngineUsed() /*LX56*/) {
			// Copy over LX56 mod settings. This is an independent layer, so it is also independent from gamePresetSettings.
			modSettings = game.gameScript()->lx56modSettings;
		}

		// First, clean up the old settings.
		gamePresetSettings.makeSet(false);
		// Now, load further mod custom settings.
		gamePresetSettings.loadFromConfig(game.gameScript()->directory() + "/gamesettings.cfg", false);
		// Now, after this, load the settings specified by the game settings preset.
		const std::string& presetCfg = gameSettings[FT_SettingsPreset].as<GameSettingsPresetInfo>()->path;
		if( !gamePresetSettings.loadFromConfig( presetCfg, false ) )
			warnings << "Game: failed to load settings preset from " << presetCfg << endl;

		// fix some broken settings
		if((int)gameSettings[FT_Lives] < 0 && (int)gameSettings[FT_Lives] != WRM_UNLIM)
			gameSettings.layerFor(FT_Lives)->set(FT_Lives) = (int)WRM_UNLIM;

		gameSettings.dumpAllLayers();
	}

	// Client: In the PrepareGame package (on older servers), we read the list from the server.
	//   Here, we just update it according to the game script.
	//   We must do it here now because we haven't loaded the gamescript earlier.
	// Server: This must be after we have setup the gamePresetSettings because it may change the WeaponRest!
	game.loadWeaponRestrictions();

	if(game.isServer()) {
		std::string errMsg;
		if(!cServer->PrepareGame(&errMsg)) {
			if (game.isLocalGame())
				GotoLocalMenu();
			else
				GotoNetMenu();
			return "starting game in local game failed for reason: " + errMsg;
		}
	}
	preparedMode = cClient->getGameLobby()[FT_GameMode];

	while(cClient->getStatus() == NET_CONNECTING) {
		notes << "client not connected yet - waiting" << endl;
		SDL_Delay(10);
		SyncServerAndClient();
	}
	if(cClient->getStatus() == NET_DISCONNECTED) {
		warnings << "prepareGameLoop: something went wrong, client not connected anymore" << endl;
		game.state = Game::S_Inactive;
		return "client did not connect";
	}

	// we need the gamescript in physics init
	while(gameScript() == NULL) {
		notes << "gamescript not loaded yet - waiting" << endl;
		SDL_Delay(10);
		SyncServerAndClient();
	}

	if(isClient() && cClient->getServerVersion() < OLXBetaVersion(0,59,6)) {
		// All the custom settings we may set in the game mod were unknown (as feature array settings)
		// to earlier versions. Thus we overwrite it this way.
		gameScript()->lx56modSettings.copyTo( cClient->getGameLobby().overwrite );
	}

	if(isServer()) {
		// resend lua event index to everyone
		network.sendEncodedLuaEvents(INVALID_CONN_ID);		
	}
	
	PhysicsEngine::Init();
		
	ClearEntities();

	game.hudPermanentText = "";

	cClient->flagInfo()->reset();
	game.teamScores.write().resize(0);

	cClient->projPosMap.clear();
	cClient->projPosMap.resize(CClient::MapPosIndex( VectorD2<int>(game.gameMap()->GetWidth(), game.gameMap()->GetHeight())).index(game.gameMap()) );
	cClient->cProjectiles.clear();

	cClient->SetupViewports();

	//TODO: Move into CTeamDeathMatch | CGameMode
	// If this is the host, and we have a team game: Send all the worm info back so the worms know what
	// teams they are on
	if(game.isServer() && !game.isLocalGame()) {
		if( game.gameMode()->GameTeams() > 1 )
			cServer->UpdateWorms();
	}

	if(game.isServer()) {
		// reset. gamemodes might overwrite those.
		// it also might make sense to generalize these resets via AttrDesc and some resetOnPrepare.
		levelDarkMode = false;
		darkMode_wormLightRadius = darkMode_wormLightRadius_Type::attrDesc()->defaultValue;

		notes << "preparing game mode " << game.gameMode()->Name() << endl;
		game.gameMode()->PrepareGame();

		// update variable if not done yet
		cServer->serverChoosesWeapons();

		// update about all other vars
		cServer->UpdateGameLobby();
		gameSettings.pushUpdateHintAll(); // because of mod specific settings and what not ...
	}

	if( GetGlobalIRC() )
		GetGlobalIRC()->setAwayMessage("Playing: " + cClient->getServerName());

	{
		// Initialize the drawing
		cClient->InitializeDrawing();

		game.gameMap()->SetMinimapDimensions(cClient->tInterfaceSettings.MiniMapW, cClient->tInterfaceSettings.MiniMapH);

		// Reset the scoreboard here so it doesn't show kills & lives when waiting for players
		cClient->InitializeIngameScore();

		// Copy the chat text from lobby to ingame chatbox
		if( game.isServer() && !game.isLocalGame() )
			cClient->sChat_Text = DeprecatedGUI::Menu_Net_HostLobbyGetText();
		else if( game.isClient() )
			cClient->sChat_Text = DeprecatedGUI::Menu_Net_JoinLobbyGetText();

		if (!cClient->sChat_Text.empty())  {
			cClient->bChat_Typing = true;
			cClient->bChat_CursorVisible = true;
			cClient->iChat_Pos = cClient->sChat_Text.size();
		}

		if(!bDedicated) {
			// TODO: move that out, that does not belong here
			// Load the chat
			DeprecatedGUI::CBrowser *lv = cClient->cChatList;
			if (lv)  {
				lv->setBorderSize(0);
				lv->InitializeChatBox();
				lines_iterator it = cClient->cChatbox.At((int)cClient->cChatbox.getNumLines()-256); // If there's more than 256 messages, we start not from beginning but from end()-256
				//int id = (lv->getLastItem() && lv->getItems()) ? lv->getLastItem()->iIndex + 1 : 0;

				for (; it != cClient->cChatbox.End(); it++)  {

					// Add only chat text (PM and Team PM messages too)
					if (it->iTextType == TXT_CHAT || it->iTextType == TXT_PRIVATE || it->iTextType == TXT_TEAMPM ) {
						lv->AddChatBoxLine(it->strLine, it->iColour, it->iTextType);
					}
				}
			}
		}

		// Start the game logging
		cClient->StartLogging((int)game.worms()->size());

		if(!bDedicated)
		{
			if( ! ( game.gameScript() && game.gameScript()->gusEngineUsed() ) )
			{
				cClient->cChatList->Setup(0,	cClient->tInterfaceSettings.ChatBoxX,
											cClient->tInterfaceSettings.ChatBoxY,
											cClient->tInterfaceSettings.ChatBoxW,
											cClient->tInterfaceSettings.ChatBoxH);
				cClient->cChatList->showScrollbar(true);
			}
			else // Expand chatbox for Gus, looks better
			{
				cClient->cChatList->Setup(0,	5,
											cClient->tInterfaceSettings.ChatBoxY,
											cClient->tInterfaceSettings.ChatBoxW + cClient->tInterfaceSettings.ChatBoxX - 5,
											cClient->tInterfaceSettings.ChatBoxH);
				cClient->cChatList->showScrollbar(false);
			}
		}

		cClient->bShouldRepaintInfo = true;

		DeprecatedGUI::bJoin_Update = true;
	}

	prepareCallbacks();
	prepareCallbacks.disconnect_all_slots();

	if(gameSettings[FT_ImmediateStart])
		game.state = Game::S_Playing;

	ProcessEvents();
	notes << "GameLoopStart. MaxFPS is " << tLXOptions->nMaxFPS << endl;

	inMainGameLoop = true;
	if( DedicatedControl::Get() )
		DedicatedControl::Get()->GameLoopStart_Signal();
	
	CrashHandler::recoverAfterCrash = tLXOptions->bRecoverAfterCrash && GetGameVersion().releasetype == Version::RT_NORMAL;
	
	simulationTime = oldtime = GetTime();
	gameWasPrepared = true;
	return true;
}


static TimeDiff simulationDelay() { return GetTime() - GetPhysicsTime(); }

bool Game::hasHighSimulationDelay() { return simulationDelay() > TimeDiff(100); }
bool Game::hasSeriousHighSimulationDelay() { return simulationDelay() > TimeDiff(200); }

struct GusSpeedScope {
	std::vector< SmartPointer<CGameObject::ScopedGusCompatibleSpeed> > scopedSpeeds;
	GusSpeedScope() {
		scopedSpeeds.reserve( game.worms()->size() * 2 );
		for_each_iterator(CWorm*, w, game.worms()) {
			scopedSpeeds.push_back( new CGameObject::ScopedGusCompatibleSpeed(*w->get()) );
			scopedSpeeds.push_back( new CGameObject::ScopedGusCompatibleSpeed(w->get()->cNinjaRope.write()) );
		}
	}
};

void Game::frame() {
	SetCrashHandlerReturnPoint("main game loop");

	// Timing
	tLX->currentTime = GetTime();
	tLX->fDeltaTime = tLX->currentTime - oldtime;
	tLX->fRealDeltaTime = tLX->fDeltaTime;
	oldtime = tLX->currentTime;

	ProcessEvents();

	// Main frame
	frameInner();

	if(game.state <= Game::S_Lobby) {
		DeprecatedGUI::Menu_Frame();
		menuFrame++;

		// If we have run fine for >=5 seconds and 10 frames, it is probably
		// safe & make sense to restart the game in case of a crash.
		if(menuFrame >= 10 && tLX->currentTime - menuStartTime >= TimeDiff(5.0f))
			CrashHandler::restartAfterCrash = true;
	}

	if(DbgSimulateSlow) SDL_Delay(700);

	doVideoFrameInMainThread();
	CapFPS();
}



///////////////////
// Game loop
void Game::frameInner()
{
	HandlePendingCommands();
	
	if(bDedicated)
		DedicatedControl::Get()->GameLoop_Frame();

	if(game.state >= Game::S_Connecting) {
		// Check if the communication link between us & server is still ok
		if(cClient->getServerError()) {
			// Check for any communication errors
			notes << "servererror: " << cClient->getServerErrorMsg() << endl;

			if(!bDedicated) {
				// Show message box, shutdown and quit back to menu
				DrawImage(DeprecatedGUI::tMenu->bmpBuffer.get(), DeprecatedGUI::tMenu->bmpMainBack_common, 0, 0);
				DeprecatedGUI::Menu_RedrawMouse(true);
				EnableSystemMouseCursor(false);

				DeprecatedGUI::Menu_MessageBox("Communication", cClient->getServerErrorMsg(), DeprecatedGUI::LMB_OK);
			}
			if(DedicatedControl::Get())
				DedicatedControl::Get()->ClientConnectionError_Signal(cClient->getServerErrorMsg());

			if(game.isLocalGame())
				GotoLocalMenu();
			else
				GotoNetMenu();
			return;
		}
	}

	// no lobby for local games
	if(game.state == Game::S_Lobby && game.isLocalGame())
		game.startGame();

	if(!gameWasPrepared && game.state >= Game::S_Preparing) {
		if(game.state.ext.updated) {
			// handle callbacks
			iterAttrUpdates();
		}
		Result r = prepareGameloop();
		if(!r) {
			warnings << "prepageGameloop failed: " << r.humanErrorMsg << endl;
			if(!bDedicated)
				DeprecatedGUI::Menu_MessageBox("Error", "Error while starting game: " + r.humanErrorMsg);
			prepareCallbacks.disconnect_all_slots();
			if(game.isLocalGame())
				game.state = Game::S_Inactive;
			else
				game.state = Game::S_Lobby;
			// handle callbacks and other stuff
			iterAttrUpdates();
			return;
		}
		else if(!gameWasPrepared) {
			errors << "prepageGameloop: no error but not prepared" << endl;
			return;
		}
		return; // skip to next frame to do stuff. also executes pending commands, etc.
	}

	// Check if user pressed screenshot key
	if (tLX->cTakeScreenshot.isDownOnce())  {
		PushScreenshot("scrshots", "");
	}
	
	// Switch between window and fullscreen mode
	// Switch only if delta time is low enough. This is because when the game does not
	// respond for >30secs and the user presses cSwitchMode in the meantime, the mainlock-detector
	// would switch to window and here we would switch again to fullscreen which is stupid.
	if( tLX->cSwitchMode.isUp() && tLX && tLX->fRealDeltaTime < 1.0f )  {
#if defined(__EMSCRIPTEN__)
		// Wasm fullscreen toggle is gated by the same master switch as
		// the initial mode in AuxLib.cpp. Eat the keypress quietly so a
		// stray Alt+Enter doesn't ruin a session while the feature is
		// off.
		tLX->cSwitchMode.reset();
#else
		// Set to fullscreen
		tLXOptions->bFullscreen = !tLXOptions->bFullscreen;

		// Set the new video mode
		doSetVideoModeInMainThread();

		tLX->cSwitchMode.reset();
#endif
	}
	
#ifdef WITH_G15
	if (OLXG15)
		OLXG15->gameFrame();
#endif //WITH_G15
	
	if(tLXOptions->bEnableChat)
		ProcessIRC();

	if(state > Game::S_Inactive) {
		cClient->ReadPackets();

		cClient->ProcessMapDownloads();
		cClient->ProcessModDownloads();
		cClient->ProcessUdpUploads();

		cClient->SimulateHud();

		if(isServer() && cServer->isServerRunning()) {
			cServer->ProcessRegister();
			cServer->ProcessGetExternalIP();
			cServer->ReadPackets();
		}
	}

	if(state >= Game::S_Preparing && gameWasPrepared && !cClient->bWaitingForMod) {
		bool update = false;
		bool updateLocal = false;

		// Prepare worms if needed
		for_each_iterator(CWorm*, w, game.worms()) {
			if(w->get()->isPrepared()) continue;

			notes << "preparing worm " << w->get()->getID() << ":" << w->get()->getName() << " for battle" << endl;

			w->get()->setLives(((int)cClient->getGameLobby()[FT_Lives] < 0) ? WRM_UNLIM : (int)cClient->getGameLobby()[FT_Lives]);
			w->get()->setKills(0);
			w->get()->setDeaths(0);
			w->get()->setTeamkills(0);
			w->get()->setDamage(0);
			w->get()->setHealth(100);
			w->get()->bWeaponsReady = false;

			// (If this is a local game?), we need to reload the worm graphics
			// We do this again because we've only just found out what type of game it is
			// Team games require changing worm colours to match the team colour
			// Inefficient, but i'm not going to redesign stuff for a simple gametype
			w->get()->ChangeGraphics();

			w->get()->Prepare();

			// needToCreateOwnWormInputHandlers: Since 0.59, player objects
			// are dynamically synced via Gusanos protocol. CWorm::Prepare
			// doesn't create the inputHandler in that case and thus we
			// also cannot setup game inputs and do the weapon selection.
			// We do that on-the-fly when we get the object.
			if(w->get()->getLocal() && needToCreateOwnWormInputHandlers()) {
				updateLocal = true;

				// Initialize the worms weapon selection menu & other stuff
				if(!w->get()->bWeaponsReady)
					w->get()->initWeaponSelection();
			}

			if(game.isServer()) {
				cServer->PrepareWorm(w->get());

				for( int i = 0; i < MAX_CLIENTS; i++ ) {
					if( !cServer->getClients()[i].isConnected() )
						continue;
					cServer->getClients()[i].getNetEngine()->SendWormProperties(w->get()); // if we have changed them in prepare or so
				}
			}

			update = true;
		}

		if(updateLocal)
			// The worms are first prepared here in this function and thus the input handlers where not set before.
			// We have to set the control keys now.
			cClient->SetupGameInputs();
	}

	if(!state.ext.updated && state >= Game::S_Preparing && gameWasPrepared) {
		// We have a separate fixed 100FPS for game simulation.
		// Note: much old code uses tLX->{currentTime, fDeltaTime, fRealDeltaTime}.
		// We cannot change tLX->currentTime because some code expects that tLX->currentTime
		// is always *only* increasing, *never* decreasing, e.g.
		// CClientNetEngineBeta9::SendReportDamage(), and many others.
		// The physics code however uses GetPhysicsTime(), which returns the simulationTime.
		TimeDiff curDeltaTime = tLX->fDeltaTime;
		tLX->fDeltaTime = tLX->fRealDeltaTime = TimeDiff(Game::FixedFrameTime);
		
		TimeDiff frameDt = TimeDiff(Game::FixedFrameTime);
		if(!(bool)cClient->getGameLobby()[FT_GameSpeedOnlyForProjs]) {
			frameDt *= 1.0f/CLAMP((float)cClient->getGameLobby()[FT_GameSpeed], 0.05f, 10.0f);
			if(frameDt <= TimeDiff(0))
				frameDt = TimeDiff(1);
		}
		
		while(simulationTime < tLX->currentTime) {

			if(hasSeriousHighSimulationDelay()) {
				TimeDiff simDelay = simulationDelay();
				if(simDelay > 0.5f)
					warnings << "deltatime " << simDelay.seconds() << " is too high" << endl;
				// Don't do anything anymore, just skip.
				// Also don't increment serverFrame so clients know about this.
				simulationTime += TimeDiff(simDelay.milliseconds() - simDelay.milliseconds() % Game::FixedFrameTime);
				// Note that the underlying LX physics still simulates all frames in between,
				// because it has its own fLastSimulationTime.
				// Earlier, that was what we had fRealDeltaTime for, to skip such lags.
				// TODO: This all doesn't work anymore, because we have fRealDeltaTime fixed
				// here. We should remove all that logic from the LX physics and just keep
				// the logic here.
				continue;
			}

			if(game.state.ext.updated)
				// re-check everything outside. too much could have happened
				break;

			if(game.state < Game::S_Preparing)
				break;

			if(game.state == Game::S_Playing && !isGamePaused())
				serverFrame++;

			// do lua/gus frames in all cases
			{
				GusSpeedScope speedScope;
				gusLogicFrame();
			}

			cClient->Frame();
			if(isServer())
				cServer->Frame();

			simulationTime += frameDt;
		}
		tLX->fDeltaTime = tLX->fRealDeltaTime = curDeltaTime;
	}

	const bool stateUpdated = state.ext.updated;
	iterAttrUpdates();

	if(tLX && !stateUpdated && state >= Game::S_Preparing)
		cClient->Draw(VideoPostProcessor::videoSurface());

	if(state > Game::S_Inactive) {
		// Gusanos network
		{
			GusSpeedScope speedScope;
			network.update();
		}

		if(isClient() && state >= Game::S_Lobby) {
			cClient->SendGameStateUpdates();
		}

		if(!stateUpdated) // only if not updated, too unsafe otherwise
			cClient->SendPackets();

		// Connecting process
		cClient->Connecting();

		if(isServer() && cServer->isServerRunning()) {
			cServer->CheckRegister();
			cServer->SendFiles();
			cServer->SendGameStateUpdates();
			cServer->SendPackets();
		}
	}

	cClient->resetDebugStr();
	
	EnableSystemMouseCursor(false);
}


void Game::cleanupAfterGameloopEnd() {
	gameWasPrepared = false;
	CrashHandler::recoverAfterCrash = false;

	cClient->ShutdownLog();

	// can happen if we have aborted a game
	if(isServer() && !gameOver)
		// call gameover because we may do some important cleanup there
		game.gameMode()->GameOver();
	
	reset();
	gusGame.unload();
	
	PhysicsEngine::UnInit();
	
	notes << "GameLoopEnd, state: " << StateAsStr(game.state) << endl;
	inMainGameLoop = false;
	if( DedicatedControl::Get() )
		DedicatedControl::Get()->GameLoopEnd_Signal();		
	
	cCache.ClearExtraEntries(); // Game ended - clear cache	

	// Cleanup settings layer so that while being in lobby,
	// no outdated settings are used for GameServer::isVersionCompatible.
	// This can (and should) be removed once we have the settings already loaded
	// immediately in lobby.
	modSettings.makeSet(false);
	gamePresetSettings.makeSet(false);
	
	cleanupCallbacks();
	cleanupCallbacks.disconnect_all_slots();

	if(isServer()) {
		if(!cServer->isServerRunning())
			stop();
	}
	else {
		if(cClient->getStatus() == NET_DISCONNECTED)
			stop();
	}

	if(game.state == Game::S_Lobby) {
		// Do a minor clean up
		cClient->MinorClear();

		// Hide the console
		Con_Hide();

		DeprecatedGUI::Menu_FloatingOptionsShutdown();

		if(game.isClient()) {
			// Tell server my worms aren't ready
			CBytestream bs;
			bs.Clear();
			bs.writeByte(C2S_UPDATELOBBY);
			bs.writeByte(0);
			cClient->cNetChan->AddReliablePacketToSend(bs);
		}

		if( GetGlobalIRC() )
			GetGlobalIRC()->setAwayMessage("Server: " + cClient->getServerName());

		// update menu
		DeprecatedGUI::bHost_Update = true;
		DeprecatedGUI::bJoin_Update = true;
	}

	if(game.state == Game::S_Lobby && game.isServer()) {
		// Clear the info
		bool bUpdateWorms = false;
		for_each_iterator(CWorm*, w, game.worms()) {
			if( w->get()->getAFK() == AFK_TYPING_CHAT )
			{
				w->get()->setAFK(AFK_BACK_ONLINE, "");
				CBytestream bs;
				bs.writeByte( S2C_AFK );
				bs.writeByte( (uchar)w->get()->getID() );
				bs.writeByte( AFK_BACK_ONLINE );
				bs.writeString( "" );

				CServerConnection *cl;
				int i;
				for( i=0, cl=cServer->getClients(); i < MAX_CLIENTS; i++, cl++ )
					if( cl->getStatus() == NET_CONNECTED && cl->getClientVersion() >= OLXBetaVersion(0,57,7) )
						cl->getNetEngine()->SendPacket(&bs);
			}
		}

		if(bUpdateWorms)
			cServer->UpdateWorms();

		if( DedicatedControl::Get() )
			DedicatedControl::Get()->BackToServerLobby_Signal();

		// Goto the host lobby

		for( short i=0; i<MAX_CLIENTS; i++ )
			cServer->getClients()[i].getUdpFileDownloader()->allowFileRequest(true);

		// Re-register the server to reflect the state change
		if( tLXOptions->bRegServer && (game.isServer() && !game.isLocalGame()) )
			cServer->RegisterServerUdp();

		cServer->CheckForFillWithBots();

		cServer->gotoLobby();
	}

	if(game.state == Game::S_Lobby && game.isClient()) {
		DeprecatedGUI::tMenu->iMenuType = DeprecatedGUI::MNU_NETWORK;
		DeprecatedGUI::bSkipStart = true;
		DeprecatedGUI::Menu_Net_JoinGotoLobby();
	}
}

static void cleanupAfterDisconnect() {
	network.olxShutdown();

	// Stop any file downloads
	if (cClient->getDownloadingMap() && cClient->getHttpDownloader())
		cClient->getHttpDownloader()->RemoveAllDownloads();
	cClient->getUdpFileDownloader()->reset();

	if( GetGlobalIRC() )
		GetGlobalIRC()->setAwayMessage("");

	if(game.state == Game::S_Inactive) {
		if(cServer && cServer->isServerRunning())
			// Tell any clients that we're leaving
			cServer->SendDisconnect();

		cClient->Shutdown();
		cServer->Shutdown();
	}
}



void Game::onPrepareWorm(CWorm* w) {
	objects.insertImmediately(w, Grid::WormColLayer, Grid::WormRenderLayer);
	objects.insertImmediately((CGameObject*) w->getNinjaRope(), 1, 1);
	LUACALLBACK(wormPrepare).call()(w->getLuaReference())();
}

void Game::onUnprepareWorm(CWorm* w) {
	// We must unlink the object now from the list because this destructor
	// is not called from Gusanos but from CClient.
	// NOTE: Not really the best way but I don't know a better way
	// Game.onNewWorm has inserted the object into the list.
	objects.unlink(w);
	objects.unlink(w->getNinjaRope());
}

void Game::onRemoveWorm(CWorm* w) {
	if(w->isPrepared())
		w->Unprepare(); // also to call onUnprepareWorm and to unlink it
	std::map<int,CWorm*>::iterator i = m_worms.find(w->getID());
	assert(i->second == w);
	m_worms.erase(i);
	gameStateUpdates->pushObjDeletion(w->thisRef);
}

void Game::onNewPlayer(CWormInputHandler* player) {
	players.push_back( player );	
}

void Game::onNewPlayer_Lua(CWormInputHandler* p) {
	LUACALLBACK(playerInit).call()(p->getLuaReference())();
}

void Game::onRemovePlayer(CWormInputHandler* p) {
	foreach(p2, players) {
		if(*p2 == p) {
			players.erase(p2);
			break;
		}
	}
	foreach(p2, localPlayers) {
		if(*p2 == p) {
			localPlayers.erase(p2);
			break;
		}
	}
}

void Game::onNewHumanPlayer(CWormHumanInputHandler* player) {
	localPlayers.push_back( player );	
	player->local = true;
}

void Game::onNewHumanPlayer_Lua(CWormHumanInputHandler* player) {
	LUACALLBACK(localplayerInit).call()(player->getLuaReference())();
}


void Game::reset() {
	notes << "Game::reset" << endl;
	
	// Delete all players
	std::vector<CWormInputHandler*> playersCopy(players); // copy to avoid problems
	foreach ( p, playersCopy )
		(*p)->deleteThis();
	players.clear();
	localPlayers.clear();
	
	// we must call this first because the references to weapons, ninjarope and what may be deleted
	for_each_iterator(CWorm*, w, worms())
		w->get()->Unprepare();
	
	// Delete all objects
	objects.clear();
}

void Game::resetWorms() {
	for_each_iterator(CWorm*, w, FullCopyIterator(worms()))
		w->get()->deleteThis();
	m_worms.clear();	
}

CMap* Game::gameMap() { return m_gameMap.get(); }

CGameScript* Game::gameScript() { return m_gameMod.get(); }

CGameMode* Game::gameMode() {
	if(tLX) {
		if(game.isServer()) return gameSettings[FT_GameMode].as<GameModeInfo>()->mode;
		return cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode;
	}
	return NULL;
}

CWpnRest* Game::weaponRestrictions() { return m_wpnRest.get(); }

bool Game::needProxyWormInputHandler() {
	return isServer();
}

bool Game::needToCreateOwnWormInputHandlers() {
	return isServer() || (cClient->getServerVersion() < OLXBetaVersion(0,59,1));
}

bool Game::needManualClientSideStateManagement() {
	if(isServer()) return false;
	if(state <= Game::S_Inactive) return false;
	if(!cClient) return true;
	if(!cClient->getNetEngine()) return true;
	// Old versions don't sync the objects/attribs, thus also not the game.state.
	// Thus, in ParsePrepareGame and co, we need to update game.state manually.
	if(cClient->getServerVersion() < OLXBetaVersion(0,59,10)) return true;
	return false;
}

bool Game::isTeamPlay() {
	return cClient->isTeamGame();
}


bool CClient::getGamePaused() {
	if(!game.isLocalGame()) return false; // pause only allowed in local game
	return bViewportMgr || bGameMenu || Con_IsVisible();
}

bool Game::isGamePaused() {
	// If we're in a menu & a local game, don't do simulation
	return cClient->getGamePaused();
}

bool Game::shouldDoPhysicsFrame() {
	if(isGamePaused()) return false;
	if(!cClient->canSimulate()) return false;
	if(gameOver) {
		// We stop a few seconds after the actual game over
		if(gameOverTime().seconds() > GAMEOVER_WAIT)
			return false;
	}
	return true;
}


Iterator<CWorm*>::Ref Game::worms() {
	return GetIterator_second(m_worms);
}

Iterator<CWorm*>::Ref Game::localWorms() {
	return GetFilterIterator(worms())(&CWorm::getLocal);
}

Iterator<CWorm*>::Ref Game::aliveWorms() {
	return GetFilterIterator(worms())(&CWorm::getAlive);
}

Iterator<CWorm*>::Ref Game::wormsOfClient(const CServerConnection* cl) {
	return GetFilterIterator(worms())( boost::bind(&CWorm::getClient, _1) == cl );
}

CWorm* Game::wormById(int wormId, bool assertExisting) {
	std::map<int,CWorm*>::iterator i = m_worms.find(wormId);
	if(i == m_worms.end()) {
		if(assertExisting)
			assert(false);
		return NULL;
	}
	return i->second;
}

CWorm* Game::firstLocalHumanWorm() {
	for_each_iterator(CWorm*, w, localWorms())
		if( w->get()->getType() == PRF_HUMAN )
			return w->get();
	return NULL;
}

CWorm* Game::findWormByName(const std::string& name) {
	for_each_iterator(CWorm*, w, worms())
		if(stringcasecmp(w->get()->getName(), name) == 0)
			return w->get();
	return NULL;
}

CWorm* Game::createNewWorm(int wormId, bool local, const SmartPointer<profile_t>& profile, const Version& clientVersion) {
	assert(wormById(wormId, false) == NULL);
	assert(wormId >= 0);

	CWorm* w = new CWorm();
	w->setLocal(local);
	if(local && game.isServer()) w->setClient(cServer->localClientConnection());
	w->setID(wormId);
	if(profile.get()) {
		w->setName(profile->sName);
		w->setSkin(profile->cSkin);
		w->setTeam(profile->iTeam);
		w->setType(WormType::fromInt(profile->iType));
		if(local && bDedicated && w->getType() == PRF_HUMAN) {
			warnings << "createNewWorm: local human worm creation on dedicated server -> make it a bot instead" << endl;
			w->setType(PRF_COMPUTER);
		}
	}
	w->setClientVersion(clientVersion);
	w->setProfile(profile.get() ? profile : new profile_t());
	w->thisRef.objId = wormId;
	m_worms[wormId] = w;
	gameStateUpdates->pushObjCreation(w->thisRef);

	DeprecatedGUI::bHost_Update = true;
	DeprecatedGUI::bJoin_Update = true;

	return w;
}

int Game::getNewUniqueWormId() {
	int lastId = -1;
	foreach(w, m_worms) {
		if(w->first > lastId + 1) // there is at least one ID free
			return lastId + 1;
		lastId = w->first;
	}
	return lastId + 1;
}

void Game::removeWorm(CWorm* w) {
	assert(w != NULL);
	w->deleteThis();
	// onRemoveWorm will be called and will remove the worm from the list

	DeprecatedGUI::bHost_Update = true;
	DeprecatedGUI::bJoin_Update = true;
}

static std::string _wormName(CWorm* w) { return itoa(w->getID()) + ":" + w->getName(); }

std::string Game::wormName(int wormId) {
	return ifWorm<std::string>(wormId, _wormName, itoa(wormId) + ":<unknown-worm>");
}

bool Game::allowedToSleepForEvent() {
	if(state > Game::S_Inactive)
		// we are in connecting, lobby, game or so -> don't sleep
		// In theory, in connecting/lobby, network events should
		// be enough. But that has been buggy earlier, so don't sleep for now.
		return false;

	if(havePendingCommands())
		// We must wait the next frames for the pending commands to be completed.
		return false;

	// in menu
	if(processedEvent)
		// LX GUI code sucks. It sometimes needs a refresh right after
		// some event was handled and thus, don't sleep for the next frame.
		// We reset processedEvent in the next ProcessEvents() call.
		return false;

	if(menuFrame < 2)
		// Again, LX GUI code sucks. It needs some refresh right at startup.
		return false;

	if(Con_IsVisible())
		// cursor blinking in console
		return false;

	return true;
}



int32_t Game::getRandomEnabledWpn() {
	if(gameScript() == NULL) return -1;
	if(!gameScript()->isLoaded()) return -1;

	std::vector<int32_t> enabledWpns;
	enabledWpns.reserve(gameScript()->GetNumWeapons());

	for(int i = 0; i < gameScript()->GetNumWeapons(); ++i) {
		const weapon_t* wpn = gameScript()->GetWeapons() + i;
		if(!game.weaponRestrictions() || game.weaponRestrictions()->isEnabled( wpn->Name ))
			enabledWpns.push_back(i);
	}

	if(enabledWpns.empty()) return -1;
	return randomChoiceFrom(enabledWpns);
}

int& Game::writeTeamScore(int team) {
	if(team < 0) team = 0;
	team %= MAX_TEAMS;
	if((size_t)team >= teamScores.get().size())
		teamScores.write().resize(team + 1);
	return teamScores.write().write(team);
}


bool Game::isLevelDarkMode() {
	return (gameMap()->config() && gameMap()->config()->darkMode) || levelDarkMode;
}


int oldLXStateInt() {
	switch(game.state) {
	case Game::S_Lobby: return 0;
	case Game::S_Preparing: return 1;
	case Game::S_Playing: return 2;
	default: warnings << "oldLXStateInt: bad game state " << game.state << endl;
	}
	return 0;
}

REGISTER_CLASS(Game, ClassId(-1))

