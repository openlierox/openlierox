/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Options
// Created 21/7/02
// Jason Boettcher


#ifndef __OLX_OPTIONS_H__
#define __OLX_OPTIONS_H__

#include <map>
#include <vector>
#include <string>
#include <cassert>
#include "game/Settings.h"
#include "CodeAttributes.h"

// Setup input id's
enum {
	// Movement
	SIN_UP=0,
	SIN_DOWN,
	SIN_LEFT,
	SIN_RIGHT,

	SIN_SHOOT,
	SIN_JUMP,
	SIN_SELWEAP,
	SIN_ROPE,

	SIN_STRAFE,

	SIN_WEAPON1,
	SIN_WEAPON2,
	SIN_WEAPON3,
	SIN_WEAPON4,
	SIN_WEAPON5,

	__SIN_PLY_BOTTOM
};

// General controls
enum {
	SIN_CHAT = 0,
    SIN_SCORE,
	SIN_HEALTH,
	SIN_SETTINGS,
	SIN_SCREENSHOTS,
	SIN_VIEWPORTS,
	SIN_SWITCHMODE,
	SIN_TOGGLETOPBAR,
	SIN_TEAMCHAT,
	SIN_IRCCHAT,
	SIN_CONSOLETOGGLE,

	__SIN_GENERAL_BOTTOM
};


// Network speed types
enum NetworkSpeed {
	NST_MODEM=0,
	NST_ISDN,
	NST_LAN,
	NST_LOCAL			// Hidden speed, only for local games
};

INLINE std::string NetworkSpeedString(NetworkSpeed s) {
	switch(s) {
		case NST_MODEM: return "Modem";
		case NST_ISDN: return "ISDN";
		case NST_LAN: return "DSL/LAN";
		case NST_LOCAL: return "local";
	}
	return "INVALID SPEED";
}


// Screenshot formats
enum {
	FMT_BMP,
	FMT_PNG,
	FMT_JPG,
	FMT_GIF
};

// input controls structure (for local players)
template<short count>
class controls_t {
private:
	std::string ctrl[count];
public:
	std::string& operator[] (const short i) { assert(i >= 0 && (unsigned)i < count ); return ctrl[i]; }
	const std::string& operator[] (const short i) const { assert(i >= 0 && (unsigned)i < count ); return ctrl[i]; }

	short ControlCount() const  { return count; }
	// TODO: add specific functions
};

typedef controls_t<__SIN_PLY_BOTTOM> PlyControls;
typedef controls_t<__SIN_GENERAL_BOTTOM> GeneralControls;


// Network strings
class NetworkTexts {
public:

	static bool Init();
	bool LoadFromDisc();

	std::string sHasLeft ;
	std::string sHasConnected ;
	std::string sHasTimedOut ;

	std::string sHasBeenKicked ;
	std::string sHasBeenKickedReason ;
	std::string sHasBeenBanned ;
	std::string sHasBeenBannedReason ;
	std::string sHasBeenMuted ;
	std::string sHasBeenUnmuted ;
	std::string sIsSpectating;
	std::string sIsPlaying;

	std::string sKickedYou ;
	std::string sKickedYouReason ;
	std::string sBannedYou ;
	std::string sBannedYouReason ;
	std::string sYouTimed ;
	std::string sYouQuit ;

	std::string sKilled ;
	std::string sCommitedSuicide ;
	std::string sFirstBlood ;
	std::string sTeamkill ;
	std::string sHasScored;
	std::string sKilledAFK ;

	std::string sSeekerMessage;
	std::string sHiderMessage;
	std::string sCaughtMessage;
	std::string sHiderVisible;
	std::string sSeekerVisible;
	std::string sVisibleMessage;
	std::string sYouAreHidden;
	std::string sHiddenMessage;

	std::string sPlayerOut ;
	std::string sPlayerHasWon ;
	std::string sTeamOut ;
	std::string sTeamHasWon ;
	std::string sTimeLimit ;

	std::string sWormIsIt ;

	std::string sSpree1 ;
	std::string sSpree2 ;
	std::string sSpree3 ;
	std::string sSpree4 ;
	std::string sSpree5 ;

	std::string sDSpree1 ;
	std::string sDSpree2 ;
	std::string sDSpree3 ;
	std::string sDSpree4 ;
	std::string sDSpree5 ;

	std::string sServerFull ;
	std::string sNoEmptySlots ;
	std::string sWrongProtocol ;
	std::string sBadVerification ;
	std::string sNoIpVerification ;
	std::string sGameInProgress ;
	std::string sYouAreBanned ;
	std::string sBotsNotAllowed ;
	std::string sWantsJoin ;

	std::string sKnownAs;
};

// Taunts - short texts that user can post to chat by pressing Ctrl+key
class Taunts {
public:
	static bool Init();
	bool LoadFromDisc();
	enum { MAX_COUNT = 10 };
	std::string texts[MAX_COUNT];
	controls_t<MAX_COUNT> keyNames;
	int keySyms[MAX_COUNT];
	
	std::string getTauntForKey(int keySym) const;
};


class CGameMode;
extern const std::string DefaultCfgFilename;

// Options structure
class GameOptions {
private:
	GameOptions();

public:
	static bool Init();
	bool LoadFromDisc(const std::string& cfgfilename);
	bool LoadFromDisc() { return LoadFromDisc(cfgFilename); }
	void SaveToDisc(const std::string& cfgfilename);
	void SaveToDisc() { SaveToDisc(cfgFilename); }
	void SaveSectionToDisc(const std::string& section, const std::string& filename);
	
	// Video
	bool	bFullscreen;
	bool	bShowFPS;
	bool	bOpenGL;
	std::string	sResolution;
	int		iColourDepth;

	// Network
	int		iNetworkPort;
	int		iNetworkSpeed;
	int		iMaxUploadBandwidth;
	bool	bCheckBandwidthSanity;
	bool	bUseIpToCountry;	
	std::string	sHttpProxy;
	bool	bAutoSetupHttpProxy;

	bool	bRegServer;
	std::string	sServerName;
	std::string	sWelcomeMessage;
	std::string sServerPassword;	// Password to authenticate (login chatcmd) as admin; this is not for server-login!
	bool	bAllowWantsJoinMsg;
	bool	bWantsJoinBanned;
	bool	bAllowRemoteBots;
	bool	bForceCompatibleConnect;
	std::string	sForceMinVersion;
	int		iMaxPlayers;
	//Chat message check options
	bool bCheckChatMessageLength;
	int iMaxChatMessageLength;
	bool bKickOversizedMsgSenders;
	//Server chat logging
	bool bLogServerChatToMainlog;
	
	bool	bAllowConnectDuringGame; // >=Beta8
	bool	bAllowStrafing;
	bool	bAllowNickChange;
	bool	bServerSideHealth;
	int		iWeaponSelectionMaxTime;	// Auto-kick worms who select their weapons too long
	
	// IRC chat
	bool	bEnableChat;
	bool	bEnableMiniChat; // Mini chat window in Net servers list

	// Audio
	bool	bSoundOn;
	int		iSoundVolume;
	int		iMusicVolume;
	bool	bMusicOn;

	// Game state
	std::string	sNewestVersion;
	bool	bFirstHosting;

	// Controls
	std::vector<PlyControls> sPlayerControls; // sPC[playernr][controlnr]
	GeneralControls	sGeneralControls;

	// Game
	int		iBloodAmount;
	bool	bShadows;
	bool	bParticles;
	bool	bOldSkoolRope;
	bool	bShowHealth;
	bool	bShowNetRates;
	bool	bShowProjectileUsage;
	bool	bColorizeNicks;
	bool	bAutoTyping;
	bool	bAntiAliasing;
	bool	bMouseAiming;
	int		iMouseSensity;
	std::string sTouchscreenControls;	// Tri-state "auto"/"true"/"false". "auto" enables on Android, disables elsewhere.
	std::string sTouchscreenLayout;	// Name of layout file under share/gamedir/touchscreen/<name>.yaml.
	bool	bAntilagMovementPrediction;
	std::string	sLastSelectedPlayer;
	std::string	sLastSelectedPlayer2;
	std::string sTheme;
	bool	bTopBarVisible;
	bool	bDamagePopups;
	bool	bColorizeDamageByWorm;
	float	fCrosshairDistance;
	float	fAimAcceleration;
	float	fAimMaxSpeed;
	float	fAimFriction;
	bool	bAimLikeLX56;
	float	fRopeAdjustSpeed;
	bool	bSetAFK;
	
	//Killing/dying spree thresholds
	int iSpreeThreshold1;
	int iSpreeThreshold2;
	int iSpreeThreshold3;
	int iSpreeThreshold4;
	int iSpreeThreshold5;
	int iDyingSpreeThreshold1;
	int iDyingSpreeThreshold2;
	int iDyingSpreeThreshold3;
	int iDyingSpreeThreshold4;
	int iDyingSpreeThreshold5;
	
    // Advanced
    int     nMaxFPS;
	int		iJpegQuality;
	int		iMaxCachedEntries;		// Amount of entries to cache, including maps, mods, images and sounds.
	bool	bMatchLogging;			// Save screenshot of every game final score
	bool	bRecoverAfterCrash;		// If we should try to recover after segfault etc, or generate coredump and quit
	bool	bCheckForUpdates;		// Check for new development version on sourceforge.net

	// Misc.
	bool    bLogConvos;
	bool	bShowPing;
	int		iScreenshotFormat;
	std::string sDedicatedScript;
	std::string sDedicatedScriptArgs;
	int		iVerbosity;			// the higher the number, the higher the amount of debug messages; 0 is default, at 10 it shows backtraces for all warnings
	bool	bLogTimestamps;  // Show timestamps in console output
	bool	bAdvancedLobby;  // Show advanced game info in join lobby
	bool	bShowCountryFlags;
	int		iRandomTeamForNewWorm; // server will randomly choose a team between 0-iRandomTeamForNewWorm
	std::string cfgFilename;
	bool	doProjectileSimulationInDedicated;
	bool	bAutoFileCacheRefresh;	// when you refocus, it will automatically reload the map/mod and the list and the caches
	bool	bUseMainLockDetector;
	
	// Widget states
	int		iInternetList[7];
	int		iLANList[6];
	int		iFavouritesList[6];
	bool	iGameInfoGroupsShown[GIG_Size];
	int		iInternetSortColumn;
	int		iLANSortColumn;
	int		iFavouritesSortColumn;
	int		iAdvancedLevelLimit;
	bool	bShowModifiedGameSettingsOnly;
	std::string	sLocalPlayGame;
	std::string sSvrListSettingsFilterCfg;
	
	// Last used custom game details
	// Server-side game settings are now in gameSettings (see game/Settings.h)
	FeatureSettingsLayer customSettings;

	std::map<std::string,int> localplayLevels;
	
	// not specified options found in options-file
	std::map< std::string, std::string > additionalOptions;
};


// Option Routines
void	ShutdownOptions();


extern	GameOptions		*tLXOptions;
extern  NetworkTexts  *networkTexts;
extern  Taunts			*taunts;


#endif  //  __OPTIONS_H__
