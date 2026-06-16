#include "bindings.h"
#include "FindFile.h"
#include "Debug.h"

#ifndef DEDICATED_ONLY
#include "../gui/lua/bindings-gui.h"
#endif
#include "bindings-math.h"
#include "bindings-network.h"
#include "bindings-objects.h"
#include "bindings-resources.h"
#include "bindings-gfx.h"
#include "bindings-game.h"

#include "gusanos/LuaCallbacks.h"
#include "../luaapi/types.h"
#include "../luaapi/macros.h"

#include "../gusgame.h"
#include "../network.h"
#include "util/log.h"

#include "../gconsole.h"
#ifndef DEDICATED_ONLY
#include "../menu.h"
#include "../blitters/context.h"
#include "CViewport.h"
#endif
#include <cmath>
#include <string>
#include <list>
#include <iostream>
#include <fstream>
#include <vector>
#include "gusanos/allegro.h"
using std::cerr;
using std::endl;
#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
using boost::lexical_cast;
using namespace boost::placeholders;

namespace LuaBindings
{


int print(lua_State* L)
{
	notes << "LUA: ";
	
	int c = lua_gettop(L);
	for(int i = 1; i <= c; ++i)
	{
		if(const char* s = lua_tostring(L, i))
		{
			notes << s;
		}
	}
	notes << endl;
	
	return 0;
}

/*! bindings.afterUpdate()

	This is called after every logic cycle is complete.
*/

/*! bindings.afterRender()

	This is called after a rendering cycle is complete
*/

/*! bindings.wormRender(x, y, worm, viewport, ownerPlayer)

	This is called for every worm and viewport combination when it's time to render
	the worm HUD.
	
	(x, y) is the position of the worm in viewport coordinates.
	
	//worm// is the Worm object for which HUD should be rendered and //viewport// is
	the CViewport object it should be rendered to. Use the bitmap() method of CViewport
	to retrieve the relevant bitmap to draw on.
	
	//ownerPlayer// is the CWormHumanInputHandler object that owns //viewport//.
*/

/*! bindings.viewportRender(viewport, worm)

	This is called for every viewport when it's time to render the viewport HUD.
	
	//viewport// is the CViewport object it should be rendered to and //worm// is the
	Worm object of the CWormHumanInputHandler object that owns //viewport//.
*/

/*! bindings.wormDeath(worm)

	This is called when a worm dies. //worm// is the Worm object that died.
*/

/*! bindings.wormRemoved(worm)

	This is called when a worm is removed from the game. //worm// is the Worm object that will be removed.
*/

/*! bindings.playerUpdate(player)

	This is called in every logic cycle for every player. //player// is the relevant CWormHumanInputHandler object.
*/

/*! bindings.playerInit(player)

	This is called when a new player is added to the game. //player// is the CWormHumanInputHandler object that was added.
*/

/*! bindings.playerNetworkInit(player, connID)

	This is called when a player is replicated to a new client. //player// is the player replicated.
	//connID// is the connection ID of the new client.
	
	The connection ID can be passed to the send() method of a NetworkPlayerEvent to send events only
	to the player on the new client.
*/

//! version 0.9c

/*! bindings.playerRemoved(player)
	
	This is called when a player is removed from the game. //player// is the CWormHumanInputHandler object that will be removed.
*/

/*! bindings.gameNetworkInit(connID)
	
	This is called when a new client joins the game. //connID// is the connection ID of the new client.
	
	The connection ID can be passed to the send() method of a NetworkGameEvent to send events only
	to the new client.
*/

/*! bindings.gameEnded(reason)

	This is called when the game ended and no new game is pending.
	
	//reason// can be one of the following:
	* EndReason.ServerQuit : The server disconnected.
	* EndReason.Kicked : You were kicked from the server.
	* EndReason.IncompatibleProtocol : You are running a protocol incompatible with the server's.
	* EndReason.IncompatibleData : Your data does not match the server's.
	
*/

//! version any

int l_bind(lua_State* L)
{
	LuaContext context(L);
	char const* s = lua_tostring(L, 2);
	if(!s)
		return 0;
		
	lua_pushvalue(L, 3);
	LuaReference ref;
	ref.create(context);
	luaCallbacks.bind(L, s, ref);

	return 0;
}

int l_console_set(lua_State* L)
{
	char const* s = lua_tostring(L, 2);
	if(!s)
		return 0;
	
	char const* v = lua_tostring(L, 3);
	if(!v)
		return 0;
	
	std::list<std::string> args;
	args.push_back(v);
	
	console.invoke(s, args, false);
	return 0;
}

int l_console_get(lua_State* L)
{
	char const* s = lua_tostring(L, 2);
	if(!s)
		return 0;
	
	std::list<std::string> args;

	lua_pushstring(L, console.invoke(s, args, false).c_str());
	return 1;
}


LUA_CALLBACK(luaConsoleCommand(LuaContext context, LuaReference ref, std::list<std::string> const& args))
	for(std::list<std::string>::const_iterator i = args.begin();
		i != args.end();
		++i)
	{
		lua_pushstring(context, i->c_str());
		++params;
	}
END_LUA_CALLBACK()


/*! console_register_command(name, function)

	Registers the function //function// as a command in the Gusanos console.
	When it is called, it will be passed each console parameter as a seperate parameter to the function.
	The command will be removed automatically when a new map is loaded.
*/
int l_console_register_command(lua_State* L)
{
	LuaContext context(L);
	char const* name = lua_tostring(L, 1);
	if(!name) return 0;
	lua_pushvalue(L, 2);
	LuaReference ref;
	ref.create(context);
	
	console.registerCommands()
			(name, boost::bind(LuaBindings::luaConsoleCommand, context, ref, _1), true);

	return 0;
}

int l_dump(lua_State* L)
{
	LuaContext context(L);
	
	lua_pushvalue(context, 2);
	lua_pushvalue(context, 3);
	lua_rawset(context, 1); // Set table entry
	
	char const* s = lua_tostring(context, 2);
	if(!s)
		return 0;
	
	// Allow only [A-Za-z0-9\-]
	for(char const* p = s; *p; ++p)
	{
		if(!isalnum(*p) && *p != '-' && *p != '_')
		{
			LUA_ELOG("Persistence name '" << s << "' invalid. Data not stored.");
			return 0;
		}
	}

	try
	{	
		std::string dumpPath(std::string("persistance") + "/" + std::string(s) + ".lpr");
		std::ofstream f(GetWriteFullFileName("gusanos/" + dumpPath, true).c_str(), std::ios::binary);
		
		if(!f.is_open())
			return 0;
			
		context.serializeT(f, 3);
	}
	catch(std::exception& e)
	{
		cerr << "dump() failed with error: " << e.what() << endl;
		return 0;
	}

	return 0;
}

int l_undump(lua_State* L)
{
	LuaContext context(L);
	
	char const* s = lua_tostring(context, 2);
	if(!s)
		return 0;
	
	// Allow only [A-Za-z0-9\-_]
	for(char const* p = s; *p; ++p)
	{
		if(!isalnum(*p) && *p != '-' && *p != '_')
		{
			LUA_ELOG("Persistence name '" << s << "' invalid. Data not retrieved.");
			return 0;
		}
	}
	
	try
	{
		std::string dumpPath(std::string("persistance") + "/" + std::string(s) + ".lpr");
	
		std::ifstream f;
		OpenGameFileR(f, dumpPath, std::ios::binary);
		
		if(!f.is_open())
			return 0;
			
		//context.deserialize(f);
		
		int r = context.evalExpression("<persistent value>", f);
		if(r != 1)
			return 0;
		context.pushvalue(2); // Key
		context.pushvalue(-2); // Value
		lua_rawset(context, 1); // Set table entry
	}
	catch(std::exception& e)
	{
		cerr << "undump() failed with error: " << e.what() << endl;
		return 0;
	}

	return 1;
}


/*
std::string runLua(LuaReference ref, std::list<std::string> const& args)
{
	AssertStack as(lua);
	
	lua.push(LuaContext::errorReport);
	lua.pushReference(ref);
	if(lua_isnil(lua, -1))
	{
		lua.pop(2);
		return "";
	}
	int params = 0;
	
	for(std::list<std::string>::const_iterator i = args.begin();
		i != args.end();
		++i)
	{
		lua_pushstring(lua, i->c_str());
		++params;
	}

	int r = lua.call(params, 1, -params-2);
	if(r < 0)
	{
		lua_pushnil(lua);
		lua.assignReference(ref);
		lua.pop(1);
		return "";
	}
	lua_remove(lua, -1-1);
	
	if(char const* s = lua_tostring(lua, -1))
	{
		std::string ret(s);
		lua.pop(1);
		return ret;
	}

	lua.pop(1);
	
	return "";
}*/

	static void print_Lua_debug(lua_State* context, const std::string& msg, bool withbacktrace) {
		lua_Debug info;
		char const* lastname;
		for(int i = 1; lua_getstack(context, i, &info); ++i)
		{
			lua_getinfo (context, "Snl", &info);
			
			char const* name = info.name ? info.name : "N/A";
			
			if(i == 1)
				cerr << info.source << ":" << info.currentline << ": " << msg << endl;
			else
				cerr << info.source << ":" << info.currentline << ": " << lastname << " called from here" << endl;
			
			lastname = name;
			if(!withbacktrace) break;
		}
	}
	
	static std::string getLuaFuncName(lua_State* context) {
		lua_Debug info;
		if(lua_getstack(context, 0, &info)) {
			lua_getinfo(context, "Snl", &info);
			if(info.name) return info.name;
			return "UNKNOWN-LUA-FUNC";
		}
		return "NO-LUA-STACK";
	}
	
#define IMPL_OLD_LUAFUNC(name) \
	int name(lua_State* L) \
	{ \
		static bool printedWarning = false; \
		if(!printedWarning) { \
			std::string my_func_name = getLuaFuncName(L); \
			print_Lua_debug(L, my_func_name + " not implemented (further warnings omitted)", false); \
			printedWarning = true; \
		} \
		return 0; \
	}

	IMPL_OLD_LUAFUNC(l_console_key_for_action);
	IMPL_OLD_LUAFUNC(l_console_bind);
	IMPL_OLD_LUAFUNC(l_console_action_for_key);
	IMPL_OLD_LUAFUNC(l_key_name);
	
void init(LuaContext& context)
{	
	initMath(context);
#ifndef DEDICATED_ONLY
	initGUI(OmfgGUI::menu, context);
#endif
	initNetwork(context);
	initObjects(context);
	initResources(context);
	initGfx(context);
	initGame(context);

	context.functions()
		("print", print)
		("console_register_command", l_console_register_command)
		("console_key_for_action", l_console_key_for_action)
		("console_bind", l_console_bind)
		("console_action_for_key", l_console_action_for_key)
		("key_name", l_key_name)
		//("dump", l_dump)
		//("undump", l_undump)
	;

	// Bindings table and metatable
	{
		lua_pushstring(context, "bindings");
		lua_newtable(context); // Bindings table
		{
			lua_newtable(context); // Bindings metatable
			{
				lua_pushstring(context, "__newindex");
				lua_pushcfunction(context, l_bind);
				lua_rawset(context, -3);
			}
			lua_setmetatable(context, -2);
		}
		lua_rawset(context, LUA_GLOBALSINDEX);
	}

	// Console table and metatable
	{
		lua_pushstring(context, "console");
		lua_newtable(context); // Console table
		{
			lua_newtable(context); // Console metatable
			{
				lua_pushstring(context, "__newindex");
				lua_pushcfunction(context, l_console_set);
				lua_rawset(context, -3);
			}
			{
				lua_pushstring(context, "__index");
				lua_pushcfunction(context, l_console_get);
				lua_rawset(context, -3);
			}
			lua_setmetatable(context, -2);
		}
		lua_rawset(context, LUA_GLOBALSINDEX);
	}

	{
		lua_pushstring(context, "DEDICATED_ONLY");
#ifdef DEDICATED_ONLY
		lua_pushboolean(context, 1);
#else
		lua_pushboolean(context, 0);
#endif
		lua_rawset(context, LUA_GLOBALSINDEX);
	}

	SHADOW_TABLE("persistence", l_undump, l_dump);
}

}

