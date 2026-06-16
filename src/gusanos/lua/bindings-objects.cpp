#include "bindings-objects.h"

#include "bindings-resources.h"
#include "../luaapi/types.h"
#include "../luaapi/macros.h"
#include "../luaapi/classes.h"

#include "game/WormInputHandler.h"
#include "CWormHuman.h"
#include "game/CWorm.h"
#include "CNinjaRope.h"
#include "../particle.h"
#include "../weapon.h"
#include "../weapon_type.h"
#include "../gusgame.h"
#include "util/log.h"
#include "game/CMap.h"
#include "game/Game.h"
#include "game/Attr.h"
#include "Timer.h"

#include <cmath>
#include <iostream>
#include "gusanos/allegro.h"
extern "C" {
#include "lauxlib.h"
}
using std::cerr;
using std::endl;
#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
using boost::lexical_cast;


static int l_baseObject_set(lua_State* L) {
	LuaContext context(L);

	BaseObject* obj = getObject<BaseObject>(context, 1);
	if(!obj) {
		context.pushError("baseobject:newindex(): called on invalid object. Did you use '.' instead of ':'?");
		return 0;
	}

	ScriptVar_t key;
	if(NegResult r = context.toScriptVar(2, key)) {
		context.pushError("baseobject:newindex() " + obj->thisRef.description() + ": key invald: " + r.res.humanErrorMsg);
		return 0;
	}

	ScriptVar_t val;
	if(NegResult r = obj->getAttrib(key, val)) {
		context.pushError("baseobject:newindex() " + obj->thisRef.description() + ": " + r.res.humanErrorMsg);
		return 0;
	}

	ScriptVar_t newVal;
	if(NegResult r = context.toScriptVar(3, newVal)) {
		context.pushError("baseobject:newindex() " + obj->thisRef.description() + ": " + r.res.humanErrorMsg);
		return 0;
	}

	if(NegResult r = val.fromScriptVar(newVal, true, false)) {
		context.pushError("baseobject:newindex() " + obj->thisRef.description() + ": error while casting: " + r.res.humanErrorMsg);
		return 0;
	}

	if(NegResult r = obj->setAttrib(key, val)) {
		context.pushError("baseobject:newindex() " + obj->thisRef.description() + ": " + r.res.humanErrorMsg);
		return 0;
	}

	return 0;
}

static int l_baseObject_get(lua_State* L) {
	LuaContext context(L);

	BaseObject* obj = getObject<BaseObject>(context, 1);
	if(!obj) {
		context.pushError("baseobject:index(): called on invalid object. Did you use '.' instead of ':'?");
		return 0;
	}

	// if we are a C closure and have a table attached, check it
	if(lua_istable(context, lua_upvalueindex(1))) {
		lua_pushvalue(L, lua_upvalueindex(1));
		lua_pushvalue(L, 2);
		lua_rawget(L, -2);
		if(!lua_isnil(L, -1)) {
			lua_remove(L, -2); // table
			return 1; // the value is at top now
		}
		context.pop(2);
	}

	ScriptVar_t key;
	if(NegResult r = context.toScriptVar(2, key)) {
		context.pushError("baseobject:index() " + obj->thisRef.description() + ": key invald: " + r.res.humanErrorMsg);
		return 0;
	}

	ScriptVar_t val;
	if(NegResult r = obj->getAttrib(key, val)) {
		context.pushError("baseobject:index() " + obj->thisRef.description() + ": " + r.res.humanErrorMsg);
		return 0;
	}

	context.pushScriptVar(val);
	return 1;
}

static int l_baseObject_tostring(lua_State* L) {
	LuaContext context(L);

	BaseObject* obj = getObject<BaseObject>(context, 1);
	if(!obj) {
		context.pushError("baseobject:tostring(): called on invalid object. Did you use '.' instead of ':'?");
		return 0;
	}

	context.push(obj->toString());
	return 1;
}

void initBaseObjMetaTable(LuaContext& context, int indexClosureNum) {
	if(indexClosureNum > 0) {
		lua_pushstring(context, "__index_table");
		lua_pushvalue(context, -2);
		lua_rawset(context, -3);
	}
	{
		lua_pushcclosure(context, l_baseObject_get, indexClosureNum);
		lua_pushstring(context, "__index");
		lua_insert(context, -2); // swap last two values
		lua_rawset(context, -3);
	}
	{
		lua_pushstring(context, "__newindex");
		lua_pushcfunction(context, l_baseObject_set);
		lua_rawset(context, -3);
	}
	{
		lua_pushstring(context, "__tostring");
		lua_pushcfunction(context, l_baseObject_tostring);
		lua_rawset(context, -3);
	}
	context.tableSetField(LuaID<BaseObject>::value);
}

void BaseObject::initMetaTable(LuaContext& context) {
	lua_newtable(context);
	initBaseObjMetaTable(context, 0);
	BaseObject::metaTable.create(context);
}

LuaReference BaseObject::metaTable;


namespace LuaBindings
{

//LuaReference wormMetaTable;
//LuaReference baseObjectMetaTable;
//LuaReference particleMetaTable;
LuaReference WeaponMetaTable;

namespace ParticleRep
{
	enum type
	{
		Position
	};
}

/* Some Gus Lua code overwrote some gameobject's (e.g. worm's) pos/vel
  to shoot from a specific pos with a specific vel. E.g. the Promode Lua
  code does this in its weapon network syncing code (see putObject() in
  promode/scripts/weaponsyncing.lua).
  When we do this for another object where we don't have authority to
  write pos/vel, the write is ignored.
  Also, ScopedGusCompatibleSpeed doesn't work correctly because
  it is not able to actually change the velocity.
  This is a quite ugly hack where we remember such writes for one frame
  and simulate the Lua environment as if we did the write.
  */
struct PosVelTempHack {
	AbsTime lastTime;
	std::map<WeakRef<BaseObject>, CVec> vel;
	std::map<WeakRef<BaseObject>, CVec> pos;

	void recheck() {
		if(tLX->currentTime != lastTime) {
			vel.clear();
			pos.clear();
			lastTime = tLX->currentTime;
		}
	}

	CVec getPos(CGameObject* o) {
		recheck();
		if(o->weOwnThis()) return o->pos();
		if(pos.find(o->thisRef.obj) != pos.end())
			return pos[o->thisRef.obj];
		return o->pos();
	}

	CVec getVel(CGameObject* o) {
		recheck();
		if(o->weOwnThis()) return o->getGusVel();
		if(vel.find(o->thisRef.obj) != vel.end())
			return vel[o->thisRef.obj];
		return o->getGusVel();
	}

	void setPos(CGameObject* o, CVec v) {
		recheck();
		if(o->weOwnThis()) o->setPos(v);
		else pos[o->thisRef.obj] = v;
	}

	void setVel(CGameObject* o, CVec v) {
		recheck();
		if(o->weOwnThis()) o->setGusVel(v);
		else vel[o->thisRef.obj] = v;
	}
};
static PosVelTempHack posVelTempHack;

int shootFromObject(lua_State* L, CGameObject* object)
{		
	if(object == NULL) {
		errors << "shootFromObject: object == NULL" << endl;
		return 0;
	}

	/*
	void* typeP = lua_touserdata (L, 2);
	if(!typeP)
		return 0;
	PartType* p = *static_cast<PartType **>(typeP);
	*/
	LuaContext context(L);
	PartType* p = ASSERT_OBJECT(PartType, 2);
	
	int amount = 1;
	int amountVariation = 0;
	lua_Number speed = 0;
	lua_Number speedVariation = 0;
	lua_Number motionInheritance = 0;
	lua_Number distanceOffset = 0;
	AngleDiff distribution(360.0);
	AngleDiff angleOffset(0.0);
	
	int params = lua_gettop(L);
	switch(params)
	{
		default: if(params < 3) return 0;
		case 10: distanceOffset = lua_tonumber(L, 10);
		case 9:  angleOffset = AngleDiff(lua_tonumber(L, 9));
		case 8:  distribution = AngleDiff(lua_tonumber(L, 8));
		case 7:  amountVariation = lua_tointeger(L, 7);
		case 6:  motionInheritance = lua_tonumber(L, 6);
		case 5:  speedVariation = lua_tonumber(L, 5);
		case 4:  speed = lua_tonumber(L, 4);
		case 3:  amount = lua_tointeger(L, 3);
	}

	char dir = object->getDir();
	Angle baseAngle(object->getPointingAngle() + angleOffset * dir);
	
	CGameObject* last = 0;
	
	int realAmount = amount + rndInt(amountVariation); // int(rnd()*amountVariation);
	for(int i = 0; i < realAmount; ++i)
	{
		Angle angle = baseAngle + distribution * midrnd();
		Vec direction(angle);
		Vec spd(direction * (float)(speed + midrnd()*speedVariation));
		if(motionInheritance)
		{
			spd += Vec(posVelTempHack.getVel(object)) * (float)motionInheritance;
			angle = spd.getAngle(); // Need to recompute angle
		}
		//gusGame.insertParticle( new Particle( p, object->getPos() + direction * distanceOffset, spd, object->getDir(), object->getOwner(), angle ));
		last = p->newParticle(p, posVelTempHack.getPos(object) + direction * (float)distanceOffset, spd, object->getDir(), object->getOwner(), angle);
	}
	
	if(last)
	{
		last->pushLuaReference(context);
		return 1;
	}
	
	return 0;
}

//! Worm inherits Object

/*
int l_worm_getPlayer(lua_State* L)
{
	CWorm* p = static_cast<CWorm *>(lua_touserdata (L, 1));
	if(!p->getOwner())
		return 0;
	lua.pushReference(p->getOwner()->luaReference);
	return 1;
}
*/

/*! Worm:health()

	(Known as get_health before 0.9c)
	Returns the health of this worm.
*/
METHODC(CWorm, worm_getHealth,  {
	context.push(p->getHealth());
	return 1;
})

#ifndef NO_DEPRECATED
int l_worm_getHealth_depr(lua_State* L)
{
	LuaContext context(L);
	LUA_WLOG_ONCE("get_health is deprecated, use the health method instead");
	return l_worm_getHealth(L);
}
#endif

METHODC(CWorm, worm_isChanging,  {
	context.push(p->isChangingWpn());
	return 1;
})


/*@ Worm:set_weapon(slot, weapon)

	Sets the weapon of a slot in [1, max] to the WeaponType //weapon//.
*/
/* TODO
LMETHOD(CWorm, worm_setWeapon,  (
	p->m_weapons )*/
	
/*
int l_worm_remove(lua_State* L)
{
	CWorm* p = static_cast<CWorm *>(lua_touserdata (L, 1));
	p->deleteMe = true;
	return 0;
}

int l_worm_pos(lua_State* L)
{
	CWorm* p = static_cast<CWorm *>(lua_touserdata (L, 1));
	lua_pushnumber(L, p->pos.x);
	lua_pushnumber(L, p->pos.y);
	return 2;
}

int l_worm_spd(lua_State* L) //
{
	CWorm* p = static_cast<CWorm *>(lua_touserdata (L, 1));
	lua_pushnumber(L, p->spd.x);
	lua_pushnumber(L, p->spd.y);
	return 2;
}

int l_worm_push(lua_State* L) //
{
	CWorm* p = static_cast<CWorm *>(lua_touserdata (L, 1));
	p->spd.x += lua_tonumber(L, 2);
	p->spd.y += lua_tonumber(L, 3);
	return 0;
}

int l_worm_data(lua_State* L)
{
	CWorm* p = static_cast<CWorm *>(lua_touserdata (L, 1));
	if(p->luaData)
	{
		lua.pushReference(p->luaData);
	}
	else
	{
		lua_newtable(L);
		lua_pushvalue(L, -1);
		p->luaData = lua.createReference();
	}
	
	return 1;
}

int l_worm_shoot(lua_State* L)
{
	CWorm* object = static_cast<CWorm *>(lua_touserdata (L, 1));
	
	return shootFromObject(L, object);
}*/

METHODC(CWorm, worm_current_weapon,  {
	if(Weapon* w = p->getCurrentWeaponRef())
	{
		w->pushLuaReference(context);
		return 1;
	}
	return 0;
})

METHODC(CWorm, worm_setSkinVisible,  {
	p->gusSkinVisble = lua_toboolean(context, 2) != 0;
	return 0;
})
	
	
/*
LBINOP(CWorm, worm_eq,  (
	context.push(a == b);
	return 1;
))*/


/*! Object:angle()

	(Known as get_angle before 0.9c)
	Returns the current angle of the object.
*/

METHODC(CGameObject, baseObject_getAngle,  {
	lua_pushnumber(context, p->getPointingAngle().toDeg());
	return 1;
})

#ifndef NO_DEPRECATED
int l_baseObject_getAngle_depr(lua_State* L)
{
	LuaContext context(L);
	LUA_WLOG_ONCE("get_angle is deprecated, use the angle method instead");
	return l_baseObject_getAngle(L);
}
#endif

/*! Object:remove()

	Removes the object in the next frame.
*/
METHODC(CGameObject, baseObject_remove,  {
	p->deleteMe = true;
	return 0;
})

/*! Object:pos()

	Returns the position of this object as a tuple.
	i.e.:
	<code>
	local x, y = object:pos()
	</code>
*/

METHODC(CGameObject, baseObject_pos,  {
	context.push(posVelTempHack.getPos(p).x);
	context.push(posVelTempHack.getPos(p).y);
	return 2;
})

/*! Object:set_pos(x, y)

	Moves the object to the location (x, y)
	e.g.:
	<code>
	object:set_spd(0, 0) -- Moves the object to the upper-left corner
	</code>
*/
METHODC(CGameObject, baseObject_setPos,  {
	posVelTempHack.setPos(p, Vec((float)lua_tonumber(context, 2), (float)lua_tonumber(context, 3)));
	return 0;
})

/*! Object:spd()

	Returns the speed of this object as a tuple.
	e.g.:
	<code>
	local vx, vy = object:spd()
	</code>
*/
METHODC(CGameObject, baseObject_spd,  {
	context.push(posVelTempHack.getVel(p).x);
	context.push(posVelTempHack.getVel(p).y);
	return 2;
})

//! version 0.9c

/*! Object:set_spd(x, y)

	Changes the speed of this object to (x, y)
	e.g.:
	<code>
	object:set_spd(10, 0) -- Makes the object move to the right
	</code>
*/
METHODC(CGameObject, baseObject_setSpd, {
	posVelTempHack.setVel(p, Vec((float)lua_tonumber(context, 2), (float)lua_tonumber(context, 3)));
	return 0;
})

//! version any

/*! Object:push(x, y)
	
	Accelerates the object in the direction (x, y)
	e.g.:
	<code>
	object:push(0, 10) -- Accelerates the object downwards
	</code>
*/
METHODC(CGameObject, baseObject_push,  {
	Vec push((float)lua_tonumber(context, 2), (float)lua_tonumber(context, 3));
	posVelTempHack.setVel(p, posVelTempHack.getVel(p) + push);
	return 0;
})

/*! Object:data()

	Returns a table associated with this object that can
	be used by Lua scripts to store values.
*/

METHODC(CGameObject, baseObject_data,  {
	if(p->luaData.isSet(context))
	{
		context.push(p->luaData);
	}
	else
	{
		lua_newtable(context);
		context.pushvalue(-1);
		p->luaData.create(context);
	}
	
	return 1;
})

/*! Object:player()

	(Known as get_player before 0.9c)
	
	Returns a CWormHumanInputHandler object of the player that owns this object.
*/
METHODC(CGameObject, baseObject_getPlayer, {
	if(p->getOwner() == NULL || p->getOwner()->getWorm() == NULL)
		return 0;
	context.push(p->getOwner()->getLuaReference());
	return 1;
})

#ifndef NO_DEPRECATED
int l_baseObject_getPlayer_depr(lua_State* L)
{
	LuaContext context(L);
	LUA_WLOG_ONCE("get_player is deprecated, use the player method instead");
	return l_baseObject_getPlayer(L);
}
#endif

//! version 0.9c

/*! Object:damage(amount[, player])

	Causes damage to the object. //player// is the player inflicting the damage.
	If //player// isn't specified or nil, the damage is anonymous.
*/
METHODC(CGameObject, baseObject_damage,  {
	lua_Number amount = lua_tonumber(context, 2);
	//CWormInputHandler* player = *static_cast<CWormInputHandler **>(lua_touserdata(context, 3));
	CWormInputHandler* player = getObject<CWormInputHandler>(context, 3);
	p->damage((float)amount, player);
	return 1;
})

//! version any

/*! Object:closest_worm()

	(Known as get_closest_worm before 0.9c)

	Returns the closest worm that fulfills these requirements:
	
	  * It is not the owner of the object.
	  * It is visible and active.
	  * The straight path to it from the object is not, for particles, blocked.
*/
METHODC(CGameObject, baseObject_getClosestWorm,  {

	Vec from = p->pos();
	
	int fromx = int(from.x);
	int fromy = int(from.y);
	
	CWorm* minWorm = 0;
	float minDistSqr = 10000000.f;
	
	for(std::vector<CWormInputHandler*>::iterator playerIter = game.players.begin(); playerIter != game.players.end(); ++playerIter)
	{
		if(p->getOwner() != *playerIter)
		{
			CWorm* worm = (*playerIter)->getWorm();
		
			if(worm->isActive())
			//if(worm->isActive())
			{
				float distSqr = (Vec(worm->pos()) - from).lengthSqr();
				if(distSqr < minDistSqr && !game.gameMap()->trace(fromx, fromy, int(worm->pos().get().x), int(worm->pos().get().y), CMap::ParticleBlockPredicate()))
				{
					minDistSqr = distSqr;
					minWorm = worm;
				}
			}
		}
	}
	
	if(!minWorm)
		return 0;
		
	minWorm->pushLuaReference(context);

	return 1;
})

#ifndef NO_DEPRECATED
int l_baseObject_getClosestWorm_depr(lua_State* L)
{
	LuaContext context(L);
	LUA_WLOG_ONCE("get_closest_worm is deprecated, use the closest_worm method instead");
	return l_baseObject_getClosestWorm(L);
}
#endif

/*! Object:shoot(type, amount, speed, speedVariation, motionInheritance, amountVariation, distribution, angleOffset, distanceOffset)

	Shoots an object of ParticleType 'type'. All parameters except 'type' are optional.
*/

METHODC(CGameObject, baseObject_shoot,  {
	return shootFromObject(context, p);
})

/*

METHOD(CGameObject, baseObject_getAngle,  (
	lua_pushnumber(context, p->getAngle().toDeg());
	return 1;
))
*/

//! Particle inherits Object

/*! Particle:set_angle(angle)

	Changes the angle of the particle to //angle//.
*/

METHODC(Particle, particle_setAngle,  {
	p->setPointingAngle(Angle((double)lua_tonumber(context, 2)));
	return 0;
})

//! version 0.9c

/*! Particle:set_replication(type, state)

	If the particle is network-aware, this function turns on or off replication
	of different aspects of the particle's state.
	
	Note that this function can only affects things for
	which replication is activated in the omfgScript of the particle type.
	
	Since the server has authority over particle replication, this function will
	have no affect on clients.
	
	//type// can be one of the following:
	Particle.Position : replication of position.
	
	//state// is either true or false, where true turns on replication and false turns it off.
*/

METHODC(Particle, particle_set_replication,  {
	
	int mask = 0;
	switch(lua_tointeger(context, 2))
	{
		case ParticleRep::Position:
			mask = Particle::RepPos;
		break;
		default:
			return 0;
	}
	
	if(lua_toboolean(context, 3))
		p->setFlag(mask);
	else
		p->resetFlag(mask);

	return 0;
})

/*! Weapon:is_reloading()

	Returns true if this weapon is reloading.
*/

METHODC(Weapon, weaponinst_reloading,  {
	context.push(p->reloading);
	return 1;
})

/*! Weapon:reload_time()

	Returns the reload time left on this weapon.
	Does only make sense if is_reloading() is true.
*/
METHODC(Weapon, weaponinst_reload_time,  {
	context.push(p->getReloadTime());
	return 1;
})

/*! Weapon:ammo()

	Returns the amount of ammo left in this weapon.
*/
METHODC(Weapon, weaponinst_ammo,  {
	context.push(p->getAmmo());
	return 1;
})

/*! Weapon:type()

	Returns the weapon type in the form of a WeaponType object.
*/
METHODC(Weapon, weaponinst_type,  {
	p->getType()->pushLuaReference(context);
	return 1;
})

// StackTop should be the metatable.
void addGameObjectFunctions(LuaContext& context)
{
	context.tableFunctions()
		("remove", l_baseObject_remove)
		("pos", l_baseObject_pos)
		("spd", l_baseObject_spd)
		("push", l_baseObject_push)
#ifndef NO_DEPRECATED
		("get_player", l_baseObject_getPlayer_depr)
		("get_closest_worm", l_baseObject_getClosestWorm_depr)
		("get_angle", l_baseObject_getAngle_depr)
#endif
		("player", l_baseObject_getPlayer)
		("closest_worm", l_baseObject_getClosestWorm)
		("data", l_baseObject_data)
		("shoot", l_baseObject_shoot)
		("angle", l_baseObject_getAngle)
		("set_pos", l_baseObject_setPos)
		("set_spd", l_baseObject_setSpd)
		("damage", l_baseObject_damage)
	;
}

static int l_list_iterator(lua_State* L) {
	LuaContext context(L);
	if(!lua_istable(L, 1)) {
		context.pushError("list iterator: bad arguments");
		return 0;
	}

	lua_getfield(L, 1, "i");
	int i = (int)lua_tonumber(L, -1);
	context.pop();
	if(i < 0) {
		context.pushError("list iterator: index negative");
		return 0;
	}

	lua_getfield(L, 1, "list");
	DynamicList* l = getObject<DynamicList>(context, -1);
	context.pop();
	if(!l) {
		context.pushError("list iterator: list invalid");
		return 0;
	}

	if((size_t)i >= l->size()) {
		lua_pushnil(L);
		return 1;
	}

	lua_pushinteger(L, i + 1);
	lua_setfield(L, 1, "i");

	context.pushScriptVar(l->getGeneric((size_t)i));
	return 1;
}

static int l_list_getIterator(lua_State* L) {
	lua_pushcfunction(L, l_list_iterator);
	lua_newtable(L); // state
	{
		lua_pushnumber(L, 0);
		lua_setfield(L, -2, "i");

		lua_pushvalue(L, 1); // the list
		lua_setfield(L, -2, "list");
	}
	lua_pushnil(L);
	return 3;
}

void initObjects(LuaContext& context)
{
	// The verbose commenting here is on purpose for readers not familiar with Lua.
	// Read also this: http://www.lua.org/manual/5.1/manual.html#3.1

	BaseObject::initMetaTable(context);

	// CGameObject method and metatable
	{
		lua_newtable(context);
		{
			lua_newtable(context);
			{
				addGameObjectFunctions(context); // [0,0]
			}
			initBaseObjMetaTable(context, 1); // [-1,0]
		}

		context.tableSetField(LuaID<CGameObject>::value); // does t[n] = v, where t=top(table), n=param(id value), v=True
		CGameObject::metaTable.create(context); // also pops one element
	}

	// Particle method and metatable
	{
		lua_newtable(context);
		{
			lua_newtable(context);
			{
				addGameObjectFunctions(context);

				context.tableFunctions()
						("set_angle", l_particle_setAngle)
						("set_replication", l_particle_set_replication)
						;
			}
			initBaseObjMetaTable(context, 1);
		}

		context.tableSetField(LuaID<Particle>::value);
		context.tableSetField(LuaID<CGameObject>::value);
		Particle::metaTable.create(context);
	}

	// Worm method and metatable
	{
		lua_newtable(context);
		{
			lua_newtable(context);
			{
				addGameObjectFunctions(context); // CWorm inherits from CGameObject

				context.tableFunctions()
		#ifndef NO_DEPRECATED
						("get_health", l_worm_getHealth_depr)
		#endif
						("health", l_worm_getHealth)
						("current_weapon", l_worm_current_weapon)
						("is_changing", l_worm_isChanging)
						("setSkinVisible", l_worm_setSkinVisible)
						;
			}
			initBaseObjMetaTable(context, 1);
		}

		context.tableSetField(LuaID<CWorm>::value);
		context.tableSetField(LuaID<CGameObject>::value);
		CWorm::metaTable.create(context);
	}

	// CNinjaRope
	{
		lua_newtable(context);
		{
			lua_newtable(context);
			{
				addGameObjectFunctions(context);
			}
			initBaseObjMetaTable(context, 1);
		}
		context.tableSetField(LuaID<CNinjaRope>::value);
		context.tableSetField(LuaID<CGameObject>::value);
		CNinjaRope::metaTable.create(context);
	}

	// DynamicList
	{
		context.newtable();
		{
			initBaseObjMetaTable(context, 0);

			context.push("__call");
			lua_pushcfunction(context, l_list_getIterator);
			lua_rawset(context, -3);
		}
		context.tableSetField(LuaID<DynamicList>::value);
		DynamicList::metaTable.create(context);
	}

	// ----

	CLASS_(Weapon,
		("is_reloading", l_weaponinst_reloading)
		("reload_time", l_weaponinst_reload_time)
		("ammo", l_weaponinst_ammo)
		("type", l_weaponinst_type)
	)
	
	ENUM(Particle,  
		("Position", ParticleRep::Position)
	)
}

}
