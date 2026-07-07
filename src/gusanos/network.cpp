#include "network.h"

#include "server.h"
#include "client.h"
#include "gusgame.h"
#include "LuaCallbacks.h"
#include "gconsole.h"
#include "net_worm.h"
#include "game/WormInputHandler.h"
#include "particle.h"
#include "util/macros.h"
#include "util/log.h"
#include "util/text.h"
#include "lua/bindings-network.h"
#include "game/Game.h"

#include <string>
#include <iostream>
#include <memory>
#include <list>
#include <set>
#include <utility>
#include "netstream.h"
#include <boost/assign/list_inserter.hpp>
using namespace boost::assign;

using namespace std;

int const Network::protocolVersion = 2;

LuaReference LuaEventDef::metaTable;

namespace
{
	Network::State state = Network::StateDisconnected;
	int stateTimeOut = 0;

	void setLuaState(Network::State s)
	{
		LUACALLBACK(networkStateChange).call()(s)();
	}

	void setState(Network::State s)
	{
		state = s;
	}

#define SET_STATE(s_) DLOG("Network state: " #s_); setState(State##s_)

	struct LuaEventList
	{
		typedef std::map<std::string, LuaEventDef*> MapT;
		MapT stringToEvent;
		std::vector<LuaEventDef*> events;

		LuaEventDef* add(std::string const& name, LuaEventDef* event)
		{
			size_t idx = events.size() + 1;

			event->idx = idx;
			std::pair<MapT::iterator, bool> r = stringToEvent.insert(std::make_pair(name, event));
			if(!r.second) {
				delete event;
				event = r.first->second;
			}
			events.push_back(event);
			return event;
		}

		LuaEventDef* assign(std::string const& name, LuaEventDef* event)
		{
			std::pair<MapT::iterator, bool> r = stringToEvent.insert(std::make_pair(name, event));
			if(!r.second) {
				r.first->second->callb = event->callb;
				delete event;
				event = r.first->second;
			}
			return event;
		}

		void index(std::string const& name)
		{
			size_t idx = events.size() + 1;
			MapT::iterator i = stringToEvent.find(name);
			if(i != stringToEvent.end()) {
				i->second->idx = idx;
				events.push_back(i->second);
			} else
				events.push_back(0);
		}

		LuaEventDef* fromIndex(size_t idx)
		{
			if(idx > 0 && idx <= events.size())
				return events[idx - 1];
			return 0;
		}

		void clear()
		{
			stringToEvent.clear();
			events.clear();
		}

		void encode(BitStream* data)
		{
			data->addInt((uint32_t)events.size(), 8);
			foreach(i, events) {
				DLOG("Encoding lua event: " << (*i)->name);
				data->addString((*i)->name.c_str());
			}
		}
	};

	std::string serverName;
	std::string serverDesc;
	std::set<Net_U32> bannedIPs;

	int reconnectTimer = 0;
	int connCount = 0;

	Net_Control* m_control = 0;
	LuaEventList luaEvents[Network::LuaEventGroup::Max];

	void registerClasses() // Factorization of class registering in client and server
	{
		CWorm::classID = m_control->Net_registerClass("worm",Net_CLASSFLAG_ANNOUNCEDATA);
		CWormInputHandler::classID = m_control->Net_registerClass("player",Net_CLASSFLAG_ANNOUNCEDATA);
		GusGame::classID = m_control->Net_registerClass("gusGame",0);
		Particle::classID = m_control->Net_registerClass("particle",Net_CLASSFLAG_ANNOUNCEDATA);
	}
}

Network network;

void LuaEventDef::call(BitStream* s)
{
	BitStream* n = s->Duplicate();
	(luaIngame.call(callb), luaReference, luaIngame.fullReference(*n, LuaBindings::BitStreamMetaTable))();
}

void LuaEventDef::call(LuaReferenceLazy obj, BitStream* s)
{
	BitStream* n = s->Duplicate();
	(luaIngame.call(callb), luaReference, obj, luaIngame.fullReference(*n, LuaBindings::BitStreamMetaTable))();
}

LuaEventDef::~LuaEventDef()
{
	luaReference.destroy();
	callb.destroy();
}

Network::Network()
{
}

Network::~Network()
{
}

void Network::log(char const* msg)
{
	cerr << "Gusanos Network: " << msg << endl;
}

void Network::init()
{
}

void Network::shutDown()
{
	delete m_control;
	m_control = 0;
}

void Network::registerInConsole()
{
	console.registerVariables()
	("NET_CHECK_CRC", &network.checkCRC, 1)
	;

}

Net_ConnID Network::getServerID()
{
	return NetConnID_server();
}

void Network::update()
{
	if ( m_control ) {
		m_control->Net_processOutput();
		m_control->Net_processInput();
	}

	switch(state) {
			case StateConnecting:
			case StateHosting:
			break;

			case StateDisconnected:
			case StateIdle:
			break;

			case StateDisconnecting: {
				if(connCount == 0 || stateTimeOut <= 0) {
					if(connCount != 0)
						WLOG(connCount << " connection(s) might not have disconnected properly.");
					setLuaState(StateDisconnected);
					SET_STATE(Disconnected);

					if(m_control) {
						m_control->Shutdown();
						delete m_control;
						m_control = 0;
					}

					connCount = 0;

					gusGame.removeNode();
				} else
					--stateTimeOut;
			}
			break;
	}

	if( reconnectTimer > 0 ) {
		//disconnect();
		if(--reconnectTimer == 0) {
			DLOG("Reconnecting Gusanos");
			olxConnect();
		}
	}
}

void Network::olxShutdown() {
	setLuaState(StateDisconnected);
	SET_STATE(Disconnected);
	
	if(m_control) {
		m_control->Shutdown();
		delete m_control;
		m_control = 0;
	}
	
	connCount = 0;
	
	gusGame.removeNode();
	
	clear();
}

void Network::olxHost()
{
	assert(game.isServer());

	if(state != StateDisconnected) { // We assume that we're disconnected
		errors << "Network::olxHost: state is not disconnected" << endl;
	}
	
	m_control = new Server();
	registerClasses();
	gusGame.assignNetworkRole(); // Gives the gusGame class node authority role
	setLuaState(StateHosting);
	SET_STATE(Idle);
}

void Network::olxConnect()
{
	assert(game.isClient());
	// moved frmo Network::update() message queue handler to here:
	
	if(!isDisconnected()) {
		disconnect();
		
		// This would leave the Connect msg in the msg queue to do the connect as soon as we are disconnected
		//mq_delay(); // Wait until network is disconnected
		
		errors << "Network::olxConnect: we were not disconnected" << endl;
	}
	
	m_control = new Client();
	registerClasses();
	//m_client = true; // We wait with setting this until we've connected
	setLuaState(StateConnecting);
	SET_STATE(Idle);
	
	m_control->Net_ConnectToServer();
	
	gusGame.assignNetworkRole();
}

void Network::disconnect()
{
	if(state == StateIdle && m_control) {
		setLuaState(StateDisconnecting);
		SET_STATE(Disconnecting);
		stateTimeOut = 1000;
		LOG("Disconnecting...");
	}
}

void Network::clear()
{
	// The lua event tables contain pointers to objects allocated by lua
	for(int t = LuaEventGroup::GusGame; t < LuaEventGroup::Max; ++t) {
		luaEvents[t].clear();
	}
}

void Network::olxReconnect(int delay)
{
	reconnectTimer = delay;
}

bool Network::isHost()
{
	return game.isServer();
}

bool Network::isClient()
{
	return game.isClient();
}

Net_Control* Network::getNetControl()
{
	return m_control;
}

LuaEventDef* Network::addLuaEvent(LuaEventGroup::type type, const std::string& name, LuaEventDef* event)
{
	if(game.isServer())
		return luaEvents[type].add(name, event);
	else if(game.isClient())
		return luaEvents[type].assign(name, event);

	return event;
}

void Network::indexLuaEvent(LuaEventGroup::type type, const std::string& name)
{
	if(game.isClient())
		luaEvents[type].index(name);
}

void Network::encodeLuaEvents(BitStream* data)
{
	for(int t = LuaEventGroup::GusGame; t < LuaEventGroup::Max; ++t) {
		luaEvents[t].encode(data);
	}
}

LuaEventDef* Network::indexToLuaEvent(LuaEventGroup::type type, int idx)
{
	return luaEvents[type].fromIndex(idx);
}

void Network::incConnCount()
{
	++connCount;
}

void Network::decConnCount()
{
	--connCount;
}

bool Network::isDisconnected()
{
	return state == StateDisconnected;
}

bool Network::isDisconnecting()
{
	return state == StateDisconnecting;
}

void Network::olxParse(Net_ConnID src, CBytestream& bs) {
	if(m_control)
		m_control->olxParse(src, bs);
	else {
		errors << "GusNetwork::olxParse: net control not initialised" << endl;
		bs.SkipAll();
	}
}

void Network::olxParseUpdate(Net_ConnID src, CBytestream& bs) {
	if(m_control)
		m_control->olxParseUpdate(src, bs);
	else {
		errors << "GusNetwork::olxParseUpdate: net control not initialised" << endl;
		bs.SkipAll();
	}
}

void Network::olxSend(bool sendPendingOnly) {
	// we can ignore Gusanos network in local play
	if(game.isLocalGame()) return;
	
	// dont send if in lobby
	if(game.state < Game::S_Preparing) return;
	
	if(m_control)
		m_control->olxSend(sendPendingOnly);
	else
		errors << "GusNetwork::olxSend: net control not initialised" << endl;
}


void Network::sendEncodedLuaEvents(Net_ConnID cid) {
	if(!m_control) {
		errors << "Network::sendEncodedLuaEvents: control unset" << endl;
		return;
	}
	
	// on the client side, this is handled in Client::Net_cbDataReceived
	std::unique_ptr<BitStream> data(new BitStream);
	Encoding::encode(*data, Network::ClientEvents::LuaEvents, Network::ClientEvents::Max);
	network.encodeLuaEvents(data.get());
	m_control->Net_sendData(cid, data.release(), eNet_ReliableOrdered);		
}



