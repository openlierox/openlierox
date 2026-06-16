/*
 *  SinglePlayer.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 15.01.10.
 *  code under LGPL
 *
 */

#include <boost/bind/bind.hpp>
#include "SinglePlayer.h"
#include "Options.h"
#include "ConfigHandler.h"
#include "game/Mod.h"
#include "game/Level.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/Menu.h"
#include "CClient.h"
#include "CServer.h"
#include "ProfileSystem.h"
#include "CWormHuman.h"
#include "OLXCommand.h"
#include "util/macros.h"
#include "game/Settings.h"
#include "game/Game.h"
#include "client/ClientConnectionRequestInfo.h"


SinglePlayerGame singlePlayerGame;


static bool gameLevelExists(const std::string& game, int levelNr) {
	std::string dir;
	if(!ReadString("games/" + game + "/game.cfg", "Level" + itoa(levelNr), "Dir", dir, "") || Trimmed(dir) == "")
		return false;
	else
		return true;
}

static bool gameExists(const std::string& game) {
	return gameLevelExists(game, 1);
}


int SinglePlayerGame::maxSelectableLevelForCurrentGame() {
	int level = 1;
	std::map<std::string,int>::iterator f = tLXOptions->localplayLevels.find(currentGame);
	if(f != tLXOptions->localplayLevels.end())
		// this is the max allowed level
		level = MAX(f->second, 1);
	
	// check the last existing level
	while(level > 1 && !gameLevelExists(currentGame, level))
		--level;
	
	return level;
}

void SinglePlayerGame::setGame(const std::string& game) {
	if(!stringcaseequal(currentGame, game)) {
		currentGame = game;		
		setLevel(maxSelectableLevelForCurrentGame());
	}
}

bool SinglePlayerGame::setLevel(int levelNr) {
	currentGameValid = false;
	image = NULL;
	levelInfo = LevelInfo();
	modInfo = ModInfo();
	description = "";
	
	if(!gameExists(currentGame)) {
		description = "<font color=red>Game <b>" + currentGame + "</b> is invalid</font>";
		errors << "SinglePlayerGame::setLevel: game " << currentGame << " unknown" << endl;
		return false;
	}
	
	currentLevel = levelNr;
	if(currentLevel <= 0) currentLevel = 1;
	
	if(!gameLevelExists(currentGame, currentLevel)) {
		warnings << "SinglePlayerGame::setLevel: game " << currentGame << " doesnt have level " << currentLevel << endl;
		do { currentLevel--; }
		while(currentLevel > 0 && !gameLevelExists(currentGame, currentLevel));
		if(currentLevel <= 0) {
			errors << "SinglePlayerGame::setLevel: woot" << endl;
			return false;
		}
	}
	
	std::string level, mod, img;
	ReadString("games/" + currentGame + "/game.cfg", "Level" + itoa(currentLevel), "Dir", level, ""); TrimSpaces(level);
	ReadString("games/" + currentGame + "/game.cfg", "Level" + itoa(currentLevel), "Mod", mod, "Classic"); TrimSpaces(mod);
	ReadString("games/" + currentGame + "/game.cfg", "Level" + itoa(currentLevel), "Image", img, ""); TrimSpaces(img);
	ReadString("games/" + currentGame + "/game.cfg", "Level" + itoa(currentLevel), "Desc", description, ""); TrimSpaces(description);
	
	level = "../games/" + currentGame + "/" + level; // it's relative to levels
	levelInfo = infoForLevel(level);
	if(!levelInfo.valid) {
		description = "<font color=red>Problem while loading level " + level + "</font>";
		errors << "SinglePlayerGame::setLevel: cannot find map " << level << " for game " << currentGame << ":" << currentLevel << endl;
		return false;
	}
	levelInfo.path = level; // to force this uncommon filename
	
	modInfo = infoForMod(mod);
	if(!modInfo.valid) {
		description = "<font color=red>Problem while loading mod " + mod + "</font>";
		errors << "SinglePlayerGame::setLevel: cannot find mod " << mod << " for game " << currentGame << ":" << currentLevel << endl;
		return false;
	}
	
	if(description == "")
		description = "Undescribed level <b>" + itoa(currentLevel) + "</b> of game <b>" + currentGame + "</b>";
	
	image = LoadGameImage("games/" + currentGame + "/" + img);
	if(image.get() == NULL) {
		warnings << "SinglePlayerGame::setLevel: cannot load preview image '" << img << "' for game " << currentGame << ":" << currentLevel << endl;
		image = gfxCreateSurfaceAlpha(200, 50);
		DrawCross(image.get(), 0, 0, 200, 50, Color(255,0,0));
	}

	currentGameValid = true;
	return true;
}

static bool addPlayerToClient() {
	// this is the current way to tell CClient to create a local worm later on with that profile
	// that is done in CClientNetEngine::ParseConnected or updateAddedWorms
	cClient->connectInfo = new ClientConnectionRequestInfo;
	cClient->connectInfo->worms.push_back(MainHumanProfile());	
	return true;
}

struct SinglePlayerSettingsScope : FeatureSettingsLayer {
	Settings::Layers oldLayers;
	
	SinglePlayerSettingsScope() : FeatureSettingsLayer("Single player game settings") {
		oldLayers.swap(gameSettings.layers);
		gameSettings.layersInitStandard(false);
		gameSettings.layers.push_back(this);
	}
	~SinglePlayerSettingsScope() { oldLayers.swap(gameSettings.layers); }
};

static SmartPointer<SinglePlayerSettingsScope> singlePlayerSettings;

static void SinglePlayer_CleanupAfterGameloopEnd() {
	singlePlayerSettings = NULL;
}

bool SinglePlayerGame::startGame() {
	levelSucceeded = false;

	if(!currentGameValid) {
		errors << "SinglePlayerGame::startGame: cannot start game: current game/level is invalid" << endl;
		return false;
	}
	
	if(bDedicated) {
		errors << "SinglePlayerGame::startGame: cannot do that in dedicated mode" << endl;
		// actually, we could but I really dont see a reason why we would want
		return false;
	}
	
	if(singlePlayerSettings.get()) singlePlayerSettings = NULL; // just to be sure
	singlePlayerSettings = new SinglePlayerSettingsScope();
		
	if(! cClient->Initialize() )
	{
		errors << "Could not initialize client" << endl;
		return false;
	}

	if(!addPlayerToClient()) {
		errors << "SinglePlayerGame::startGame: didn't found human worm" << endl;
		return false;
	}
	
	if(!cServer->StartServer()) {
		errors << "Could not start server" << endl;
		return false;
	}
	
	// standardGameMode is the lower level game mode - can be set by custom config file
	standardGameMode = NULL;
	
	// don't have any wpn restrictions
	cServer->setWeaponRestFile("");

	gameSettings.overwrite[FT_Map].as<LevelInfo>()->path = levelInfo.path;
	gameSettings.overwrite[FT_Map].as<LevelInfo>()->name = levelInfo.name;
	gameSettings.overwrite[FT_Mod].as<ModInfo>()->path = modInfo.path;
	gameSettings.overwrite[FT_Mod].as<ModInfo>()->name = modInfo.name;

	gameSettings.overwrite[FT_Lives] = -2;
	gameSettings.overwrite[FT_KillLimit] = -1;
	gameSettings.overwrite[FT_TimeLimit] = -1.0f;
	
	gameSettings.overwrite[FT_GameMode].as<GameModeInfo>()->mode = this;
	
	{
		std::string extraConfig;
		ReadString("games/" + currentGame + "/game.cfg", "Level" + itoa(currentLevel), "Config", extraConfig, ""); TrimSpaces(extraConfig);
		if(extraConfig != "") {
			notes << "SinglePlayerGame: config: " << extraConfig << endl;
			singlePlayerSettings->loadFromConfig("games/" + currentGame + "/" + extraConfig, false);
		}
	}
	
	{
		std::string extraCmdStr;
		std::vector<std::string> extraCmds;
		ReadString("games/" + currentGame + "/game.cfg", "Level" + itoa(currentLevel), "Exec", extraCmdStr, ""); TrimSpaces(extraCmdStr);
		extraCmds = explode(extraCmdStr, ";");
		foreach(c, extraCmds) {
			notes << "SinglePlayerGame: exec: " << *c << endl;
			game.prepareCallbacks.connect(boost::bind(Execute, &stdoutCLI(), *c));
		}
	}
	
	// this can happen if the config has overwritten it
	if(gameSettings[FT_GameMode].as<GameModeInfo>()->mode != this) {
		// we set the fallback gamemode
		standardGameMode = gameSettings[FT_GameMode].as<GameModeInfo>()->mode;
		gameSettings.overwrite[FT_GameMode].as<GameModeInfo>()->mode = this;
		notes << "singlePlayer: special game mode " << standardGameMode->Name() << endl;
	}
	
	game.cleanupCallbacks.connect(boost::bind(&SinglePlayer_CleanupAfterGameloopEnd));
	
	return true;
}

void SinglePlayerGame::setLevelSucceeded() {
	if(levelSucceeded) return;
	
	notes << "SinglePlayerGame: level was succeeded" << endl;
	levelSucceeded = true;
	game.gameOver = true;
	tLXOptions->localplayLevels[currentGame] = MAX((int)tLXOptions->localplayLevels[currentGame], (int)currentLevel + 1);
	
	if(gameLevelExists(currentGame, currentLevel + 1))
		setLevel(currentLevel + 1);
}

void SinglePlayerGame::Simulate() {
	if(standardGameMode)
		standardGameMode->Simulate();
	
	if(levelSucceeded)
		cServer->RecheckGame();
}

bool SinglePlayerGame::CheckGameOver() {
	if(standardGameMode && standardGameMode->CheckGameOver()) {
		notes << "SinglePlayer game gameOver, winner is " << standardGameMode->Winner() << ", winner team is " << standardGameMode->WinnerTeam() << endl;
		CWorm* w = game.firstLocalHumanWorm();
		if(w && standardGameMode->Winner() == w->getID())
			setLevelSucceeded();
		else if(w && standardGameMode->isTeamGame() && standardGameMode->WinnerTeam() == w->getTeam())
			setLevelSucceeded();
		
		return true;
	}
	
	return levelSucceeded;
}

int SinglePlayerGame::Winner() {
	if(!levelSucceeded) {
		if(standardGameMode) return standardGameMode->Winner();
		return -1;
	}
	
	CWorm* w = game.firstLocalHumanWorm();
	if(w) return w->getID();
	
	return -1;
}

void SinglePlayerGame::GameOver() {
	if(standardGameMode) standardGameMode->GameOver();
}

