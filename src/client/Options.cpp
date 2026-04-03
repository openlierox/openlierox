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

#include <ctype.h> // isspace


#include "LieroX.h"
#include "Debug.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "Options.h"
#include "FindFile.h"
#include "ConfigHandler.h"
#include "CScriptableVars.h"
#include "IniReader.h"
#include "Version.h"
#include "Iter.h"
#include "CGameMode.h"
#include "AuxLib.h"
#include "CInput.h"
#include "game/Settings.h"
#include "game/GameMode.h"


GameOptions	*tLXOptions = NULL;
NetworkTexts	*networkTexts = NULL;
Taunts *taunts = NULL;

const std::string DefaultCfgFilename = "cfg/options.cfg";

const std::string    ply_keys[] = {"Up", "Down", "Left", "Right", "Shoot", "Jump", "SelectWeapon", "Rope", "Strafe", "Weapon1", "Weapon2", "Weapon3", "Weapon4", "Weapon5" };
const std::string    ply_def1[] =
#ifdef __APPLE__
	{"up", "down", "left", "right", "lalt", "lmeta", "space", "x", "c", "1", "2", "3", "4", "5" };
#else
	{"up", "down", "left", "right", "lctrl", "lalt", "lshift", "x", "z", "1", "2", "3", "4", "5" };
#endif
const std::string    ply_def2[] = {"kp 8",  "kp 5",    "kp 4",    "kp 6",     "kp +", "kp enter", "kp 0", "kp -", "kp .", "6", "7", "8", "9", "0" };
const std::string    gen_keys[] = {"Chat", "ShowScore", "ShowHealth", "ShowSettings",  "TakeScreenshot",  "ViewportManager", "SwitchMode", "ToggleTopBar", "TeamChat",	"IrcChat", "Console"};
const std::string    gen_def[]  = {"i",    "tab",		"h",		  "space",	       "F12",				"F2",				 "alt+enter", "F8",		   "o",			"F4",	"F3"};

static_assert( sizeof(ply_keys) / sizeof(std::string) == __SIN_PLY_BOTTOM, "ply_keys__sizecheck" );
static_assert( sizeof(ply_def1) / sizeof(std::string) == __SIN_PLY_BOTTOM, "ply_def1__sizecheck" );
static_assert( sizeof(ply_def2) / sizeof(std::string) == __SIN_PLY_BOTTOM, "ply_def2__sizecheck" );
static_assert( sizeof(gen_keys) / sizeof(std::string) == __SIN_GENERAL_BOTTOM, "gen_keys__sizecheck" );
static_assert( sizeof(gen_def) / sizeof(std::string) == __SIN_GENERAL_BOTTOM, "gen_def__sizecheck" );

static const Version defaultMinVersion("OpenLieroX/0.58_rc1");



static void InitSearchPaths() {
	// have to set to find the config at some of the default places
	InitBaseSearchPaths();
	
	std::string value;
	int i = 1;
	while(true) {
		if(!ReadString(DefaultCfgFilename, "FileHandling", "SearchPath" + itoa(i,10), value, ""))
			break;
		
		AddToFileList(&tSearchPaths, value);
		i++;
	}
	
	// add the basesearchpaths to the searchpathlist as they should be saved in the end
	for(searchpathlist::const_iterator p1 = basesearchpaths.begin(); p1 != basesearchpaths.end(); i++,p1++)  {
		AddToFileList(&tSearchPaths, *p1);
	}
	
	// print the searchpaths, this may be very usefull for the user
	notes << "I have now the following searchpaths (in this order):\n";
	for(searchpathlist::const_iterator p2 = tSearchPaths.begin(); p2 != tSearchPaths.end(); p2++) {
		std::string path = *p2;
		ReplaceFileVariables(path);
		notes << "  " << path << "\n";
	}
	notes << " And that's all." << endl;
}

static void InitWidgetStates(GameOptions& opts) {
	// this has to be done explicitly at the moment as scriptablevars doesn't support arrays
	// TODO: add this feature
	
	const int	 def_widths[] = {32,180,70,80,50,150,22};
	
	for (size_t i=0; i<sizeof(opts.iInternetList)/sizeof(int); i++)
		opts.iInternetList[i] = def_widths[i];
	
	for (size_t i=0; i<sizeof(opts.iLANList)/sizeof(int); i++)  {
		opts.iLANList[i] = def_widths[i];
		opts.iFavouritesList[i] = def_widths[i];
	}
	
	for (size_t i=0; i<GIG_Size; i++)
		opts.iGameInfoGroupsShown[i] = true;
	
	// Widget states
	ReadIntArray(opts.cfgFilename, "Widgets","InternetListCols",	&opts.iInternetList[0],7);
	ReadIntArray(opts.cfgFilename, "Widgets","LANListCols",		&opts.iLANList[0],6);
	ReadIntArray(opts.cfgFilename, "Widgets","FavouritesListCols",	&opts.iFavouritesList[0],6);
	ReadArray<bool>(opts.cfgFilename, "Widgets","GameInfoGroupsShown",	&opts.iGameInfoGroupsShown[0], GIG_Size);
	
}




bool GameOptions::Init() {
	if(tLXOptions) {
		warnings << "it seems that the GameOptions are already inited" << endl;
		return true;
	}

	tLXOptions = new GameOptions;
	if(tLXOptions == NULL) {
		errors << "not enough mem for GameOptions" << endl;
		return false;
	}

	// gamesettings depends on GameOptions (tLXOptions), thus here is the right place to init the standard settings layers
	gameSettings.layersInitStandard(true);
	
	CScriptableVars::RegisterVars("GameOptions")
		( tLXOptions->bFullscreen, "Video.Fullscreen", true )
		( tLXOptions->bShowFPS, "Video.ShowFPS", false )
		( tLXOptions->bOpenGL, "Video.OpenGL",
#ifdef __APPLE__
			true )
#else
			false )
#endif
		( tLXOptions->iColourDepth, "Video.ColourDepth", 32 )
		( tLXOptions->sResolution, "Video.Resolution", "" )

		( tLXOptions->iNetworkPort, "Network.Port", (int)LX_PORT )
		( tLXOptions->iNetworkSpeed, "Network.Speed", (int)NST_LAN )
		( tLXOptions->bUseIpToCountry, "Network.UseIpToCountry", true )
		( tLXOptions->iMaxUploadBandwidth, "Network.MaxUploadBandwidth", 50000 )
		( tLXOptions->bCheckBandwidthSanity, "Network.CheckBandwidthSanity", true )
		( tLXOptions->sHttpProxy, "Network.HttpProxy", "" )
		( tLXOptions->bAutoSetupHttpProxy, "Network.AutoSetupHttpProxy", true )

		( tLXOptions->bEnableChat, "Network.EnableChat", true )
		( tLXOptions->bEnableMiniChat, "Network.EnableMiniChat", true )
		( tLXOptions->sServerName, "Network.ServerName", "OpenLieroX Server" )
		( tLXOptions->sWelcomeMessage, "Network.WelcomeMessage", "Welcome to <server>, <player>" )
		( tLXOptions->sServerPassword, "Network.Password", "" )
		( tLXOptions->bRegServer, "Network.RegisterServer", true )
		( tLXOptions->bAllowWantsJoinMsg, "Network.AllowWantsJoinMsg", true )
		( tLXOptions->bWantsJoinBanned, "Network.WantsToJoinFromBanned", true )
		( tLXOptions->bAllowRemoteBots, "Network.AllowRemoteBots", true )
		( tLXOptions->bForceCompatibleConnect, "Network.ForceCompatibleConnect", true, "Force Compatible", "Don't allow incompatible clients to connect" )
		( tLXOptions->sForceMinVersion, "Network.ForceMinVersion", defaultMinVersion.asString(), "Force Min Version", "Minimal version needed to play on this server" )
		( tLXOptions->bCheckChatMessageLength, "Network.CheckChatMessageLength", true)	//Check chat message length
		( tLXOptions->iMaxChatMessageLength, "Network.MaxChatMessageLength", 800)	//Max chat message length, 800 should be more than enough for reasonable messages? NOTE: Includes nickname and ": " after it.
		( tLXOptions->bKickOversizedMsgSenders, "Network.KickOversizedMsgSenders", false)	//Drop clients who send oversized messages NOTE: This should be off by default. It can be used on dedicated servers, but it is better to implement kicking in the control script.
		( tLXOptions->bLogServerChatToMainlog, "Network.LogServerChatToMainlog", true)
		
		( tLXOptions->bFirstHosting, "State.FirstHosting", true )
		( tLXOptions->sNewestVersion, "State.NewestVersion", "" )

		( tLXOptions->bSoundOn, "Audio.Enabled", true )
		( tLXOptions->iSoundVolume, "Audio.Volume", 70 )
		( tLXOptions->bMusicOn, "Audio.MusicEnabled", true )
		( tLXOptions->iMusicVolume, "Audio.MusicVolume", 70 )

		( tLXOptions->iBloodAmount, "Game.Blood", 100 )
		( tLXOptions->bShadows, "Game.Shadows", true )
		( tLXOptions->bParticles, "Game.Particles", true )
		( tLXOptions->bOldSkoolRope, "Game.OldSkoolRope", false )
		( tLXOptions->bShowHealth, "Game.ShowWormHealth", false )
		( tLXOptions->bColorizeNicks, "Game.ColorizeNicks", true )
		( tLXOptions->bAutoTyping, "Game.AutoTyping", false )
		( tLXOptions->sSkinPath, "Game.SkinPath", "" )
		( tLXOptions->bNewSkinnedGUI, "Game.NewSkinnedGUI", false )
		( tLXOptions->sTheme, "Game.Theme", "" )
		( tLXOptions->bAntiAliasing, "Game.Antialiasing", true )
		( tLXOptions->bMouseAiming, "Game.MouseAiming", false ) // TODO: rename to mouse control?
		( tLXOptions->iMouseSensity, "Game.MouseSensity", 200 )
		( tLXOptions->bAntilagMovementPrediction, "Game.AntilagMovementPrediction", true )
		( tLXOptions->sLastSelectedPlayer, "Game.LastSelectedPlayer", "" )
		( tLXOptions->sLastSelectedPlayer2, "Game.LastSelectedPlayer2", "" )
		( tLXOptions->bTopBarVisible, "Game.TopBarVisible", true )
		( tLXOptions->bDamagePopups, "Game.DamagePopups", true )
		( tLXOptions->bColorizeDamageByWorm, "Game.ColorizeDamageByWorm", false )
		( tLXOptions->iRandomTeamForNewWorm, "Game.RandomTeamForNewWorm", 1, "Random team for new worm", "Joining worms will be randomly in a team of [0,value]", GIG_Other, ALT_Advanced, true, 0, 3 )
		( tLXOptions->fCrosshairDistance, "Game.CrosshairDistance", 32.0f, "Crosshair distance", "", GIG_Other, ALT_OnlyViaConfig, true, 5.0f, 100.0f )
		( tLXOptions->fAimAcceleration, "Game.AimAcceleration", /* Gusanos promode default */ 1299.91f, "Aim speed acceleration", "aim speed acceleration - kind of the sensibility of up/down keys for aiming", GIG_Other, ALT_VeryAdvanced, true, 100.0f, 2000.0f )
		( tLXOptions->fAimMaxSpeed, "Game.AimMaxSpeed", /* Gusanos promode default */ 232.996f, "Aim max speed", "maximum possible aim speed for worm", GIG_Other, ALT_VeryAdvanced, true, 20.0f, 1000.0f )
		( tLXOptions->fAimFriction, "Game.AimFriction", /* Gusanos promode default */ 0.0f, "Aim friction", "aim speed friction for worm", GIG_Other, ALT_VeryAdvanced, true, 0.0f, 1.0f )
		( tLXOptions->bAimLikeLX56, "Game.AimLikeLX56", false, "Aim friction like LX56", "aim speed friction behaves like LX56", GIG_Other, ALT_OnlyViaConfig )
		( tLXOptions->fRopeAdjustSpeed, "Game.RopeAdjustSpeed", /* Gusanos default */ 0.5f, "Rope adjust speed", "rope length adjust speed", GIG_Other, ALT_VeryAdvanced, true, 0.1f, 2.0f )
		( tLXOptions->bSetAFK, "Game.SetAFK", true, "set AFK status", "set AFK status on local human worms in game", GIG_Other, ALT_Dev )
		
		// Killing spree thresholds
		( tLXOptions->iSpreeThreshold1, "Game.SpreeThreshold1", 3 )
		( tLXOptions->iSpreeThreshold2, "Game.SpreeThreshold2", 5 )
		( tLXOptions->iSpreeThreshold3, "Game.SpreeThreshold3", 7 )
		( tLXOptions->iSpreeThreshold4, "Game.SpreeThreshold4", 9 )
		( tLXOptions->iSpreeThreshold5, "Game.SpreeThreshold5", 10 )
		// Dying spree thresholds
		( tLXOptions->iDyingSpreeThreshold1, "Game.DyingSpreeThreshold1", 3 )
		( tLXOptions->iDyingSpreeThreshold2, "Game.DyingSpreeThreshold2", 5 )
		( tLXOptions->iDyingSpreeThreshold3, "Game.DyingSpreeThreshold3", 7 )
		( tLXOptions->iDyingSpreeThreshold4, "Game.DyingSpreeThreshold4", 9 )
		( tLXOptions->iDyingSpreeThreshold5, "Game.DyingSpreeThreshold5", 10 )
		
		( tLXOptions->nMaxFPS, "Advanced.MaxFPS", 95 )
		( tLXOptions->iJpegQuality, "Advanced.JpegQuality", 80 )
		( tLXOptions->iMaxCachedEntries, "Advanced.MaxCachedEntries", 300 ) // Should be enough for every mod (we have 2777 .png and .wav files total now) and does not matter anyway with SmartPointer
		( tLXOptions->bMatchLogging, "Advanced.MatchLogging", true )
		( tLXOptions->bRecoverAfterCrash, "Advanced.RecoverAfterCrash",
#ifndef DEDICATED_ONLY
																		true )
#else
																		false )
#endif
		( tLXOptions->bCheckForUpdates, "Advanced.CheckForUpdates", true )

		( tLXOptions->bLogConvos, "Misc.LogConversations", false )
		( tLXOptions->bShowPing, "Misc.ShowPing", true )
		( tLXOptions->bShowNetRates, "Misc.ShowNetRate", false )
		( tLXOptions->bShowProjectileUsage, "Misc.ShowProjectileUsage", false )
		( tLXOptions->iScreenshotFormat, "Misc.ScreenshotFormat", (int)FMT_PNG )
		( tLXOptions->sDedicatedScript, "Misc.DedicatedScript", "dedicated_control" )
		( tLXOptions->sDedicatedScriptArgs, "Misc.DedicatedScriptArgs", "cfg/dedicated_config" )
		( tLXOptions->iVerbosity, "Misc.Verbosity", 0 )	
		( tLXOptions->bLogTimestamps, "Misc.LogTimestamps", false )	
		( tLXOptions->bAdvancedLobby, "Misc.ShowAdvancedLobby", false )
		( tLXOptions->bShowCountryFlags, "Misc.ShowCountryFlags", true )
		( tLXOptions->doProjectileSimulationInDedicated, "Misc.DoProjectileSimulationInDedicated", true )
		( tLXOptions->bAutoFileCacheRefresh, "Misc.AutoFileCacheRefresh", true )
		( tLXOptions->bUseMainLockDetector, "Misc.UseMainLockDetector", true )

		( tLXOptions->iInternetSortColumn, "Widgets.InternetSortColumn", 4 )
		( tLXOptions->iLANSortColumn, "Widgets.LANSortColumn", 4 )
		( tLXOptions->iFavouritesSortColumn, "Widgets.FavouritesSortColumn", 4 )
		( tLXOptions->iAdvancedLevelLimit, "Widgets.AdvancedLevelLimit", 0 )
		( tLXOptions->bShowModifiedGameSettingsOnly, "Widgets.ShowModifiedGameSettingsOnly", false )
		( tLXOptions->sLocalPlayGame, "Widgets.LocalPlayGame", "introduction" )
		( tLXOptions->sSvrListSettingsFilterCfg, "Widgets.SvrListSettingsFilterCfg", "" )
		;

	for( uint i = 0; i < sizeof(ply_keys) / sizeof(ply_keys[0]) ; i ++ )
	{
		CScriptableVars::RegisterVars("GameOptions.Ply1Controls") ( tLXOptions->sPlayerControls[0][i], ply_keys[i], ply_def1[i] );
		CScriptableVars::RegisterVars("GameOptions.Ply2Controls") ( tLXOptions->sPlayerControls[1][i], ply_keys[i], ply_def2[i] );
	}
	for( uint i = 0; i < sizeof(gen_keys) / sizeof(gen_keys[0]) ; i ++ )
	{
		CScriptableVars::RegisterVars("GameOptions.GeneralControls") ( tLXOptions->sGeneralControls[i], gen_keys[i], gen_def[i] );
	}
	
	CScriptableVars::RegisterVars("GameOptions.Server")
	( tLXOptions->iMaxPlayers, "MaxPlayers", 14, "Max players", "Max amount of players allowed on server", GIG_General, ALT_Basic, true, 1, 32 )
	( tLXOptions->bAllowConnectDuringGame, "AllowConnectDuringGame", true, "Connect during game", "Allow new players to connect during game", GIG_Advanced, ALT_Basic )
	( tLXOptions->bAllowNickChange, "AllowNickChange", true, "Allow name change", "Allow players to change name with /setmyname command", GIG_Other, ALT_VeryAdvanced )
	( tLXOptions->bAllowStrafing, "AllowStrafing", true, "Allow strafing", "Allow players to use the Strafe key", GIG_Other, ALT_VeryAdvanced )
	( tLXOptions->bServerSideHealth, "ServerSideHealth", false, "Server sided health", "Health is calculated on server, to prevent cheating", GIG_Other, ALT_OnlyViaConfig )
	( tLXOptions->iWeaponSelectionMaxTime, "WeaponSelectionMaxTime", 120, "Weapon selection max time", "Max time to allow players to select weapons, in seconds", GIG_Weapons, ALT_VeryAdvanced, true, 10, 500 )
	;
	
	for_each_iterator( Feature*, f, ArrayRef(featureArray,featureArrayLen()) ) {
		CScriptableVars::RegisterVars("GameOptions.GameInfo")
		( &gameSettings.wrappers[featureArrayIndex(f->get())], f->get()->name, f->get()->defaultValue, 
			f->get()->humanReadableName, f->get()->description, f->get()->group, f->get()->advancedLevel, f->get()->unsignedValue, f->get()->minValue, f->get()->maxValue );
	}

	// Game is an alias to GameOptions.GameInfo
	for_each_iterator( Feature*, f, ArrayRef(featureArray,featureArrayLen()) ) {
		CScriptableVars::RegisterVars("Game")
		( &gameSettings.wrappers[featureArrayIndex(f->get())], f->get()->name, f->get()->defaultValue,
			f->get()->humanReadableName, f->get()->description, f->get()->group, f->get()->advancedLevel, f->get()->unsignedValue, f->get()->minValue, f->get()->maxValue );
	}
	
	
	// We still use the old ReadKeyword&co functions, they need this.
	// It's save to add same keyword multiple times, so no need to care about restarts.
	AddKeyword("true",true);
	AddKeyword("false",false);
	
	// Load all variables that should be treated specially
	
	// File handling
	// read this first, because perhaps we will have new searchpaths
	InitSearchPaths();
		
	// first set the standards (else the vars would be undefined if not defined in options.cfg)
	for( CScriptableVars::const_iterator it = CScriptableVars::begin();
		it != CScriptableVars::end(); it++ )
	{
		if( strStartsWith(it->first, "GameOptions.") )
		{
			// but not from GameInfo - that is all handled automatically via the settings layers
			// setting it would be bad because we would have all customsettings set
			if( !strStartsWith(it->first, "GameOptions.GameInfo.") )
				it->second.var.setDefault();
		}
	}

	// reset it because we need the CGameMode* ptr working and in some cases, it is not init correctly
	featureArray[FT_GameMode].defaultValue = featureArray[FT_GameMode].unsetValue = ScriptVar_t(GameModeInfo().getRefCopy());
	
	notes << "Reading game options from " << GetFullFileName(tLXOptions->cfgFilename) << endl;
	notes << "Will write game options to " << GetWriteFullFileName(tLXOptions->cfgFilename, true) << endl;
		
	bool ret = tLXOptions->LoadFromDisc();

	/*notes << "Skinnable vars:\n" << CGuiSkin::DumpVars() << endl;
	notes << "Skinnable widgets:\n" << CGuiSkin::DumpWidgets() << endl;*/
	return ret;
}



///////////////////
// Load the options
bool GameOptions::LoadFromDisc(const std::string& cfgfilename)
{
	additionalOptions.clear();

	// TODO: these use arrays which are not handled by scriptablevars
	InitWidgetStates(*this);

	// define parser handler
	struct MyIniReader : public IniReader {
		GameOptions* opts;
		typedef std::list< std::pair<std::string,std::string> > GameInfoData;
		GameInfoData lx56_LastGameFallback;
		GameInfoData gameInfoSection;
		bool haveGameInfo;
		Version savedBy;
		MyIniReader(const std::string& fn, GameOptions* o) : IniReader(fn), opts(o), haveGameInfo(false) {}

		bool OnEntry(const std::string& section, const std::string& propname, const std::string& value) {
			// ConfigFileInfo is additional data about the config file itself - we ignore it atm at this place
			if(stringcaseequal(section, "ConfigFileInfo")) {
				if(stringcaseequal(propname, "SavedBy"))
					savedBy = Version(value);
				return true;
			}
			if(stringcaseequal(section, "GameInfo")) haveGameInfo = true;
			
			if(stringcaseequal(section, "Games_Levels")) {
				opts->localplayLevels[propname] = from_string<int>(value);
				return true;
			}
						
			RegisteredVar* var = CScriptableVars::GetVar("GameOptions." + section + "." + propname);
			if( var !=  NULL ) { // found entry
				if(stringcaseequal(section, "GameInfo"))
					gameInfoSection.push_back(make_pair(propname, value));
				else
					CScriptableVars::SetVarByString(var->var, value);
			} else {
				if( (stringcaseequal(section, "FileHandling") && stringcasefind(propname, "SearchPath") == 0)
				|| stringcaseequal(section, "Widgets") ) {
					// ignore these atm
				} else {
					opts->additionalOptions[section + "." + propname] = value;

					if(stringcaseequal(section, "LastGame"))
						lx56_LastGameFallback.push_back(make_pair(propname, value));
					else
						notes << "the option \"" << section << "." << propname << "\" defined in " << m_filename << " is unknown" << endl;
				}
			}

			return true;
		}
	}
	iniReader(cfgfilename, this);

	// parse the file now
	if( ! iniReader.Parse() ) {
		hints << cfgfilename << " not found, will use standards" << endl;
	}
	else {
		MyIniReader::GameInfoData* gameInfoData = NULL;

		// Fallback: this is old LX56 config file
		if(!iniReader.haveGameInfo && iniReader.lx56_LastGameFallback.size() > 0) {
			notes << "Old LX56 config file" << endl;
			gameInfoData = &iniReader.lx56_LastGameFallback;
		}

		// load custom settings from GameInfo section
		else {
			if(iniReader.savedBy < OLXBetaVersion(0,59,6))
				notes << "config was saved with " << iniReader.savedBy.asHumanString() << ", so we ignore quite a lot of game settings from GameInfo" << endl;
			
			gameInfoData = &iniReader.gameInfoSection;
		}

		for(MyIniReader::GameInfoData::iterator i = gameInfoData->begin(); i != gameInfoData->end(); ++i) {
			const std::string& propname = i->first;
			const std::string& value = i->second;

			RegisteredVar* var = CScriptableVars::GetVar("GameOptions.GameInfo." + propname);
			if( var !=  NULL ) { // found entry
				// Version <0.59beta6 just always save everything.
				// We don't want to have all custom settings set because that would overwrite all other settings in the settings layers.
				// So in that case, we skip all
				if(iniReader.savedBy < OLXBetaVersion(0,59,6)) {
					Feature* f = featureByVar(var->var);
					if(f == NULL) {
						errors << "Options::LoadFromDisc " << cfgfilename << ": var " << propname << " seems invalid" << endl;
						continue;
					}
					const FeatureIndex i = featureArrayIndex(f);
					
					// check for settings we still want to overtake, otherwise ignore
					if(
					   i != FT_Map &&
					   i != FT_Mod &&
					   i != FT_SettingsPreset &&
					   i != FT_Lives &&
					   i != FT_KillLimit &&
					   i != FT_TimeLimit &&
					   featureArray[i].minVersion < OLXBetaVersion(0,59,6)) // all options since 0.59beta6 are also ok
						continue;
				}				
				
				CScriptableVars::SetVarByString(var->var, value);
			} else
				notes << "the gameoption " << propname << " is unknown" << endl;
		}

	}

	initSpecialSearchPathForTheme();
	if(getSpecialSearchPathForTheme()) {
		notes << "Special searchpath for the theme: " << *getSpecialSearchPathForTheme() << endl;
	} else
		notes << "Default theme is used" << endl;


	if(additionalOptions.size() > 0) {
		hints << "Unknown options were found." << endl;
	}

	// Clamp the Jpeg quality
	if (iJpegQuality < 1)
		iJpegQuality = 1;
	if (iJpegQuality > 100)
		iJpegQuality = 100;

	const Version newestVersion(sNewestVersion);
	// check if option file was saved with version where we had LX56 as default min version
	if(newestVersion <= Version("OpenLieroX/0.58_rc1") ||
		(newestVersion >= OLXBetaVersion(0,59,1) && newestVersion <= OLXBetaVersion(0,59,4)))
		// overwrite it to new default
		tLXOptions->sForceMinVersion = MAX(defaultMinVersion, Version(tLXOptions->sForceMinVersion)).asString();
	
	if(tLXOptions->sForceMinVersion == "")
		tLXOptions->sForceMinVersion = defaultMinVersion.asString();

	notes << "DONE loading options" << endl;

	return true;
}

///////////////////
// Save & shutdown the options
void ShutdownOptions()
{
	// just to be sure that we don't have any reference on it anymore
	gameSettings.layersClear();
	
	CScriptableVars::DeRegisterVars("GameOptions");
	if(tLXOptions) {
		delete tLXOptions;
		tLXOptions = NULL;
	}

	if(networkTexts) {
		delete networkTexts;
		networkTexts = NULL;
	}

	if(taunts) {
		delete taunts;
		taunts = NULL;
	}
}


///////////////////
// Save the options
void GameOptions::SaveToDisc(const std::string& cfgfilename)
{
    FILE *fp = OpenGameFile(cfgfilename, "wt");
    if(fp == NULL) {
		errors << "GameOptions::SaveToDisc: cannot open " << cfgfilename << endl;
        return;
	}
	
	// Set the FirstRun to false, we're just quitting
	if(tLXOptions->sNewestVersion == "" || Version(tLXOptions->sNewestVersion) < GetGameVersion())
		tLXOptions->sNewestVersion = GetGameVersion().asString();

	// Hosted a net game? Set the FirstHosting variables to false
	if (tLX->bHosted)  {
		tLXOptions->bFirstHosting = false;
	}

	// TODO: remove this UTF8 information here
	fprintf(fp, "%c%c%c", 0xEF, 0xBB, 0xBF);  // mark that this file is UTF-8 encoded
    fprintf(fp, "# OpenLieroX Options File\n");
    fprintf(fp, "# Note: This file is automatically generated by ");
	// just looks better with version-info and is a good information for the user
	// this should not be used in parsing to change any behaviour
	fprintf(fp, "%s", GetFullGameName());
	fprintf(fp, "\n\n");

	// Save all variables that should be treated specially
	fprintf(fp, "[FileHandling]\n");
	{
		int i=1;
		for(searchpathlist::const_iterator p = tSearchPaths.begin(); p != tSearchPaths.end(); p++, i++)
			fprintf(fp, "SearchPath%i = %s\n", i, p->c_str());
	}
	fprintf(fp,"\n");

	// TODO: same issue as in InitWidgetStates()
	fprintf(fp, "[Widgets]\n");
	fprintf(fp, "InternetListCols = ");
	for (int i=0;i<6;i++)
		fprintf(fp, "%i,",iInternetList[i]);
	fprintf(fp, "%i\n",iInternetList[6]);
	fprintf(fp, "LANListCols = ");
	for (int i=0;i<5;i++)
		fprintf(fp, "%i,",iLANList[i]);
	fprintf(fp, "%i\n",iLANList[5]);
	fprintf(fp, "FavouritesListCols = ");
	for (int i=0;i<5;i++)
		fprintf(fp, "%i,",iFavouritesList[i]);
	fprintf(fp, "%i\n",iFavouritesList[5]);
	fprintf(fp, "GameInfoGroupsShown = ");
	for (int i=0;i<GIG_Size-1;i++)
		fprintf(fp, "%i,",iGameInfoGroupsShown[i]);
	fprintf(fp, "%i\n",iGameInfoGroupsShown[GIG_Size-1]);

	// The following part has the disadvantage that section
	// could occur multiple times in the config file
	// (most often because of additionalOptions).
	// The current parsing of a INI-file allowes this but it
	// looks not so nice.
	// TODO: make it better (but in a clean way, else just leave it!)

	// Save variables registered with CGuiSkin
	std::string currentSection;
	for( CScriptableVars::const_iterator it = CScriptableVars::lower_bound("GameOptions.");
			it != CScriptableVars::end(); it++ )
	{
		if( strStartsWith(it->first, "GameOptions.") )
		{
			if(strStartsWith(it->first, "GameOptions.GameInfo."))
				// skip those - we handle them in a special way
				continue;
			
			size_t dot1 = it->first.find(".");
			size_t dot2 = it->first.find( ".", dot1 + 1 );
			if(dot2 == std::string::npos) {
				errors << "section " << it->first << " is strange" << endl;
				continue;
			}
			std::string section = it->first.substr( dot1 + 1, dot2 - dot1 - 1 );	// Between two dots
			std::string key = it->first.substr( dot2 + 1 );	// After last dot
			if( currentSection != section )
			{
			    fprintf( fp, "\n[%s]\n", section.c_str() );
				currentSection = section;
			}
			fprintf( fp, "%s = %s\n", key.c_str(), it->second.var.toString().c_str() );
		}
		else
			break;
	}

	fprintf(fp, "\n[GameInfo]\n");
	for_each_iterator( Feature*, f, ArrayRef(featureArray,featureArrayLen()) ) {
		if(customSettings.isSet[featureArrayIndex(f->get())])
			fprintf(fp, "%s = %s\n", f->get()->name.c_str(), customSettings[f->get()].toString().c_str());
	}
	
	fprintf(fp, "\n[Games_Levels]\n");
	for(std::map<std::string,int>::iterator i = localplayLevels.begin(); i != localplayLevels.end(); ++i)
		fprintf(fp, "%s = %i\n", i->first.c_str(), i->second);

	
	fprintf(fp, "\n\n# This is for OLX to know what type of config file this is.\n");
	fprintf(fp, "[ConfigFileInfo]\n");
	fprintf(fp, "Type = MainConfig\n");
	fprintf(fp, "SavedBy = %s\n", GetFullGameName());
	fprintf(fp, "Date = %s\n", GetDateTimeText().c_str());
	
	if(additionalOptions.size() > 0)
		fprintf(fp, "\n\n# The following options are unknown in %s\n\n", GetFullGameName());
	
	// save additional options
	// HINT: as this is done seperatly, some sections may be double; though the current parsing and therefore all future parsings handle this correctly
	currentSection = "";
	for( std::map< std::string, std::string > :: iterator it = additionalOptions.begin(); it != additionalOptions.end(); it++ ) {
		size_t dot = it->first.find(".");
		std::string section = it->first.substr( 0, dot ); // before the dot
		std::string key = it->first.substr( dot + 1 ); // after the dot
		if( currentSection != section )
		{
			fprintf( fp, "\n[%s]\n", section.c_str() );
			currentSection = section;
		}
		fprintf( fp, "%s = %s\n", key.c_str(), it->second.c_str() );
	}

	fprintf(fp, "\n\n# End of options\n\n");
	
    fclose(fp);
}

void GameOptions::SaveSectionToDisc(const std::string& presection, const std::string& filename) {
    FILE *fp = OpenGameFile(filename, "wt");
    if(fp == NULL) {
		errors << "GameOptions::SaveSectionToDisc(" << presection << "): cannot open " << filename << endl;
        return;
	}
	
	// TODO: remove this UTF8 information here
	fprintf(fp, "%c%c%c", 0xEF, 0xBB, 0xBF);  // mark that this file is UTF-8 encoded
    fprintf(fp, "# OpenLieroX Section Options File\n");
    fprintf(fp, "# Note: This file is automatically generated by ");
	fprintf(fp, "%s", GetFullGameName());
	fprintf(fp, "\n\n");
	
	// Save variables registered with CGuiSkin
	std::string currentSection;
	for( CScriptableVars::const_iterator it = CScriptableVars::lower_bound(presection + ".");
		it != CScriptableVars::end(); it++ )
	{
		if( strCaseStartsWith(it->first, presection + ".") )
		{
			size_t dot1 = it->first.find(".");
			size_t dot2 = it->first.find( ".", dot1 + 1 );
			if(dot2 == std::string::npos) {
				errors << "section " << it->first << " is strange" << endl;
				continue;
			}
			std::string section = it->first.substr( dot1 + 1, dot2 - dot1 - 1 );	// Between two dots
			std::string key = it->first.substr( dot2 + 1 );	// After last dot
			if( currentSection != section )
			{
			    fprintf( fp, "\n[%s]\n", section.c_str() );
				currentSection = section;
			}
			fprintf( fp, "%s = %s\n", key.c_str(), it->second.var.toString().c_str() );
		}
		else
			break;
	}
	
	fprintf(fp, "\n\n# This is for OLX to know what type of config file this is.\n");
	fprintf(fp, "[ConfigFileInfo]\n");
	fprintf(fp, "Type = SectionConfig\n");
	fprintf(fp, "Section = %s\n", presection.c_str());
	fprintf(fp, "SavedBy = %s\n", GetFullGameName());
	fprintf(fp, "Date = %s\n", GetDateTimeText().c_str());

	fprintf(fp, "\n# End of options\n");	
    fclose(fp);
}



bool NetworkTexts::Init() {
	if(networkTexts) {
		warnings << "networktexts are already inited; ignoring ..." << endl;
		return true;
	}

	networkTexts = new NetworkTexts;
	if(!networkTexts) {
		errors << "not enough mem for networktexts" << endl;
		return false;
	}

	return networkTexts->LoadFromDisc();
}

////////////////////
// Loads the texts used by server
bool NetworkTexts::LoadFromDisc()
{
	notes << "Loading network texts... ";

	// TODO: use the general INI-parser here
	const std::string f = "cfg/network.txt";
	ReadString (f, "NetworkTexts", "HasConnected",    sHasConnected,	"<player> has connected");
	ReadString (f, "NetworkTexts", "HasLeft",	      sHasLeft,			"<player> has left");
	ReadString (f, "NetworkTexts", "HasTimedOut",     sHasTimedOut,		"<player> has timed out");

	ReadString (f, "NetworkTexts", "HasBeenKicked",   sHasBeenKicked,	"<player> has been kicked out");
	ReadString (f, "NetworkTexts", "HasBeenKickedReason",	sHasBeenKickedReason,	"<player> has been kicked out because <reason>");
	ReadString (f, "NetworkTexts", "HasBeenBanned",   sHasBeenBanned,	"<player> has been banned");
	ReadString (f, "NetworkTexts", "HasBeenBannedReason",   sHasBeenBannedReason,	"<player> has been banned because <reason>");
	ReadString (f, "NetworkTexts", "HasBeenMuted",    sHasBeenMuted,	"<player> has been muted");
	ReadString (f, "NetworkTexts", "HasBeenUnmuted",  sHasBeenUnmuted,	"<player> has been unmuted");
	ReadString (f, "NetworkTexts", "IsSpectating",    sIsSpectating,	"<player> will only spectate this round");
	ReadString (f, "NetworkTexts", "IsPlaying",		  sIsPlaying,		"<player> will play this round!");
	ReadString (f, "NetworkTexts", "KickedYou",		  sKickedYou,		"You have been kicked");
	ReadString (f, "NetworkTexts", "KickedYouReason", sKickedYouReason, "You have been kicked because <reason>");
	ReadString (f, "NetworkTexts", "BannedYou",		  sBannedYou,		"You have been banned");
	ReadString (f, "NetworkTexts", "BannedYouReason", sBannedYouReason, "You have been banned because <reason>");
	ReadString (f, "NetworkTexts", "YouQuit",		  sYouQuit,			"You have quit");
	ReadString (f, "NetworkTexts", "YouTimed",		  sYouTimed,		"You timed out");

	ReadString (f, "NetworkTexts", "Killed",	      sKilled,			"<killer> killed <victim> with <weapon>");
	ReadString (f, "NetworkTexts", "CommitedSuicide", sCommitedSuicide,	"<player> commited suicide");
	ReadString (f, "NetworkTexts", "FirstBlood",	  sFirstBlood,		"<player> drew first blood");
	ReadString (f, "NetworkTexts", "TeamKill",		  sTeamkill,		"<player> is an ugly teamkiller");
	ReadString (f, "NetworkTexts", "CTFScore",		  sHasScored,		"<player> has scored");
	ReadString (f, "NetworkTexts", "KilledAFK",	      sKilledAFK,		"<killer> typekilled <victim>");

	ReadString (f, "NetworkTexts", "PlayerOut",		  sPlayerOut,		"<player> is out of the game");
	ReadString (f, "NetworkTexts", "TeamOut",		  sTeamOut,			"The <team> team is out of the game");
	ReadString (f, "NetworkTexts", "PlayerHasWon",	  sPlayerHasWon,	"<player> has won the match");
	ReadString (f, "NetworkTexts", "TeamHasWon",	  sTeamHasWon,		"The <team> team has won the match");
	ReadString (f, "NetworkTexts", "TimeLimit",		  sTimeLimit,		"Timelimit has been reached");

	ReadString (f, "NetworkTexts", "WormIsIt",		  sWormIsIt,		"<player> is IT!");

	ReadString (f, "NetworkTexts", "SeekerMessage",	  sSeekerMessage,	"You are a seeker, you have to find and catch the hiders. You have to catch the hiders before <time> seconds are up.");
	ReadString (f, "NetworkTexts", "HiderMessage",	  sHiderMessage,	"You are a hider, you have to run away from the seekers who are red. You have to hide for <time> seconds.");
	ReadString (f, "NetworkTexts", "CaughtMessage",	  sCaughtMessage,	"<seeker> caught <hider>!");
	ReadString (f, "NetworkTexts", "HiderVisible",	  sHiderVisible,	"You are visible to the seekers, run!");
	ReadString (f, "NetworkTexts", "SeekerVisible",	  sSeekerVisible,	"You are visible to the hiders");
	ReadString (f, "NetworkTexts", "VisibleMessage",  sVisibleMessage,	"<player> is visible!");
	ReadString (f, "NetworkTexts", "YouAreHidden",    sYouAreHidden,	"You are invisible again!");
	ReadString (f, "NetworkTexts", "HiddenMessage",   sHiddenMessage,	"<player> is hiding!");
	

	ReadString (f, "NetworkTexts", "Spree1",		  sSpree1,			"<player> is on a killing spree!");
	ReadString (f, "NetworkTexts", "Spree2",		  sSpree2,			"<player> is on a rampage!");
	ReadString (f, "NetworkTexts", "Spree3",		  sSpree3,			"<player> is dominating!");
	ReadString (f, "NetworkTexts", "Spree4",		  sSpree4,			"<player> is unstoppable!");
	ReadString (f, "NetworkTexts", "Spree5",		  sSpree5,			"<player> is GODLIKE!");

	ReadString (f, "NetworkTexts", "DyingSpree1",	  sDSpree1,			"<player> is on a dying spree!");
	ReadString (f, "NetworkTexts", "DyingSpree2",	  sDSpree2,			"<player> is a target!");
	ReadString (f, "NetworkTexts", "DyingSpree3",	  sDSpree3,			"<player> is a sitting duck!");
	ReadString (f, "NetworkTexts", "DyingSpree4",	  sDSpree4,			"<player> is free kills!");
	ReadString (f, "NetworkTexts", "DyingSpree5",	  sDSpree5,			"<player> is WORTHLESS!");

	ReadString (f, "NetworkTexts", "ServerFull",	  sServerFull,		"Server is full");
	ReadString (f, "NetworkTexts", "NoEmptySlots",	  sNoEmptySlots,	"The server has no emtpy slots");
	ReadString (f, "NetworkTexts", "WrongProtocol",	  sWrongProtocol,	"Wrong protocol version. Server protocol version is <version>.");
	ReadString (f, "NetworkTexts", "BadVerification", sBadVerification,	"Bad connection verification");
	ReadString (f, "NetworkTexts", "NoIpVerification",sNoIpVerification,"No verification for address");
	ReadString (f, "NetworkTexts", "GameInProgress",  sGameInProgress,	"Cannot join, the game is currently in progress");
	ReadString (f, "NetworkTexts", "YouAreBanned",	  sYouAreBanned,	"You are banned on this server");
	ReadString (f, "NetworkTexts", "BotsNotAllowed",  sBotsNotAllowed,	"Sorry, bots are not allowed on this server");
	ReadString (f, "NetworkTexts", "WantsJoin",		  sWantsJoin,		"<player> wants to join the server");

	ReadString (f, "NetworkTexts", "KnownAs",		  sKnownAs,			"<oldname> is now known as <newname>");

	notes << "DONE" << endl;
	return true;
}


GameOptions::GameOptions() : customSettings("custom user settings") {
	// we need to set some initial values for these
	bLogTimestamps = false;
	iVerbosity = 0;
	cfgFilename = DefaultCfgFilename;
	
	// TODO: don't hardcode the size here
	sPlayerControls.resize(2);	// Don't change array size or we'll get segfault when vector memory allocation changes

	// The only place where we call this constructor is in GameOptions::Init().
	// We init all the other variables there.
}

bool Taunts::Init() {
	if(taunts) {
		warnings << "taunts are already inited; ignoring ..." << endl;
		return true;
	}

	taunts = new Taunts;

	return taunts->LoadFromDisc();
}

bool Taunts::LoadFromDisc()
{
	notes << "Loading taunts... ";

	// TODO: use the general INI-parser here
	const std::string cfg = "cfg/taunts.txt";
	
	for( int f=0; f < Taunts::MAX_COUNT; f++ )
	{
		texts[f] = "";
		keyNames[f] = "";
		keySyms[f] = 0;
		ReadString (cfg, "Taunts", std::string("Taunt") + itoa(f), texts[f], "");
		ReadString (cfg, "Taunts", std::string("TauntKey") + itoa(f), keyNames[f], "");
		if( keyNames[f] != "" )
			keySyms[f] = keys_t::keySymFromName(keyNames[f]);
	}

	notes << "DONE" << endl;
	return true;
}

std::string Taunts::getTauntForKey(int keySym) const {
	for( int f=0; f < Taunts::MAX_COUNT; f++ )
		if( keySyms[f] == keySym )
			return texts[f];
	
	return "";
}
