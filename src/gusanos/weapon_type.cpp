#include "weapon_type.h"

#include "events.h"
#include "sprite_set.h"
#include "util/text.h"
#include "resource_base.h"
#include "game_actions.h"
#include "omfgscript/omfg_script.h"
#include "util/macros.h"
#include "timer_event.h"
#include "luaapi/context.h"
#include "FindFile.h"

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

using namespace std;

LuaReference WeaponType::metaTable;

WeaponType::WeaponType() : ResourceBase()
{
	ammo = 1;
	reloadTime = 0;

#ifndef DEDICATED_ONLY
	firecone = NULL;
	skin = 0;
	laserSightBlender = BlitterContext::TNone;
#endif

	syncHax = false;
	syncReload = true;

	laserSightColour = makecol(255,0,0);
	laserSightRange = -1;
	laserSightIntensity = 0;
	laserSightAlpha = 255;


	primaryShoot = NULL;
	primaryPressed = NULL;
	primaryReleased = NULL;
	outOfAmmo = NULL;
	reloadEnd = NULL;
}

WeaponType::~WeaponType()
{
	delete primaryShoot;
	delete primaryPressed;
	delete primaryReleased;
	delete outOfAmmo;
	delete reloadEnd;

	foreach( t, timer ) {
		delete (*t);
	}

	foreach( t, activeTimer ) {
		delete (*t);
	}

	foreach( t, shootTimer ) {
		delete (*t);
	}
}


namespace GameEventID
{
	enum type
	{
	    PrimaryShoot,
	    PrimaryPress,
	    PrimaryRelease,
	    OutOfAmmo,
	    ReloadEnd,
	    Timer,
	    ActiveTimer
	};
}

bool WeaponType::load(std::string const& filename)
{
	std::ifstream fileStream;
	OpenGameFileR(fileStream, filename, std::ios::binary | std::ios::in);

	if (!fileStream ) {
		//notes << "WeaponType: cannot load " << filename << endl;
		return false;
	}
	
	fileName = filename;

	OmfgScript::Parser parser(fileStream, gameActions, filename);

	namespace af = OmfgScript::ActionParamFlags;

	parser.addEvent("primary_shoot", GameEventID::PrimaryShoot, af::Weapon | af::Object);
	parser.addEvent("primary_press", GameEventID::PrimaryPress, af::Weapon | af::Object);
	parser.addEvent("primary_release", GameEventID::PrimaryRelease, af::Weapon | af::Object);
	parser.addEvent("out_of_ammo", GameEventID::OutOfAmmo, af::Weapon | af::Object);
	parser.addEvent("reload_end", GameEventID::ReloadEnd, af::Weapon | af::Object);

	parser.addEvent("timer", GameEventID::Timer, af::Weapon | af::Object)
	("delay")
	("delay_var")
	("max_trigger")
	("start_delay")
	;

	parser.addEvent("active_timer", GameEventID::ActiveTimer, af::Weapon | af::Object)
	("delay")
	("delay_var")
	("max_trigger")
	("start_delay")
	;

	if(!parser.run()) {
		if(parser.incomplete())
			parser.error("Trailing garbage");
		notes << "WeaponType: error at parsing" << endl;
		return false;
	}

	crc = parser.getCRC();

	ammo = parser.getInt("ammo", 1);
	name = parser.getString("name");
	reloadTime = parser.getInt("reload_time", 0);
	syncHax = parser.getBool("sync_shot", false);
	syncReload = parser.getBool("sync_reload", true);
	laserSightIntensity = (float)parser.getDouble("laser_sight_intensity", 0.0);
	laserSightRange = parser.getInt("laser_sight_range", -1);
	laserSightAlpha = parser.getInt("laser_sight_alpha", 255);

	laserSightColour = parser.getProperty("laser_sight_colour", "laser_sight_color")->toColor(255, 0, 0);
#ifndef DEDICATED_ONLY
	{
		std::string str = parser.getString("laser_sight_blender", "none");
		if(str == "add")
			laserSightBlender = BlitterContext::Add;
		else if(str == "alpha")
			laserSightBlender = BlitterContext::Alpha;
		else if(str == "none")
			laserSightBlender = BlitterContext::TNone;
	}

	{
		OmfgScript::TokenBase* v = parser.getProperty("firecone");
		if(!v->isDefault())
			firecone = spriteList.load(v->toString());

		v = parser.getProperty("skin");
		if(!v->isDefault())
			skin = spriteList.load(v->toString());
	}
#endif

	OmfgScript::Parser::GameEventIter i(parser);
	for(; i; ++i) {
		std::vector<OmfgScript::TokenBase*> const& p = i.params();
		switch(i.type()) {
				case GameEventID::PrimaryShoot:
				primaryShoot = new GameEvent(i.actions());
				break;

				case GameEventID::PrimaryPress:
				primaryPressed = new GameEvent(i.actions());
				break;

				case GameEventID::PrimaryRelease:
				primaryReleased = new GameEvent(i.actions());
				break;

				case GameEventID::OutOfAmmo:
				outOfAmmo = new GameEvent(i.actions());
				break;

				case GameEventID::ReloadEnd:
				reloadEnd = new GameEvent(i.actions());
				break;

				case GameEventID::Timer:
				timer.push_back(new TimerEvent(i.actions(), p[0]->toInt(100), p[1]->toInt(0), p[2]->toInt(0), p[3]->toInt(0) ) );
				break;

				case GameEventID::ActiveTimer:
				activeTimer.push_back(new TimerEvent(i.actions(), p[0]->toInt(100), p[1]->toInt(0), p[2]->toInt(0), p[3]->toInt(0) ) );
				break;
		}
	}

	return true;
}

void WeaponType::finalize()
{
	delete primaryShoot;
	primaryShoot = 0;
	delete primaryPressed;
	primaryPressed = 0;
	delete primaryReleased;
	primaryReleased = 0;
	delete outOfAmmo;
	outOfAmmo = 0;
	delete reloadEnd;
	reloadEnd = 0;

	foreach( t, timer ) {
		delete (*t);
	}
	timer.clear();

	foreach( t, activeTimer ) {
		delete (*t);
	}
	activeTimer.clear();

	foreach( t, shootTimer ) {
		delete (*t);
	}
	shootTimer.clear();
}

