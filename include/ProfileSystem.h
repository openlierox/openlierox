/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Profile system
// Created 13/8/02
// Jason Boettcher


#ifndef __PROFILESYSTEM_H__
#define __PROFILESYSTEM_H__

#include <SDL.h>
#include <string>
#include <stdlib.h>
#include "SmartPointer.h"
#include "CGameSkin.h"
#include "Iter.h"

#define		PROFILE_VERSION		5

/*
#define		PRF_HUMAN			0
#define		PRF_COMPUTER		1
*/

class CWorm;
class CWormInputHandler;

struct WormType {
	virtual ~WormType() {}
	virtual CWormInputHandler* createInputHandler(CWorm* w) = 0;
	virtual int toInt() = 0;
	static WormType* fromInt(int type);
	bool operator==(const WormType& t) { return &t == this; }
	bool operator!=(const WormType& t) { return &t != this; }
};

extern WormType* PRF_HUMAN;
extern WormType* PRF_COMPUTER;



// AI Difficulty
#define		AI_EASY			0
#define		AI_MEDIUM		1
#define		AI_HARD			2
#define		AI_XTREME		3



// Player profile structure
struct profile_t {
	profile_t() :
		iType(PRF_HUMAN->toInt()), nDifficulty(AI_EASY),
		R(0), G(0), B(0), iTeam(0),
		cSkin(CGameSkin::WormSkin(), true) {}

	int				iType;
	std::string		sName;
    int             nDifficulty;
	std::string		sUsername;
	std::string		sPassword;
	Uint8			R,G,B;
	std::vector<std::string>	sWeaponSlots;
	int				iTeam;
	CGameSkin		cSkin;

	std::string getWeaponSlot(int i);
	std::string& writeWeaponSlot(int i);
};



int		LoadProfiles();
void	SaveProfiles();
void	ShutdownProfiles();

// Recreate any of the default local (split-screen) player profiles that are
// missing. Call after LoadProfiles(). See ProfileSystem.cpp for details.
void	EnsureDefaultLocalPlayerProfiles();

int		AddProfile(const std::string& name, const std::string& skin, const std::string& username, const std::string& password,  int R, int G, int B, int type, int difficulty);
int		AddProfile(const SmartPointer<profile_t>& prof);
void	DeleteProfile(const SmartPointer<profile_t>& prof);


Iterator<SmartPointer<profile_t> >::Ref GetProfiles();
SmartPointer<profile_t> FindProfile(int id);
SmartPointer<profile_t> FindProfile(const std::string& name);
int GetProfileId(const SmartPointer<profile_t>& p);

std::string FindFirstCPUProfileName();
SmartPointer<profile_t> MainHumanProfile(); 

class CWorm;
SmartPointer<profile_t> profileFromWorm(CWorm* w);

struct WormJoinInfo;
SmartPointer<profile_t> profileFromWormJoinInfo(const WormJoinInfo& info);


#endif  //  __PROFILESYSTEM_H__
