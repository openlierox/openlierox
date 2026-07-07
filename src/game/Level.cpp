/*
 *  Level.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 08.12.09.
 *  code under LGPL
 *
 */

#include <memory>
#include "Level.h"
#include "MapLoader.h"
#include "StringUtils.h"
#include "GfxPrimitives.h"
#include "gusanos/luaapi/classes.h"

LevelInfo infoForLevel(const std::string& f, bool absolute) {
	MapLoad* loader = MapLoad::open(absolute ? f : ("levels/" + f), absolute, false);
	if(!loader) return LevelInfo();

	LevelInfo info;
	info.valid = true;
	info.name = loader->header().name;
	info.path = GetBaseFilename(f);
	info.type = loader->format();
	info.typeShort = loader->formatShort();
	
	delete loader;
	
	return info;
}

SmartPointer<SDL_Surface> minimapForLevel(const std::string& f, bool absolute) {
	LevelInfo info = infoForLevel(f, absolute);
	
	std::unique_ptr<MapLoad> loader( MapLoad::open(absolute ? f : ("levels/" + f), absolute, false) );
	if(!loader.get()) {
		SmartPointer<SDL_Surface> minimap = gfxCreateSurface(128,96);
		DrawCross(minimap.get(), 0, 0, 128, 96, Color(0,0,255));
		return minimap;
	}
	
	return loader->getMinimap();
}

LevelInfo::LevelInfo() : valid(false) {
	thisRef.classId = LuaID<LevelInfo>::value;
}

bool LevelInfo::operator==(const CustomVar& o) const {
	const LevelInfo* oi = dynamic_cast<const LevelInfo*> (&o);
	return oi && stringcaseequal(path, oi->path);
}

bool LevelInfo::operator<(const CustomVar& o) const {
	const LevelInfo* oi = dynamic_cast<const LevelInfo*> (&o);
	if(oi) return stringcasecmp(path, oi->path) < 0;
	return this < &o;		
}

std::string LevelInfo::toString() const {
	return path;
}

bool LevelInfo::fromString( const std::string & str) {
	*this = infoForLevel(str, false);
	return true;
}

REGISTER_CLASS(LevelInfo, LuaID<CustomVar>::value)
