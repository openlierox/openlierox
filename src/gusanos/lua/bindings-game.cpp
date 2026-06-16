#include "bindings-game.h"
#include "bindings-objects.h"
#include "bindings.h"

#include "../luaapi/types.h"
#include "../luaapi/macros.h"
#include "../luaapi/classes.h"

#include "../gconsole.h"
#include "../gusgame.h"
#include "game/WormInputHandler.h"
#include "CWormHuman.h"
#include "game/CWorm.h"
#include "../level.h"
#include "util/log.h"
#include "util/stringbuild.h"
#include "game/CMap.h"
#include "game/Game.h"
#include "CClientNetEngine.h"
#include "game/SinglePlayer.h"
#include "OLXCommand.h"
#include "gusanos/weapon_type.h"
#include "CServerConnection.h" // ClientRights
#include "OLXCommand.h"
#include "Timer.h"

#include <cmath>
#include <iostream>
#include <list>
#include "gusanos/allegro.h"
using std::cerr;
using std::endl;
#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
using boost::lexical_cast;
using namespace boost::placeholders;

namespace LuaBindings
{
	
LuaReference playerIterator;
LuaReference CWormInputHandlerMetaTable;

LUA_CALLBACK(luaControl(LuaContext context, LuaReference ref, size_t playerIdx, bool state, std::list<std::string> const& args))
	if(playerIdx >= game.localPlayers.size())
		LUA_ABORT();
	game.localPlayers[playerIdx]->pushLuaReference(context);
	context.push(state);
	params += 2;
END_LUA_CALLBACK()


//! version 0.9c

/*! console_register_control(name, function)

	Registers a number of console commands that work like player controls.
	
	//function// is a function that is called when the control is activated or
	deactivated. It is of the form:
	<code>
	function(player, state)
	end
	</code>
	
	Where //player// is the CWormHumanInputHandler object the control applies to, and //state// is either true
	or false depending on if the control is activated or deactivated.
	
	The control can be bound to keys like built-in controls such as FIRE and JUMP
	by binding to the command +P//x//_//name// where //x// is the local player number
	and //name// is the name passed to this function.
*/

int l_console_register_control(lua_State* L)
{
	LuaContext context(L);

	char const* name = lua_tostring(L, 1);
	if(!name) return 0;
	lua_pushvalue(L, 2);
	LuaReference ref;
	ref.create(context);
	
	for(size_t i = 0; i < GusGame::MAX_LOCAL_PLAYERS; ++i)
	{
		console.registerCommands()
			((S_("+P") << i << '_' << name), boost::bind(LuaBindings::luaControl, context, ref, i, true, _1), true)
			((S_("-P") << i << '_' << name), boost::bind(LuaBindings::luaControl, context, ref, i, false, _1), true);
	}
	
	return 0;
}



//! version OLX 0.59 b9 (or so)

static int l_olx_setLevelSucceeded(lua_State* L) {
	singlePlayerGame.setLevelSucceeded();
	return 0;
}

//! version OLX 0.59 b9 (or so)

static int l_olx_message(lua_State* L) {
	std::string msg = lua_tostring(L,1);
	notes << "Lua: OLX message: " << msg << endl;
	game.hudPermanentText = msg;
	return 0;
}
		
		
//! version any

/*! game_players()

	Returns an iterator object that returns a CWormHumanInputHandler object
	for every player in the gusGame.
	
	Intended to be use together
	with a for loop, like this:
	<code>
	for p in game_players() do
	    -- Do something with p here
	end
	</code>
*/
int l_game_players(lua_State* L)
{
	LuaContext context(L);
	context.push(LuaBindings::playerIterator);
	typedef long iter;
	iter& i = *(iter *)lua_newuserdata_init (context, sizeof(iter));
	i = 0;
	lua_pushnil(context);
	
	return 3;
}

/*! game_local_player(i)

	Returns a CWormHumanInputHandler object of the local player with index i.
	If the index is invalid, nil is returned.
*/
int l_game_localPlayer(lua_State* L)
{
	LuaContext context(L);
	size_t i = (size_t)lua_tointeger(context, 1);
	if(i < game.localPlayers.size())
	{
		game.localPlayers[i]->pushLuaReference(context);
		return 1;
	}
	else
		return 0;
}

int l_game_localPlayerName(lua_State* L)
{
	LuaContext context(L);
	size_t i = (int)lua_tointeger(context, 1);
	if(i < game.localPlayers.size())
	{
		context.push(game.localPlayers[i]->name());
		return 1;
	}
	else
		return 0;
}


/*! CWormHumanInputHandler:kills()

	Returns the number of kills a player has made.
*/
METHODC(CWormInputHandler, player_kills,  {
	context.push(p->stats->kills);
	return 1;
})


/*! CWormHumanInputHandler:deaths()

	Returns the number of deaths a player has suffered.
*/
METHODC(CWormInputHandler, player_deaths,  {
	context.push(p->stats->deaths);
	return 1;
})

/*! CWormHumanInputHandler:name()

	Returns the name of the player.
*/
METHODC(CWormInputHandler, player_name,  {
	if(p->worm()) {
		context.push(p->worm()->getName().c_str());
		return 1;
	}
	return 0;
})

/*! CWormHumanInputHandler:team()

	Returns the team number of the player.
*/
METHODC(CWormInputHandler, player_team,  {
	if(p->worm()) {
		// +1 because OLX team nrs start at 0, gus team nrs at 1
		context.push(p->worm()->getTeam() + 1);
		return 1;		
	}
	return 0;
})

/*! CWormHumanInputHandler:worm()

	Returns the worm of the player.
*/
METHODC(CWormInputHandler, player_worm,  {
	if(CWorm* worm = p->getWorm())
	{
		worm->pushLuaReference(context);
		return 1;
	}
	return 0;
})

//! version 0.9c

/*! CWormHumanInputHandler:is_local()

	Returns true if this player is a local player, otherwise false.
*/
METHODC(CWormInputHandler, player_isLocal,  {
	context.push(p->local);
	return 1;
})

//! version any

/*! CWormHumanInputHandler:data()

	Returns a lua table associated with this player.
*/
METHODC(CWormInputHandler, player_data,  {
	if(p->luaData.isSet(context))
		context.push(p->luaData);
	else
	{
		context
			.newtable()
			.pushvalue(-1);
		p->luaData.create(context);
	}
	
	return 1;
})

/*! CWormHumanInputHandler:stats()

	Returns a lua table associated with the stats of this player.
*/
METHODC(CWormInputHandler, player_stats,  {
	if(p->stats->luaData.isSet(context))
		context.push(p->stats->luaData);
	else
	{
		context
			.newtable()
			.pushvalue(-1);
		p->stats->luaData.create(context);
	}
	
	return 1;
})

/*! CWormHumanInputHandler:say(text)

	Makes the player send 'text' as a chat message.
*/
METHODC(CWormInputHandler, player_say,  {
	char const* s = context.tostring(2);
	CWorm* w = p->worm();
	if(s && w) {
		cClient->getNetEngine()->SendText(s, w->getName());
	}
	return 0;
})

/*! CWormHumanInputHandler:select_weapons(weapons)

	Tries to change the player's weapons to the WeaponType objects
	in the array //weapons//.
*/
METHODC(CWormInputHandler, player_selectWeapons,  {
	
	std::vector<WeaponType *> weapons;
	for(size_t i = 1;; ++i)
	{
		context.rawgeti(2, i);
		if(lua_isnil(context, -1))
		{
			break;
		}
		else
		{
			//WeaponType* weapon = *static_cast<WeaponType **>(lua_touserdata(context, -1));
			WeaponType* weapon = ASSERT_OBJECT_P(WeaponType, -1, "in weapon array");
			weapons.push_back(weapon);
			context.pop(); // Pop value
		}
	}
	
	p->selectWeapons(weapons);
	
	return 0;
})


//! version OLX 0.59b10

/*! CWormHumanInputHandler:weaponTypes()

	Returns an array of the selected weapon types.
*/
METHODC(CWormInputHandler, player_weaponTypes,  {
	std::vector<WeaponType*> wpns(p->worm()->getWeaponTypes());
	context.push(wpns);

	return 1;
})

int l_game_getClosestWorm(lua_State* L)
{
	LuaContext context(L);

	Vec from((float)lua_tonumber(L, 1), (float)lua_tonumber(L, 2));
	
	CWorm* minWorm = 0;
	float minDistSqr = 10000000.f;
	
	for(std::vector<CWormInputHandler*>::iterator playerIter = game.players.begin(); playerIter != game.players.end(); ++playerIter)
	{
		CWorm* worm = (*playerIter)->getWorm();
		
		if(worm->isActive())
		{
			float distSqr = (Vec(worm->pos()) - from).lengthSqr();
			if(distSqr < minDistSqr)
			{
				minDistSqr = distSqr;
				minWorm = worm;
			}
		}
	}
	
	if(!minWorm)
		return 0;
		
	minWorm->pushLuaReference(context);
	return 1;
}

// see l_game_players() for reference
int l_game_playerIterator(lua_State* L)
{
	LuaContext context(L);
	if(lua_gettop(context) < 1) return 0; // bad usage of iterator

	typedef long iter;
	iter* iPt = (iter *)lua_touserdata(context, 1);
	if(iPt == NULL) return 0; // bad usage of iterator
	iter& i = *iPt;
	if(i < 0 || (size_t)i >= game.players.size())
		lua_pushnil(context);
	else {
		std::vector<CWormInputHandler*>::iterator it = game.players.begin() + i;
		(*it)->pushLuaReference(context);
		++i;
	}
	
	return 1;
}


/*! map_is_blocked(x1, y1, x2, y2)

	Returns true if the line between (x1, y1) and (x2, y2) on the map is blocked for particles.
	Otherwise false.
*/
int l_map_isBlocked(lua_State* L)
{
	int x1 = lua_tointeger(L, 1);
	int y1 = lua_tointeger(L, 2);
	int x2 = lua_tointeger(L, 3);
	int y2 = lua_tointeger(L, 4);
	lua_pushboolean(L, game.gameMap()->trace(x1, y1, x2, y2, CMap::ParticleBlockPredicate()));
	return 1;
}

/*! map_is_particle_pass(x1, y1)

	Returns true if the point (x1, y1) on the map is passable by particles.
*/

int l_map_isParticlePass(lua_State* L)
{
	int x = lua_tointeger(L, 1);
	int y = lua_tointeger(L, 2);

	lua_pushboolean(L, game.gameMap()->getMaterial(x, y).particle_pass);
	return 1;
}


static int l_worms_get(lua_State* L) {
	LuaContext context(L);

	if(lua_isnumber(L, 2)) {
		// try worm index
		int i = lua_tointeger(L, 2);
		CWorm* w = game.wormById(i, false);
		if(!w) {
			context.pushError("worm ID " + itoa(i) + " is invalid");
			return 0;
		}
		 w->pushLuaReference(context);
		 return 1;
	}

	const char* _s = lua_tostring(L, 2); // attrib
	if(_s == NULL) return 0;
	std::string s = _s;

	// check if "w[0-9]+"
	if(s.size() >= 2 && s[0] == 'w') {
		bool fail = false;
		uint i = from_string<uint>(s.substr(1), fail);
		if(!fail) {
			CWorm* w = game.wormById(i, false);
			if(!w) {
				context.pushError("worm ID " + itoa(i) + " is invalid");
				return 0;
			}
			w->pushLuaReference(context);
			return 1;
		}
	}

	context.pushError("doesn't understand '" + s + "'");
	return 0;
}

static int l_worms_iterator(lua_State* L) {
	LuaContext context(L);

	int lastWormId = -1;
	CWorm* lastWorm = getObject<CWorm>(context, 2);
	if(lastWorm) lastWormId = lastWorm->getID();

	for_each_iterator(CWorm*, w, game.worms()) {
		if(w->get()->getID() > lastWormId) {
			w->get()->pushLuaReference(context);
			return 1;
		}
	}

	lua_pushnil(L);
	return 1;
}

static int l_worms_getIterator(lua_State* L) {
	lua_pushcfunction(L, l_worms_iterator);
	lua_pushnil(L);
	lua_pushnil(L);
	return 3;
}

static void initWormsWrapper(LuaContext& context) { // [0,1]
	context.newtable(); // worms wrapper object

	// metatable object
	{
		context.newtable();
		{
			lua_pushstring(context, "__index");
			lua_pushcfunction(context, l_worms_get);
			lua_rawset(context, -3);

			lua_pushstring(context, "__call");
			lua_pushcfunction(context, l_worms_getIterator);
			lua_rawset(context, -3);
		}
		lua_setmetatable(context, -2);
	}
}

static int l_gameSettings_get(lua_State* L) {
	LuaContext context(L);
	const char* name = lua_tostring(context, 2);
	if(!name) {
		context.pushError("bad arguments");
		return 0;
	}

	Feature* f = featureByName(name);
	if(!f) {
		context.pushError("no game setting '" + std::string(name) + "'");
		return 0;
	}

	if(game.state < Game::S_Lobby) {
		context.pushError("game-settings only available in game");
		return 0;
	}

	context.pushScriptVar(cClient->getGameLobby()[f]);
	return 1;
}

static int l_gameSettings_set(lua_State* L) {
	LuaContext context(L);
	const char* name = lua_tostring(context, 2);
	if(!name) {
		context.pushError("bad arguments");
		return 0;
	}

	Feature* f = featureByName(name);
	if(!f) {
		context.pushError("no game setting '" + std::string(name) + "'");
		return 0;
	}

	if(game.state < Game::S_Lobby) {
		context.pushError("game-settings only available in game");
		return 0;
	}

	if(game.isClient()) {
		context.pushError("game-settings are only writeable as server");
		return 0;
	}

	ScriptVar_t newValue;
	if(NegResult r = context.toScriptVar(3, newValue)) {
		context.pushError("value error: " + r.res.humanErrorMsg);
		return 0;
	}

	ScriptVar_t value = gameSettings[f];
	if(NegResult r = value.fromScriptVar(newValue, true, false)) {
		context.pushError("value conversion error: " + r.res.humanErrorMsg);
		return 0;
	}

	/*
	  // hm, it might make sense to silently ignore this.
	  // there is no simple exception handling in Lua.
	if(!gameSettings.isRelevant(modSettings, f)) {
		FeatureSettingsLayer* relevantLayer = gameSettings.layerFor(featureArrayIndex(f));
		if(relevantLayer)
			context.pushError("cannot make effective setting change, effective layer is: " + relevantLayer->debug_name);
		else
			context.pushError("cannot make effective setting change, " + modSettings.debug_name + " layer is not registered in this mode");
		return 0;
	}
	*/

	modSettings.set(featureArrayIndex(f)).fromScriptVar(value);
	return 0;
}

static void initGameSettingsWrapper(LuaContext& context) { // [0,1]
	context.newtable(); // settings wrapper object

	// metatable object
	{
		context.newtable();
		{
			lua_pushstring(context, "__index");
			lua_pushcfunction(context, l_gameSettings_get);
			lua_rawset(context, -3);

			lua_pushstring(context, "__newindex");
			lua_pushcfunction(context, l_gameSettings_set);
			lua_rawset(context, -3);
		}
		lua_setmetatable(context, -2);
	}
}

static void initGameWrapper(LuaContext& context) {
	// game object
	BaseObject** p = (BaseObject **)lua_newuserdata_init(context, sizeof(void*));
	*p = &game;

	{
		context.newtable(); // meta
		{
			context.newtable(); // meta exteded index table
			{
				context.push("worms");
				initWormsWrapper(context);
				lua_rawset(context, -3);

				context.push("settings");
				initGameSettingsWrapper(context);
				lua_rawset(context, -3);
			}
			initBaseObjMetaTable(context, 1);
		}
		context.tableSetField(LuaID<CGameObject>::value);

		lua_setmetatable(context, -2);
	}
}

static void initSettingsWrapper(LuaContext& context, const std::string& snamespace);

static int l_settings_get(lua_State* L) {
	LuaContext context(L);

	const char* snamespace = lua_tostring(context, lua_upvalueindex(1));
	if(!snamespace) snamespace = "";

	const char* varname = lua_tostring(context, 2);
	if(!varname) {
		context.pushError("bad arguments");
		return 0;
	}

	std::string fullVarName = std::string(snamespace) + varname;

	if(CScriptableVars::haveSomethingWith(fullVarName + ".")) {
		initSettingsWrapper(context, fullVarName + ".");
		return 1;
	}

	RegisteredVar* var = CScriptableVars::GetVar(fullVarName);
	if(!var) {
		context.pushError("no var named " + fullVarName);
		return 0;
	}

	ClientRights rights; rights.Everything();
	if(NegResult r = var->allowedToAccess(false, rights)) {
		context.pushError("no read access: " + r.res.humanErrorMsg);
		return 0;
	}

	context.pushScriptVar(var->var.asScriptVar());
	return 1;
}

static int l_settings_set(lua_State* L) {
	LuaContext context(L);

	if(context != luaGlobal) {
		context.pushError("settings are not writeable from within game scripts. use game.settings instead for game-related settings.");
		return 0;
	}

	const char* snamespace = lua_tostring(context, lua_upvalueindex(1));
	if(!snamespace) snamespace = "";

	const char* varname = lua_tostring(context, 2);
	if(!varname) {
		context.pushError("bad arguments");
		return 0;
	}

	std::string fullVarName = std::string(snamespace) + varname;

	RegisteredVar* var = CScriptableVars::GetVar(fullVarName);
	if(!var) {
		context.pushError("no var named " + fullVarName);
		return 0;
	}

	ClientRights rights; rights.Everything();
	if(NegResult r = var->allowedToAccess(true, rights)) {
		context.pushError("no write access: " + r.res.humanErrorMsg);
		return 0;
	}

	ScriptVar_t newValue;
	if(NegResult r = context.toScriptVar(3, newValue)) {
		context.pushError("value error: " + r.res.humanErrorMsg);
		return 0;
	}

	ScriptVar_t value = var->var.asScriptVar();
	if(NegResult r = value.fromScriptVar(newValue, true, false)) {
		context.pushError("value conversion error: " + r.res.humanErrorMsg);
		return 0;
	}

	var->var.fromScriptVar(value);
	return 0;
}

static void initSettingsWrapper(LuaContext& context, const std::string& snamespace) {
	context.newtable(); // settings wrapper object

	{
		context.newtable(); // meta
		{
			context.push("__index");
			context.push(snamespace);
			lua_pushcclosure(context, l_settings_get, 1);
			lua_rawset(context, -3);

			context.push("__newindex");
			context.push(snamespace);
			lua_pushcclosure(context, l_settings_set, 1);
			lua_rawset(context, -3);
		}
		lua_setmetatable(context, -2);
	}
}

static int l_commands_exec(lua_State* L) {
	LuaContext context(L);

	if(context != luaGlobal) {
		context.pushError("OLX command execution only allowed from global Lua");
		return 0;
	}

	const char* cmdname = lua_tostring(context, lua_upvalueindex(1));
	if(!cmdname) {
		context.pushError("bad closure value");
		return 0;
	}

	CommandDesc* cmd = GetCommandDesc(cmdname);
	if(!cmd) {
		context.pushError("bad closure value. no command named '" + std::string(cmdname) + "'");
		return 0;
	}

	unsigned int nargs = lua_gettop(L);
	if(nargs < cmd->minParams) {
		context.pushError("too less args (" + itoa(nargs) + ") given, " + itoa(cmd->minParams) + " needed");
		return 0;
	}
	if(nargs > cmd->maxParams) {
		context.pushError("too much args (" + itoa(nargs) + ") given, " + itoa(cmd->maxParams) + " maximum");
		return 0;
	}

	std::vector<std::string> args(nargs);
	for(unsigned int i = 0; i < nargs; ++i)
		args[i] = context.convert_tostring(i + 1);

	struct LuaContextCLI : CmdLineIntf {
		LuaContext& context;
		int nreturns;
		LuaContextCLI(LuaContext& c_) : context(c_), nreturns(0) {}

		virtual void pushReturnArg(const std::string& str) {
			context.push(str);
			nreturns++;
		}
		virtual void finalizeReturn() {}

		virtual void writeMsg(const std::string& msg, CmdLineMsgType type) {
			lua_getfield(context, LUA_GLOBALSINDEX, "print");
			context.push(msg);
			lua_call(context, 1, 0);
		}
	};
	LuaContextCLI cli(context);

	cmd->exec(&cli, args);
	return cli.nreturns;
}

static int l_commands_get(lua_State* L) {
	LuaContext context(L);

	const char* cmdname = lua_tostring(context, 2);
	if(!cmdname) {
		context.pushError("bad arguments");
		return 0;
	}

	CommandDesc* cmd = GetCommandDesc(cmdname);
	if(!cmd) {
		context.pushError("no command named '" + std::string(cmdname) + "'");
		return 0;
	}

	lua_pushstring(context, cmdname);
	lua_pushcclosure(context, l_commands_exec, 1);
	return 1;
}

static void initCommandsWrapper(LuaContext& context) {
	context.newtable();
	{
		context.newtable(); // meta
		{
			context.push("__index");
			lua_pushcfunction(context, l_commands_get);
			lua_rawset(context, -3);
		}
		lua_setmetatable(context, -2);
	}
}

static int l_system_get(lua_State* L) {
	LuaContext context(L);

	const char* s_ = lua_tostring(L, 2);
	if(!s_) {
		context.pushError("bad arguments");
		return 0;
	}
	std::string s = s_;

	if(s == "time") {
		lua_pushinteger(L, (lua_Integer)GetTime().milliseconds());
		return 1;
	}

	if(s == "version") {
		lua_pushstring(L, GetFullGameName());
		return 1;
	}

	context.pushError("system attrib '" + s + "' unknown");
	return 0;
}

static void initSystemWrapper(LuaContext& context) {
	context.newtable();
	{
		context.newtable(); // meta
		{
			context.push("__index");
			lua_pushcfunction(context, l_system_get);
			lua_rawset(context, -3);
		}
		lua_setmetatable(context, -2);
	}
}

struct Data_lua_setTimeout {
	LuaReference ref;
	LuaContext context;
};

static void lua_setTimeout_callback(Timer::EventData ev) {
	Data_lua_setTimeout* data = (Data_lua_setTimeout*)ev.userData;

	if(data->context) {
		// error func
		lua_pushcfunction(data->context, LuaContext::errorReport);
		// callback func
		data->ref.push(data->context);

		int result = lua_pcall(data->context, 0, 0, -2);

		switch(result) {
		case LUA_ERRRUN:
		case LUA_ERRMEM:
		case LUA_ERRERR:
			warnings << "Lua setTimeout exec error: " + std::string(data->context.tostring(-1)) << endl;
			data->context.pop(1); // pop error message
		}

		data->context.pop(1); // pop error func
	}

	data->ref.destroy();
	delete data;
}

static int l_setTimeout(lua_State* L) {
	LuaContext context(L);
	if(!lua_isnumber(L, 2)) {
		context.pushError("bad arguments");
		return 0;
	}
	int msec = lua_tointeger(L, 2);
	if(msec < 0) {
		context.pushError("bad arguments. time must be >= 0");
		return 0;
	}

	Data_lua_setTimeout* data = new Data_lua_setTimeout;
	lua_pushvalue(L, 1);
	data->ref.create(context);
	data->context = context;

	Timer("Lua timer", lua_setTimeout_callback, data, msec, true).startHeadless();

	return 0;
}

void initGame(LuaContext& context)
{	
	context.function("game_players", l_game_players);
	lua_pushcfunction(context, l_game_playerIterator);
	playerIterator.create(context);

	{
		initGameWrapper(context);
		lua_setfield(context, LUA_GLOBALSINDEX, "game");
	}
	{
		initSettingsWrapper(context, "");
		lua_setfield(context, LUA_GLOBALSINDEX, "settings");
	}
	{
		initCommandsWrapper(context);
		lua_setfield(context, LUA_GLOBALSINDEX, "commands");
	}
	{
		initSystemWrapper(context);
		lua_setfield(context, LUA_GLOBALSINDEX, "system");
	}

	context.function("setTimeout", l_setTimeout);

	context.functions()
		("game_local_player", l_game_localPlayer)
		("game_local_player_name", l_game_localPlayerName)
		("console_register_control", l_console_register_control)
		("game_get_closest_worm", l_game_getClosestWorm)
		("map_is_blocked", l_map_isBlocked)
		("map_is_particle_pass", l_map_isParticlePass)
	// NOTE: Too much access (like executing any random command or setting any random setting)
	// is not what Lua mod code is supposed to be able to do.
	// It might be that we use Lua also for other stuff later but that should be a separate
	// Lua context then.
	// Here we have some restricted subset.
		("setLevelSucceeded", l_olx_setLevelSucceeded)
		("message", l_olx_message)
	;
	
	// CWormHumanInputHandler method and metatable
	
	CLASS_(CWormInputHandler,
		("kills", l_player_kills)
		("deaths", l_player_deaths)
		("name", l_player_name)
		("team", l_player_team)
		("say", l_player_say)
		("data", l_player_data)
		("stats", l_player_stats)
		("worm", l_player_worm)
		("select_weapons", l_player_selectWeapons)
		("weapon_types", l_player_weaponTypes)
		("is_local", l_player_isLocal)
	)
	
	ENUM(Player,  
		("Left", CWormHumanInputHandler::LEFT)
		("Right", CWormHumanInputHandler::RIGHT)
		("Up", CWormHumanInputHandler::UP)
		("Down", CWormHumanInputHandler::DOWN)
		("Fire", CWormHumanInputHandler::FIRE)
		("Jump", CWormHumanInputHandler::JUMP)
		("Change", CWormHumanInputHandler::CHANGE)
		("Ninjarope", CWormHumanInputHandler::NINJAROPE)
	)
	
/*
	lua_newtable(context); 
	lua_pushstring(context, "__index");
	
	lua_newtable(context);
	
	context.tableFunction("kills", l_player_kills);
	context.tableFunction("deaths", l_player_deaths);
	context.tableFunction("name", l_player_name);
	context.tableFunction("say", l_player_say);
	
	lua_rawset(context, -3);
	playerMetaTable = context.createReference();*/
}

}
