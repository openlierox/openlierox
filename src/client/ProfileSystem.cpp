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


#include <assert.h>
#include <sstream>
#include <yaml-cpp/yaml.h>

#include "LieroX.h"
#include "ProfileSystem.h"
#include "EndianSwap.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "FileUtils.h"
#include "client/ClientConnectionRequestInfo.h" // WormJoinInfo
#include "game/CWorm.h"

typedef std::list<SmartPointer<profile_t> > ProfileList;
static ProfileList tProfiles;


static void	SaveProfile(FILE *fp, const SmartPointer<profile_t>& p);
static void	LoadProfile(FILE *fp);
static void	SaveProfileYaml(YAML::Emitter& out, const SmartPointer<profile_t>& p);
static void	LoadProfileYaml(const YAML::Node& node);


static void AddDefaultHumanPlayer() {
	std::string name = "OpenLieroXor";
	if(tLXOptions && tLXOptions->sLastSelectedPlayer != "") {
		SmartPointer<profile_t> p = FindProfile(tLXOptions->sLastSelectedPlayer);
		if(!p.get())
			name = tLXOptions->sLastSelectedPlayer;
	}
    // Add the default worm
    AddProfile(name, "default.png", "", "", 100,100,255, PRF_HUMAN->toInt(),0);	
}

static void AddDefaultBotPlayers()
{
	// Pre-set cpu colours
	static const Uint32 cpuColours[] = { 255,0,0,  0,255,0,  0,0,255,  255,0,255,  0,255,255,  128,128,128,
		128,255,0,  0,128,255, 0,128,0 };
	
    // Pre-set cpu difficulties
    static const int Diff[] = {AI_EASY, AI_MEDIUM, AI_MEDIUM, AI_HARD, AI_XTREME, AI_MEDIUM, AI_EASY};
		
	// Add 7 ai players
	for(short i=0; i<7; i++)		
		AddProfile("CPU "+itoa(i+1), "default.png", "", "", cpuColours[i*3], cpuColours[i*3+1], cpuColours[i*3+2], PRF_COMPUTER->toInt(), Diff[i]);
}

static void AddDefaultPlayers() {
	AddDefaultHumanPlayer();
	AddDefaultBotPlayers();
}

// The default set of local (split-screen) player profiles, matching the ones
// bundled in cfg/players.dat.
struct DefaultLocalProfile { const char* name; int r, g, b; };
static const DefaultLocalProfile DefaultLocalProfiles[] = {
	{ "OpenLieroXor",   0, 177,   0 }, // player 1 — green
	{ "The Second",   255, 255, 255 }, // player 2 — white
	{ "The Third",     20,  90, 255 }, // player 3 — strong blue
	{ "The Fourth",   255, 220,   0 }, // player 4 — yellow
};

// Make sure each default local player profile exists, recreating any that are
// missing (checked one by one, so a partially-present set is completed rather
// than left alone). This runs on startup so users who deleted a profile — or
// whose cfg/players.dat predates players 3 and 4 — still get ready-made worms
// to assign to the local split-screen slots. Existing profiles are left as-is.
void EnsureDefaultLocalPlayerProfiles() {
	for(size_t i = 0; i < sizeof(DefaultLocalProfiles)/sizeof(DefaultLocalProfiles[0]); i++) {
		const DefaultLocalProfile& d = DefaultLocalProfiles[i];
		if(FindProfile(d.name).get())
			continue; // this local player profile already exists — keep it
		AddProfile(d.name, "default.png", "", "", d.r, d.g, d.b, PRF_HUMAN->toInt(), 0);
		notes << "ProfileSystem: recreated missing local player profile '" << d.name << "'" << endl;
	}
}

static void checkProfileSane(const SmartPointer<profile_t>& p) {
	TrimSpaces(p->sName);
	if(p->sName == "")
		p->sName = "OpenLieroXor";
}

SmartPointer<profile_t> MainHumanProfile() {
	if(tProfiles.empty())
		AddDefaultHumanPlayer();
	assert(!tProfiles.empty());
	if((*tProfiles.begin())->iType != PRF_HUMAN->toInt())
		AddDefaultHumanPlayer();

	// try with last selected player if it is a human player
	SmartPointer<profile_t> p = FindProfile(tLXOptions->sLastSelectedPlayer);
	if(p.get() && p->iType == PRF_HUMAN->toInt()) {
		checkProfileSane(p);
		return p;
	}
	
	// just take first human
	assert(!tProfiles.empty());
	p = *tProfiles.begin();
	assert(p->iType == PRF_HUMAN->toInt());
	checkProfileSane(p);
	return p;
}

static std::pair<ProfileList::iterator,int> getFirstBotProf() {
	int i = 0;
	foreach(p, tProfiles) {
		if((*p)->iType == PRF_COMPUTER->toInt())
			return std::make_pair(p, i);
		++i;
	}
	return std::make_pair(tProfiles.end(), -1);
}

///////////////////
// Load the profiles
int LoadProfiles()
{
	tProfiles.clear();
	
	//
	// Open the file
	//
	FILE *fp = OpenGameFile("cfg/players.dat","rb");
	if(fp == NULL) {
        // Add the default players
        AddDefaultPlayers();
		return false;
	}


	//
	// Header
	//

	// Check ID
	std::string id;
	fread_fixedwidthstr<32>(id, fp);
	if(id != "lx:profile") {
		errors << "Could not load profiles: \"" << id << "\" is not equal to \"lx:profile\"" << endl;

		// Add the default players
		AddDefaultPlayers();
		fclose(fp);
		return false;
	}

	// Check version
	int ver = 0;
	fread_compat(ver, sizeof(int), 1, fp);
	EndianSwap(ver);
	if(ver != PROFILE_VERSION) {
		std::string tmp = "Could not load profiles: \""+itoa(ver)+"\" is not equal to \""+itoa(PROFILE_VERSION)+"\"";
		errors << tmp << endl;
		
        // Add the default players
        AddDefaultPlayers();
		fclose(fp);
		return false;
	}

	// Get number of profiles
	int num = 0;
	fread_compat(num,	sizeof(int), 1, fp);
	EndianSwap(num);

	// Safety check
	if(num < 0) {
		// Just leave
		fclose(fp);
        // Add the default players
        AddDefaultPlayers();
		return true;
	}


	// Load the profiles
	for(int i=0; i<num; i++)
		LoadProfile(fp);

	fclose(fp);

	MainHumanProfile(); // this call will ensures that there is a human profile

	if(getFirstBotProf().second < 0)
		AddDefaultBotPlayers();
	
	return true;
}



///////////////////
// Save the profiles
void SaveProfiles()
{
	if(tProfiles.size() == 0)
		// Profiles not loaded, don't write and empty file (and delete all user's prifiles!)
		return;


	//
	// Open the file
	//
	FILE *fp = OpenGameFile("cfg/players.dat","wb");
	if(fp == NULL)  {
		errors << "Could not open cfg/players.dat for writing" << endl;
		return;
	}

	// ID & Version
	fwrite("lx:profile", 32, fp);

	int ver = PROFILE_VERSION;
	fwrite_endian_compat((ver), sizeof(int), 1, fp);

	int num = (int)tProfiles.size();
	fwrite_endian_compat(num, sizeof(int), 1, fp);

	foreach(p, tProfiles)
		// Save the profile
		SaveProfile(fp, *p);

	fclose(fp);
}


///////////////////
// Shutdown & save the profiles
void ShutdownProfiles()
{
	SaveProfiles();
	tProfiles.clear();
}


///////////////////
// Load a profile
static void LoadProfile(FILE *fp)
{
	SmartPointer<profile_t> p(new profile_t);
	if(p.get() == NULL)
		return;

	// Name
	p->sName = freadfixedcstr(fp, 32);
	p->cSkin.Change(freadfixedcstr(fp, 128));
    fread_compat(p->iType,    sizeof(int),    1,  fp);
    EndianSwap(p->iType);
    fread_compat(p->nDifficulty,sizeof(int),  1,  fp);
	EndianSwap(p->nDifficulty);

	if (p->iType == PRF_COMPUTER->toInt())
		p->cSkin.setBotIcon(p->nDifficulty);
	
	// Multiplayer
	p->sUsername = freadfixedcstr(fp,16);
	p->sPassword = freadfixedcstr(fp,16);

	// Colour
	fread_compat(p->R,		sizeof(Uint8),	1,	fp);
	EndianSwap(p->R);
	fread_compat(p->G,		sizeof(Uint8),	1,	fp);
	EndianSwap(p->G);
	fread_compat(p->B,		sizeof(Uint8),	1,	fp);
	EndianSwap(p->B);

	p->cSkin.setDefaultColor(Color(p->R, p->G, p->B));
	p->cSkin.Colorize(p->cSkin.getDefaultColor());
	
	// Weapons
	p->sWeaponSlots.resize(5);
	for(int i=0; i<5; i++)
		p->sWeaponSlots[i] = freadfixedcstr(fp,64);

	AddProfile(p);
}


///////////////////
// Save a profile
void SaveProfile(FILE *fp, const SmartPointer<profile_t>& p)
{
	// Name & Type
	fwrite(p->sName,	32,	fp);
	fwrite(p->cSkin.getFileName(),    128,fp);
    fwrite_endian_compat((p->iType),   sizeof(int),    1,  fp);
    fwrite_endian_compat((p->nDifficulty),sizeof(int), 1,  fp);

	// Multiplayer
	fwrite(p->sUsername, 	16, fp);
	fwrite(p->sPassword, 	16, fp);

	// Colour
	fwrite(&p->R, 1,	1,	fp);
	fwrite(&p->G, 1,	1,	fp);
	fwrite(&p->B, 1,	1,	fp);

	// Weapons		
	for(int i=0; i<5; i++)
		fwrite(p->getWeaponSlot(i),		64,	fp);
}


///////////////////
// Load the profiles from the YAML format (cfg/players.yaml).
// Returns true if the file was found and parsed cleanly. On any failure the
// profile list is left untouched (so a caller can fall back to the .dat format).
int LoadProfilesYaml()
{
	std::ifstream f;
	if(!OpenGameFileR(f, "cfg/players.yaml"))
		// No YAML profiles yet - not an error, the caller decides what to do.
		return false;

	std::stringstream buf;
	buf << f.rdbuf();

	try {
		YAML::Node root = YAML::Load(buf.str());
		if(!root.IsMap()) {
			errors << "Could not load cfg/players.yaml: root is not a map" << endl;
			return false;
		}

		// Check version
		int ver = root["version"] ? root["version"].as<int>() : 0;
		if(ver != PROFILE_VERSION) {
			errors << "Could not load cfg/players.yaml: version \"" << ver
				<< "\" is not equal to \"" << PROFILE_VERSION << "\"" << endl;
			return false;
		}

		const YAML::Node& profiles = root["profiles"];
		if(!profiles || !profiles.IsSequence()) {
			errors << "Could not load cfg/players.yaml: 'profiles' is missing or not a sequence" << endl;
			return false;
		}

		// All checks passed - replace the current list.
		tProfiles.clear();
		for(YAML::const_iterator it = profiles.begin(); it != profiles.end(); ++it)
			LoadProfileYaml(*it);
	} catch(const YAML::Exception& e) {
		errors << "Could not load cfg/players.yaml: " << e.what() << endl;
		return false;
	}

	MainHumanProfile(); // this call ensures that there is a human profile

	if(getFirstBotProf().second < 0)
		AddDefaultBotPlayers();

	return true;
}


///////////////////
// Save the profiles to the YAML format (cfg/players.yaml)
void SaveProfilesYaml()
{
	if(tProfiles.size() == 0)
		// Profiles not loaded, don't write an empty file (and delete all user's profiles!)
		return;

	std::ofstream f;
	if(!OpenGameFileW(f, "cfg/players.yaml")) {
		errors << "Could not open cfg/players.yaml for writing" << endl;
		return;
	}

	YAML::Emitter out;
	out << YAML::BeginMap;
	out << YAML::Key << "version" << YAML::Value << PROFILE_VERSION;
	out << YAML::Key << "profiles" << YAML::Value << YAML::BeginSeq;

	foreach(p, tProfiles)
		SaveProfileYaml(out, *p);

	out << YAML::EndSeq;
	out << YAML::EndMap;

	f << out.c_str() << std::endl;
}


///////////////////
// Load a single profile from a YAML node
static void LoadProfileYaml(const YAML::Node& node)
{
	if(!node.IsMap())
		return;

	SmartPointer<profile_t> p(new profile_t);
	if(p.get() == NULL)
		return;

	// Name & Type
	p->sName = node["name"] ? node["name"].as<std::string>() : "";
	p->cSkin.Change(node["skin"] ? node["skin"].as<std::string>() : "default.png");
	p->iType = node["type"] ? node["type"].as<int>() : PRF_HUMAN->toInt();
	p->nDifficulty = node["difficulty"] ? node["difficulty"].as<int>() : AI_EASY;

	if(p->iType == PRF_COMPUTER->toInt())
		p->cSkin.setBotIcon(p->nDifficulty);

	// Note: the binary .dat format has sUsername/sPassword fields, but they are
	// unused vestigial fields (always empty, never consumed). The YAML format
	// deliberately omits them; they stay empty (their profile_t default).

	// Colour
	p->R = p->G = p->B = 0;
	const YAML::Node& color = node["color"];
	if(color && color.IsSequence() && color.size() >= 3) {
		p->R = (Uint8) color[0].as<int>();
		p->G = (Uint8) color[1].as<int>();
		p->B = (Uint8) color[2].as<int>();
	}

	p->cSkin.setDefaultColor(Color(p->R, p->G, p->B));
	p->cSkin.Colorize(p->cSkin.getDefaultColor());

	// Weapons
	p->sWeaponSlots.resize(5);
	const YAML::Node& weapons = node["weapons"];
	if(weapons && weapons.IsSequence()) {
		for(int i=0; i<5 && (size_t)i < weapons.size(); i++)
			p->sWeaponSlots[i] = weapons[i].as<std::string>();
	}

	AddProfile(p);
}


///////////////////
// Save a single profile as a YAML map
static void SaveProfileYaml(YAML::Emitter& out, const SmartPointer<profile_t>& p)
{
	out << YAML::BeginMap;

	// Name & Type
	out << YAML::Key << "name" << YAML::Value << p->sName;
	out << YAML::Key << "type" << YAML::Value << p->iType;
	out << YAML::Key << "difficulty" << YAML::Value << p->nDifficulty;
	out << YAML::Key << "skin" << YAML::Value << p->cSkin.getFileName();

	// Note: the binary .dat format's sUsername/sPassword fields are intentionally
	// not written here - they are unused vestigial fields (always empty).

	// Colour
	out << YAML::Key << "color" << YAML::Value
		<< YAML::Flow << YAML::BeginSeq
		<< (int)p->R << (int)p->G << (int)p->B
		<< YAML::EndSeq;

	// Weapons
	out << YAML::Key << "weapons" << YAML::Value << YAML::BeginSeq;
	for(int i=0; i<5; i++)
		out << p->getWeaponSlot(i);
	out << YAML::EndSeq;

	out << YAML::EndMap;
}


///////////////////
// Delete a profile
void DeleteProfile(const SmartPointer<profile_t>& prof)
{
	foreach(p, tProfiles) {
		if((*p).get() == prof.get()) {
			tProfiles.erase(p);
			return;
		}
	}
}


int AddProfile(const SmartPointer<profile_t>& prof) {
	if(prof.get() == NULL) {
		errors << "AddProfile with NULL" << endl;
		return -1;
	}
	if(prof->iType == PRF_HUMAN->toInt()) {
		std::pair<ProfileList::iterator,int> i = getFirstBotProf();
		tProfiles.insert(i.first, prof);
		return i.second;
	}
	else if(prof->iType == PRF_COMPUTER->toInt()) {
		tProfiles.push_back(prof);
		return (int)tProfiles.size() - 1;
	}
	else
		errors << "AddProfile " << prof->sName << " with undefined type " << prof->iType << endl;
	return -1;
}

///////////////////
// Add a profile to the list
int AddProfile(const std::string& name, const std::string& skin, const std::string& username, const std::string& password,  int R, int G, int B, int type, int difficulty)
{
	SmartPointer<profile_t>	p(new profile_t());
	if(p.get() == NULL)
		return -1;

	p->iType = type;
    p->nDifficulty = difficulty;

	p->sName = name;
	p->cSkin.Change(skin);
	p->R = R;
	p->G = G;
	p->B = B;
	p->cSkin.Colorize(Color(R, G, B));

	p->sUsername = username;
	p->sPassword = password;


	// Default weapons
	p->sWeaponSlots.resize(5);
	p->sWeaponSlots[0] = "minigun";
	p->sWeaponSlots[1] = "super shotgun";
	p->sWeaponSlots[2] = "blaster";
	p->sWeaponSlots[3] = "gauss gun";
	p->sWeaponSlots[4] = "big nuke";

	return AddProfile(p);
}



///////////////////
// Get the profiles
Iterator<SmartPointer<profile_t> >::Ref GetProfiles()
{
	MainHumanProfile(); // ensure that there is a human profile
	return GetIterator(tProfiles);
}


///////////////////
// Find a profile based on id
SmartPointer<profile_t> FindProfile(int id)
{
	if(id < 0) return NULL;
	if((size_t)id >= tProfiles.size()) return NULL;
	int i = 0;
	foreach(p, tProfiles) {
		if(i == id) return *p;
		++i;
	}
	return NULL;
}

SmartPointer<profile_t> FindProfile(const std::string& name) {
	foreach(p, tProfiles) {
		if((*p)->sName == name)
			return *p;
	}
	return NULL;
}

int GetProfileId(const SmartPointer<profile_t>& prof) {
	int i = 0;
	foreach(p, tProfiles) {
		if(p->get() == prof.get())
			return i;
		++i;
	}
	return -1;
}

std::string FindFirstCPUProfileName() {
	ProfileList::iterator i = getFirstBotProf().first;
	if(i != tProfiles.end()) return (*i)->sName;
	
	AddDefaultBotPlayers();
	return FindFirstCPUProfileName();
}



SmartPointer<profile_t> profileFromWorm(CWorm* w) {
	SmartPointer<profile_t> p(new profile_t);
	p->iType = w->getType()->toInt();
	p->sName = w->getName();
	p->nDifficulty = w->getProfile().get() ? w->getProfile()->nDifficulty : AI_EASY;
	p->R = w->getSkin().getDefaultColor().r;
	p->G = w->getSkin().getDefaultColor().g;
	p->B = w->getSkin().getDefaultColor().b;
	p->iTeam = w->getTeam();
	p->cSkin.Change(w->getSkin().getFileName());
	p->cSkin.setDefaultColor(w->getSkin().getDefaultColor());
	p->cSkin.ColorizeDefault();
	return p;
}

SmartPointer<profile_t> profileFromWormJoinInfo(const WormJoinInfo& info) {
	SmartPointer<profile_t> p(new profile_t);
	p->iType = info.m_type ? info.m_type->toInt() : PRF_HUMAN->toInt();
	p->sName = info.sName;
	p->nDifficulty = AI_EASY;
	p->R = info.skinColor.r;
	p->G = info.skinColor.g;
	p->B = info.skinColor.b;
	p->iTeam = info.iTeam;
	p->cSkin.Change(info.skinFilename);
	p->cSkin.setDefaultColor(Color(p->R, p->G, p->B));
	p->cSkin.ColorizeDefault();
	return p;
}


std::string profile_t::getWeaponSlot(int i) {
	if(i < 0) return "";
	if((size_t)i >= sWeaponSlots.size()) return "";
	return sWeaponSlots[i];
}

std::string& profile_t::writeWeaponSlot(int i) {
	static std::string dummy;
	if(i < 0) return dummy;
	if((size_t)i >= sWeaponSlots.size()) return dummy;
	return sWeaponSlots[i];
}
