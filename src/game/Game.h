/*
 *  Game.h
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 09.12.09.
 *  code under LGPL
 *
 */

#ifndef __OLX_GAME_H__
#define __OLX_GAME_H__

#include <boost/signals2.hpp>
#include <boost/function.hpp>
#include <vector>
#include <list>
#include <map>
#include "SmartPointer.h"
#include "olx-types.h"
#include "gusanos/object_grid.h"
#include "util/Result.h"
#include "Iter.h"
#include "util/BaseObject.h"
#include "Attr.h"
#include "util/Result.h"
#include "util/List.h"

class CWormHumanInputHandler;
class CWormInputHandler;
class CWorm;
class CMap;
class CGameScript;
class CGameMode;
class CWpnRest;
class CServerConnection;
struct Version;
struct profile_t;
struct GameState;
struct GameStateUpdates;

class Game : public BaseObject {
public:
	Game();

	void init();
	void startServer(bool localGame);
	void startClient();
	void startGame();
	void restartGame();
	void gotoLobby(const std::string& reason);
	void stop();

	void frame();
		
	void onPrepareWorm(CWorm* w);
	void onUnprepareWorm(CWorm* w);
	void onRemoveWorm(CWorm* w);
	void onNewPlayer(CWormInputHandler*);
	void onNewPlayer_Lua(CWormInputHandler*);
	void onRemovePlayer(CWormInputHandler*);
	void onNewHumanPlayer(CWormHumanInputHandler*);
	void onNewHumanPlayer_Lua(CWormHumanInputHandler*);
	static void onSettingsUpdate(BaseObject*,const AttrDesc*,ScriptVar_t);

	bool isServer() { return m_isServer; }
	bool isClient() { return !isServer(); }
	bool isLocalGame() { return m_isLocalGame; }
	// Whether this game should render in widescreen (the full screen width)
	// rather than the fair 640-wide view. Single decision point: currently
	// only local games, but this may later also cover network games where
	// every player uses widescreen. See doc/dev/WIDESCREEN.md.
	bool useWideScreen() { return isLocalGame(); }
	bool isTeamPlay();
	bool isGamePaused();
	bool shouldDoPhysicsFrame();
	
	bool needToCreateOwnWormInputHandlers();
	bool needProxyWormInputHandler();
	bool needManualClientSideStateManagement();
	
	enum State {
		S_Quit = 0,
		S_Inactive,
		S_Connecting,
		S_Lobby,
		S_Preparing, // game loaded but no simulation. we might wait for wpn selection or so. it doesn't imply wpn selection though, CWorm::bWeaponsReady says if a specific worm is doing wpn selection or not.
		S_Playing
	};
	static std::string StateAsStr(int s) {
		switch(s) {
		case S_Quit: return "Quit";
		case S_Inactive: return "Inactive";
		case S_Connecting: return "Connecting";
		case S_Lobby: return "Lobby";
		case S_Preparing: return "Preparing";
		case S_Playing: return "Playing";
		}
		return "INVALID STATE";
	}

	ATTR(Game, int, state, 1, { defaultValue = (int)S_Inactive; onUpdate = Game::onStateUpdate; })

	static const int FixedFPS = 100;
	static const uint64_t FixedFrameTime = 1000 / FixedFPS;

	ATTR(Game, uint64_t, serverFrame, 2, {}) // always 100FPS
	ATTR(Game, bool, gameOver, 3, { onUpdate = Game::onGameOverUpdate; })
	ATTR(Game, uint64_t, gameOverFrame, 4, {})

	TimeDiff serverTime() { return TimeDiff(serverFrame * 10); }
	TimeDiff gameOverTime() {
		if(!gameOver) return TimeDiff(0);
		if(gameOverFrame > serverFrame) return TimeDiff(0);
		return TimeDiff((serverFrame - gameOverFrame) * 10);
	}
	AbsTime simulationAbsTime() const { return simulationTime; }

	bool hasHighSimulationDelay();
	bool hasSeriousHighSimulationDelay();

	ATTR(Game, bool, bServerChoosesWeapons, 5, {}) // the clients will not get the weapon selection screen and the server sets it; if true, only >=Beta7 is supported

	ATTR(Game, List<int>, teamScores, 10, {})
	int& writeTeamScore(int team);

	ATTR(Game, int, iMatchWinner, 11, {})
	ATTR(Game, int, iMatchWinnerTeam, 12, {})

	ATTR(Game, std::string, hudPermanentText, 20, {}) // drawn always on top, only reset at gamestart
	ATTR(Game, bool, levelDarkMode, 30, { defaultValue = false; })
	ATTR(Game, int, darkMode_wormLightRadius, 31, { defaultValue = 200; })

	bool isLevelDarkMode();

	std::vector<CWormHumanInputHandler*> localPlayers;
	std::vector<CWormInputHandler*> players;
	
	Grid objects;
	SmartPointer<GameStateUpdates> gameStateUpdates;

	Iterator<CWorm*>::Ref worms();
	Iterator<CWorm*>::Ref localWorms();
	Iterator<CWorm*>::Ref aliveWorms();
	Iterator<CWorm*>::Ref wormsOfClient(const CServerConnection* cl);
	CWorm* wormById(int wormId, bool assertExisting = true);
	CWorm* firstLocalHumanWorm();
	CWorm* findWormByName(const std::string& name);
	CWorm* createNewWorm(int wormId, bool local, const SmartPointer<profile_t>& profile, const Version& clientVersion);
	int getNewUniqueWormId();
	void removeWorm(CWorm* w);
	void resetWorms();
	
	template<typename T>
	T ifWorm(int wormId, boost::function<T (CWorm*)> f, T fallback = T()) {
		CWorm* w = wormById(wormId, false);
		if(w) return f(w);
		return fallback;
	}
	
	std::string wormName(int wormId);
	
	CMap* gameMap();
	CGameScript* gameScript();
	CGameMode* gameMode();
	CWpnRest* weaponRestrictions();
	
	// they will be called in prepareGameloop and cleaned up after that
	boost::signals2::signal<void()> prepareCallbacks;
	// they will be called in cleanupAfterGameloopEnd and the slot will be cleaned after that
	boost::signals2::signal<void()> cleanupCallbacks;
	
	Result loadMap();
	Result loadMod();
	Result loadWeaponRestrictions();
	
	bool		isMapReady() const;	
	int32_t		getRandomEnabledWpn();


	bool allowedToSleepForEvent();

private:
	static void onStateUpdate(BaseObject*,const AttrDesc*,ScriptVar_t);
	static void onGameOverUpdate(BaseObject*,const AttrDesc*,ScriptVar_t);
	void reset();
	void frameInner();
	void prepareMenu();
	Result prepareGameloop();
	void cleanupAfterGameloopEnd();

	bool m_isServer;
	bool m_isLocalGame;

	AbsTime menuStartTime;
	uint64_t menuFrame;
	AbsTime oldtime;
	AbsTime simulationTime;
	SmartPointer<CMap> m_gameMap;
	SmartPointer<CGameScript> m_gameMod;
	SmartPointer<CGameMode> m_gameMode;
	SmartPointer<CWpnRest> m_wpnRest;
	std::map<int,CWorm*> m_worms; // ID -> worm
};

extern Game game;

int oldLXStateInt();

#endif
