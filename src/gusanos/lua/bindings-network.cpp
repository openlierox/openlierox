#include "bindings-network.h"

#include "../luaapi/types.h"
#include "../luaapi/macros.h"
#include "../luaapi/classes.h"

#include "../network.h"
#include "../gusgame.h"
#include "game/WormInputHandler.h"
#include "game/CWorm.h"
#include "../particle.h"
#include "../encoding.h"
#include "util/log.h"
#include "game/Game.h"

#include <iostream>
#include <memory>
#include <utility>
#include <list>
#include <cstring>
using std::cerr;
using std::endl;
#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
using boost::lexical_cast;

namespace LuaBindings
{
	
LuaReference SocketMetaTable;
LuaReference BitStreamMetaTable;
LuaReference LuaGameEventMetaTable, LuaPlayerEventMetaTable, LuaWormEventMetaTable, LuaParticleEventMetaTable;


/*! Bitstream:dump(value)
	
	Adds a lua object to the bitstream.
	
	WARNING: This function is not very space efficient and can only encode tables
	, strings, numbers, booleans and nil (or tables with keys and values of those types)
*/
METHODC(BitStream, bitStream_dump,  {
	context.serialize(*p, 2);
	context.pushvalue(1);
	return 1;
})

/*! Bitstream:undump()
	
	Extracts a lua object added to the bitstream using the //dump// method.
*/
METHODC(BitStream, bitStream_undump,  {
	context.deserialize(*p);
	return 1;
})

/*! Bitstream:encode_elias_gamma(value)
	
	Adds an integer above or equal to 1 encoded using the elias gamma universal encoding.
*/
METHODC(BitStream, bitStream_encodeEliasGamma,  {
	int v = lua_tointeger(context, 2);
	if(v < 1)
	{
		WLOG("Number " << v << " can't be encoded with elias gamma. Encoded as 1.");
		v = 1;
	}
	Encoding::encodeEliasGamma(*p, static_cast<unsigned int>(v));
	context.pushvalue(1);
	return 1;
})

/*! Bitstream:decode_elias_gamma()
	
	Extracts an integer above or equal to 1 encoded using the elias gamma universal encoding.
*/
METHODC(BitStream, bitStream_decodeEliasGamma,  {
	context.push(Encoding::decodeEliasGamma(*p));
	return 1;
})

/*! Bitstream:add_bool()
	
	Extracts a boolean value from this bitstream (Returns either true or false).
*/
METHODC(BitStream, bitStream_addBool,  {
	p->addBool(lua_toboolean(context, 2) != 0);
	context.pushvalue(1);
	return 1;
})

/*! Bitstream:get_bool()
	
	Extracts a boolean value from this bitstream (Returns either true or false).
*/
METHODC(BitStream, bitStream_getBool,  {
	context.push(p->getBool());
	return 1;
})

/*! Bitstream:add_int(value[, bits])
	
	Adds an integer to this bitstream encoded in //bits// bits.
	
	If bits is left out, 32 bits is assumed.
*/
METHODC(BitStream, bitStream_addInt,  {
	int bits = 32;
	if(lua_gettop(context) >= 3)
		bits = lua_tointeger(context, 3);
	p->addInt(lua_tointeger(context, 2), bits);
	context.pushvalue(1);
	return 1;
})

/*! Bitstream:get_int([bits])
	
	Extracts an integer from this bitstream encoded in //bits// bits.
	
	If bits is left out, 32 bits is assumed.
*/
METHODC(BitStream, bitStream_getInt,  {
	int bits = 32;
	if(lua_gettop(context) >= 2)
		bits = lua_tointeger(context, 2);
	context.push(static_cast<int>(p->getInt(bits)));
	return 1;
})

/*! Bitstream:add_string(str)
	
	Adds the string //str// to this bitstream.
*/
METHODC(BitStream, bitStream_addString,  {
	p->addString(lua_tostring(context, 2));
	context.pushvalue(1);
	return 1;
})

/*! Bitstream:get_string()
	
	Extracts a string from this bitstream.
*/
METHODC(BitStream, bitStream_getString,  {
	static std::string staticstr = p->getString();
	context.push(staticstr.c_str());
	return 1;
})

METHODC(BitStream, bitStream_destroy,  {
	delete p;
	return 0;
})

/*! new_bitstream()
	
	Returns a new Bitstream object.
*/
int l_new_bitstream(lua_State* L)
{
	LuaContext context(L);
	BitStream* p = new BitStream;
	context.pushFullReference(*p, BitStreamMetaTable);
	return 1;
}

#define LUA_EVENT_SEND_METHOD(type_, params_, decl_, cases_, body_) \
LMETHOD(LuaEventDef, luaEvent_##type_##_send, {\
	if(p->idx > 0)	{ \
		BitStream* userdata = 0; \
		eNet_SendMode mode = eNet_ReliableOrdered; \
		Net_ConnID connID = 0; \
		Net_U8 rules = Net_REPRULE_AUTH_2_ALL | Net_REPRULE_OWNER_2_AUTH; \
		decl_ \
		switch(lua_gettop(context)) \
		{ \
			default: if(lua_gettop(context) < params_+3) break; \
			case params_+5: rules = lua_tointeger(context, params_+5); \
			case params_+4: mode = (eNet_SendMode)lua_tointeger(context, params_+4); \
			case params_+3: connID = (Net_ConnID)lua_tointeger(context, params_+3); \
			case params_+2: userdata = ASSERT_OBJECT(BitStream, params_+2); \
			cases_ \
		} \
		body_ \
	} \
	return 0; \
})

/*! NetworkGameEvent:send([data[, connection[, mode[, rules]]]])

	Sends a game event to one or more computers.
	
	//data// is a bitstream to send with the event.
	
	//connection// is a connection ID to send the event on. If it's 0, the //rules// parameter decides who to send to.
	
	//mode// is one of these values (default is SendMode.ReliableOrdered):
	* SendMode.ReliableUnordered : The events always arrive, but may come in a different order than sent.
	* SendMode.ReliableOrdered : The events always arrive and in the order sent.
	* SendMode.Unreliable : The events may or may not arrive and in any order.
	
	//rules// decides what computers to send the event to if //connection// is 0 or left out.
	It can be any (or sometimes a sum) of:
	* RepRule.Auth2All : GameEvent is sent from the server to clients.
	* RepRule.Auth2Owner : GameEvent is sent from the server to clients owning the node.
	* RepRule.Auth2Proxy : GameEvent is sent from the server to clients not owning the node.
	* RepRule.None : GameEvent is not sent.
	* RepRule.Owner2Auth : GameEvent is sent from clients owning the node to the server.
	
	A valid combination is (RepRule.Owner2Auth + RepRule.Auth2All) which sends the event
	from owners to the server or from the server to all clients (depending on if we're the server or not).
*/
LUA_EVENT_SEND_METHOD(game, 0, /* no declarations */, /* no cases */,
	gusGame.sendLuaEvent(p, mode, rules, userdata, connID);
)

/*! NetworkPlayerEvent:send(player, [data[, connection[, mode[, rules]]]])

	Sends a player event to one or more computers.
	
	//player// is the CWormHumanInputHandler object to send the event on.
	
	See NetworkGameEvent:send for documentation of the other parameters.
*/
LUA_EVENT_SEND_METHOD(player, 1,
	CWormInputHandler* player = 0;
,
	case 2: player = ASSERT_OBJECT(CWormInputHandler, 2);
,
	if(player)
		player->sendLuaEvent(p, mode, rules, userdata, connID);
)

/*! NetworkWormEvent:send(worm, [data[, connection[, mode[, rules]]]])

	Sends a worm event to one or more computers.
	
	//worm// is the Worm object to send the event on.
	
	See NetworkGameEvent:send for documentation of the other parameters.
*/
LUA_EVENT_SEND_METHOD(worm, 1,
	CWorm* worm = 0;
,
	case 2: worm = ASSERT_OBJECT(CWorm, 2);
,
	if(worm)
		worm->sendLuaEvent(p, mode, rules, userdata, connID);
)

//! version 0.9c

/*! NetworkParticleEvent:send(particle, [data[, connection[, mode[, rules]]]])

	Sends a particle event to one or more computers.
	
	//particle// is the Particle object to send the event on.
	
	See NetworkGameEvent:send for documentation of the other parameters.
*/
LUA_EVENT_SEND_METHOD(particle, 1,
	Particle* particle = 0;
,
	case 2: particle = ASSERT_OBJECT(Particle, 2);
,
	if(particle)
		particle->sendLuaEvent(p, mode, rules, userdata, connID);
)

//! version any

/*! network_game_event(name, handler)
	
	Creates and returns a NetworkGameEvent object.
	//name// is any string that is unique to this NetworkGameEvent.
	//handler// is a function that is called when this event is recieved on this client/server.
	It has the the form:
	<code>
	function(event, data)
	</code>
	Where //event// is the NetworkGameEvent returned by //network_game_event// and //data//
	is a bitstream with data sent with the event.
*/
int l_network_game_event(lua_State* L)
{
	LuaContext context(L);
	char const* name = lua_tostring(context, 1);
	if(!name) return 0;
	lua_pushvalue(context, 2);
	LuaReference ref;
	ref.create(context);
	LuaEventDef* event = lua_new_m_keep(LuaEventDef, (name, ref), context, LuaGameEventMetaTable);
	event = network.addLuaEvent(Network::LuaEventGroup::GusGame, name, event);

	return 1;
}

/*! network_player_event(name, handler)
	
	Creates and returns a NetworkPlayerEvent object.
	//name// is any string that is unique to this NetworkPlayerEvent.
	//handler// is a function that is called when this event is recieved on this client/server.
	It has the the form:
	<code>
	function(event, player, data)
	</code>
	Where //event// is the NetworkPlayerEvent returned by //network_player_event//, //player//
	is the CWormHumanInputHandler object the event was sent on, and //data// is a bitstream with data sent
	with the event.
*/
int l_network_player_event(lua_State* L)
{
	LuaContext context(L);
	char const* name = lua_tostring(context, 1);
	if(!name) return 0;
	lua_pushvalue(context, 2);
	LuaReference ref;
	ref.create(context);
	LuaEventDef* event = lua_new_m_keep(LuaEventDef, (name, ref), context, LuaPlayerEventMetaTable);
	event = network.addLuaEvent(Network::LuaEventGroup::CWormHumanInputHandler, name, event);

	return 1;
}

/*! network_worm_event(name, handler)
	
	Creates and returns a NetworkWormEvent object.
	//name// is any string that is unique to this NetworkWormEvent.
	//handler// is a function that is called when this event is recieved on this client/server.
	It has the the form:
	<code>
	function(event, worm, data)
	</code>
	Where //event// is the NetworkWormEvent returned by //network_worm_event//, //worm//
	is the Worm object the event was sent on, and //data// is a bitstream with data sent
	with the event.
*/
int l_network_worm_event(lua_State* L)
{
	LuaContext context(L);
	char const* name = lua_tostring(context, 1);
	if(!name) return 0;
	lua_pushvalue(context, 2);
	LuaReference ref;
	ref.create(context);
	LuaEventDef* event = lua_new_m_keep(LuaEventDef, (name, ref), context, LuaWormEventMetaTable);
	event = network.addLuaEvent(Network::LuaEventGroup::Worm, name, event);

	return 1;
}

//! version 0.9c

/*! network_particle_event(name, handler)
	
	Creates and returns a NetworkParticleEvent object.
	//name// is any string that is unique to this NetworkParticleEvent.
	//handler// is a function that is called when this event is recieved on this client/server.
	It has the the form:
	<code>
	function(event, particle, data)
	</code>
	Where //event// is the NetworkParticleEvent returned by //network_particle_event//, //particle//
	is the Particle object the event was sent on, and //data// is a bitstream with data sent
	with the event.
*/
int l_network_particle_event(lua_State* L)
{
	LuaContext context(L);
	char const* name = lua_tostring(context, 1);
	if(!name) return 0;
	lua_pushvalue(context, 2);
	LuaReference ref;
	ref.create(context);
	LuaEventDef* event = lua_new_m_keep(LuaEventDef, (name, ref), context, LuaParticleEventMetaTable);
	event = network.addLuaEvent(Network::LuaEventGroup::Particle, name, event);

	return 1;
}

//! version any

void initNetwork(LuaContext& context)
{
	context.functions()
		("network_game_event", l_network_game_event)
		("network_player_event", l_network_player_event)
		("network_worm_event", l_network_worm_event)
		("network_particle_event", l_network_particle_event)
		("new_bitstream", l_new_bitstream)
	;
		
	CLASS(LuaGameEvent,  
		("send", l_luaEvent_game_send)
	)
	
	CLASS(LuaPlayerEvent,  
		("send", l_luaEvent_player_send)
	)
	
	CLASS(LuaWormEvent,  
		("send", l_luaEvent_worm_send)
	)
	
	CLASS(LuaParticleEvent,  
		("send", l_luaEvent_particle_send)
	)
	
	CLASSM(BitStream,  
		("__gc", l_bitStream_destroy)
	,
		("add_int", l_bitStream_addInt)
		("get_int", l_bitStream_getInt)
		("add_string", l_bitStream_addString)
		("get_string", l_bitStream_getString)
		("encode_elias_gamma", l_bitStream_encodeEliasGamma)
		("decode_elias_gamma", l_bitStream_decodeEliasGamma)
		("add_bool", l_bitStream_addBool)
		("get_bool", l_bitStream_getBool)
		("dump", l_bitStream_dump)
		("undump", l_bitStream_undump)
	)
	
	ENUM(SendMode,  
		("ReliableUnordered", eNet_ReliableUnordered)
		("ReliableOrdered", eNet_ReliableOrdered)
		("Unreliable", eNet_Unreliable)
	)
	
	ENUM(RepRule,  
		("Auth2All", Net_REPRULE_AUTH_2_ALL)
		("Auth2Owner", Net_REPRULE_AUTH_2_OWNER)
		("Auth2Proxy", Net_REPRULE_AUTH_2_PROXY)
		("None", Net_REPRULE_NONE)
		("Owner2Auth", Net_REPRULE_OWNER_2_AUTH)
	)
	
	
	ENUM(Network,  
		("Connecting", Network::StateConnecting)
		("Disconnecting", Network::StateDisconnecting)
		("Disconnected", Network::StateDisconnected)
		("Hosting", Network::StateHosting)
	)

	notes << "Lua: Registering as " << (game.isServer() ? "AUTH" : "non-AUTH") << endl;
	lua_pushboolean(context, game.isServer());
	lua_setfield(context, LUA_GLOBALSINDEX, "AUTH");

	lua_pushboolean(context, bDedicated);
	lua_setfield(context, LUA_GLOBALSINDEX, "DEDSERV");

}

}
