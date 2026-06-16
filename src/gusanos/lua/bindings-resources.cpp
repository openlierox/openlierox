#include "bindings-resources.h"
#include "bindings-gfx.h"
#include "bindings.h"

#include "../luaapi/types.h"
#include "../luaapi/macros.h"
#include "../luaapi/classes.h"

#include "../gusgame.h"
#include "../gfx.h"
#include "../script.h"
#ifndef DEDICATED_ONLY
#include "../sound.h"
#include "sound/sfx.h"
#endif
#include "../part_type.h"
#include "../weapon_type.h"
#include "game/CMap.h"

//TEMP:
#include "../sprite_set.h"
#include "../sprite.h"

#ifndef DEDICATED_ONLY
#include "../font.h"
#endif
#include <cmath>
#include <string>
#include <list>
#include <iostream>
#include <vector>
using std::cerr;
using std::endl;
#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
using boost::lexical_cast;

namespace LuaBindings
{

#ifndef DEDICATED_ONLY
LuaReference FontMetaTable;
LuaReference SpriteSetMetaTable;
LuaReference SoundMetaTable;
#endif
//LuaReference PartTypeMetaTable;
//LuaReference WeaponTypeMetaTable;

enum FontFlags
{
	None        = 0,
	CenterV     = (1<<0),
	CenterH     = (1<<1),
	Shadow      = (1<<2),
	Formatting  = (1<<3),
	Right       = (1<<4),
	Bottom      = (1<<5),
};

#ifndef DEDICATED_ONLY

/*! sprites_load(name)

	Loads and returns a SpriteSet object of the sprite set with the passed name.
	
	If the sprite set couldn't be loaded, nil is returned.
*/
int l_sprites_load(lua_State* L)
{
	LuaContext context(L);
	
	const char* n = lua_tostring(context, 1);
	if(!n) return 0;

	SpriteSet *s = spriteList.load(n);
	if(!s) return 0;
	
	context.pushFullReference(*s, LuaBindings::SpriteSetMetaTable);

	return 1;
}

/*! SpriteSet:render(bitmap, frame, x, y)

	Draws the frame //frame// of the sprite set on //bitmap// with the pivot at position (x, y).
*/
METHODC(SpriteSet, sprites_render,
	//ALLEGRO_BITMAP* b = *static_cast<ALLEGRO_BITMAP **>(lua_touserdata(context, 2));
	ALLEGRO_BITMAP* b = ASSERT_OBJECT(ALLEGRO_BITMAP, 2);

	// All images in doubleRes.
	int frame = lua_tointeger(context, 3);
	int x = int(lua_tonumber(context, 4) * 2);
	int y = int(lua_tonumber(context, 5) * 2);
	p->getSprite(frame)->draw(b, x, y, blitter);
	
	return 0;
)

/*! SpriteSet:render_skinned_box(bitmap, x1, y1, x2, y2, color)

	Draws the sprite set as a skinned box with the rectangle (x1, y1) - (x2, y2)
	and background color //color//.
	
	**Note that in versions before 0.9c, color was specified as
	three parameters. This has been deprecated.**
*/
#ifndef NO_DEPRECATED
METHODC(SpriteSet, sprites_render_skinned_box,  {
	ALLEGRO_BITMAP* b = ASSERT_OBJECT(ALLEGRO_BITMAP, 2);
	
	// All images in doubleRes.
	int x1 = int(lua_tonumber(context, 3) * 2);
	int y1 = int(lua_tonumber(context, 4) * 2);
	int x2 = int(lua_tonumber(context, 5) * 2);
	int y2 = int(lua_tonumber(context, 6) * 2);
	int c = lua_tointeger(context, 7);

	if(lua_gettop(context) >= 9) // Deprecated
	{
		LUA_WLOG_ONCE("The r, g, b version of render_skinned_box is deprecated, replace the r, g, b parameters by color(r, g, b)");
		int g = lua_tointeger(context, 8);
		int b = lua_tointeger(context, 9);
		c = makecol(c, g, b);
	}

	p->drawSkinnedBox(b, blitter, Rect(x1, y1, x2+1, y2+1), c);
	
	return 0;
})
#else
METHODC(SpriteSet, sprites_render_skinned_box,  (
	ALLEGRO_BITMAP* b = ASSERT_OBJECT(ALLEGRO_BITMAP, 2);
	
	// All images in doubleRes.
	int x1 = int(lua_tonumber(context, 3) * 2);
	int y1 = int(lua_tonumber(context, 4) * 2);
	int x2 = int(lua_tonumber(context, 5) * 2);
	int y2 = int(lua_tonumber(context, 6) * 2);
	int c = lua_tointeger(context, 7);
	p->drawSkinnedBox(b, blitter, Rect(x1, y1, x2+1, y2+1), c);
	
	return 0;
))
#endif

/*! SpriteSet:frames()

	Returns the number of frames in this sprite set.
*/
METHODC(SpriteSet, sprites_frames,  {
	context.push(static_cast<int>(p->getFramesWidth()));
	return 1;
})


/*! font_load(name)

	Loads and returns a Font object of the font with the passed name.
	
	If the font couldn't be loaded, nil is returned.
*/
int l_font_load(lua_State* L)
{
	LuaContext context(L);
	
	char const* n = lua_tostring(L, 1);
	if(!n) return 0;

	Font *f = fontLocator.load(n);
	if(!f) return 0;
	
	context.pushFullReference(*f, LuaBindings::FontMetaTable);

	return 1;
}

int l_font_load2(lua_State* L)
{
	LuaContext context(L);
	
	char const* n = lua_tostring(L, 2);
	
	if(!n) return 0;

	Font *f = fontLocator.load(n);
	if(!f) return 0;
	
	context.pushFullReference(*f, LuaBindings::FontMetaTable);
	lua_pushvalue(L, -1);
	lua_setfield(L, 1, n);
	return 1;
}



/*! Font:render(bitmap, string, x, y[, color[, flags]])

	Draws the text 'string' on 'bitmap' at the position (x, y).
	
	If //color// is supplied, it draws the text with that color,
	otherwise it draws the text white.
	
	**Note that in versions before 0.9c, color was specified as
	three parameters. This has been deprecated.**

	//flags// can be a sum of these values:
	* Font.None : No flags (default).
	* Font.CenterV : Center the text vertically with y at the middle.
	* Font.CenterH : Center the text horizontally with x at the middle.
	* Font.Shadow : Draw a shadow under the text.
	
	Font.Formatting : Draw the text with formatting.
	
*/

typedef std::pair<int, int> pair_int;

METHODC(Font, font_render,  {
	int params = lua_gettop(context);
	if(params < 5)
		return 0;
		
	//ALLEGRO_BITMAP* b = *static_cast<ALLEGRO_BITMAP **>(lua_touserdata(L, 2));
	ALLEGRO_BITMAP* b = ASSERT_OBJECT(ALLEGRO_BITMAP, 2);
	
	char const* sc = lua_tostring(context, 3);
	if(!sc)
		return 0;
		
	std::string s(sc);
		
	// All images in doubleRes.
	int x = int(lua_tonumber(context, 4) * 2);
	int y = int(lua_tonumber(context, 5) * 2);
	
	int cr = 255;
	int cg = 255;
	int cb = 255;
	int flags = 0;
	
	if(params >= 6)
	{
		int c = lua_tointeger(context, 6);
		if(params >= 8)
		{
			LUA_WLOG_ONCE("The r, g, b version of Font:render is deprecated, replace the r, g, b parameters by color(r, g, b)");
			cr = c;
			cg = lua_tointeger(context, 7);
			cb = lua_tointeger(context, 8);
			if(params >= 9)
				flags = lua_tointeger(context, 9);
		}
		else
		{
			cr = getr(c);
			cg = getg(c);
			cb = getb(c);
			if(params >= 7)
				flags = lua_tointeger(context, 7);
		}
	}
	
	int realFlags = 0;
	
	if(flags & Shadow)
		realFlags |= Font::Shadow;
	if(flags & Formatting)
		realFlags |= Font::Formatting;
	
	if(flags & (CenterV | CenterH | Right | Bottom))
	{
		pair_int dim = p->getDimensions(s, 0, realFlags);

		if(flags & Right)
			x -= dim.first;
		else if(flags & CenterH)
			x -= (dim.first - 1) / 2;
		if(flags & Bottom)
			y -= dim.second;
		else if(flags & CenterV)
			y -= (dim.second - 1) / 2;
	}

	int fact = (blitter.type() != BlitterContext::Alpha ? 256 : blitter.fact());
	p->draw(b, s, x, y, 0, fact, cr, cg, cb, realFlags);
	
	return 0;
})

//! version 0.9c

/*! sounds

	This table returns Sound objects when indexed with a valid sound name, and
	nil otherwise.
	
	Example:
	<code>
	local mySound = sounds["bazooka.wav"]
	mySound:play(10, 10)
	</code>
*/

int l_sound_load2(lua_State* L)
{
	if(bDedicated) return 0;
	if(!sfx) return 0;
	
	LuaContext context(L);
	
	char const* n = lua_tostring(L, 2);
	
	if(!n) return 0;

	Sound* s = soundList.load(n);
	if(!s) return 0;
	
	context.pushFullReference(*s, LuaBindings::SoundMetaTable);
	lua_pushvalue(L, -1);
	lua_setfield(L, 1, n);
	return 1;
}

/*! Sound:play([x, y | object] [, loudness, pitch, pitchVariation])

	Plays a sound either at position (x, y) on the map, or attached to
	object //object// (depending on which is passed to the function).
*/
METHODC(Sound, sound_play,  {
	lua_Number loudness = 100.0;
	lua_Number pitch = 1.0;
	lua_Number pitchVariation = 1.0;
	
	if(CGameObject* obj = getObject<CGameObject>(context, 2))
	{
		int params = lua_gettop(context);
		switch(params)
		{
			default: if(params < 2) return 0;
			case 5:  pitchVariation = lua_tonumber(context, 6);
			case 4:  pitch = lua_tonumber(context, 5);
			case 3:  loudness = lua_tonumber(context, 4);
			case 2:  break;
		}
		
		//CGameObject* obj = *static_cast<CGameObject**>(lua_touserdata(context, 2));

		p->play2D(obj, (float)loudness, (float)pitch, (float)pitchVariation);
	}
	else
	{
		int params = lua_gettop(context);

		switch(params)
		{
			default: if(params < 3) return 0;
			case 6:  pitchVariation = lua_tonumber(context, 6);
			case 5:  pitch = lua_tonumber(context, 5);
			case 4:  loudness = lua_tonumber(context, 4);
			case 3:  break;
		}
		
		lua_Number x = lua_tonumber(context, 2);
		lua_Number y = lua_tonumber(context, 3);
		
		p->play2D(Vec((float)x, (float)y), (float)loudness, (float)pitch, (float)pitchVariation);
	}
	
	return 0;
})
#endif

//! version any

/*! map_is_loaded()

	Returns true if a map is loaded, otherwise false.
*/
int l_map_is_loaded(lua_State* L)
{
	lua_pushboolean(L, game.isMapReady());
	
	return 1;
}


int l_load_script(lua_State* L)
{
	char const* s = lua_tostring(L, 2);
	if(!s)
		return 0;
	Script* script = scriptLocator.load(s);
	
	if(!script) {
		//notes << "Lua load script: cannot load " << s << endl;
		return 0;
	}
	
	// Return the allocated table
	lua_pushvalue(L, 2);
	lua_rawget(L, LUA_GLOBALSINDEX);
	return 1;
}

/*! load_particle(name)

	Loads a particle type with the name //name// and returns it as a
	ParticleType object.
*/
int l_load_particle(lua_State* L)
{
	LuaContext context(L);

	char const* s = lua_tostring(L, 1);
	if(!s)
		return 0;
		
	PartType* type = partTypeList.load(s);
	if(!type)
		return 0;
	
	type->pushLuaReference(context);
	return 1;
}

/*! weapon_random()

	Returns a random WeaponType object.
*/
int l_weapon_random(lua_State* L)
{
	LuaContext context(L);

	if(gusGame.weaponList.size() == 0) {
		errors << "Lua: l_weapon_random: no weapons available" << endl;
		return 0;
	}

	WeaponType* p = gusGame.weaponList[rndInt(gusGame.weaponList.size())];
	p->pushLuaReference(context);
	return 1;
}

/*! weapon_count()

	Returns the total number of weapons.
*/
int l_weapon_count(lua_State* L)
{
	lua_pushinteger(L, gusGame.weaponList.size());
	return 1;
}

/*! WeaponType:next()

	Returns the next weapon type after this one.
*/
METHODC(WeaponType, weapon_next,  {
	if(gusGame.weaponList.size() == 0) return 0;
	size_t n = p->getIndex() + 1;
	if(n >= gusGame.weaponList.size())
		n = 0;
	gusGame.weaponList[n]->pushLuaReference(context);
	return 1;
})

/*! WeaponType:prev()

	Returns the previous weapon type after this one.
*/
METHODC(WeaponType, weapon_prev,  {
	if(gusGame.weaponList.size() == 0) return 0;
	size_t n = p->getIndex();
	if(n == 0)
		n = gusGame.weaponList.size() - 1;
	else
		--n;
	gusGame.weaponList[n]->pushLuaReference(context);
	return 1;
})

/*! WeaponType:name()

	Returns the name of this weapon type.
*/
METHODC(WeaponType, weapon_name,  {
	lua_pushstring(context, p->name.c_str());
	return 1;
})

/*! WeaponType:reload_time()

	Returns the time it takes for this weapon type to reload.
*/
METHODC(WeaponType, weapon_reload_time,  {
	context.push(p->reloadTime);
	return 1;
})

/*! WeaponType:ammo()

	Returns the amount of ammo for this weapon type when reloaded.
*/
METHODC(WeaponType, weapon_ammo,  {
	context.push(p->ammo);
	return 1;
})

/*
BINOP(WeaponType, weapon_eq,  {
	context.push(a == b);
	return 1;
})*/


METHODC(PartType, parttype_put,  {
	float x = 0.f;
	float y = 0.f;
	float xspd = 0.f;
	float yspd = 0.f;
	Angle angle(0.0);
	
	int params = lua_gettop(context);
	switch(params)
	{
		default: if(params < 3) return 0;
		case 6:  angle = Angle(lua_tonumber(context, 6));
		case 5:  yspd = (float)lua_tonumber(context, 5);
		case 4:  xspd = (float)lua_tonumber(context, 4);
		case 3:  y = (float)lua_tonumber(context, 3);
		case 2:  x = (float)lua_tonumber(context, 2);
	}
	
	CGameObject* last = p->newParticle(p, Vec(x, y), Vec(xspd, yspd), 1, 0, angle);

	if(last)
	{
		last->pushLuaReference(context);
		return 1;
	}
	
	return 0;
})

void initResources(LuaContext& context)
{
	AssertStack as(context);
		
	context.functions()
		("load_particle", l_load_particle)
		("weapon_random", l_weapon_random)
		("weapon_count", l_weapon_count)
#ifndef DEDICATED_ONLY
		("sprites_load", l_sprites_load)
		("font_load", l_font_load)
#endif
		("map_is_loaded", l_map_is_loaded)
	;
	
	CLASS_(PartType,
		("put", l_parttype_put)
	)
	
	CLASS_(WeaponType,
		("next", l_weapon_next)
		("prev", l_weapon_prev)
		("name", l_weapon_name)
		("reload_time", l_weapon_reload_time)
		("ammo", l_weapon_ammo)
	)

#ifndef DEDICATED_ONLY
	// Font method and metatable

	CLASS(Font,  
		("render", l_font_render)
	)
	
	ENUM(Font,  
		("None", None)
		("CenterV", CenterV)
		("CenterH", CenterH)
		("Right", Right)
		("Bottom", Bottom)
		("Shadow", Shadow)
		("Formatting", Formatting)
	)
	
	CLASS(Sound,  
		("play", l_sound_play)
	)
	
	CLASS(SpriteSet,  
		("render", l_sprites_render)
		("render_skinned_box", l_sprites_render_skinned_box)
		("frames", l_sprites_frames)
	)
	
	REQUEST_TABLE("fonts", l_font_load2);
	REQUEST_TABLE("sounds", l_sound_load2);

#endif
	// Global metatable
	
	lua_pushvalue(context, LUA_GLOBALSINDEX);
	
	lua_newtable(context);
	context.tableFunction("__index", l_load_script);
	
	lua_setmetatable(context, -2);
	context.pop(1); // Pop global table

	//std::cerr << "Old: " << as.stack << std::endl;
	//std::cerr << "New: " << lua_gettop(context) << std::endl;
}

}
