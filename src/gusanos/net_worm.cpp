#include "net_worm.h"
#include "game/CWorm.h"
#include "CVec.h"
#include "util/angle.h"
#include "util/log.h"
#include "gusgame.h"
#include "weapon.h"
#include "weapon_type.h"
#include "game/CGameObject.h"
#include "game/WormInputHandler.h"
#include "network.h"
#include "vector_replicator.h"
#include "posspd_replicator.h"
#include "encoding.h"
#include "gconsole.h"
#include "game/CMap.h"
#include "game/Game.h"
#include "CServer.h"
#include "CGameScript.h"
#include "netstream.h"
#include "AngleReplicator.h"

#include <math.h>
#include <vector>

using namespace std;

Net_ClassID CWorm::classID = INVALID_CLASS_ID;

const float CWorm::MAX_ERROR_RADIUS = 10.0f;

void CWorm::NetWorm_Init()
{
	if(m_node || m_interceptor) {
		errors << "CWorm::NetWorm_Init(" << getID() << "," << weOwnThis() << "): earlier node was not correctly uninitialised" << endl;
		return; // we return here because the chance is high that the old node is really in use and it was incorrect to call NetWorm_Init here again
	}
	
	timeSinceLastUpdate = 1;
	
	m_node = new Net_Node();
	if (!m_node)
	{
		allegro_message("ERROR: Unable to create worm node.");
	}
	
	m_node->beginReplicationSetup();
	
		//static Net_ReplicatorSetup posSetup( Net_REPFLAG_MOSTRECENT | Net_REPFLAG_INTERCEPT, Net_REPRULE_AUTH_2_PROXY | Net_REPRULE_OWNER_2_AUTH , Position, -1, 1000);
		
		//m_node->setInterceptID( static_cast<Net_InterceptID>(Position) );
		
		//m_node->addReplicator(new PosSpdReplicator( &posSetup, &pos(), &velocity() ), true);
		
		//static Net_ReplicatorSetup nrSetup( Net_REPFLAG_MOSTRECENT, Net_REPRULE_AUTH_2_PROXY | Net_REPRULE_OWNER_2_AUTH );
		
		//m_node->addReplicator(new VectorReplicator( &nrSetup, &cNinjaRope.getPosReference() ), true);
		
		//m_node->addReplicationFloat ((Net_Float*)&cNinjaRope.getLengthReference(), 16, Net_REPFLAG_MOSTRECENT, Net_REPRULE_AUTH_2_PROXY | Net_REPRULE_OWNER_2_AUTH);
		
		//static Net_ReplicatorSetup angleSetup( Net_REPFLAG_MOSTRECENT, Net_REPRULE_AUTH_2_PROXY | Net_REPRULE_OWNER_2_AUTH );
				
		//m_node->addReplicationFloat ((Net_Float*)&health, 16, Net_REPFLAG_MOSTRECENT, Net_REPRULE_AUTH_2_ALL);
		
		//m_node->addReplicator(new AngleReplicator( &angleSetup, &aimAngle), true );
				
	m_node->endReplicationSetup();

	m_interceptor = new NetWormInterceptor( this );
	m_node->setReplicationInterceptor(m_interceptor);

	BitStream* announceData = new BitStream();
	announceData->addInt(getID(), 8);
	m_node->setAnnounceData(announceData);
	
	/* // could this code have ever been executed?
	if(isAuthority && !bLocal) {
		CServerConnection* cl = getClient();
		if(cl)
			m_node->setOwner(NetConnID_conn(cl));
		else
			errors << "NetWorm_Init: connection of worm " << getName() << " not found" << endl;
	}*/
	
	if(weOwnThis())
	{
		m_node->setEventNotification(true, false); // Enables the eEvent_Init.
		if( !m_node->registerNodeDynamic(classID, network.getNetControl() ) )
			allegro_message("ERROR: Unable to register worm authority node.");
	}else
	{
		if( !m_node->registerRequestedNode( classID, network.getNetControl() ) )
		allegro_message("ERROR: Unable to register worm requested node.");
	}
	
	m_node->applyForNetLevel(1);
}

void CWorm::NetWorm_Shutdown()
{
	if(m_node) delete m_node;
	m_node = NULL;
	if(m_interceptor) delete m_interceptor;
	m_interceptor = NULL;
}

void CWorm::addEvent(BitStream* data, CWorm::NetEvents event)
{
#ifdef COMPACT_EVENTS
	//data->addInt(event, Encoding::bitsOf(NetWorm::EVENT_COUNT - 1));
	Encoding::encode(*data, event, CWorm::EVENT_COUNT);
#else
	data->addInt(static_cast<int>(event),8 );
#endif
}

void CWorm::NetWorm_think()
{
#ifndef DEDICATED_ONLY
	//renderPos += (pos - renderPos)*0.2;
	double fact = 1.0 / (1.0 + Vec(pos() - renderPos).length() / 4.0);
	renderPos = renderPos * (1.0f - (float)fact) + Vec(pos()) * (float)fact;
#endif

	++timeSinceLastUpdate;
	
	if ( !m_node )
		return;
	
	while ( m_node->checkEventWaiting() )
	{
		eNet_Event type;
		eNet_NodeRole    remote_role;
		Net_ConnID       conn_id;
		
		BitStream *data = m_node->getNextEvent(&type, &remote_role, &conn_id);
		switch(type)
		{
		case eNet_EventUser:
			if ( data )
			{
#ifdef COMPACT_EVENTS
				//NetEvents event = (NetEvents)data->getInt(Encoding::bitsOf(EVENT_COUNT - 1));
				NetEvents event = (NetEvents)Encoding::decode(*data, EVENT_COUNT);
#else
				NetEvents event = (NetEvents)data->getInt(8);
#endif
				switch ( event )
				{
					case Dig:
					{
						Vec digPos = game.gameMap()->vectorEncoding.decode<Vec>(*data);
						Angle digAngle = Angle((int)data->getInt(Angle::prec));
						CWorm::dig(digPos, digAngle);
					}
					break;
					case ChangeWeapon:
					{
						//size_t weapIndex = data->getInt(Encoding::bitsOf(gusGame.weaponList.size() - 1));
						size_t weapIndex = Encoding::decode(*data, m_weapons.size());
						changeWeaponTo( weapIndex );
					}
					break;
					case WeaponMessage:
					{
						//size_t weapIndex = data->getInt(Encoding::bitsOf(gusGame.weaponList.size() - 1));
						size_t weapIndex = Encoding::decode(*data, m_weapons.size());
						if ( weapIndex < m_weapons.size() && m_weapons[weapIndex] )
							m_weapons[weapIndex]->recieveMessage( data );
						else {
							if(weapIndex >= m_weapons.size())
								errors << "WeaponMessage: weapIndex " << weapIndex << " invalid, #slots: " << m_weapons.size() << endl;
							else if(!m_weapons[weapIndex])
								errors << "WeaponMessage: weapon slot " << weapIndex << " is unset" << endl;
						}
					}
					break;
					case SetWeapon:
					{
						size_t index = Encoding::decode(*data, gusGame.options.maxWeapons);
						if ( data->getBool() )
						{
							size_t weaponIndex = Encoding::decode(*data, gusGame.weaponList.size());
							if(weaponIndex < gusGame.weaponList.size())
								CWorm::base_setWeapon( index, gusGame.weaponList[weaponIndex] );
							else
								CWorm::base_setWeapon( index, 0 );
						}
						else
						{
							CWorm::base_setWeapon( index, 0 );
						}
					}
					break;
					case ClearWeapons:
					{
						DLOG("Clearing weapons");
						CWorm::base_clearWeapons();
					}
					break;
					case SYNC:
					{
						CWorm::base_clearWeapons();
						while ( data->getBool() )
						{
							size_t index = Encoding::decode(*data, m_weapons.size());
							size_t weapTypeIndex = Encoding::decode(*data, gusGame.weaponList.size());
							if(weapTypeIndex < gusGame.weaponList.size() && index < m_weapons.size())
							{
								luaDelete(m_weapons[index]); m_weapons[index] = 0; 
								m_weapons[index] = new Weapon(gusGame.weaponList[weapTypeIndex], this);
							}
							else {
								errors << "NetWorm SYNC: #weapons: " << gusGame.weaponList.size() << ", weapIndex: " << weapTypeIndex << ", #weaponSlots: " << m_weapons.size() << ", index: " << index << "; sth is messed up" << endl;
							}
						}
					}
					break;
					
					case LuaEvent:
					{
						int index = data->getInt(8);
						if(LuaEventDef* event = network.indexToLuaEvent(Network::LuaEventGroup::Worm, index))
						{
							event->call(getLuaReference(), data);
						}
					}
					break;
					
					case EVENT_COUNT: break;
				}
			}
			break;
			
			case eNet_EventInit:
			{
				sendSyncMessage( conn_id );
			}
			break;
			
			default: break; // Annoying warnings >:O
		}
	}
}

void CWorm::sendLuaEvent(LuaEventDef* event, eNet_SendMode mode, Net_U8 rules, BitStream* userdata, Net_ConnID connID)
{
	if(!m_node) return;
	BitStream* data = new BitStream;
	addEvent(data, LuaEvent);
	data->addInt(event->idx, 8);
	if(userdata)
	{
		data->addBitStream(*userdata);
	}
	if(!connID)
		m_node->sendEvent(mode, rules, data);
	else
		m_node->sendEventDirect(mode, data, connID);
}



void CWorm::sendSyncMessage( Net_ConnID id )
{
	BitStream *data = new BitStream;
	addEvent(data, SYNC);
	
	for( size_t i = 0; i < m_weapons.size(); ++i )
	{
		if ( m_weapons[i] )
		{
			data->addBool(true);
			Encoding::encode(*data, i, m_weapons.size());
			Encoding::encode(*data, m_weapons[i]->getType()->getIndex(), gusGame.weaponList.size());
		}
	}
	data->addBool(false);
	
	m_node->sendEventDirect(eNet_ReliableOrdered, data, id);
}

void CWorm::sendWeaponMessage( int index, BitStream* weaponData, Net_U8 repRules )
{
	BitStream *data = new BitStream;
	addEvent(data, WeaponMessage);
	//data->addInt(index, Encoding::bitsOf(gusGame.weaponList.size() - 1));
	Encoding::encode(*data, index, m_weapons.size());
	data->addBitStream( *weaponData );
	m_node->sendEvent(eNet_ReliableOrdered, repRules, data);
}

Net_NodeID CWorm::getNodeID()
{
	if ( m_node )
		return m_node->getNetworkID();
	else
		return INVALID_NODE_ID;
}



NetWormInterceptor::NetWormInterceptor( CWorm* parent )
{
	m_parent = parent;
}

bool NetWormInterceptor::inPreUpdateItem (Net_Node *_node, Net_ConnID _from, eNet_NodeRole _remote_role, Net_Replicator *_replicator)
{
	switch ( _replicator->getSetup()->getInterceptID() )
	{
		case CWorm::PlayerID:
		{
			Net_NodeID recievedID = *static_cast<Net_U32*>(_replicator->peekData());
			vector<CWormInputHandler*>::iterator playerIter;
			for ( playerIter = game.players.begin(); playerIter != game.players.end(); playerIter++)
			{
				if ( (*playerIter)->getNodeID() == recievedID )
				{
					(*playerIter)->assignWorm(m_parent);
				}
			}
		}
		break;
		/*case NetWorm::Position:
		{
			Vec recievedPos = *static_cast<Vec*>(_replicator->peekData());
			Vec speedPrediction = (recievedPos - m_parent->lastPosUpdate) / m_parent->timeSinceLastUpdate;
			m_parent->lastPosUpdate = recievedPos;
			m_parent->timeSinceLastUpdate = 0;
			m_parent->spd = m_parent->spd*0.2 + speedPrediction*0.8;
			return true;
		} break;*/
	}
	return true;
}

bool NetWormInterceptor::outPreUpdate (Net_Node* node, eNet_NodeRole remote_role) {
	// do the update exactly in this case
	return game.gameScript()->gusEngineUsed();
}

bool NetWormInterceptor::outPreUpdateItem (Net_Node* node, eNet_NodeRole remote_role, Net_Replicator* replicator)
{

	switch ( replicator->getSetup()->getInterceptID() )
	{
		case CWorm::Position:
			if(!m_parent->getAlive()) // Prevent non-active worms from replicating position
				return false;
		break;
	}

	return true;
}
