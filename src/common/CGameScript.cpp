/////////////////////////////////////////
//
//                  OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Game script class
// Created 7/2/02
// Jason Boettcher

/*
 reference for original Compile* sources:
 http://openlierox.svn.sourceforge.net/viewvc/openlierox/src/common/CGameScript.cpp?revision=3781&view=markup
 */

#include <cstdarg>

#include "EndianSwap.h"
#include "LieroX.h"
#include "Debug.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "CGameScript.h"
#include "Error.h"
#include "ConfigHandler.h"
#include "ProjectileDesc.h"
#include "WeaponDesc.h"
#include "IniReader.h"
#include "game/Mod.h"
#include "gusanos/gusanos.h"
#include "sound/SoundsBase.h"




// Worm structure
// WARNING: never change this!
// it's used in CGameScript.cpp and it represents
// the original file format
struct gs_worm_t {
	float	AngleSpeed; // was always ignored
	
	float	GroundSpeed;  // -> FT_WormGroundSpeed
	float	AirSpeed;  // -> FT_WormAirSpeed
	float	Gravity; // -> FT_WormGravity
	float	JumpForce; // -> FT_WormJumpForce
	float	AirFriction; // -> FT_WormAirFriction
	
	float	GroundFriction; // was always ignored
};


void CGameScript::initNewWeapons(int num) {
	if(Weapons) delete[] Weapons;
	SetNumWeapons(num);
	Weapons = new weapon_t[num];
}



///////////////////
// Write a string in pascal format
static void writeString(const std::string& szString, FILE *fp)
{
    if(szString == "") return;
	
	size_t length = szString.size();
	if(length > 255) {
		warnings << "i will cut the following string for writing: " << szString << endl;
		length = 255;
	}
	uchar len = (uchar)length;
	
    fwrite( &len, sizeof(uchar), 1, fp );
    fwrite( szString.c_str(),sizeof(char), length, fp );
}


///////////////////
// Read a string in pascal format
static std::string readString(FILE *fp)
{
	char buf[256];
	
	uchar length;
	if(fread( &length, sizeof(uchar), 1, fp ) == 0) return "";
	if(fread( buf,sizeof(char), length, fp ) < length) return "";
	
	buf[length] = '\0';
	
	return buf;
}



///////////////////
// Save the script (compiler)
int CGameScript::Save(const std::string& filename)
{
	FILE *fp;
	int n;


	// Open it
	fp = OpenGameFile(filename,"wb");
	if(fp == NULL) {
		errors << "CGameScript::Save: Could not open " << filename << " for writing" << endl;
		return false;
	}

	Header.Version = GS_VERSION;
	strcpy(Header.ID,"Liero Game Script");


	// Header
	gs_header_t tmpheader = Header;
	EndianSwap(tmpheader.Version);
	fwrite(&tmpheader,sizeof(gs_header_t),1,fp);

	fwrite_endian_compat((NumWeapons),sizeof(int),1,fp);

	savedProjs.clear();
	
	// Weapons
	weapon_t *wpn = Weapons;

	for(n=0;n<NumWeapons;n++,wpn++) {

        writeString(wpn->Name, fp);
		fwrite_endian_compat((wpn->Type),          sizeof(int),    1, fp);

		// Special
		if(wpn->Type == WPN_SPECIAL) {
			fwrite_endian_compat((wpn->Special),   sizeof(int),    1, fp);
			fwrite_endian_compat((wpn->tSpecial),  sizeof(gs_special_t), 1, fp); // WARNING: this works, because it contains only one int
			fwrite_endian_compat((wpn->Recharge*10.f),  sizeof(float),  1, fp);
			fwrite_endian_compat((wpn->Drain),     sizeof(float),  1, fp);
			fwrite_endian_compat((wpn->ROF*1000.0f),       sizeof(float),  1, fp);
			fwrite_endian<int>(fp, wpn->LaserSight);
		}

		// Beam
		if(wpn->Type == WPN_BEAM) {
			fwrite_endian_compat((wpn->Recoil),    sizeof(int),    1, fp);
			fwrite_endian_compat((wpn->Recharge*10.f),  sizeof(float),  1, fp);
			fwrite_endian_compat((wpn->Drain),     sizeof(float),  1, fp);
			fwrite_endian_compat((wpn->ROF*1000.0f),       sizeof(float),  1, fp);
			fwrite_endian<int>(fp, wpn->LaserSight);

			wpn->Bm.write(this, fp);
		}

		// Projectile
		if(wpn->Type == WPN_PROJECTILE) {
			fwrite_endian_compat((wpn->Class),     sizeof(int),    1, fp);
			fwrite_endian_compat((wpn->Recoil),    sizeof(int),    1, fp);
			fwrite_endian_compat((wpn->Recharge*10.f),  sizeof(float),  1, fp);
			fwrite_endian_compat((wpn->Drain),     sizeof(float),  1, fp);
			fwrite_endian_compat((wpn->ROF*1000.0f),       sizeof(float),  1, fp);

			if(Header.Version <= GS_LX56_VERSION) {
				fwrite_endian_compat((wpn->Proj.Speed), sizeof(int),    1, fp);
				fwrite_endian_compat((wpn->Proj.SpeedVar),sizeof(float),1, fp);
				fwrite_endian_compat((wpn->Proj.Spread),sizeof(float),  1, fp);
				fwrite_endian_compat((wpn->Proj.Amount),sizeof(int),    1, fp);
				fwrite_endian<int>(fp, wpn->LaserSight);

				fwrite_endian<int>(fp, wpn->UseSound);
				if(wpn->UseSound)
					writeString(wpn->SndFilename, fp);

				// Recursively save the projectile id's
				SaveProjectile(wpn->Proj.Proj,fp);
			}
			else { // newer GS versions
				fwrite_endian<char>(fp, wpn->LaserSight);
				
				fwrite_endian<char>(fp, wpn->UseSound);
				if(wpn->UseSound)
					writeString(wpn->SndFilename, fp);
				
				// Recursively save the projectile id's
				wpn->Proj.write(this, fp);
			}
		}

		if(Header.Version > GS_LX56_VERSION)
			wpn->FinalProj.write(this, fp);	
	}

	// Extra stuff


	// Ninja Rope
	fwrite_endian_compat((int)gameSettings[FT_RopeMaxLength],sizeof(int),1,fp);
	fwrite_endian_compat((int)gameSettings[FT_RopeRestLength],sizeof(int),1,fp);
	fwrite_endian_compat((float)gameSettings[FT_RopeStrength],sizeof(float),1,fp);

	// Worm
	gs_worm_t tmpworm;
	tmpworm.AngleSpeed = 100;
	tmpworm.GroundSpeed = gameSettings[FT_WormGroundSpeed];
	tmpworm.AirSpeed = gameSettings[FT_WormAirSpeed];
	tmpworm.Gravity = gameSettings[FT_WormGravity];
	tmpworm.JumpForce = gameSettings[FT_WormJumpForce];
	tmpworm.AirFriction = gameSettings[FT_WormAirFriction];
	tmpworm.GroundFriction = 0;
	EndianSwap(tmpworm.AngleSpeed);
	EndianSwap(tmpworm.GroundSpeed);
	EndianSwap(tmpworm.AirSpeed);
	EndianSwap(tmpworm.Gravity);
	EndianSwap(tmpworm.JumpForce);
	EndianSwap(tmpworm.AirFriction);
	EndianSwap(tmpworm.GroundFriction);
	fwrite(&tmpworm, sizeof(gs_worm_t), 1, fp);


	fclose(fp);

	savedProjs.clear();
	
	return true;
}


///////////////////
// Save a projectile
bool CGameScript::SaveProjectile(proj_t *proj, FILE *fp)
{
	if(!proj)
		return false;

	if(Header.Version > GS_LX56_VERSION) {
		// well, that's slow but it's not worth to create a reverted-map just for saving
		int index = -1;
		for(Projectiles::iterator it = projectiles.begin(); it != projectiles.end(); ++it) {
			if(it->second == proj) {
				index = it->first;
				break;
			}
		}
		fwrite_endian<int>(fp, index);
		if(savedProjs.find(proj) != savedProjs.end()) return false;
		savedProjs.insert(proj);
	}
	
	fwrite_endian_compat((proj->Type),			sizeof(int),1,fp);
	fwrite_endian_compat((proj->Timer.Time),	sizeof(float),1,fp);
	fwrite_endian_compat((proj->Timer.TimeVar),sizeof(float),1,fp);
	fwrite_endian_compat((proj->Trail.Type),		sizeof(int),1,fp);
	//fwrite_endian_compat(proj->UseCustomGravity, sizeof(int), 1, fp); // use this to test static assert (should fail if sizeof(bool)!=sizeof(int))
	fwrite_endian<int>(fp, proj->UseCustomGravity);

	if(proj->UseCustomGravity)
		fwrite_endian_compat((proj->Gravity),	sizeof(int), 1, fp);

	fwrite_endian_compat((proj->Dampening),	sizeof(int), 1, fp);

	if(Header.Version > GS_LX56_VERSION) {
		fwrite_endian<int>(fp, proj->Width);
		fwrite_endian<int>(fp, proj->Height);
	}
	
    // Pixel type
	switch(proj->Type) {
		case PRJ_POLYGON:
			fwrite_endian<Uint32>(fp, (Uint32)proj->polygon.getPoints().size());
			for(Polygon2D::Points::const_iterator p = proj->polygon.getPoints().begin(); p != proj->polygon.getPoints().end(); ++p) {
				fwrite_endian<int>(fp, p->x);
				fwrite_endian<int>(fp, p->y);				
			}
			
			// fallthrough to save color
		case PRJ_RECT:
		case PRJ_CIRCLE:
		case PRJ_PIXEL:
			fwrite_endian_compat(((int)proj->Colour.size()), sizeof(int), 1, fp);
			for(size_t i = 0; i < proj->Colour.size(); ++i) {
				if(Header.Version <= GS_LX56_VERSION && i >= 2) {
					warnings << "SaveProjectile: projectile has more than 2 colors, other colors are ignored for LX56 format" << endl;
					break;
				}
				if(Header.Version <= GS_LX56_VERSION) {
					fwrite_endian_compat(((int)proj->Colour[i].r),   sizeof(int),1,fp);
					fwrite_endian_compat(((int)proj->Colour[i].g),   sizeof(int),1,fp);
					fwrite_endian_compat(((int)proj->Colour[i].b),   sizeof(int),1,fp);
				} else {
					fwrite_endian_compat((proj->Colour[i].r), 1,1,fp);
					fwrite_endian_compat((proj->Colour[i].g), 1,1,fp);
					fwrite_endian_compat((proj->Colour[i].b), 1,1,fp);
					fwrite_endian_compat((proj->Colour[i].a), 1,1,fp);	
				}
			}
			break;
			
		case PRJ_IMAGE:
			writeString(proj->ImgFilename, fp);
			fwrite_endian<int>(fp, proj->Rotating);
			fwrite_endian_compat((proj->RotIncrement), sizeof(int), 1, fp);
			fwrite_endian_compat((proj->RotSpeed), sizeof(int),1,fp);
			fwrite_endian<int>(fp, proj->UseAngle);
			fwrite_endian<int>(fp, proj->UseSpecAngle);
			if(proj->UseAngle || proj->UseSpecAngle)
				fwrite_endian_compat((proj->AngleImages),sizeof(int),1,fp);
			fwrite_endian<int>(fp, proj->Animating);
			if(proj->Animating) {
				fwrite_endian_compat((proj->AnimRate),sizeof(float),1,fp);
				fwrite_endian_compat((proj->AnimType),sizeof(int),1,fp);
			}
			break;
					
		case __PRJ_LBOUND: case __PRJ_UBOUND: errors << "SaveProjectile: PRJ BOUND" << endl;
	}




	//
	// Hit
	//
	if(Header.Version <= GS_LX56_VERSION) {
		fwrite_endian_compat((proj->Hit.Type),sizeof(int),1,fp);

		// Hit::Explode
		if(proj->Hit.Type == PJ_EXPLODE) {
			fwrite_endian_compat((proj->Hit.Damage),		sizeof(int),1,fp);
			fwrite_endian<int>(fp, proj->Hit.Projectiles);
			fwrite_endian<int>(fp, proj->Hit.UseSound);
			fwrite_endian_compat((proj->Hit.Shake),		sizeof(int),1,fp);
			if(proj->Hit.UseSound)
				writeString(proj->Hit.SndFilename, fp);
		}

		// Hit::Bounce
		if(proj->Hit.Type == PJ_BOUNCE) {
			fwrite_endian_compat((proj->Hit.BounceCoeff),	sizeof(float),	1,fp);
			fwrite_endian_compat((proj->Hit.BounceExplode),sizeof(int),	1,fp);
		}

		// Hit::Carve
		if(proj->Hit.Type == PJ_CARVE) {
			fwrite_endian_compat((proj->Hit.Damage),		sizeof(int),1,fp);
		}
	}
	else { // newer GS version
		proj->Hit.write(this, fp);
	}




	//
	// Timer
	//
	if(proj->Timer.Time > 0) {
		if(Header.Version <= GS_LX56_VERSION) {
			fwrite_endian_compat((proj->Timer.Type),sizeof(int),1,fp);
			if(proj->Timer.Type == PJ_EXPLODE) {
				fwrite_endian_compat((proj->Timer.Damage),		sizeof(int),1,fp);
				fwrite_endian<int>(fp, proj->Timer.Projectiles);
				fwrite_endian_compat((proj->Timer.Shake),sizeof(int),1,fp);
			}
		}
		else {
			proj->Timer.write(this, fp);
		}
	}


	//
	// Player hit
	//
	if(Header.Version <= GS_LX56_VERSION) {
		fwrite_endian_compat((proj->PlyHit.Type),sizeof(int),1,fp);

		// PlyHit::Explode || PlyHit::Injure
		if(proj->PlyHit.Type == PJ_EXPLODE || proj->PlyHit.Type == PJ_INJURE) {
			fwrite_endian_compat((proj->PlyHit.Damage),sizeof(int),1,fp);
			fwrite_endian<int>(fp, proj->PlyHit.Projectiles);
		}

		// PlyHit::Bounce
		if(proj->PlyHit.Type == PJ_BOUNCE) {
			fwrite_endian_compat((proj->PlyHit.BounceCoeff),sizeof(float),1,fp);
		}
	}
	else { // newer GS version
		proj->PlyHit.write(this, fp);
	}

	
	if(Header.Version <= GS_LX56_VERSION) {
		// NOTE: this is obsolete, they are not used
		
		//
		// Explode
		//
		fwrite_endian_compat((proj->Exp.Type),     sizeof(int), 1, fp);
		fwrite_endian_compat((proj->Exp.Damage),   sizeof(int), 1, fp);
		fwrite_endian<int>(fp, proj->Exp.Projectiles);
		fwrite_endian<int>(fp, proj->Exp.UseSound);
		if(proj->Exp.UseSound)
			writeString(proj->Exp.SndFilename, fp);


		//
		// Touch
		//
		fwrite_endian_compat((proj->Tch.Type),     sizeof(int), 1, fp);
		fwrite_endian_compat((proj->Tch.Damage),   sizeof(int), 1, fp);
		fwrite_endian<int>(fp, proj->Tch.Projectiles);
		fwrite_endian<int>(fp, proj->Tch.UseSound);
		if(proj->Tch.UseSound)
			writeString(proj->Tch.SndFilename, fp);
	}
	
	if(Header.Version > GS_LX56_VERSION) {
		fwrite_endian<Uint32>(fp, (Uint32)proj->actions.size());
		for(Uint32 i = 0; i < proj->actions.size(); ++i) {
			proj->actions[i].write(this, fp);
		}
	}
	
	if(Header.Version > GS_LX56_VERSION) {
		// always save, there are many cases where we could possibly need it
		proj->GeneralSpawnInfo.write(this, fp);
	}
	else if(proj->Timer.Projectiles || proj->Hit.Projectiles || proj->PlyHit.Projectiles || proj->Exp.Projectiles ||
       proj->Tch.Projectiles) {
		fwrite_endian<int>(fp, proj->GeneralSpawnInfo.Useangle);
		fwrite_endian_compat((proj->GeneralSpawnInfo.Angle),	sizeof(int),	1, fp);
		
		fwrite_endian_compat((proj->GeneralSpawnInfo.Amount),	sizeof(int),	1, fp);
		fwrite_endian_compat((proj->GeneralSpawnInfo.Spread),	sizeof(float),	1, fp);
		fwrite_endian_compat((proj->GeneralSpawnInfo.Speed),	sizeof(int),	1, fp);
		fwrite_endian_compat((proj->GeneralSpawnInfo.SpeedVar),	sizeof(float),	1, fp);

		SaveProjectile(proj->GeneralSpawnInfo.Proj,fp);
	}


	// Projectile trail
	if(proj->Trail.Type == TRL_PROJECTILE) {

		fwrite_endian<int>(fp, proj->Trail.Proj.UseParentVelocityForSpread);
		fwrite_endian_compat((proj->Trail.Delay*1000.0f),				sizeof(float),	1, fp);
	
		if(Header.Version <= GS_LX56_VERSION) {
			fwrite_endian_compat((proj->Trail.Proj.Amount),			sizeof(int),	1, fp);
			fwrite_endian_compat((proj->Trail.Proj.Speed),				sizeof(int),	1, fp);
			fwrite_endian_compat((proj->Trail.Proj.SpeedVar),			sizeof(float),	1, fp);
			fwrite_endian_compat((proj->Trail.Proj.Spread),			sizeof(float),	1, fp);

			SaveProjectile(proj->Trail.Proj.Proj, fp);
		}
		else { // newer GS versions
			proj->Trail.Proj.write(this, fp);
		}
		
	}

	return true;
}



static bool checkLXBinMod(const std::string& dir, bool abs_filename, ModInfo& info) {
	// Open it
	FILE *fp = NULL;
	if(abs_filename) {
		fp = OpenAbsFile(dir + "/script.lgs", "rb");
	} else
		fp = OpenGameFile(dir + "/script.lgs", "rb");
	
	if(!fp) return false;
	
	// Header
	gs_header_t head = {};
	fread_compat(head,sizeof(gs_header_t),1,fp);
	fclose(fp);
	
	EndianSwap(head.Version);
	// for security
	fix_markend(head.ID);
	fix_markend(head.ModName);
	
	// Check ID
	if(strcmp(head.ID,"Liero Game Script") != 0) {
		warnings << "GS:CheckFile: WARNING: " << dir << "/script.lgs is not a Liero game script";
		warnings << " (but \"" << head.ID << "\" instead)" << endl;
		return false;
	}
	
	// Check version
	if(head.Version < GS_FIRST_SUPPORTED_VERSION || head.Version > GS_VERSION) {
		warnings << "GS:CheckFile: WARNING: " << dir << "/script.lgs has the wrong version";
		warnings << " (" << (unsigned)head.Version << ", required is in the range ";
		warnings << "[" << GS_FIRST_SUPPORTED_VERSION << "," << GS_VERSION << "])" << endl;
		return false;
	}
	
	info.valid = true;
	info.name = head.ModName;
	info.path = GetBaseFilename(dir);
	info.type = "LieroX mod";
	info.typeShort = "LX";
	
	return true;
}

static bool checkLXSourceMod(const std::string& dir, bool abs_filename, ModInfo& info) {
	// try source gamescript
	
	std::string filename = dir + "/main.txt";
	if(abs_filename) {
		if(!GetExactFileName(dir + "/main.txt", filename))
			return false;
	} else {
		if(!IsFileAvailable(filename))
			return false;
	}
	
	info.valid = true;
	ReadString(filename,"General","ModName", info.name.write(),"untitled", abs_filename);
	info.path = GetBaseFilename(dir);
	info.type = "LieroX mod source";
	info.typeShort = "LX src";
	
	return true;	
}

static bool checkGusMod(const std::string& dir, bool abs_filename, ModInfo& info) {
	std::string basefn = GetBaseFilename(dir);
	
	// TODO: absfn
	// there is no better way to check that
	if(IsDirectory(basefn + "/objects")) {
		info.valid = true;
		info.name = basefn;
		info.path = basefn;
		info.type = "Gusanos mod";
		info.typeShort = "Gus";
		
		return true;
	}
	
	return false;
}

///////////////////
// Check if a file is a valid LX game script
bool CGameScript::CheckFile(const std::string& dir, std::string& name, bool abs_filename, ModInfo* _i)
{
	ModInfo __modInfo;
	ModInfo& info = _i ? *_i : __modInfo;
	
	info = ModInfo();
	name = "";
	
	if(!IsDirectory(dir, abs_filename)) return false;
	if(checkLXBinMod(dir, abs_filename, info)) goto CheckFileEnd;
	if(checkLXSourceMod(dir, abs_filename, info)) goto CheckFileEnd;
	if(checkGusMod(dir, abs_filename, info)) goto CheckFileEnd;
	
CheckFileEnd:
	name = info.name;
	return info.valid;
}


///////////////////
// Load the game script from a file (game)
int CGameScript::Load(const std::string& dir, bool loadImagesAndSounds)
{	
	// Already cached externally
	/*
	// Try cache first
	CGameScript *cached = cCache.GetMod(dir);
	if (cached != NULL)  {
		CopyFrom(cached);
		return GSE_OK;
	}
	*/
	Shutdown();

	int n;
	std::string filename = dir + "/script.lgs";
	sDirectory = dir;

	// Open it
	FILE* fp = OpenGameFile(filename,"rb");
	if(fp == NULL) {
		if(IsFileAvailable(dir + "/main.txt")) {
			hints << "GameScript: '" << dir << "': loading from gamescript source" << endl;
			if(Compile(dir))
				return GSE_OK;
			else {
				warnings << "GameScript::Load(): could not compile source gamescript '" << dir << "'" << endl;
				return GSE_BAD;
			}
		}
		
		ModInfo info;
		if(checkGusMod(dir, false, info)) {
			// Note: In case of Gusanos, this will not actually load the mod.
			// The actuall loading happens mostly when we finish the map loading,
			// indirectly in game.loadMap(), and also in gusGame.runInitScripts().
			// See Game::prepareGameloop().
			// Note that this behavior might change!
			if(gusInit(info.path)) {
				m_gusEngineUsed = true;
				loaded = true;
				modname = info.name;				
				return GSE_OK;
			}
			
			warnings << "CGameScript::Load(): Could not load Gusanos mod " << filename << endl;
			return GSE_BAD;
		}
		
		warnings << "CGameScript::Load(): Could not load file " << filename << endl;
		return GSE_FILE;
	}

	// Header
	fread_compat(Header,sizeof(gs_header_t),1,fp);
	EndianSwap(Header.Version);
	// for security
	fix_markend(Header.ID);
	fix_markend(Header.ModName);

	// Check ID
	if(strcmp(Header.ID,"Liero Game Script") != 0) {
		fclose(fp);
		SetError("CGameScript::Load(): Bad script id");
		return GSE_BAD;
	}

	// Check version
	if(Header.Version < GS_FIRST_SUPPORTED_VERSION || Header.Version > GS_VERSION) {
		warnings << "GS:CheckFile: WARNING: " << dir << "/script.lgs has a wrong version";
		warnings << " (" << (unsigned)Header.Version << ", required is in the range ";
		warnings << "[" << GS_FIRST_SUPPORTED_VERSION << "," << GS_VERSION << "])" << endl;
		fclose(fp);
		SetError("CGameScript::Load(): Bad script version");
		return GSE_VERSION;
	}

	modname = Header.ModName;
	
    // Clear an old mod file
    modLog("Loading game mod file " + filename);
	//modLog("  ID = %s", Header.ID);
	//modLog("  Version = %i", Header.Version);

	fread_compat(NumWeapons,sizeof(int),1,fp);
	EndianSwap(NumWeapons);
	//modLog("  NumWeapons = %i", NumWeapons);

	// Do Allocations
	Weapons = new weapon_t[NumWeapons];
	if(Weapons == NULL) {
		SetError("SGameScript::Load(): Out of memory");
		return GSE_MEM;
	}

	// Weapons
	weapon_t *wpn;
	for(n=0;n<NumWeapons;n++) {
		wpn = &Weapons[n];

		wpn->ID = n;
		wpn->Proj.Proj = NULL;

        wpn->Name = readString(fp);
		fread_compat(wpn->Type,           sizeof(int),    1,fp);
		EndianSwap(wpn->Type);

		// Special
		if(wpn->Type == WPN_SPECIAL) {
			fread_compat(wpn->Special,    sizeof(int),    1, fp);
			EndianSwap(wpn->Special);
			fread_compat(wpn->tSpecial,   sizeof(gs_special_t), 1, fp);
			EndianSwap(wpn->tSpecial.Thrust);
			fread_compat(wpn->Recharge,   sizeof(float),  1, fp);
			EndianSwap(wpn->Recharge);
			fread_compat(wpn->Drain,      sizeof(float),  1, fp);
			EndianSwap(wpn->Drain);
			fread_compat(wpn->ROF,        sizeof(float),  1, fp);
			EndianSwap(wpn->ROF);
			fread_endian<int>(fp, wpn->LaserSight);
		}

		// Beam
		if(wpn->Type == WPN_BEAM) {
			fread_compat(wpn->Recoil,     sizeof(int),    1, fp);
			EndianSwap(wpn->Recoil);
			fread_compat(wpn->Recharge,   sizeof(float),  1, fp);
			EndianSwap(wpn->Recharge);
			fread_compat(wpn->Drain,      sizeof(float),  1, fp);
			EndianSwap(wpn->Drain);
			fread_compat(wpn->ROF,        sizeof(float),  1, fp);
			EndianSwap(wpn->ROF);
			fread_endian<int>(fp, wpn->LaserSight);

			wpn->Bm.read(this, fp);
		}

		// Projectile
		if(wpn->Type == WPN_PROJECTILE) {
			fread_compat(wpn->Class,sizeof(int),1,fp);
			EndianSwap(wpn->Class);
			fread_compat(wpn->Recoil,sizeof(int),1,fp);
			EndianSwap(wpn->Recoil);
			fread_compat(wpn->Recharge,sizeof(float),1,fp);
			EndianSwap(wpn->Recharge);
			fread_compat(wpn->Drain,sizeof(float),1,fp);
			EndianSwap(wpn->Drain);
			fread_compat(wpn->ROF,sizeof(float),1,fp);
			EndianSwap(wpn->ROF);

			if(Header.Version <= GS_LX56_VERSION) {
				fread_compat(wpn->Proj.Speed,sizeof(int),1,fp);
				EndianSwap(wpn->Proj.Speed);
				fread_compat(wpn->Proj.SpeedVar,sizeof(float),1,fp);
				EndianSwap(wpn->Proj.SpeedVar);
				fread_compat(wpn->Proj.Spread,sizeof(float),1,fp);
				EndianSwap(wpn->Proj.Spread);
				fread_compat(wpn->Proj.Amount,sizeof(int),1,fp);
				EndianSwap(wpn->Proj.Amount);
				fread_endian<int>(fp, wpn->LaserSight);

				fread_endian<int>(fp, wpn->UseSound);
				if(wpn->UseSound)
					wpn->SndFilename = readString(fp);

				wpn->Proj.Proj = LoadProjectile(fp, loadImagesAndSounds);
			}
			else { // newer GS versions
				fread_endian<char>(fp, wpn->LaserSight);
				
				fread_endian<char>(fp, wpn->UseSound);
				if(wpn->UseSound)
					wpn->SndFilename = readString(fp);
				
				wpn->Proj.read(this, fp);
			}

			if(!bDedicated && wpn->UseSound && loadImagesAndSounds) {
				// Load the sample
				wpn->smpSample = LoadGSSample(dir,wpn->SndFilename);

				if(wpn->smpSample == NULL)
					wpn->UseSound = false;
			}
		}

		wpn->ROF /= 1000.0f;
		wpn->Recharge /= 10.0f;
		
		if(Header.Version > GS_LX56_VERSION)
			wpn->FinalProj.read(this, fp);
	}


	// Extra stuff


	// Ninja Rope
	int RopeLength = 0;
	fread_compat(RopeLength,sizeof(int),1,fp);
	EndianSwap(RopeLength);
	int RestLength = 0;
	fread_compat(RestLength,sizeof(int),1,fp);
	EndianSwap(RestLength);
	float Strength = 0;
	fread_compat(Strength,sizeof(float),1,fp);
	EndianSwap(Strength);

	lx56modSettings.set(FT_RopeMaxLength) = RopeLength;
	lx56modSettings.set(FT_RopeRestLength) = RestLength;
	lx56modSettings.set(FT_RopeStrength) = Strength;
	
	// Worm
	gs_worm_t Worm;
	fread_compat(Worm, sizeof(gs_worm_t), 1, fp);
	EndianSwap(Worm.AngleSpeed);
	EndianSwap(Worm.GroundSpeed);
	EndianSwap(Worm.AirSpeed);
	EndianSwap(Worm.Gravity);
	EndianSwap(Worm.JumpForce);
	EndianSwap(Worm.AirFriction);
	EndianSwap(Worm.GroundFriction);

	lx56modSettings.set(FT_WormGroundSpeed) = Worm.GroundSpeed;
	lx56modSettings.set(FT_WormAirSpeed) = Worm.AirSpeed;
	lx56modSettings.set(FT_WormGravity) = Worm.Gravity;
	lx56modSettings.set(FT_WormJumpForce) = Worm.JumpForce;
	lx56modSettings.set(FT_WormAirFriction) = Worm.AirFriction;

	fclose(fp);

	// Already cached externally
	// Save to cache
	//cCache.SaveMod(dir, this);

	loaded = true;
	
	return GSE_OK;
}


///////////////////
// Load a projectile
proj_t *CGameScript::LoadProjectile(FILE *fp, bool loadImagesAndSounds)
{
	int projIndex = -1;
	if(Header.Version > GS_LX56_VERSION) {
		fread_endian<int>(fp, projIndex);
		std::map<int, proj_t*>::iterator f = projectiles.find(projIndex);
		if(f != projectiles.end())
			return f->second;
	}
	else
		projIndex = (int)projectiles.size();
		
	proj_t *proj = new proj_t;
	if(proj == NULL)
		return NULL;
	projectiles[projIndex] = proj;

	fread_compat(proj->Type,			sizeof(int),  1,fp);
	EndianSwap(proj->Type);
	fread_compat(proj->Timer.Time,	sizeof(float),1,fp);
	EndianSwap(proj->Timer.Time);
	fread_compat(proj->Timer.TimeVar,	sizeof(float),1,fp);
	EndianSwap(proj->Timer.TimeVar);
	fread_compat(proj->Trail.Type,			sizeof(int),  1,fp);
	EndianSwap(proj->Trail.Type);
	fread_endian<int>(fp, proj->UseCustomGravity);
	if(proj->UseCustomGravity)
	{
		fread_compat(proj->Gravity,	sizeof(int), 1, fp);
		EndianSwap(proj->Gravity);
	}
	fread_compat(proj->Dampening,		sizeof(int),  1, fp);
	EndianSwap(proj->Dampening);

	if(Header.Version > GS_LX56_VERSION) {
		fread_endian<int>(fp, proj->Width);
		fread_endian<int>(fp, proj->Height);
	}
	else {
		if(proj->Type == PRJ_PIXEL)
			proj->Width = proj->Height = 2;
		else
			proj->Width = proj->Height = 4;
	}
	
	proj->Trail.Proj.Proj = NULL;
	proj->GeneralSpawnInfo.Proj = NULL;
	proj->Hit.Projectiles = false;
	proj->Timer.Projectiles = false;
	proj->PlyHit.Projectiles = false;
	proj->Animating = false;
	proj->Rotating = false;
	proj->RotIncrement = 0;
	proj->Timer.Shake = 0;

	switch(proj->Type) {
		case PRJ_POLYGON: {
			proj->polygon.clear();
			Uint32 NumPoints = 0;
			fread_endian<Uint32>(fp, NumPoints);
			proj->polygon.startPointAdding();
			for(Uint32 i = 0; i < NumPoints; ++i) {
				VectorD2<int> p;
				fread_endian<int>(fp, p.x);
				fread_endian<int>(fp, p.y);
				proj->polygon.addPoint(p);
			}
			proj->polygon.endPointAdding();
		}
			// fallthrough to read color
		case PRJ_RECT:	
		case PRJ_CIRCLE:
		case PRJ_PIXEL:
		{
			Uint32 NumColours = 0;
			fread_endian<int>(fp, NumColours);
			proj->Colour.resize(NumColours);
			
			for(size_t i = 0; i < NumColours; ++i) {
				if(Header.Version <= GS_LX56_VERSION) {
					int color[3] = {0,0,0};
					fread_endian<int>(fp, color[0]);
					fread_endian<int>(fp, color[1]);
					fread_endian<int>(fp, color[2]);
					proj->Colour[i] = Color(color[0],color[1],color[2]);
				} else {
					fread_compat(proj->Colour[i].r,1,1,fp);
					fread_compat(proj->Colour[i].g,1,1,fp);
					fread_compat(proj->Colour[i].b,1,1,fp);
					fread_compat(proj->Colour[i].a,1,1,fp);
				}
			}
			break;
		}
		case PRJ_IMAGE:
			proj->ImgFilename = readString(fp);
		
			if(!bDedicated && loadImagesAndSounds) {
				proj->bmpImage = LoadGSImage(sDirectory, proj->ImgFilename);
				if(!proj->bmpImage)
					modLog("Could not open image '" + proj->ImgFilename + "'");
				else
					proj->bmpShadow = GenerateShadowSurface(proj->bmpImage);
			}
			
			fread_endian<int>(fp, proj->Rotating);
			fread_compat(proj->RotIncrement, sizeof(int), 1, fp);
			EndianSwap(proj->RotIncrement);
			fread_compat(proj->RotSpeed,sizeof(int),1,fp);
			EndianSwap(proj->RotSpeed);
			fread_endian<int>(fp, proj->UseAngle);
			fread_endian<int>(fp, proj->UseSpecAngle);
			if(proj->UseAngle || proj->UseSpecAngle)
			{
				fread_compat(proj->AngleImages,sizeof(int),1,fp);
				EndianSwap(proj->AngleImages);
			}
			fread_endian<int>(fp, proj->Animating);
			if(proj->Animating) {
				fread_compat(proj->AnimRate,sizeof(float),1,fp);
				EndianSwap(proj->AnimRate);
				fread_compat(proj->AnimType,sizeof(int),1,fp);
				EndianSwap(proj->AnimType);
			}
			break;
						
		case __PRJ_LBOUND: case __PRJ_UBOUND: errors << "LoadProjectile: PRJ BOUND" << endl;
	}


	//
	// Hit
	//
	if(Header.Version <= GS_LX56_VERSION) {
		fread_compat(proj->Hit.Type,sizeof(int),1,fp);
		EndianSwap(proj->Hit.Type);

		// Hit::Explode
		if(proj->Hit.Type == PJ_EXPLODE) {
			fread_compat(proj->Hit.Damage,		sizeof(int),1,fp);
			EndianSwap(proj->Hit.Damage);
			fread_endian<int>(fp, proj->Hit.Projectiles);
			fread_endian<int>(fp, proj->Hit.UseSound);
			fread_compat(proj->Hit.Shake,			sizeof(int),1,fp);
			EndianSwap(proj->Hit.Shake);

			if(proj->Hit.UseSound) {
				proj->Hit.SndFilename = readString(fp);
			}
		}

		// Hit::Bounce
		if(proj->Hit.Type == PJ_BOUNCE) {
			fread_compat(proj->Hit.BounceCoeff,	sizeof(float), 1, fp);
			EndianSwap(proj->Hit.BounceCoeff);
			fread_compat(proj->Hit.BounceExplode, sizeof(int),   1, fp);
			EndianSwap(proj->Hit.BounceExplode);
		}

		// Hit::Carve
		if(proj->Hit.Type == PJ_CARVE) {
			fread_compat(proj->Hit.Damage,		sizeof(int),1,fp);
			EndianSwap(proj->Hit.Damage);
		}

		if(!bDedicated && proj->Hit.UseSound && loadImagesAndSounds) {
			// Load the sample
			proj->Hit.Sound = LoadGSSample(sDirectory,proj->Hit.SndFilename);
			
			if(proj->Hit.Sound == NULL) {
				proj->Hit.UseSound = false;
				modLog("Could not open sound '" + proj->Hit.SndFilename + "'");
			}
		}		
	}
	else { // newer GS version
		proj->Hit.read(this, fp);
	}
		



	//
	// Timer
	//
	if(proj->Timer.Time > 0) {
		if(Header.Version <= GS_LX56_VERSION) {
			fread_compat(proj->Timer.Type,sizeof(int),1,fp);
			EndianSwap(proj->Timer.Type);
			if(proj->Timer.Type == PJ_EXPLODE) {
				fread_compat(proj->Timer.Damage,sizeof(int),1,fp);
				EndianSwap(proj->Timer.Damage);
				fread_endian<int>(fp, proj->Timer.Projectiles);
				fread_compat(proj->Timer.Shake,sizeof(int),1,fp);
				EndianSwap(proj->Timer.Shake);
			}
		}
		else {
			proj->Timer.read(this, fp);
		}
	}



	//
	// Player hit
	//
	if(Header.Version <= GS_LX56_VERSION) {
		fread_compat(proj->PlyHit.Type,sizeof(int),1,fp);
		EndianSwap(proj->PlyHit.Type);

		// PlyHit::Explode || PlyHit::Injure
		if(proj->PlyHit.Type == PJ_INJURE || proj->PlyHit.Type == PJ_EXPLODE) {
			fread_endian<int>(fp, proj->PlyHit.Damage);
			fread_endian<int>(fp, proj->PlyHit.Projectiles);
		}

		// PlyHit::Bounce
		if(proj->PlyHit.Type == PJ_BOUNCE) {
			fread_compat(proj->PlyHit.BounceCoeff, sizeof(float), 1, fp);
			EndianSwap(proj->PlyHit.BounceCoeff);
		}
	}
	else {
		proj->PlyHit.read(this, fp);
	}

	if(Header.Version <= GS_LX56_VERSION) {
		// NOTE: this is obsolete, it is used nowhere
		
		//
		// Explode
		//
		fread_compat(proj->Exp.Type,     sizeof(int), 1, fp);
		EndianSwap(proj->Exp.Type);
		fread_compat(proj->Exp.Damage,   sizeof(int), 1, fp);
		EndianSwap(proj->Exp.Damage);
		fread_endian<int>(fp, proj->Exp.Projectiles);
		fread_endian<int>(fp, proj->Exp.UseSound);
		if(proj->Exp.UseSound) {
			proj->Exp.SndFilename = readString(fp);
		}

		//
		// Touch
		//
		fread_compat(proj->Tch.Type,     sizeof(int), 1, fp);
		EndianSwap(proj->Tch.Type);
		fread_compat(proj->Tch.Damage,   sizeof(int), 1, fp);
		EndianSwap(proj->Tch.Damage);
		fread_endian<int>(fp, proj->Tch.Projectiles);
		fread_endian<int>(fp, proj->Tch.UseSound);
		if(proj->Tch.UseSound) {
			proj->Tch.SndFilename = readString(fp);
		}
	}
	
	if(Header.Version > GS_LX56_VERSION) {
		Uint32 projHitC = 0;
		fread_endian<Uint32>(fp, projHitC);
		proj->actions.resize(projHitC);
		for(Uint32 i = 0; i < projHitC; ++i) {
			proj->actions[i].read(this, fp);
		}
	}
	
	if(Header.Version > GS_LX56_VERSION) {
		proj->GeneralSpawnInfo.read(this, fp);
	}
	else if(proj->Timer.Projectiles || proj->Hit.Projectiles || proj->PlyHit.Projectiles || proj->Exp.Projectiles ||
       proj->Tch.Projectiles) {
		fread_endian<int>(fp, proj->GeneralSpawnInfo.Useangle);
		fread_compat(proj->GeneralSpawnInfo.Angle,		sizeof(int),	1, fp);
		EndianSwap(proj->GeneralSpawnInfo.Angle);
		fread_compat(proj->GeneralSpawnInfo.Amount,	sizeof(int),	1, fp);
		EndianSwap(proj->GeneralSpawnInfo.Amount);
		fread_compat(proj->GeneralSpawnInfo.Spread,	sizeof(float),	1, fp);
		EndianSwap(proj->GeneralSpawnInfo.Spread);
		fread_compat(proj->GeneralSpawnInfo.Speed,		sizeof(int),	1, fp);
		EndianSwap(proj->GeneralSpawnInfo.Speed);
		fread_compat(proj->GeneralSpawnInfo.SpeedVar,	sizeof(float),	1, fp);
		EndianSwap(proj->GeneralSpawnInfo.SpeedVar);

		proj->GeneralSpawnInfo.Proj = LoadProjectile(fp, loadImagesAndSounds);
	}


	// Projectile trail
	if(proj->Trail.Type == TRL_PROJECTILE) {

		fread_endian<int>(fp, proj->Trail.Proj.UseParentVelocityForSpread);
		fread_endian<float>(fp, proj->Trail.Delay);
		// Change from milli-seconds to seconds
		proj->Trail.Delay /= 1000.0f;
		
		if(Header.Version <= GS_LX56_VERSION) {
			fread_compat(proj->Trail.Proj.Amount,			sizeof(int),	1, fp);
			EndianSwap(proj->Trail.Proj.Amount);
			fread_compat(proj->Trail.Proj.Speed,			sizeof(int),	1, fp);
			EndianSwap(proj->Trail.Proj.Speed);
			fread_compat(proj->Trail.Proj.SpeedVar,		sizeof(float),	1, fp);
			EndianSwap(proj->Trail.Proj.SpeedVar);
			fread_compat(proj->Trail.Proj.Spread,			sizeof(float),	1, fp);
			EndianSwap(proj->Trail.Proj.Spread);

			proj->Trail.Proj.Proj = LoadProjectile(fp, loadImagesAndSounds);
		}
		else { // new GS versions
			proj->Trail.Proj.read(this, fp);
		}

	}

	return proj;
}

///////////////////
// Load an image
SDL_Surface * CGameScript::LoadGSImage(const std::string& dir, const std::string& filename)
{
	if(bDedicated) return NULL;

	SmartPointer<SDL_Surface> img = NULL;

	// First, check the gfx directory in the mod dir
	img = LoadGameImage(dir + "/gfx/" + filename, true);
	if(img.get())  {
		SetColorKey(img.get());
		CachedImages.push_back(img);
		return img.get();
	}

	// Check the gfx directory in the data dir
	img = LoadGameImage("data/gfx/" + filename, true);
	if(img.get())
	{ 
		SetColorKey(img.get());
		CachedImages.push_back(img);
	}
	return img.get();
}

///////////////////
// Load a sample
SoundSample * CGameScript::LoadGSSample(const std::string& dir, const std::string& filename)
{
	if(bDedicated) return NULL;
	
	SmartPointer<SoundSample> smp = NULL;

	// First, check the sfx directory in the mod dir
	smp = LoadSample(dir + "/sfx/" + filename, 10);

	if(smp.get())
	{
		CachedSamples.push_back(smp);
		return smp.get();
	}

	// Check the sounds directory in the data dir
	smp = LoadSample("data/sounds/" + filename, 10);
	if(smp.get())
		CachedSamples.push_back(smp);
	return smp.get();
}



///////////////////
// Find a weapon based on its name
const weapon_t *CGameScript::FindWeapon(const std::string& name)
{
	if(!Weapons) return NULL;
	if(name == "") return NULL;
	
	// Go through each weapon
	weapon_t *wpn = Weapons;
	for(int n=0;n<NumWeapons;n++,wpn++) {

		if(stringcaseequal(wpn->Name, name))
			return wpn;
	}

	return NULL;
}

int32_t CGameScript::FindWeaponId(const std::string &name) {
	const weapon_t* wpn = FindWeapon(name);
	if(wpn) return wpn->ID;
	return -1;
}


///////////////////
// Returns true if the weapon is in the game script
bool CGameScript::weaponExists(const std::string& szName)
{
    // Go through each weapon
	weapon_t *wpn = Weapons;
	for(int n=0;n<NumWeapons;n++,wpn++) {

		if(stringcasecmp(wpn->Name, szName) == 0)
			return true;
	}

    // Not found
    return false;
}



// Helper function
static size_t GetProjSize(proj_t *prj)
{
	if (prj)
		return 	prj->Exp.SndFilename.size() + prj->filename.size() +
				prj->Hit.SndFilename.size() + prj->ImgFilename.size() +
				prj->Tch.SndFilename.size() +
				/*GetSurfaceMemorySize(prj->bmpImage.get()) + */
				sizeof(proj_t);
	else
		return 0;
}

//////////////////
// Returns the memory occupied by this gamescript
size_t CGameScript::GetMemorySize()
{
	size_t res = sizeof(CGameScript) + sDirectory.size();
	weapon_t *it = Weapons;
	for (int i = 0; i < NumWeapons; i++, it++)  {
		res += sizeof(weapon_t) + sizeof(SoundSample);
		res += it->SndFilename.size();
	}
	for(Projectiles::iterator i = projectiles.begin(); i != projectiles.end(); ++i) {
		res += GetProjSize(i->second);		
	}
	
	return res;
}


///////////////////
// Shutdown the game script
void CGameScript::Shutdown()
{
	loaded = false;
	modname = "";
	m_gusEngineUsed = false;
	needCollisionInfo = false;
	
    // Close the log file
    if(pModLog) {
        fclose(pModLog);
        pModLog = NULL;
    }

	for(Projectiles::iterator i = projectiles.begin(); i != projectiles.end(); ++i) {
		ShutdownProjectile(i->second);
	}
	projectiles.clear();
	savedProjs.clear();
	projFileIndexes.clear();
	
	if(Weapons)
		delete[] Weapons;
	Weapons = NULL;
	
	lx56modSettings.makeSet(false);
}


///////////////////
// Shutdown a projectile
void CGameScript::ShutdownProjectile(proj_t *prj)
{
	if(prj) {
		delete prj;
	}
}


///////////////////
// Return an error message based on code
std::string CGameScript::getError(int code)
{
	std::string text = "Undefined error";

	switch(code) {

		case GSE_MEM:
			text = "Out of memory";
			break;

		case GSE_VERSION:
			text = "Incorrect version";
			break;

		case GSE_FILE:
			text = "Could not open file";
			break;

		case GSE_BAD:
			text = "Bad file format";
			break;
	}

	return text;
}


///////////////////
// Write info to a mod log file
void CGameScript::modLog(const std::string& text)
{
	notes << "modLog: " << text << endl;

	if(!pModLog) {
		pModLog = OpenGameFile("modlog.txt","wt");
		if(!pModLog)
			return;
		fprintf(pModLog,"Log file for mod:\n%s\n--------------------------------\n",Header.ModName);
	}

	if (text.size() != 0)
		fprintf(pModLog,"%s\n", text.c_str());
}

// Not needed with new caching system - game script won't ever change it's state during game
/*
///////////////////
// Copies infor from anothe gamescript
void CGameScript::CopyFrom(CGameScript *cg)
{
	sDirectory = cg->sDirectory;
	Header = cg->Header;
	NumWeapons = cg->NumWeapons;

	// HINT: only a pointer is copied here, because the weapon info does not change
	// so it would be wasting of memory if we copied the whole list
	Weapons = cg->Weapons;

	Worm = cg->Worm;
	RopeLength = cg->RopeLength;
	RestLength = cg->RestLength;
	Strength = cg->Strength;
}
*/

template <> void SmartPointer_ObjectDeinit<CGameScript> ( CGameScript * obj )
{
	delete obj;
}






static IniReader::KeywordList compilerKeywords;

void CGameScript::InitDefaultCompilerKeywords()
{
	if (compilerKeywords.empty())  {
		// Add some keywords
		// IMPORTANT: Every keyword (key) need to be different.
		compilerKeywords["WPN_PROJECTILE"] = WPN_PROJECTILE;
		compilerKeywords["WPN_SPECIAL"] = WPN_SPECIAL;
		compilerKeywords["WPN_BEAM"] = WPN_BEAM;
		
		compilerKeywords["WCL_AUTOMATIC"] = WCL_AUTOMATIC;
		compilerKeywords["WCL_POWERGUN"] = WCL_POWERGUN;
		compilerKeywords["WCL_GRENADE"] = WCL_GRENADE;
		compilerKeywords["WCL_MISSILE"] = WCL_MISSILE;
		compilerKeywords["WCL_CLOSERANGE"] = WCL_CLOSERANGE;
		
		compilerKeywords["PRJ_PIXEL"] = PRJ_PIXEL;
		compilerKeywords["PRJ_IMAGE"] = PRJ_IMAGE;
		compilerKeywords["PRJ_CIRCLE"] = PRJ_CIRCLE;
		compilerKeywords["PRJ_POLYGON"] = PRJ_POLYGON;
		compilerKeywords["PRJ_RECT"] =  PRJ_RECT;
		
		// action types
		compilerKeywords["Bounce"] = PJ_BOUNCE;
		compilerKeywords["Explode"] = PJ_EXPLODE;
		compilerKeywords["Injure"] = PJ_INJURE;
		compilerKeywords["InjureProj"] = PJ_INJUREPROJ;
		compilerKeywords["Carve"] = PJ_CARVE;
		compilerKeywords["Dirt"] = PJ_DIRT;
		compilerKeywords["GreenDirt"] = PJ_GREENDIRT;
		compilerKeywords["Disappear"] = PJ_DISAPPEAR;
		compilerKeywords["Nothing"] = PJ_NOTHING;
		compilerKeywords["Disappear2"] =  PJ_DISAPPEAR2;
		compilerKeywords["GoThrough"] =  PJ_GOTHROUGH;
		compilerKeywords["PlaySound"] =  PJ_PLAYSOUND;
		compilerKeywords["InjureWorm"] =  PJ_INJUREWORM;
		compilerKeywords["ChangeRadius"] = PJ_ChangeRadius;
		compilerKeywords["OverwriteOwnSpeed"] =  PJ_OverwriteOwnSpeed;
		compilerKeywords["MultiplyOwnSpeed"] =  PJ_MultiplyOwnSpeed;
		compilerKeywords["DiffOwnSpeed"] =  PJ_DiffOwnSpeed;
		compilerKeywords["OverwriteTargetSpeed"] =  PJ_OverwriteTargetSpeed;
		compilerKeywords["MultiplyTargetSpeed"] =  PJ_MultiplyTargetSpeed;
		compilerKeywords["DiffTargetSpeed"] =  PJ_DiffTargetSpeed;
		compilerKeywords["HeadingToNextWorm"] =  PJ_HeadingToNextWorm;
		compilerKeywords["HeadingToOwner"] =  PJ_HeadingToOwner;
		compilerKeywords["HeadingToNextOtherWorm"] =  PJ_HeadingToNextOtherWorm;
		compilerKeywords["HeadingToNextEnemyWorm"] =  PJ_HeadingToNextEnemyWorm;
		compilerKeywords["HeadingToNextTeamMate"] =  PJ_HeadingToNextTeamMate;
		compilerKeywords["HeadTargetToUs"] =  PJ_HeadTargetToUs;
		
		// event types
		compilerKeywords["Timer"] =  Proj_Event::PET_TIMER; 
		compilerKeywords["ProjHit"] =  Proj_Event::PET_PROJHIT; 
		compilerKeywords["WormHit"] =  Proj_Event::PET_WORMHIT; 
		compilerKeywords["TerrainHit"] =  Proj_Event::PET_TERRAINHIT; 
		compilerKeywords["Death"] =  Proj_Event::PET_DEATH; 
		compilerKeywords["Fallback"] =  Proj_Event::PET_FALLBACK; 
		
		// trail types
		compilerKeywords["TRL_NONE"] = TRL_NONE;
		compilerKeywords["TRL_SMOKE"] = TRL_SMOKE;
		compilerKeywords["TRL_CHEMSMOKE"] = TRL_CHEMSMOKE;
		compilerKeywords["TRL_PROJECTILE"] = TRL_PROJECTILE;
		compilerKeywords["TRL_DOOMSDAY"] = TRL_DOOMSDAY;
		compilerKeywords["TRL_EXPLOSIVE"] = TRL_EXPLOSIVE;
		
		compilerKeywords["SPC_JETPACK"] = SPC_JETPACK;
		
		compilerKeywords["ANI_ONCE"] = ANI_ONCE;
		compilerKeywords["ANI_LOOP"] = ANI_LOOP;
		compilerKeywords["ANI_PINGPONG"] = ANI_PINGPONG;
			
		compilerKeywords["ATO_NONE"] = ATO_NONE;
		compilerKeywords["ATO_PLAYERS"] = ATO_PLAYERS;
		compilerKeywords["ATO_PROJECTILES"] = ATO_PROJECTILES;
		compilerKeywords["ATO_ROPE"] = ATO_ROPE;
		compilerKeywords["ATO_BONUSES"] = ATO_BONUSES;
		compilerKeywords["ATO_ALL"] = ATO_ALL;
		
		compilerKeywords["ATC_NONE"] = ATC_NONE;
		compilerKeywords["ATC_OWNER"] = ATC_OWNER;
		compilerKeywords["ATC_ENEMY"] = ATC_ENEMY;
		compilerKeywords["ATC_TEAMMATE"] = ATC_TEAMMATE;
		compilerKeywords["ATC_ALL"] = ATC_ALL;
		
		compilerKeywords["ATT_GRAVITY"] = ATT_GRAVITY;
		compilerKeywords["ATT_CONSTANT"] = ATT_CONSTANT;
		compilerKeywords["ATT_LINEAR"] = ATT_LINEAR;
		compilerKeywords["ATT_QUADRATIC"] = ATT_QUADRATIC;
		
		compilerKeywords["true"] = true;
		compilerKeywords["false"] = false;
	}
}

///////////////////
// Compile
bool CGameScript::Compile(const std::string& dir)
{
	Shutdown();
	
	CGameScript* Game = this;

	InitDefaultCompilerKeywords();
	IniReader ini(dir + "/Main.txt", compilerKeywords);

	if (!ini.Parse())  {
		errors << "Error while parsing the gamescript " << dir << endl;
		return false;
	}
	


	sDirectory = dir;
	
	int num,n;

	ini.ReadString("General","ModName", modname,"untitled");
	fix_strncpy(Header.ModName, modname.c_str());

	notes << "Compiling '" << modName() << "'" << endl;

	ini.ReadInteger("Weapons","NumWeapons",&num,0);


	// Weapons
	Game->initNewWeapons(num);


	// Compile the weapons
	for(n=0;n<Game->GetNumWeapons();n++) {
		std::string wpn = "Weapon" + itoa(n+1);

		std::string weap;
		ini.ReadString("Weapons", wpn, weap, "");

		if(!CompileWeapon(dir, weap, n))
			return false;
	}

	// Compile the extra stuff
	CompileExtra(ini);

	loaded = true;
	
	return true;
}


///////////////////
// Compile a weapon
bool CGameScript::CompileWeapon(const std::string& dir, const std::string& weapon, int id)
{
	CGameScript* Game = this;
	
	weapon_t *Weap = Game->Weapons+id;
	IniReader ini(dir + "/" + weapon, compilerKeywords);
	if (!ini.Parse())  {
		errors << "Error while parsing weapon file " << weapon << endl;
		return false;
	}

	Weap->ID = id;
	Weap->Proj.Proj = NULL;
	Weap->UseSound = false;
	Weap->Special = SPC_NONE;
	Weap->Type = WPN_PROJECTILE;
	Weap->Proj.Proj = NULL;
	Weap->LaserSight = false;

	ini.ReadString("General", "Name", Weap->Name, "");
	notes << "  Compiling Weapon '" << Weap->Name << "'" << endl;

	ini.ReadKeyword("General", "Type", (int *)&Weap->Type, WPN_PROJECTILE);

	
	
	// Special Weapons
	if(Weap->Type == WPN_SPECIAL) {
		
		ini.ReadKeyword("General", "Special", (int*)&Weap->Special, SPC_NONE);

		// If it is a special weapon, read the values depending on the special weapon
		// We don't bother with the 'normal' values
		switch(Weap->Special) {
			// Jetpack
			case SPC_JETPACK:
				CompileJetpack(ini, Weap);
				break;

			default:
				notes << "   Error: Unknown special type" << endl;
		}
		return true;
	}


	// Beam Weapons
	if(Weap->Type == WPN_BEAM) {

		CompileBeam(ini, Weap);
		return true;
	}


	// Projectile Weapons
	ini.ReadKeyword("General","Class",(int*)&Weap->Class,WCL_AUTOMATIC);
	ini.ReadInteger("General","Recoil",&Weap->Recoil,0);
	ini.ReadFloat("General","Recharge",&Weap->Recharge,0); Weap->Recharge /= 10.0f;
	ini.ReadFloat("General","Drain",&Weap->Drain,0);
	ini.ReadFloat("General","ROF",&Weap->ROF,0); Weap->ROF /= 1000.0f;
	ini.ReadKeyword("General", "LaserSight", &Weap->LaserSight, false);
	if(ini.ReadString("General","Sound",Weap->SndFilename,"")) {
		Weap->UseSound = true;
	
		if(!bDedicated) {
			// Load the sample
			Weap->smpSample = LoadGSSample(dir,Weap->SndFilename);
		}
	}
	
	Weap->Proj.readFromIni(this, dir, ini, "Projectile");
	
	if(Weap->Proj.UseParentVelocityForSpread) {
		warnings << "UseProjVelocity is set in Projectile-section (" << weapon << "); this was not supported in LX56 thus we ignore it" << endl;
		Weap->Proj.UseParentVelocityForSpread = false;
	}
	
	if(!Weap->Proj.Useangle) {
		warnings << "Useangle is set in Projectile-section (" << weapon << "); this was not supported in LX56 thus we ignore it" << endl;
		Weap->Proj.Useangle = true;
	}

	if(Weap->Proj.Angle != 0) {
		warnings << "Angle is set in Projectile-section (" << weapon << "); this was not supported in LX56 thus we ignore it" << endl;
		Weap->Proj.Angle = 0;
	}	

	if(Weap->Proj.Proj == NULL) {
		warnings << "projectile not set for weapon " << weapon << endl;
		return false;
	}
	
	Weap->FinalProj.readFromIni(this, dir, ini, "FinalProj");
	
	return true;
}


///////////////////
// Compile a beam weapon
void CGameScript::CompileBeam(const IniReader& ini, weapon_t *Weap)
{
	ini.ReadInteger("General","Recoil",&Weap->Recoil,0);
	ini.ReadFloat("General","Recharge",&Weap->Recharge,0); Weap->Recharge /= 10.0f;
	ini.ReadFloat("General","Drain",&Weap->Drain,0);
	ini.ReadFloat("General","ROF",&Weap->ROF,0); Weap->ROF /= 1000.0f;
	if(ini.ReadString("General","Sound",Weap->SndFilename,""))
		Weap->UseSound = true;

	Weap->Bm.readFromIni(ini, "Beam");
}


///////////////////
// Compile a projectile
proj_t *CGameScript::CompileProjectile(const std::string& dir, const std::string& pfile)
{
	ProjFileMap::iterator f = projFileIndexes.find(pfile);
	if(f != projFileIndexes.end()) {
		notes << "    Reuse already compiled Projectile '" << pfile << "'" << endl;
		return projectiles[f->second];
	}

	proj_t *proj = new proj_t;
	if(proj == NULL)
		return NULL;

	int projIndex = (int)projectiles.size();
	projectiles[projIndex] = proj;	
	projFileIndexes[pfile] = projIndex;
	
	// Load the projectile
	IniReader ini(dir + "/" + pfile, compilerKeywords);
	notes << "    Compiling Projectile '" << pfile << "'" << endl;
	
	proj->filename = pfile;
	
	proj->Timer.Projectiles = false;
	proj->Hit.Projectiles = false;
	proj->PlyHit.Projectiles = false;
	proj->Exp.Projectiles = false;
	proj->Tch.Projectiles = false;
	proj->GeneralSpawnInfo.Proj = NULL;
	proj->Trail.Proj.Proj = NULL;
	proj->Animating = false;
	proj->UseCustomGravity = false;

	if (!ini.Parse())  {
		warnings << "projectile file " << pfile << " could not be parsed, using defaults..." << endl;
		return proj;
	}

	ini.ReadKeyword("General","Type",(int*)&proj->Type,PRJ_PIXEL);
	ini.ReadFloat("General","Timer",&proj->Timer.Time,0);
	ini.ReadFloat("General", "TimerVar", &proj->Timer.TimeVar, 0);
	ini.ReadKeyword("General","Trail",(int*)&proj->Trail.Type,TRL_NONE);
	
	if (ini.ReadInteger("General","Gravity",&proj->Gravity, 0))
		proj->UseCustomGravity = true;

	ini.ReadFloat("General","Dampening",&proj->Dampening,1.0f);

	if(proj->Type == PRJ_PIXEL)
		proj->Width = proj->Height = 2;
	else
		proj->Width = proj->Height = 4;
	
	ini.ReadInteger("General", "Width", &proj->Width, proj->Width);
	ini.ReadInteger("General", "Height", &proj->Height, proj->Height);
	
	proj->Colour.clear();
	proj->polygon.clear();
	switch(proj->Type) {
		case PRJ_POLYGON:
			proj->polygon.startPointAdding();
			for(size_t i = 0; ; ++i) {				
				VectorD2<int> p;
				if(ini.ReadVectorD2("General", "P" + itoa((unsigned)i+1), p) ) {		
					proj->polygon.addPoint(p);
				} else
					break;
			}
			proj->polygon.endPointAdding();

			if(proj->polygon.getPoints().empty()) {
				warnings << "no points specified for PRJ_POLYGON projectile " << pfile << "; fallback to PRJ_PIXEL" << endl;
				proj->Type = PRJ_PIXEL;
			}
			
			// fallthrough to read color
		case PRJ_RECT:	
		case PRJ_CIRCLE:
		case PRJ_PIXEL:
			for(size_t i = 0; ; ++i) {
				Color col;
				if(ini.ReadColour("General","Colour" + itoa((unsigned)i+1), col, Color()) ) {
					proj->Colour.push_back(col);
				} else
					break;
			}
			if(proj->Colour.size() == 0) {
				warnings << "no colors specified for projectile " << pfile << ", using black" << endl;
				proj->Colour.push_back(Color());
			}
			break;
			
		case PRJ_IMAGE:
			ini.ReadString("General","Image",proj->ImgFilename,"");
			ini.ReadKeyword("General","Rotating",&proj->Rotating,false);
			ini.ReadInteger("General","RotIncrement",&proj->RotIncrement,0);
			ini.ReadInteger("General","RotSpeed",&proj->RotSpeed,0);
			ini.ReadKeyword("General","UseAngle",&proj->UseAngle,0);
			ini.ReadKeyword("General","UseSpecAngle",&proj->UseSpecAngle,0);
			if(proj->UseAngle || proj->UseSpecAngle)
				ini.ReadInteger("General","AngleImages",&proj->AngleImages,0);

			ini.ReadKeyword("General","Animating",&proj->Animating,0);
			if(proj->Animating) {
				ini.ReadFloat("General","AnimRate",&proj->AnimRate,0);
				ini.ReadKeyword("General","AnimType",(int*)&proj->AnimType,ANI_ONCE);
			}
	
			if(!bDedicated) {
				proj->bmpImage = LoadGSImage(dir, proj->ImgFilename);
				if(!proj->bmpImage)
					modLog("Could not open image '" + proj->ImgFilename + "'");
				else
					proj->bmpShadow = GenerateShadowSurface(proj->bmpImage);
			}
			break;
			
		case __PRJ_LBOUND: case __PRJ_UBOUND: errors << "PRJ BOUND err" << endl;
	}

	// general Projectile spawn info
	{
		proj->GeneralSpawnInfo.readFromIni(this, dir, ini, "Projectile");
		
		if(proj->GeneralSpawnInfo.UseParentVelocityForSpread) {
			warnings << "UseProjVelocity is set in Projectile-section (" << pfile << "); this was not supported in LX56 thus we ignore it" << endl;
			proj->GeneralSpawnInfo.UseParentVelocityForSpread = false;
		}
	}
	
	proj->Hit.readFromIni(this, dir, ini, "Hit");
	
	if(proj->Hit.needGeneralSpawnInfo() && !proj->GeneralSpawnInfo.isSet()) {
		warnings << dir << "/" << pfile << ": Hit section wants to spawn projectiles but there is no spawning information" << endl;
		proj->Hit.Projectiles = false;
	}
	
	// Timer	
	if(proj->Timer.Time > 0) {
		proj->Timer.readFromIni(this, dir, ini, "Time");

		if(proj->Timer.needGeneralSpawnInfo() && !proj->GeneralSpawnInfo.isSet()) {
			warnings << dir << "/" << pfile << ": Timer section wants to spawn projectiles but there is no spawning information" << endl;
			proj->Timer.Projectiles = false;
		}		
	}

	// Player hit
	proj->PlyHit.Type = PJ_INJURE;
	proj->PlyHit.readFromIni(this, dir, ini, "PlayerHit");
	
	if(proj->PlyHit.Shake != 0) {
		warnings << "projectile " << ini.getFileName() << " has PlayerHit.Shake != 0 which was not supported earlier. this is ignored" << endl;
		proj->PlyHit.Shake = 0;
	}
	
	if(proj->PlyHit.BounceExplode != 0) {
		warnings << "projectile " << ini.getFileName() << " has PlayerHit.BounceExplode != 0 which was not supported earlier. this is ignored" << endl;
		proj->PlyHit.BounceExplode = 0;
	}
	
	// If PJ_PLAYSOUND is set, we support it because PJ_PLAYSOUND didn't existed earlier.
	// But you could also just use a custom action to play the sound.
	if(proj->PlyHit.UseSound && proj->PlyHit.Type != PJ_PLAYSOUND) {
		warnings << "projectile " << ini.getFileName() << " has sound set, which was not supported earlier, thus it's ignored now" << endl;
		proj->PlyHit.UseSound = false;
	}

	if(proj->PlyHit.needGeneralSpawnInfo() && !proj->GeneralSpawnInfo.isSet()) {
		warnings << dir << "/" << pfile << ": PlayerHit section wants to spawn projectiles but there is no spawning information" << endl;
		proj->PlyHit.Projectiles = false;
	}
	
	/*
    // OnExplode
	ReadKeyword( file, "Explode", "Type",   (int*)&proj->Exp.Type, PJ_NOTHING );
	ReadInteger( file, "Explode", "Damage",     &proj->Exp.Damage, 0 );
	ReadKeyword( file, "Explode", "Projectiles",&proj->Exp.Projectiles, false );
	ReadInteger( file, "Explode", "Shake",      &proj->Exp.Shake, 0 );
	proj->Exp.UseSound = false;
	if( ReadString(file, "Explode", "Sound", proj->Exp.SndFilename,"") ) {
		proj->Exp.UseSound = true;
	}

    // Touch
	ReadKeyword( file, "Touch", "Type",   (int*)&proj->Tch.Type, PJ_NOTHING );
	ReadInteger( file, "Touch", "Damage",     &proj->Tch.Damage, 0 );
	ReadKeyword( file, "Touch", "Projectiles",&proj->Tch.Projectiles, false );
	ReadInteger( file, "Touch", "Shake",      &proj->Tch.Shake, 0 );
	proj->Tch.UseSound = false;
	if( ReadString(file, "Touch", "Sound", proj->Tch.SndFilename,"") )
		proj->Tch.UseSound = true;
	 */
	
	{
		int projHitC = 0;
		ini.ReadInteger("General", "ActionNum", &projHitC, 0);
		for(int i = 0; i < projHitC; ++i) {
			Proj_EventAndAction act;
			if(!act.readFromIni(this, dir, ini, "Action" + itoa(i+1))) continue;
			
			if(act.needGeneralSpawnInfo() && !proj->GeneralSpawnInfo.isSet()) {
				warnings << dir << "/" << pfile << ": Action" << (i+1) << " section wants to spawn projectiles but there is no spawning information" << endl;
				act.Projectiles = false;
			}

			if(!act.hasAction()) {
				warnings << "section Action" << (i+1) << " (" << pfile << ") doesn't have any effect" << endl;
				continue;
			}
			
			proj->actions.push_back(act);
		}
	}	
	
	// Projectile trail
	if(proj->Trail.Type == TRL_PROJECTILE) {
		ini.ReadFloat("ProjectileTrail", "Delay",  &proj->Trail.Delay, 100); proj->Trail.Delay /= 1000.0f;

		// we have some other default values here
		proj->Trail.Proj.Amount = 1;
		proj->Trail.Proj.Speed = 100;
		
		proj->Trail.Proj.readFromIni(this, dir, ini, "ProjectileTrail");
		
		if(proj->Trail.Proj.Useangle) {
			warnings << "Useangle is set in ProjectileTrail-section (" << pfile << "); this was not supported in LX56 thus we ignore it" << endl;
			proj->Trail.Proj.Useangle = false;
		}
		
		if(proj->Trail.Proj.Angle != 0) {
			warnings << "Angle is set in ProjectileTrail-section (" << pfile << "); this was not supported in LX56 thus we ignore it" << endl;
			proj->Trail.Proj.Useangle = false;
		}
		
		if(proj->Trail.Type == TRL_PROJECTILE && !proj->Trail.Proj.isSet()) {
			warnings << dir << "/" << pfile << ": ProjectileTrail section wants to spawn projectiles but there is no spawning information" << endl;
			proj->Trail.Type = TRL_NONE;			
		}
	}

	return proj;
}


///////////////////
// Compile the extra stuff
bool CGameScript::CompileExtra(const IniReader& ini)
{
	CGameScript* Game = this;
	
	notes << "   Compiling Extras" << endl;

	// Ninja Rope
	notes << "  Compiling Ninja Rope" << endl;

	int ropel, restl;
	float strength;

	ini.ReadInteger("NinjaRope","RopeLength",&ropel,0);
	ini.ReadInteger("NinjaRope","RestLength",&restl,0);
	ini.ReadFloat("NinjaRope","Strength",&strength,0);

	Game->lx56modSettings.set(FT_RopeMaxLength) = ropel;
	Game->lx56modSettings.set(FT_RopeRestLength) = restl;
	Game->lx56modSettings.set(FT_RopeStrength) = strength;


	// Worm
	notes << "  Compiling Worm" << endl;
	gs_worm_t wrm;

	ini.ReadFloat( "Worm", "AngleSpeed",		&wrm.AngleSpeed, 150);
	ini.ReadFloat( "Worm", "GroundSpeed",		&wrm.GroundSpeed, 8);
	ini.ReadFloat( "Worm", "AirSpeed",		&wrm.AirSpeed, 1);
	ini.ReadFloat( "Worm", "Gravity",			&wrm.Gravity, 175);
	ini.ReadFloat( "Worm", "JumpForce",		&wrm.JumpForce, -140);
	ini.ReadFloat( "Worm", "AirFriction",		&wrm.AirFriction, 0);
	ini.ReadFloat( "Worm", "GroundFriction",	&wrm.GroundFriction, 0.6f);

	Game->lx56modSettings.set(FT_WormGroundSpeed) = wrm.GroundSpeed;
	Game->lx56modSettings.set(FT_WormAirSpeed) = wrm.AirSpeed;
	Game->lx56modSettings.set(FT_WormGravity) = wrm.Gravity;
	Game->lx56modSettings.set(FT_WormJumpForce) = wrm.JumpForce;
	Game->lx56modSettings.set(FT_WormAirFriction) = wrm.AirFriction;

	return true;
}


/*
===============================

		Special items

===============================
*/


///////////////////
// Compile the jetpack
bool CGameScript::CompileJetpack(const IniReader& ini, weapon_t *Weap)
{
	Weap->Proj.Proj = NULL;

	ini.ReadInteger("JetPack", "Thrust", (int*)&Weap->tSpecial.Thrust, 0);
	ini.ReadFloat("JetPack", "Drain", &Weap->Drain, 0);
	ini.ReadFloat("JetPack", "Recharge", &Weap->Recharge, 0);	Weap->Recharge /= 10.0f;

	Weap->ROF = 1.0f / 1000.0f;

	return true;
}


bool Proj_SpawnInfo::readFromIni(CGameScript* gs, const std::string& dir, const IniReader& ini, const std::string& section) {
	ini.ReadKeyword(section, "AddParentVel", &AddParentVel, AddParentVel); // new in OLX beta9
	ini.ReadMatrixD2(section, "ParentVelFactor", ParentVelFactor, ParentVelFactor); // new in OLX beta9
	ini.ReadVectorD2(section, "PosDiff", PosDiff, PosDiff); // new in OLX beta9
	ini.ReadVectorD2(section, "SnapToGrid", SnapToGrid, SnapToGrid); // new in OLX beta9
	
	ini.ReadKeyword(section, "Useangle", &Useangle, Useangle);
	ini.ReadInteger(section, "Angle", &Angle, Angle);
	
	ini.ReadKeyword(section, "UseProjVelocity", &UseParentVelocityForSpread, UseParentVelocityForSpread);
	
	ini.ReadInteger(section, "Amount", &Amount, Amount);
	ini.ReadInteger(section, "Speed",  &Speed, Speed);
	ini.ReadFloat(section, "SpeedVar",  &SpeedVar, SpeedVar);
	ini.ReadFloat(section, "Spread", &Spread, Spread);
	
	std::string prjfile;
	ini.ReadString(section, "Projectile", prjfile, "");
	if (prjfile.size()) 
		Proj = gs->CompileProjectile(dir, prjfile);
	else 
		Proj = NULL;
	
	return true;
}

bool Proj_SpawnInfo::read(CGameScript* gs, FILE* fp) {
	if(gs->GetHeader()->Version <= GS_LX56_VERSION) {
		errors << "Proj_SpawnInfo::read called for old GS version" << endl;
		return false;
	}

	fread_endian<char>(fp, AddParentVel);
	fread_endian_M<float>(fp, ParentVelFactor);
	fread_endian_V<int>(fp, PosDiff);
	fread_endian_V<int>(fp, SnapToGrid);
	fread_endian<char>(fp, Useangle);
	fread_endian<int>(fp, Angle);
	fread_endian<int>(fp, Amount);
	fread_endian<int>(fp, Speed);
	fread_endian<float>(fp, SpeedVar);
	fread_endian<float>(fp, Spread);
	Proj = gs->LoadProjectile(fp);
	return Proj != NULL;
}

bool Proj_SpawnInfo::write(CGameScript* gs, FILE* fp) {
	if(gs->GetHeader()->Version <= GS_LX56_VERSION) {
		errors << "Proj_SpawnInfo::write called for old GS version" << endl;
		return false;
	}

	fwrite_endian<char>(fp, AddParentVel);
	fwrite_endian_M<float>(fp, ParentVelFactor);
	fwrite_endian_V<int>(fp, PosDiff);
	fwrite_endian_V<int>(fp, SnapToGrid);
	fwrite_endian<char>(fp, Useangle);
	fwrite_endian<int>(fp, Angle);
	fwrite_endian<int>(fp, Amount);
	fwrite_endian<int>(fp, Speed);
	fwrite_endian<float>(fp, SpeedVar);
	fwrite_endian<float>(fp, Spread);
	return gs->SaveProjectile(Proj, fp);
}


bool Wpn_Beam::readFromIni(const IniReader& ini, const std::string& section) {
	ini.ReadInteger(section, "Damage", &Damage, Damage);
	ini.ReadInteger(section, "Length", &Length, Length);
	ini.ReadInteger(section, "PlayerDamage", &PlyDamage, PlyDamage);
	ini.ReadColour(section, "Colour", Colour, Colour);
	ini.ReadInteger(section, "InitWidth", &InitWidth, InitWidth);
	ini.ReadFloat(section, "WidthIncrease", &WidthIncrease, WidthIncrease);
	ini.ReadKeyword(section, "DistributeDamageOverWidth", &DistributeDamageOverWidth, DistributeDamageOverWidth);
	return true;
}

bool Wpn_Beam::read(CGameScript* gs, FILE* fp) {
	if(gs->GetHeader()->Version <= GS_LX56_VERSION) {
		int r=0,g=0,b=0;
		fread_endian<int>(fp, r);
		fread_endian<int>(fp, g);
		fread_endian<int>(fp, b);
		Colour = Color(r,g,b);
	}
	else {
		fread_compat(Colour.r, 1,  1, fp);
		fread_compat(Colour.g, 1,  1, fp);
		fread_compat(Colour.b, 1,  1, fp);
		fread_compat(Colour.a, 1,  1, fp);
	}
	
	fread_endian<int>(fp, Damage);
	fread_endian<int>(fp, PlyDamage);
	fread_endian<int>(fp, Length);

	if(gs->GetHeader()->Version > GS_LX56_VERSION) {
		fread_endian<int>(fp, InitWidth);
		fread_endian<float>(fp, WidthIncrease);
		fread_endian<char>(fp, DistributeDamageOverWidth);
	}
	
	return true;
}

bool Wpn_Beam::write(CGameScript* gs, FILE* fp) {	
	if(gs->GetHeader()->Version <= GS_LX56_VERSION) {
		fwrite_endian<int>(fp, Colour.r);
		fwrite_endian<int>(fp, Colour.g);
		fwrite_endian<int>(fp, Colour.b);
	}
	else {
		fwrite_endian<Uint8>(fp, Colour.r);
		fwrite_endian<Uint8>(fp, Colour.g);
		fwrite_endian<Uint8>(fp, Colour.b);
		fwrite_endian<Uint8>(fp, Colour.a);
	}
	
	fwrite_endian<int>(fp, Damage);
	fwrite_endian<int>(fp, PlyDamage);
	fwrite_endian<int>(fp, Length);

	if(gs->GetHeader()->Version > GS_LX56_VERSION) {
		fwrite_endian<int>(fp, InitWidth);
		fwrite_endian<float>(fp, WidthIncrease);
		fwrite_endian<char>(fp, DistributeDamageOverWidth);
	}
	
	return true;
}


bool Proj_Action::readFromIni(CGameScript* gs, const std::string& dir, const IniReader& ini, const std::string& section, int deepCounter) {
	ini.ReadKeyword(section, "Type", (int*)&Type, Type);
	ini.ReadKeyword(section,"Projectiles",&Projectiles,false);
	ini.ReadInteger(section,"Damage",&Damage,Damage);
	ini.ReadInteger(section,"Shake",&Shake,Shake);
		
	UseSound = false;
	if(ini.ReadString(section,"Sound",SndFilename,"")) {
		UseSound = true;

		if(!bDedicated) {
			// Load the sample
			Sound = gs->LoadGSSample(dir, SndFilename);
			
			if(Sound == NULL)
				gs->modLog(ini.getFileName() + ":" + section + ": Could not open sound '" + SndFilename + "'");
		}
	}
	
	ini.ReadFloat(section,"BounceCoeff",&BounceCoeff,BounceCoeff);
	ini.ReadInteger(section,"BounceExplode",&BounceExplode,BounceExplode);

	ini.ReadFloat(section,"GoThroughSpeed",&GoThroughSpeed,GoThroughSpeed);	
	ini.ReadVectorD2(section, "ChangeRadius", ChangeRadius, ChangeRadius);
	
	switch(Type) {
		case PJ_OverwriteOwnSpeed:
		case PJ_DiffOwnSpeed:
		case PJ_OverwriteTargetSpeed:
		case PJ_DiffTargetSpeed:
			if(!ini.ReadVectorD2(section, "Speed", Speed, Speed)) {
				warnings << "Speed attribute missing in " << dir << "/" << ini.getFileName() << ":" << section << endl;
			}
		default: break;
	}			
	switch(Type) {
		case PJ_MultiplyOwnSpeed:
		case PJ_MultiplyTargetSpeed:
		case PJ_HeadingToNextWorm:
		case PJ_HeadingToOwner:
		case PJ_HeadingToNextOtherWorm:
		case PJ_HeadingToNextEnemyWorm:
		case PJ_HeadingToNextTeamMate:
		case PJ_HeadTargetToUs:
			if(!ini.ReadMatrixD2(section, "SpeedMult", SpeedMult, SpeedMult)) {
				warnings << "SpeedMult attribute missing in " << dir << "/" << ini.getFileName() << ":" << section << endl;
			}
		default:
			// Read it as well but don't complain when missing
			ini.ReadMatrixD2(section, "SpeedMult", SpeedMult, SpeedMult);
		break;
	}
	
	if(Projectiles)
		Proj.readFromIni(gs, dir, ini, section + ".Projectile");
	
	std::string addActionSection;
	ini.ReadString(section, "Additional", addActionSection, "");
	TrimSpaces(addActionSection);
	if(addActionSection != "") {
		if(deepCounter > 1000) {
			warnings << "There is probably an additional action definition loop in " << ini.getFileName() << ":" << section << endl;
			return false;
		}
		
		additionalAction = new Proj_Action();
		additionalAction->Type = PJ_NOTHING;
		if(!additionalAction->readFromIni(gs, dir, ini, addActionSection, deepCounter + 1)) {
			delete additionalAction; additionalAction = NULL;
			return false;
		}
		else if(!additionalAction->hasAction()) {
			warnings << "additional action " << addActionSection << "(in " << ini.getFileName() << ":" << section << ") does not have any effect" << endl;
			delete additionalAction; additionalAction = NULL;
		}
	}
	else
		additionalAction = NULL;
	
	return true;
}

bool Proj_Action::read(CGameScript* gs, FILE* fp) {
	if(gs->GetHeader()->Version <= GS_LX56_VERSION) {
		errors << "Proj_Action::read called for old GS version" << endl;
		return false;
	}
	
	fread_endian<int>(fp, Type);
	fread_endian<char>(fp, Projectiles);
	fread_endian<int>(fp, Damage);
	fread_endian<int>(fp, Shake);
	fread_endian<char>(fp, UseSound);
	if(UseSound) {
		SndFilename = readString(fp);

		if(!bDedicated) {
			// Load the sample
			Sound = gs->LoadGSSample(gs->sDirectory, SndFilename);
			
			if(Sound == NULL)
				gs->modLog("Could not open sound '" + SndFilename + "'");
		}
	}
	fread_endian<float>(fp, BounceCoeff);
	fread_endian<int>(fp, BounceExplode);
	fread_endian<float>(fp, GoThroughSpeed);
	fread_endian_V<int>(fp, ChangeRadius);
	fread_endian_V<float>(fp, Speed);
	fread_endian_M<float>(fp, SpeedMult);	
	if(Projectiles) Proj.read(gs, fp);
	
	bool haveAddAction = false;
	fread_endian<char>(fp, haveAddAction);
	if(haveAddAction) {
		additionalAction = new Proj_Action();
		return additionalAction->read(gs, fp);
	}
	return true;
}

bool Proj_Action::write(CGameScript* gs, FILE* fp) {
	if(gs->GetHeader()->Version <= GS_LX56_VERSION) {
		errors << "Proj_Action::write called for old GS version" << endl;
		return false;
	}

	fwrite_endian<int>(fp, Type);
	fwrite_endian<char>(fp, Projectiles);
	fwrite_endian<int>(fp, Damage);
	fwrite_endian<int>(fp, Shake);
	fwrite_endian<char>(fp, UseSound);
	if(UseSound) writeString(SndFilename, fp);
	fwrite_endian<float>(fp, BounceCoeff);
	fwrite_endian<int>(fp, BounceExplode);
	fwrite_endian<float>(fp, GoThroughSpeed);
	fwrite_endian_V<int>(fp, ChangeRadius);
	fwrite_endian_V<float>(fp, Speed);
	fwrite_endian_M<float>(fp, SpeedMult);
	
	if(Projectiles) Proj.write(gs, fp);
	
	if(!additionalAction)
		fwrite_endian<char>(fp, 0);
	else {
		fwrite_endian<char>(fp, 1);
		additionalAction->write(gs, fp);
	}
	return true;
}

bool Proj_Event::readFromIni(CGameScript* gs, const std::string& dir, const IniReader& ini, const std::string& section) {
	if(!ini.ReadKeyword(section, "Type", (int*)&type, type)) {
		warnings << ini.getFileName() << ":" << section << ": Type attribute isn't set for event" << endl;
		return false;
	}
	if(get() == NULL) {
		warnings << ini.getFileName() << ":" << section << ": Type attribute is invalid for event" << endl;
		return false;
	}
	if(!get()->readFromIni(gs, dir, ini, section))
		return false;
	
	if(!get()->canMatch()) {
		warnings << ini.getFileName() << ":" << section << ": Event can never occur with these settings" << endl;
		return false;
	}
	return true;
}

bool Proj_Event::read(CGameScript* gs, FILE* fp) {
	fread_endian<int>(fp, type);
	if(get() == NULL) return false;
	return get()->read(gs, fp);	
}

bool Proj_Event::write(CGameScript* gs, FILE* fp) {
	fwrite_endian<int>(fp, type);
	if(get() == NULL) return false;
	return get()->write(gs, fp);
}

bool Proj_EventAndAction::readFromIni(CGameScript* gs, const std::string& dir, const IniReader& ini, const std::string& section) {
	std::string eventSection;
	ini.ReadString(section, "Event", eventSection, eventSection);
	if(eventSection == "") {
		warnings << ini.getFileName() << ":" << section << ": Event not set" << endl;
		return false;
	}

	while(eventSection != "") {
		if(events.size() > 1000) {
			warnings << ini.getFileName() << ":" << section << ": Probably there is a loop definition for Event" << endl;
			events.clear();
			return false;
		}
		Proj_Event ev;
		if(!ev.readFromIni(gs, dir, ini, eventSection)) return events.size() > 0;
		events.push_back(ev);
		if(!ini.ReadString(std::string(eventSection), "AndEvent", eventSection, eventSection)) break;
	}
	
	return Proj_Action::readFromIni(gs, dir, ini, section);
}

bool Proj_EventAndAction::read(CGameScript* gs, FILE* fp) {
	Uint32 eventNum = 0;
	fread_endian<Uint32>(fp, eventNum);
	events.resize(eventNum);
	
	for(Uint32 i = 0; i < eventNum; ++i)
		if(!events[i].read(gs, fp)) {
			errors << "Proj_EventAndAction: error while reading game script projectile actions" << endl;
			events.clear(); // would crash otherwise
			return false;
		}
	
	return Proj_Action::read(gs, fp);
}

bool Proj_EventAndAction::write(CGameScript* gs, FILE* fp) {
	fwrite_endian<Uint32>(fp, (Uint32)events.size());
	
	for(Uint32 i = 0; i < events.size(); ++i)
		events[i].write(gs, fp);
	
	return Proj_Action::write(gs, fp);	
}


bool Proj_TimerEvent::readFromIni(CGameScript* gs, const std::string& dir, const IniReader& ini, const std::string& section) {
	ini.ReadFloat(section, "Delay", &Delay, Delay);
	ini.ReadKeyword(section, "Repeat", &Repeat, Repeat);
	ini.ReadKeyword(section, "UseGlobalTime", &UseGlobalTime, UseGlobalTime);	
	ini.ReadInteger(section, "PermanentMode", &PermanentMode, PermanentMode);
	return true;
}

bool Proj_TimerEvent::read(CGameScript* gs, FILE* fp) {
	fread_endian<float>(fp, Delay);
	fread_endian<char>(fp, Repeat);
	fread_endian<char>(fp, UseGlobalTime);
	fread_endian<int>(fp, PermanentMode);
	return true;
}

bool Proj_TimerEvent::write(CGameScript* gs, FILE* fp) {
	fwrite_endian<float>(fp, Delay);
	fwrite_endian<char>(fp, Repeat);
	fwrite_endian<char>(fp, UseGlobalTime);
	fwrite_endian<int>(fp, PermanentMode);
	return true;
}

bool Proj_ProjHitEvent::readFromIni(CGameScript* gs, const std::string& dir, const IniReader& ini, const std::string& section) {
	gs->needCollisionInfo = true;
	
	ownerWorm.readFromIni(gs, dir, ini, section);
	
	ini.ReadInteger(section, "MinHitCount", &MinHitCount, MinHitCount);
	ini.ReadInteger(section, "MaxHitCount", &MaxHitCount, MaxHitCount);
	
	if(MaxHitCount >= 0 && MaxHitCount < MinHitCount) {
		warnings << ini.getFileName() << ":" << section << ": ignoring MaxHitCount because MaxHitCount < MinHitCount" << endl;
		MaxHitCount = -1;
	}
		
	ini.ReadInteger(section, "Width", &Width, Width);
	ini.ReadInteger(section, "Height", &Height, Height);	
	
	ini.ReadKeyword(section, "TargetHealthIsMore", &TargetHealthIsMore, TargetHealthIsMore);
	ini.ReadKeyword(section, "TargetHealthIsLess", &TargetHealthIsLess, TargetHealthIsLess);
	ini.ReadKeyword(section, "TargetTimeIsMore", &TargetTimeIsMore, TargetTimeIsMore);
	ini.ReadKeyword(section, "TargetTimeIsLess", &TargetTimeIsLess, TargetTimeIsLess);
	
	std::string prjfile;
	ini.ReadString(section, "Target", prjfile, "");
	if(prjfile != "")
		Target = gs->CompileProjectile(dir, prjfile);
	else
		Target = NULL;
	return true;
}

bool Proj_ProjHitEvent::read(CGameScript* gs, FILE* fp) {
	if(gs->GetHeader()->Version <= GS_LX56_VERSION) {
		errors << "Proj_ProjHitEvent::read called for old GS version" << endl;
		return false;
	}

	ownerWorm.read(gs, fp);

	gs->needCollisionInfo = true;
	fread_endian<int>(fp, MinHitCount);
	fread_endian<int>(fp, MaxHitCount);
	fread_endian<int>(fp, Width);
	fread_endian<int>(fp, Height);
	fread_endian<char>(fp, TargetHealthIsMore);
	fread_endian<char>(fp, TargetHealthIsLess);
	fread_endian<char>(fp, TargetTimeIsMore);
	fread_endian<char>(fp, TargetTimeIsLess);
	
	bool hasSpecificTarget = false;
	fread_endian<char>(fp, hasSpecificTarget);
	if(hasSpecificTarget)
		Target = gs->LoadProjectile(fp);
	else
		Target = NULL;
	return !hasSpecificTarget || Target != NULL;	
}

bool Proj_ProjHitEvent::write(CGameScript* gs, FILE* fp) {
	if(gs->GetHeader()->Version <= GS_LX56_VERSION) {
		errors << "Proj_ProjHitEvent::write called for old GS version" << endl;
		return false;
	}

	ownerWorm.write(gs, fp);
	
	fwrite_endian<int>(fp, MinHitCount);
	fwrite_endian<int>(fp, MaxHitCount);
	fwrite_endian<int>(fp, Width);
	fwrite_endian<int>(fp, Height);
	fwrite_endian<char>(fp, TargetHealthIsMore);
	fwrite_endian<char>(fp, TargetHealthIsLess);
	fwrite_endian<char>(fp, TargetTimeIsMore);
	fwrite_endian<char>(fp, TargetTimeIsLess);
	
	fwrite_endian<char>(fp, Target != NULL);
	if(Target) return gs->SaveProjectile(Target, fp);
	return true;	
}



bool Proj_WormHitEvent::readFromIni(CGameScript* gs, const std::string& dir, const IniReader& ini, const std::string& section) {
	ini.ReadKeyword(section, "SameWormAsProjOwner", &SameWormAsProjOwner, SameWormAsProjOwner);
	ini.ReadKeyword(section, "SameTeamAsProjOwner", &SameTeamAsProjOwner, SameTeamAsProjOwner);
	ini.ReadKeyword(section, "DiffWormAsProjOwner", &DiffWormAsProjOwner, DiffWormAsProjOwner);
	ini.ReadKeyword(section, "DiffTeamAsProjOwner", &DiffTeamAsProjOwner, DiffTeamAsProjOwner);
	ini.ReadKeyword(section, "TeamMateOfProjOwner", &TeamMateOfProjOwner, TeamMateOfProjOwner);	
	ini.ReadKeyword(section, "EnemyOfProjOwner", &EnemyOfProjOwner, EnemyOfProjOwner);	
	return true;
}

bool Proj_WormHitEvent::read(CGameScript* gs, FILE* fp) {
	fread_endian<char>(fp, SameWormAsProjOwner);
	fread_endian<char>(fp, SameTeamAsProjOwner);
	fread_endian<char>(fp, DiffWormAsProjOwner);
	fread_endian<char>(fp, DiffTeamAsProjOwner);
	fread_endian<char>(fp, TeamMateOfProjOwner);
	fread_endian<char>(fp, EnemyOfProjOwner);
	return true;
}

bool Proj_WormHitEvent::write(CGameScript* gs, FILE* fp) {
	fwrite_endian<char>(fp, SameWormAsProjOwner);
	fwrite_endian<char>(fp, SameTeamAsProjOwner);
	fwrite_endian<char>(fp, DiffWormAsProjOwner);
	fwrite_endian<char>(fp, DiffTeamAsProjOwner);
	fwrite_endian<char>(fp, TeamMateOfProjOwner);
	fwrite_endian<char>(fp, EnemyOfProjOwner);
	return true;
}



bool Proj_TerrainHitEvent::readFromIni(CGameScript* gs, const std::string& dir, const IniReader& ini, const std::string& section) {
	ini.ReadKeyword(section, "MapBound", &MapBound, MapBound);
	ini.ReadKeyword(section, "Dirt", &Dirt, Dirt);
	ini.ReadKeyword(section, "Rock", &Rock, Rock);
	return true;
}

bool Proj_TerrainHitEvent::read(CGameScript* gs, FILE* fp) {
	fread_endian<char>(fp, MapBound);
	fread_endian<char>(fp, Dirt);
	fread_endian<char>(fp, Rock);
	return true;
}

bool Proj_TerrainHitEvent::write(CGameScript* gs, FILE* fp) {
	fwrite_endian<char>(fp, MapBound);
	fwrite_endian<char>(fp, Dirt);
	fwrite_endian<char>(fp, Rock);
	return true;
}

std::vector<std::string> CGameScript::LoadWeaponList(const std::string dir)
{
	std::vector<std::string> ret;
	ModInfo info;
	std::string name;
	if( !CheckFile(dir, name, false, &info) )
		return ret;
	
	if( info.typeShort == "LX src" )
	{
		// Read only weapons
		//InitDefaultCompilerKeywords();
		IniReader ini(dir + "/Main.txt", compilerKeywords);

		if (!ini.Parse())
		{
			errors << "Error while parsing the gamescript " << dir << endl;
			return ret;
		}

		int numWeapons = 0;
		ini.ReadInteger("Weapons","NumWeapons", &numWeapons,0);
		for(int i = 0; i < numWeapons; i++) 
		{
			std::string wpn = "Weapon" + itoa(i+1);
			std::string weap;
			ini.ReadString("Weapons", wpn, weap, "");
			if( weap != "" )
			{
				IniReader ini2(dir + "/" + weap, compilerKeywords);
				if (!ini2.Parse())
				{
					errors << "Error while parsing the gamescript " << dir << endl;
					return ret;
				}
				ini2.ReadString("General", "Name", weap, "");
				if( weap != "" )
					ret.push_back( weap );
			}
		}
		return ret;
	}
	
	if( info.typeShort == "LX" )
	{
		// Too complicated to parse here
		CGameScript loader;
		if( loader.Load(dir, false) != GSE_OK )
			return ret;
		for( int i = 0; i < loader.NumWeapons; i++ )
			ret.push_back( loader.Weapons[i].Name );
		return ret;
	}
	
	// TODO: Gusanos does not support weapon restrictions
	return ret;
}

std::vector<std::string> CGameScript::GetWeaponList() const
{
	std::vector<std::string> ret;
	if( ! loaded )
		return ret;
	for( int i = 0; i < NumWeapons; i++ )
		ret.push_back( Weapons[i].Name );
	return ret;
}
