/*
 *  FeatureList.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 22.12.08.
 *  code under LGPL
 *
 */


#include "FeatureList.h"
#include "Version.h"
#include "CServer.h"
#include "game/Settings.h"
#include "game/Level.h"
#include "game/Mod.h"
#include "game/SettingsPreset.h"
#include "game/GameMode.h"
#include "Consts.h"
#include "Debug.h"


// WARNING: Keep this always synchronised with FeatureIndex!
// Legend:	Name in options,		Human-readable-name,			Long description,	
//			Unset,	Default,		Min client Version,	Group,	advancedlevel,		[Min,]	[Max,]	[server-side only] [optional for client] [is value unsigned] (Min and Max are only for Int and Float)
// Old clients are kicked if feature version is greater that client version, no matter if feature is server-sided or safe to ignore
// Min/Max is only for GUI and we aren't really strict about it. All other values not in that range should also be handled correctly!

Feature featureArray[] = {

Feature( 1, "Lives", "Lives", "Lives",-2,-2, Version(), GIG_General, ALT_Basic, -1, 150, true, false/*not exactly sure*/, true ),
Feature( 2, "KillLimit", "Max kills", "Game ends when a player reaches the specified number of kills", 15,15, Version(), GIG_General, ALT_Basic, -1, 150, true, true, true ),
Feature( 3, "TimeLimit", "Time limit", "Time limit, in minutes", 6.0f,6.0f, Version(), GIG_General, ALT_Basic, -0.15f, 20.0f, true,true, true ),
Feature( 4, "TagLimit", "Tag limit", "Tag limit, for Tag game mode. It's the time how long a player must be tagged until the game ends", 5.0f,5.0f, Version(), GIG_Tag, ALT_Basic,1.0f, 150.0f,true,true, true ),
Feature( 5, "LoadingTime", "Loading time", "Loading time of weapons, in percent",100,100, Version(), GIG_General, ALT_Basic, 0, 500, false,false, true),

Feature( 6, "LevelName", "Map", "Map", LevelInfo(), LevelInfo::ByPath("Dirt Level.lxl"), Version(), GIG_General, ALT_Basic, false, false ),
Feature( 7, "GameType", "Game mode", "Game mode is the type of game you want to play (DM, TDM, CTF, etc.)", GameModeInfo(), GameModeInfo(), Version(), GIG_General, ALT_Basic, false, false ),
Feature( 8, "ModName", "Mod", "Mod", ModInfo(), ModInfo::ByPath("Classic"), Version(), GIG_General, ALT_Basic, false, false ),
Feature( 9, "Settings", "Settings", "Game settings", GameSettingsPresetInfo(), GameSettingsPresetInfo::Default(), Version(), GIG_General, ALT_Basic, true, true ),
Feature( 10, "WeaponRestrictionsFile", "Weapon restrictions", "Weapon restrictions set which defines which weapons are allowed/banned/bonus", "", "Standard 100lt", Version(), GIG_Weapons, ALT_OnlyViaConfig, true,true ),
Feature( 11, "ForceRandomWeapons", "Force random weapons", "Force all players to select random weapons. You might also want to disable 'AllowWeaponsChange'.",false,false,Version(), GIG_Weapons, ALT_Basic, true,true ),
Feature( 12, "SameWeaponsAsHostWorm", "Same weapons as host worm", "Force all players to select the same weapons as host worm. You might also want to disable 'AllowWeaponsChange'.",false,false,Version(), GIG_Weapons, ALT_Advanced, true,true ),

Feature( 20, "LX56PhysicsFPS", "LX56 physics FPS", "Simulation frames per second for LX56 physics", 84, 84, OLXBetaVersion(0,59,9), GIG_Advanced, ALT_VeryAdvanced, 1, 200, false, true /* we do a custom check for compatibility */ ),
Feature( 21, "ForceSameLX56PhysicsFPS", "Force same LX56 physics FPS", "If enabled (default), the physics simulation will be everywhere the same by using the same FPS. If disabled, it can be different everywhere, like it was in original LX56.", true, true, OLXBetaVersion(0,59,9), GIG_Advanced, ALT_VeryAdvanced, false, true ),
Feature( 22, "NinjaropePrecision", "Ninjarope precision", "Defines the accuracy of the ninja rope simulation. 0 is like LX56 (no extra precision, just FPS dependent), 1 is like modern OLX. The higher the value, the higher the accuracy.", 1.0f, 1.0f, OLXBetaVersion(0,59,9), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 10.0f, false, true /* we do a custom check for compatibility */ ),
Feature( 23, "ForceLX56Aim", "Force LX56 aim", "Force the aim speed/acceleration to be everywhere like LX56", false, false, OLXBetaVersion(0,59,9), GIG_Advanced, ALT_VeryAdvanced, false, true /* we do a custom check for compatibility */ ),

Feature( 30, "Bonuses", "Bonuses", "Bonuses enabled",false, false, Version(), GIG_Bonus, ALT_Basic, true,true ),
Feature( 31, "BonusNames", "Show Bonus names", "Show bonus name above its image",true,true, Version(), GIG_Bonus, ALT_VeryAdvanced, false,false ),
Feature( 32, "BonusFrequency", "Bonus spawn time", "How often a new bonus will be spawned (every N seconds)",30.0f,30.0f,Version(), GIG_Bonus, ALT_Advanced, 1.0f, 150.0f, true,true ),
Feature( 33, "BonusLife", "Bonus life time", "Bonus life time, in seconds",60.0f,60.0f,Version(), GIG_Bonus, ALT_VeryAdvanced, 1.0f, 150.0f,true,true,true),
Feature( 34, "BonusHealthToWeaponChance", "Bonus weapon chance", "Chance of spawning a weapon bonus instead of a health bonus",0.5f,0.5f,Version(), GIG_Bonus, ALT_Advanced, 0.0f, 1.0f, true,true,true ),

Feature( 40, "RespawnTime", "Respawn time", "Player respawn time, in seconds",2.5f,2.5f,Version(), GIG_Advanced, ALT_Advanced, 0.0f, 20.0f, true,true, true ),
Feature( 41, "MaxRespawnTime", "Max respawn time", "Max player respawn time, in seconds. If enabled (>=0) and if both this time and RespawnTime went by, the worm is automatically spawned.",0.f,0.f,Version(), GIG_Advanced, ALT_Advanced, -1.0f, 20.0f, true,true ),
Feature( 42, "RespawnGroupTeams", "Group teams", "Respawn player closer to its team, and farther from enemy",true,true,Version(), GIG_Advanced, ALT_Advanced, true,true ),
Feature( 43, "EmptyWeaponsOnRespawn", "Empty weapons on respawn", "Your weapon ammo is emptied when you respawn",false,false,Version(), GIG_Weapons, ALT_VeryAdvanced, true,true ),

/*
 Note that for these LX56 mod specific settings (which were earlier in CGameScript),
 we set minVersion to LX56 and do some special version check in GameServer::isVersionCompatible.
 See there for further details.
 */
// most of these are moved from CGameScript::Worm
Feature( 50, "WormGroundSpeed", "Worm ground speed", "Worm ground speed", 8.0f, 8.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 100.0f, false, false),
Feature( 51, "WormAirSpeed", "Worm air speed", "Worm air speed", 4.0f, 4.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 30.0f, false, false),
Feature( 52, "WormAirFriction", "Worm air friction", "Worm air friction", 0.0f, 0.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 10.0f, false, false),
Feature( 53, "WormGravity", "Worm gravity", "Worm gravity", 50.0f, 50.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, -200.0f, 1000.0f, false, false),
Feature( 54, "WormJumpForce", "Worm jump force", "Worm jump force", -75.0f, -75.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, -500.0f, 100.0f, false, false),

// these are moved from Gusanos
Feature(55, "WormSimpleFriction", "Worm simple friction", "Worm simple friction (atm only used for Gusanos engine)", 0.0, 0.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 1.0f, false, false, true),
Feature(56, "WormAcceleration", "Worm acceleration", "Worm acceleration; Gusanos only", 396.694f, 396.694f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 1000.0f, false, false),
Feature(57, "WormAirAccelerationFactor", "WormAirAccelerationFactor", "Worm air acc factor; Gus only", 0.7f, 0.7f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 2.0f, false, false),
Feature(58, "WormBounceQuotient", "WormBounceQuotient", "WormBounceQuotient; Gus only", 0.333f, 0.333f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 1.0f, false, false),
Feature(59, "WormBounceLimit", "WormBounceLimit", "WormBounceLimit; Gus only", 0.5f, 0.5f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 10.0f, false, false),
Feature(60, "WormWallHugging", "WormWallHugging", "WormWallHugging; Gus only", false, false, Version(), GIG_Advanced, ALT_VeryAdvanced, false, false),
Feature(61, "WormWeaponHeight", "WormWeaponHeight", "WormWeaponHeight; Gus only", 5, 5, Version(), GIG_Advanced, ALT_VeryAdvanced, 0, 20, false, false),
Feature(62, "WormHeight", "WormHeight", "WormHeight; Gus only", 9, 9, Version(), GIG_Advanced, ALT_VeryAdvanced, 0, 20, false, false),
Feature(63, "WormWidth", "WormWidth", "WormWidth; Gus only", 3, 3, Version(), GIG_Advanced, ALT_VeryAdvanced, 0, 20, false, false),
Feature(64, "WormMaxClimb", "WormMaxClimb", "WormMaxClimb; Gus only", 4, 4, Version(), GIG_Advanced, ALT_VeryAdvanced, 0, 20, false, false),
Feature(65, "WormBoxRadius", "WormBoxRadius", "WormBoxRadius; Gus only", 2.0f, 2.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 10.0f, false, false),
Feature(66, "WormBoxTop", "WormBoxTop", "WormBoxTop; Gus only", 3.0f, 3.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 10.0f, false, false),
Feature(67, "WormBoxBottom", "WormBoxBottom", "WormBoxBottom; Gus only", 4.0f, 4.0f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 10.0f, false, false),

// most of these are moved from CGameScript::Rope*
Feature(70, "RopeMaxLength", "Rope max length", "Rope max length", 300, 300, Version(), GIG_Advanced, ALT_VeryAdvanced, 0, 2000, false, false, true),
Feature(71, "RopeRestLength", "Rope rest length", "Rope rest length", 30, 30, Version(), GIG_Advanced, ALT_VeryAdvanced, 0, 200, false, false, true),
Feature(72, "RopeStrength", "Rope strength", "Rope strength", 3.5f, 3.5f, Version(), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 10.0f, false, false),
Feature(73, "RopeSpeed", "Rope speed", "Rope speed", 250.0f, 250.f, OLXBetaVersion(0,59,6), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 1000.0f, false, false),

Feature(74, "RopeAddParentSpeed", "Rope add parent speed", "worm speed is added to rope speed when you shoot the rope", false, false, OLXBetaVersion(0,59,9), GIG_Advanced, ALT_VeryAdvanced, false, false),
Feature(75, "RopeGravity", "Rope gravity", "rope gravity when shooting", 100.0f, 100.0f, OLXBetaVersion(0,59,9), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 200.0f, false, false),
Feature(76, "RopeFallingGravity", "Rope falling gravity", "rope gravity after rope has get its max length and when it is falling", 150.0f, 150.0f, OLXBetaVersion(0,59,9), GIG_Advanced, ALT_VeryAdvanced, 0.0f, 200.0f, false, false),
Feature(77, "RopeCanAttachWorm", "Rope can attach worm", "when false, you cannot attach worms with your rope", true, true, OLXBetaVersion(0,59,10), GIG_Advanced, ALT_VeryAdvanced, false, false),
	
	Feature(80, "GameSpeed", 			"Game-speed multiplicator", 	"Game simulation speed is multiplicated by the given value.",
			1.0f, 	1.0f,			OLXBetaVersion(0,57,7), 	GIG_Advanced, ALT_Advanced,		0.1f, 	10.0f ),
	Feature(81, "GameSpeedOnlyForProjs", "Speed multiplier only for projs",	"Game-speed multiplicator applies only for projectiles and weapons, everything else will be normal speed",
			false, false,			OLXBetaVersion(0,58,1),	GIG_Advanced,	ALT_Advanced,				false),
	Feature(82, "ScreenShaking",		"Screen shaking", 		"Screen shaking when something explodes",
			true, 	false, 			OLXBetaVersion(0,58,1),	GIG_Other, 	ALT_VeryAdvanced,				false,	true ),
	Feature(83, "FullAimAngle",			"Full aim angle", 		"Enables full aim angle, i.e. also allows to aim straight down",
			false, 	false, 			OLXRcVersion(0,58,3),	GIG_Other, 	ALT_VeryAdvanced,				false,	true ),
	Feature(84, "MiniMap",				"Mini map", 		"Show mini map",
			true, 	true, 			OLXBetaVersion(0,58,1),	GIG_Other, 	ALT_Advanced,					false,	false ),

	Feature(90, "SuicideDecreasesScore", "Suicide decreases score", "The score descreases after a suicide.",
			false, 	false, 			Version(), 			GIG_Score, 	ALT_Advanced,					false,	true ),
	Feature(91, "TeamkillDecreasesScore", "Teamkill decreases score", "The score descreases after a teamkill.",
			false, 	false, 			Version(), 			GIG_Score, 	ALT_Advanced,					false,	true ),
	Feature(92, "DeathDecreasesScore", "Death decreases score", "The score decreases after each death by this factor",
			0.0f, 	0.0f, 			Version(), 			GIG_Score, 	ALT_Advanced,	0.0f,	5.0f,	false,	true ),
	Feature(93, "CountTeamkills", 		"Count teamkills", 				"When killing player from your team increase your score",
			false, 	false, 			Version(), 			GIG_Score, 	ALT_VeryAdvanced,				false,	true ),
	Feature(94, "AllowNegativeScore",	"Allow negative score", 		"Allow negative score, when this option is not selected death/suicide/teamkill are not counted if score is zero",
			false, 	false, 			Version(), 			GIG_Score, 	ALT_VeryAdvanced,				false,	true ),

	Feature(100, "TeamInjure", 			"Damage team members", 			"If disabled, your bullets and projectiles don't damage other team members. It's like friendlyfire in other games.",
			true, 	true, 			OLXBetaVersion(0,58,1), 	GIG_Weapons, ALT_Advanced ),
	Feature(101, "TeamHit", 				"Hit team members", 			"If disabled, your bullets and projectiles will fly through your team members.",
			true, 	true, 			OLXBetaVersion(0,58,1), 	GIG_Weapons, ALT_Advanced ),
	Feature(102, "SelfInjure", 			"Damage yourself", 				"If disabled, your bullets and projectiles don't damage you.",
			true, 	true, 			OLXBetaVersion(0,58,1), 	GIG_Weapons, ALT_Advanced ),
	Feature(103, "SelfHit", 				"Hit yourself", 				"If disabled, your bullets and projectiles will fly through yourself.",
			true, 	true, 			OLXBetaVersion(0,58,1), 	GIG_Weapons, ALT_Advanced ),

	Feature(110, "AllowEmptyGames", 		"Allow empty games", 			"If enabled, games with one or zero worms will not quit. This is only possible if you have infinite lives set and also only for network games.",
			false, 	false, 			Version(), 			GIG_Other, 	ALT_Advanced,					true,	true ),

	Feature(121, "HS_HideTime", 			"Hiding time", 					"AbsTime at the start of the game for hiders to hide",
			20.0f, 	20.0f, 			Version(), 			GIG_HideAndSeek, ALT_Basic,	0.0f,	100.0f,	true,	true ),
	Feature(122, "HS_AlertTime", 		"Alert time", 					"When player discovered but escapes the time for which it's still visible",
			10.0f, 	10.0f, 			Version(), 			GIG_HideAndSeek, ALT_Basic,	0.1f,	100.0f,	true,	true ),
	Feature(123, "HS_HiderVision",	 	"Hider vision", 				"How far hider can see, in pixels (whole screen = 320 px)",
			175, 	175, 			Version(), 			GIG_HideAndSeek, ALT_Advanced,	0,	320, 	true,	true ),
	Feature(124, "HS_HiderVisionThroughWalls", "Hider vision thorough walls", "How far hider can see through walls, in pixels (whole screen = 320 px)",
			75, 	75, 			Version(), 			GIG_HideAndSeek, ALT_Advanced,	0,	320, 	true,	true ),
	Feature(125, "HS_SeekerVision",		"Seeker vision", 				"How far seeker can see, in pixels (whole screen = 320 px)",
			125, 	125, 			Version(), 			GIG_HideAndSeek, ALT_Advanced,	0,	320, 	true,	true ),
	Feature(126, "HS_SeekerVisionThroughWalls", "Seeker vision thorough walls", "How far seeker can see through walls, in pixels (whole screen = 320 px)",
			0, 		0, 				Version(), 			GIG_HideAndSeek, ALT_Advanced,	0,	320, 	true,	true ),
	Feature(127, "HS_SeekerVisionAngle",	"Seeker vision angle",			"The angle of seeker vision (180 = half-circle, 360 = full circle)",
			360, 	360, 			Version(),			GIG_HideAndSeek, ALT_Advanced,	0,	360,	false,	true ),

	Feature(130, "NewNetEngine", 		"New net engine (restricted)",	"New net engine, it was never finished and later was removed",
			false, 	false, 			OLXBetaVersion(0,58,1),	GIG_Advanced, ALT_DevKnownUnstable ),

	Feature(140, "FillWithBotsTo",		"Fill with bots up to",	"If too less players, it will get filled with bots",
			0,	0,					OLXBetaVersion(0,58,1),		GIG_Other, ALT_Advanced,	0,	MAX_PLAYERS, true,	true),

	Feature(151, "WormSpeedFactor",		"Worm speed factor",	"Initial factor to worm speed",
			1.0f,	1.0f,			OLXBetaVersion(0,58,1),		GIG_Other, ALT_Advanced,	-2.0f,	10.0f,	true),
	Feature(152, "WormDamageFactor",		"Worm damage factor",	"Initial factor to worm damage",
			1.0f,	1.0f,			OLXBetaVersion(0,58,1),		GIG_Other,	ALT_VeryAdvanced,	-2.0f,	10.0f,	true),
	Feature(153, "WormShieldFactor",		"Worm shield factor",	"Initial factor to worm shield",
			1.0f,	1.0f,			OLXBetaVersion(0,58,1),		GIG_Other,	ALT_VeryAdvanced,	-2.0f,	10.0f,	true),
	Feature(154, "InstantAirJump",		"Instant air jump",		"Worms can jump in air instantly, this allows floating in air",
			false,	false,			OLXBetaVersion(0,58,1),		GIG_Other, ALT_Advanced,	true),	// Server-side
	Feature(155, "RelativeAirJump",		"Relative air jump",	"Worms can jump in air in a given time interval",
			false,	false,			OLXBetaVersion(0,58,1),		GIG_Other, ALT_Advanced),				// Client-side
	Feature(156, "RelativeAirJumpDelay",	"Delay for relative air jumps",	"How fast can you do air-jumps",
			0.7f,	0.7f,			Version(),				GIG_Other,	ALT_VeryAdvanced,	0.0f, 	5.0f),
	Feature(157, "JumpToAimDir",			"Jump into aim direction",	"When you jump, you don't get right up but into your aim direction with this setting",
			false,	false,			OLXBetaVersion(0,59,6),				GIG_Other,	ALT_VeryAdvanced,	false, false),
	Feature(158, "AllowWeaponsChange",	"Allow weapons change",	"Everybody can change its weapons at any time",
			true,	true,			OLXBetaVersion(0,58,1),		GIG_Weapons, ALT_Advanced,	true, true),
	Feature(159, "ImmediateStart",		"Immediate start",		"Immediate start of game, don't wait for other players weapon selection",
			true,	true,			OLXBetaVersion(0,57,8),		GIG_Advanced, ALT_Advanced,	true),
	Feature(160, "WeaponSlotsNum", "Weapon slots number", "amount of weapon slots each worm has", 5, 5, OLXBetaVersion(0,59,10), GIG_Weapons, ALT_VeryAdvanced),
	Feature(161, "DisableWpnsWhenEmpty",	"Disable weapons when empty", "When a weapon got uncharged, it got disabled and you have to catch a bonus (be sure that you have bonuses activated). This is usefull in games like Race.",
			false,	false,			OLXBetaVersion(0,57,7) /* it needs wpninfo packet which is there since beta7 */,		GIG_Weapons, ALT_VeryAdvanced,	true),
	Feature(162, "DisableWpnsAtStartup",	"Disable weapons at startup", "You start with empty weapon slots. You should enable bonuses.",
			false,	false,			OLXBetaVersion(0,59,10),		GIG_Weapons, ALT_VeryAdvanced,	true),
	Feature(169, "WeaponCombos",			"Weapon combos",	"Enable/disable weapon combos, i.e. fast changing of weapons and shooting at the same time",
			true,	true,			OLXBetaVersion(0,58,10),	GIG_Weapons, ALT_Advanced,	false, false),
	Feature(170, "InfiniteMap",			"Infinite map",			"Map has no borders and is tiled together",
			false,	false,			OLXBetaVersion(0,58,1),		GIG_Other,	ALT_Advanced,	false),
	Feature(171, "WormFriction",			"Worm Friction",		"Friction coefficient for worms (0 = disabled)",
			0.0f, 0.0f,				OLXBetaVersion(0,58,1),		GIG_Other,	ALT_VeryAdvanced,	0.0f, 2.0f,	false),
	Feature(172, "WormGroundFriction",	"Worm Ground Friction",		"Air friction coefficient when worms are on ground (0.1 = default; 1 = stucked; 0 = no friction)",
			0.1f, 0.1f,				OLXBetaVersion(0,58,9),		GIG_Other,	ALT_VeryAdvanced,	0.0f, 1.0f,	false),
	Feature(173, "WormGroundStopSpeed",	"Worm Ground Stop Speed",	"Once the worms has this velocity or less (in x-direction), the x-velocity is set to 0",
			5.0f, 5.0f,				OLXBetaVersion(0,59,6),		GIG_Other,	ALT_VeryAdvanced,	0.0f, 10.0f,	false),
	Feature(174, "WormMaxGroundMoveSpeed",	"Worm max ground move speed",		"When moving the worm via left/right, this is the maximum on ground (default: 30)",
			30.0f, 30.0f,			OLXBetaVersion(0,59,6),		GIG_Other,	ALT_VeryAdvanced,	0.1f, 200.0f,	false, false),
	Feature(175, "WormMaxAirMoveSpeed",		"Worm max air move speed",		"When moving the worm via left/right, this is the maximum in air (default: 30)",
			30.0f, 30.0f,			OLXBetaVersion(0,59,6),		GIG_Other,	ALT_VeryAdvanced,	0.1f, 200.0f,	false, false),
	Feature(176, "GusanosWormPhysics",	"Gusanos worm physics",		"Worm moving is done with Gusanos worm physics",
			false, false,			OLXBetaVersion(0,59,6),		GIG_Other,	ALT_VeryAdvanced,	false, false),

	Feature(180, "ProjFriction",			"Projectile Friction",		"Air friction coefficient for projectiles (0 = disabled)",
			0.0f, 0.0f,				OLXBetaVersion(0,58,1),		GIG_Weapons,	ALT_VeryAdvanced,	0.0f, 2.0f,	false),
	Feature(181, "ProjRelativeVel",		"Relative projectile velocity",	"Worm velocity is added to projectile velocity when you shoot",
			true, true,				Version(),					GIG_Weapons,	ALT_VeryAdvanced,	true, true),
	Feature(182, "ProjGravityFactor",	"Projectile gravity factor",	"Projectile gravity factor",
			1.0f, 1.0f,				OLXBetaVersion(0,59,6),		GIG_Weapons,	ALT_VeryAdvanced,	-10.0f, 10.0f, false, false),
	Feature(183, "LX56WallShooting",		"LX56 wall shooting",		"LX56-like wall shooting enabled. To disable this, check also the option ShootSpawnDistance",
			true, true,				OLXBetaVersion(0,59,6),		GIG_Weapons,	ALT_VeryAdvanced,	false, false),
	Feature(184, "ShootSpawnDistance",	"Shoot spawn distance",		"Projectile shoot spawn distance",
			8.0f, 8.0f,				OLXBetaVersion(0,59,6),		GIG_Weapons,	ALT_VeryAdvanced,	0.0f, 20.0f, false, false),

	Feature(200, "TeamScoreLimit",		"Team Score limit",		"Team score limit",
			-1, -1,					OLXBetaVersion(0,58,1),		GIG_General, ALT_Basic,	-1, 100,	true, true, true),

	Feature(290, "SizeFactor",			"Size factor",			"The size of everything in game will be changed by this factor (i.e. made bigger or smaller)",
			1.0f, 1.0f,				OLXBetaVersion(0,58,1),		GIG_Advanced, ALT_Advanced,	0.5f, 4.0f, false),
	Feature(191, "CollideProjectiles",	"Collide projectiles",		"You'll be able to shoot down enemy rockets and grenades",
			false,	false,			OLXBetaVersion(0,58,9),		GIG_Weapons,	ALT_DevKnownUnstable,	false),

	Feature(300, "CTF_AllowRopeForCarrier", "Allow rope for carrier", "The worm who is holding the flag can use ninja rope",
			true, true,				OLXBetaVersion(0,58,1),		GIG_CaptureTheFlag, ALT_Basic, true),
	Feature(301, "CTF_SpeedFactorForCarrier", "Speed factor for carrier", "Changes the carrier speed by this factor. Perhaps you want to make the carrier slower so you can more easily get the flag again.",
			1.0f, 1.0f,				OLXBetaVersion(0,58,1),		GIG_CaptureTheFlag, ALT_Basic, 0.1f, 3.0f, true),

	Feature(310, "Race_Rounds", "Rounds", "Amount of rounds",
			5,5,					Version(),				GIG_Race,	ALT_Basic,	-1,		100,	true,	true),
	Feature(311, "Race_AllowWeapons", "Allow weapons", "If disabled, you cannot shoot",
			false,	false,			Version(),				GIG_Race,	ALT_Advanced,	false),
	Feature(312, "Race_CheckPointRadius", "Checkpoint radius", "The radius of the checkpoints (bigger value makes race easier)",
			15.0f, 15.0f,			Version(),				GIG_Race,	ALT_VeryAdvanced, 5.0f, 100.f, true, true),
};

static_assert(__FTI_BOTTOM == sizeof(featureArray)/sizeof(Feature), "featureArray__sizecheck");



Feature* featureByName(const std::string& name) {
	for_each_iterator( Feature*, f, ArrayRef(featureArray,featureArrayLen()) ) {
		if( stringcaseequal(f->get()->name, name) )
			return f->get();
	}
	return NULL;
}


Feature* featureByVar(const ScriptVarPtr_t& var, bool printErrors) {
	if(var.type != SVT_DYNAMIC) {
		if(printErrors) errors << "featureByVar: var is not a dynamic var" << endl;
		return NULL;
	}

	if(var.ptr.dynVar == NULL) {
		if(printErrors) errors << "featureByVar: var is NULL" << endl;
		return NULL;
	}
	
	const Settings::ScriptVarWrapper* wrapperAddr = dynamic_cast<Settings::ScriptVarWrapper*> (var.ptr.dynVar);
	if(wrapperAddr == NULL) {
		if(printErrors) errors << "featureByVar: var is not a Settings::ScriptVarWrapper" << endl;
		return NULL;
	}
	
	const size_t relAddr = wrapperAddr - &gameSettings.wrappers[0];
	if(relAddr >= FeatureArrayLen) {
		if(printErrors) errors << "options::loadfromdisc: var is not from gameSettings.wrappers" << endl;
		return NULL;
	}
	
	return &featureArray[relAddr];
}

