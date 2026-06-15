/*
 *  FeatureList.h
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 22.12.08.
 *	code under LGPL
 *
 */

/*
 Note: Since 0.59 beta6, all gameplay related settings where moved to here.
 Maybe we should rename this file because it is not just about advanced
 feature settings but about just every gameplay related settings.
 */

#ifndef __FEATURELIST_H__
#define __FEATURELIST_H__

#include <string>
#include <set>
#include <map>
#include <stdint.h>
#include <boost/function.hpp>
#include "Iter.h"
#include "CScriptableVars.h"
#include "Version.h"
#include "util/CustomVar.h"
#include "CodeAttributes.h"

struct Feature {
	uint32_t id; // fixed for all future OLX versions. used also as game attrib id
	std::string name; // for config, network and other identification
	std::string humanReadableName;
	std::string description;
	typedef ScriptVarType_t VarType;
	typedef ScriptVar_t Var;
	VarType valueType; // for example: float for gamespeed, bool for ropelenchange
	Var unsetValue; // if the server does not provide this; for example: gamespeed=1; should always be like the behaviour without the feature to keep backward compatibility
	Var defaultValue; // for config, if not set before, in most cases the same as unsetValue

	Version minVersion; // min supported version (<=beta8 is not updated automatically by the system) 
	// Old clients are kicked if feature version is greater that client version, no matter if feature is server-sided or optinal

	GameInfoGroup group;	// For grouping similar options in GUI

	AdvancedLevel advancedLevel;

	// TODO: make special type VarRange (which holds these hasmin/hasmax/min/max/signed)
	// TODO: move that to ScriptVarType_t
	Var minValue; // Min and max values are used in GUI to make sliders (only for float/int)
	Var maxValue; // Min and max values are used in GUI to make sliders (only for float/int)
	bool unsignedValue; // If the value is unsigned - ints and floats only
	
	bool serverSideOnly; // if true, all the following is just ignored
	bool optionalForClient; // Optional client-sided feature, like vision cone drawn for seekers, or SuicideDecreasesScore which required for precise damage calculation in scoreboard
	
	typedef boost::function<Var (const Var& preset)> GetValueFunction;
	GetValueFunction getValueFct; // if set, it uses the return value for hostGet
	
	
	Feature(uint32_t id_, const std::string& n, const std::string& hn, const std::string& desc, bool unset, bool def,
				Version ver, GameInfoGroup g = GIG_Invalid, AdvancedLevel l = ALT_Basic, bool ssdo = false, bool opt = false, 
				GetValueFunction f = GetValueFunction())
	: id(id_), name(n), humanReadableName(hn), description(desc), valueType(SVT_BOOL), unsetValue(Var(unset)), defaultValue(Var(def)),
		minVersion(ver), group(g), advancedLevel(l), serverSideOnly(ssdo), optionalForClient(opt), 
		getValueFct(f) {}

	Feature(uint32_t id_, const std::string& n, const std::string& hn, const std::string& desc, int unset, int def,
				Version ver, GameInfoGroup g = GIG_Invalid, AdvancedLevel l = ALT_Basic, int minval = 0, int maxval = 0, bool ssdo = false, bool opt = false, 
				bool unsig = false, GetValueFunction f = GetValueFunction())
	: id(id_), name(n), humanReadableName(hn), description(desc), valueType(SVT_INT32), unsetValue(Var(unset)), defaultValue(Var(def)),
		minVersion(ver), group(g), advancedLevel(l), minValue(minval), maxValue(maxval), unsignedValue(unsig), serverSideOnly(ssdo), 
		optionalForClient(opt), getValueFct(f) {}

	Feature(uint32_t id_, const std::string& n, const std::string& hn, const std::string& desc, float unset, float def,
				Version ver, GameInfoGroup g = GIG_Invalid, AdvancedLevel l = ALT_Basic, float minval = 0.0f, float maxval = 0.0f, bool ssdo = false, bool opt = false, 
				bool unsig = false, GetValueFunction f = GetValueFunction())
	: id(id_), name(n), humanReadableName(hn), description(desc), valueType(SVT_FLOAT), unsetValue(Var(unset)), defaultValue(Var(def)),
		minVersion(ver), group(g), advancedLevel(l), minValue(minval), maxValue(maxval), unsignedValue(unsig), serverSideOnly(ssdo), 
		optionalForClient(opt), getValueFct(f) {}

	Feature(uint32_t id_, const std::string& n, const std::string& hn, const std::string& desc, const std::string& unset, const std::string& def,
				Version ver, GameInfoGroup g = GIG_Invalid, AdvancedLevel l = ALT_Basic, bool ssdo = false, bool opt = false, 
				GetValueFunction f = GetValueFunction())
	: id(id_), name(n), humanReadableName(hn), description(desc), valueType(SVT_STRING), unsetValue(Var(unset)), defaultValue(Var(def)),
		minVersion(ver), group(g), advancedLevel(l), serverSideOnly(ssdo), optionalForClient(opt), 
		getValueFct(f) {}

	Feature(uint32_t id_, const std::string& n, const std::string& hn, const std::string& desc, const char* unset, const char* def,
				Version ver, GameInfoGroup g = GIG_Invalid, AdvancedLevel l = ALT_Basic, bool ssdo = false, bool opt = false,
				GetValueFunction f = GetValueFunction())
	: id(id_), name(n), humanReadableName(hn), description(desc), valueType(SVT_STRING), unsetValue(Var(std::string(unset))), defaultValue(Var(std::string(def))),
		minVersion(ver), group(g), advancedLevel(l), serverSideOnly(ssdo), optionalForClient(opt),
		getValueFct(f) {}

	Feature(uint32_t id_, const std::string& n, const std::string& hn, const std::string& desc, const CustomVar& unset, const CustomVar& def,
			Version ver, GameInfoGroup g = GIG_Invalid, AdvancedLevel l = ALT_Basic, bool ssdo = false, bool opt = false)
	: id(id_), name(n), humanReadableName(hn), description(desc), valueType(SVT_CUSTOM), unsetValue(Var(unset.getRefCopy())), defaultValue(Var(def.getRefCopy())),
	minVersion(ver), group(g), advancedLevel(l), serverSideOnly(ssdo), optionalForClient(opt), 
	getValueFct(GetValueFunction()) {}
	
};


// Put everything that impacts gameplay here, both server and client-sided

// Indexes of features in featureArray
// These indexes are only for local game use, not for network! (name is used there)
// (I am not that happy with this solution right now, I would like to put both these indexes together
//  with the actual declaration of the Feature. Though I don't know a way how to put both things together
//  in the header file.)
// WARNING: Keep this always synchronised with featureArray!
enum FeatureIndex {
	FT_Lives,
	FT_KillLimit,
	FT_TimeLimit, // Time limit in minutes
	FT_TagLimit,
	FT_LoadingTime,

	FT_Map,
	FT_GameMode,
	FT_Mod,
	FT_SettingsPreset,
	FT_WeaponRest,
	FT_ForceRandomWeapons, // only for server; implies bServerChoosesWeapons=true
	FT_SameWeaponsAsHostWorm, // implies bServerChoosesWeapons=true

	FT_LX56PhysicsFPS,
	FT_ForceSameLX56PhysicsFPS,
	FT_NinjaropePrecision,
	FT_ForceLX56Aim,
	
	FT_Bonuses,
	FT_ShowBonusName,
	FT_BonusFreq,
	FT_BonusLife,
	FT_BonusHealthToWeaponChance, // if 0.0f only health will be generated, if 1.0f - only weapons
	
	FT_RespawnTime,
	FT_MaxRespawnTime,
	FT_RespawnGroupTeams, // respawn all team in single spot
	FT_EmptyWeaponsOnRespawn, // When worm respawns it should wait until all weapons are reloaded
		
	FT_WormGroundSpeed, // float
	FT_WormAirSpeed, // float
	FT_WormAirFriction, // float
	FT_WormGravity, // float
	FT_WormJumpForce, // float
	FT_WormSimpleFriction,
	FT_WormAcceleration,
	FT_WormAirAccelerationFactor,
	FT_WormBounceQuotient,
	FT_WormBounceLimit,
	FT_WormWallHugging,
	FT_WormWeaponHeight,
	FT_WormHeight,
	FT_WormWidth,
	FT_WormMaxClimb,
	FT_WormBoxRadius,
	FT_WormBoxTop,
	FT_WormBoxBottom,
	
	FT_RopeMaxLength, // int
	FT_RopeRestLength, // int
	FT_RopeStrength, // float
	FT_RopeSpeed, // float
	FT_RopeAddParentSpeed,
	FT_RopeGravity,
	FT_RopeFallingGravity,
	FT_RopeCanAttachWorm,
	
	FT_GameSpeed,
	FT_GameSpeedOnlyForProjs,
	FT_ScreenShaking,
	FT_FullAimAngle,
	FT_MiniMap,
	FT_SuicideDecreasesScore,
	FT_TeamkillDecreasesScore,
	FT_DeathDecreasesScore,
	FT_CountTeamkills,
	FT_AllowNegativeScore,
	FT_TeamInjure,
	FT_TeamHit,
	FT_SelfInjure,
	FT_SelfHit,
	FT_AllowEmptyGames,
	FT_HS_HideTime,		// Hide and Seek gamemode settings
	FT_HS_AlertTime,
	FT_HS_HiderVisionRange,
	FT_HS_HiderVisionRangeThroughWalls,
	FT_HS_SeekerVisionRange,
	FT_HS_SeekerVisionRangeThroughWalls,
	FT_HS_SeekerVisionAngle,
	FT_NewNetEngine, // Unused, but we keep it to preserve enum IDs
	FT_FillWithBotsTo,
	FT_WormSpeedFactor,
	FT_WormDamageFactor,
	FT_WormShieldFactor,
	FT_InstantAirJump,
	FT_RelativeAirJump,
	FT_RelativeAirJumpDelay,
	FT_JumpToAimDir,
	FT_AllowWeaponsChange,
	FT_ImmediateStart,
	FT_WeaponSlotsNum,
	FT_DisableWpnsWhenEmpty,
	FT_DisableWpnsAtStartup,
	FT_WeaponCombos,
	FT_InfiniteMap,
	FT_WormFriction,
	FT_WormGroundFriction,
	FT_WormGroundStopSpeed,
	FT_WormMaxGroundMoveSpeed,
	FT_WormMaxAirMoveSpeed,
	FT_GusanosWormPhysics,
	
	FT_ProjFriction,
	FT_ProjRelativeVel,
	FT_ProjGravityFactor,
	FT_LX56WallShooting,
	FT_ShootSpawnDistance,
	
	FT_TeamScoreLimit,
	FT_SizeFactor,
	FT_CollideProjectiles,
	FT_CTF_AllowRopeForCarrier,
	FT_CTF_SpeedFactorForCarrier,
	FT_Race_Rounds,
	FT_Race_AllowWeapons,
	FT_Race_CheckPointRadius,
 
 	__FTI_BOTTOM
};

static const size_t FeatureArrayLen = __FTI_BOTTOM;

extern Feature featureArray[];
INLINE size_t featureArrayLen() { return FeatureArrayLen; }
INLINE FeatureIndex featureArrayIndex(Feature* f) { assert(f >= &featureArray[0] && f < &featureArray[FeatureArrayLen]); return FeatureIndex(f - &featureArray[0]); }
Feature* featureByName(const std::string& name);
Feature* featureByVar(const ScriptVarPtr_t& var, bool printErrors = true); // assumes that var is from gameSettings.wrappers array

class FeatureCompatibleSettingList {
public:
	struct Feature {
		std::string name;
		std::string humanName;
		ScriptVar_t var;
		enum Type { FCSL_SUPPORTED, FCSL_JUSTUNKNOWN, FCSL_INCOMPATIBLE };
		Type type;
	};
	std::map< std::string, Feature > list;
	Iterator< Feature & >::Ref iterator() { return GetIteratorRef_second(list); }
	void set(const Feature& f) { list[ f.name ] = f; }
	void set(const std::string& name, const std::string& humanName, const ScriptVar_t& var, Feature::Type type) {
		Feature f;
		f.name = name;
		f.humanName = humanName;
		f.var = var;
		f.type = type;
		set(f);
	}
	const Feature * find( const std::string & name )
	{
		if( list.find( name ) == list.end() )
			return NULL;
		return &(list.find( name )->second);
	}
	void clear() { list.clear(); }
};

/*
 Class for specific game settings. For the layered version, look at class Settings (game/Settings.h).
 It initialises with all unset values. It is used in CClient for all client side settings.
 */
template<typename __IndexType, Feature* __FeatureArray, size_t __FeatureArrayLen>
class _FeatureSettings {
public:
//	static const Feature* FeatureArray = __FeatureArray;
	static const size_t FeatureArrayLen = __FeatureArrayLen;
	typedef __IndexType Index;
private:
	ScriptVar_t settings[FeatureArrayLen];
public:
	_FeatureSettings() {
		for(size_t i = 0; i < FeatureArrayLen; ++i)
			(*this)[(FeatureIndex)i] = __FeatureArray[i].unsetValue;		
	}
	
	ScriptVar_t& operator[](Index i) { return settings[i]; }
	ScriptVar_t& operator[](Feature* f) { return settings[f - &__FeatureArray[0]]; }
	const ScriptVar_t& operator[](Index i) const { return settings[i]; }
	const ScriptVar_t& operator[](Feature* f) const { return settings[f - &__FeatureArray[0]]; }
	
	ScriptVar_t hostGet(Feature* f) { return hostGet(Index(f - &__FeatureArray[0])); }
	ScriptVar_t hostGet(Index i) {
		ScriptVar_t var = (*this)[i];
		Feature* f = &__FeatureArray[i];
		if(f->getValueFct)
			var = f->getValueFct( var );
		
		return var;		
	}
	
	bool olderClientsSupportSetting(Feature* f) {
		if( f->optionalForClient ) return true;
		return hostGet(f) == f->unsetValue;		
	}
};

typedef _FeatureSettings<FeatureIndex, featureArray, FeatureArrayLen> FeatureSettings;
	
#endif
