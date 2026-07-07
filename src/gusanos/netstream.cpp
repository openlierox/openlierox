/*
 *  netstream.cpp
 *  Gusanos
 *
 *  Created by Albert Zeyer on 30.11.09.
 *  code under LGPL
 *
 */

#include "netstream.h"
#include "util/Bitstream.h"
#include "Networking.h"
#include "EndianSwap.h"
#include "gusanos/encoding.h"
#include "CBytestream.h"

#include "Protocol.h"
#include "CServer.h"
#include "CServerConnection.h"
#include "CClient.h"
#include "CServerNetEngine.h"
#include "CChannel.h"


struct NetControlIntern {
	NetControlIntern() {
		isServer = false;
		controlId = 0;
		myConnIdOnServer = INVALID_CONN_ID;
		cbNodeRequest_Dynamic = false;
		cbNodeRequest_nodeId = INVALID_NODE_ID;
	}

	bool isServer;
	int controlId;
	std::string debugName;
	Net_ConnID myConnIdOnServer;	

	bool cbNodeRequest_Dynamic;
	Net_NodeID cbNodeRequest_nodeId;
	
	// A raw data package structure. All Gusanos packages are of this kind.
	struct DataPackage {
		DataPackage() : type(Type(-1)), sendMode(eNet_ReliableOrdered), repRules(Net_REPRULE_NONE) {}

		enum Type {
			GPT_NodeInit,
			GPT_NodeRemove,
			GPT_NodeEvent, /* eNet_EventUser */
			GPT_NodeUpdate,
			
			// here start all types where we dont send the nodeID
			GPT_Direct,
			GPT_UniqueNodeEvent,
			GPT_ConnectRequest,
			GPT_ConnectResult,
		};

		Type type;
		Net_ConnID connID; // target or source
		SmartPointer<NetNodeIntern> node; // node this is about; this is used for sending
		Net_NodeID nodeId; // node this is about; this is used for receiving; NULL iff !nodeMustBeSet()
		BitStream data;
		eNet_SendMode sendMode;
		Net_RepRules repRules; // if node is set, while sending, these are checked
		
		bool nodeMustBeSet() { return type < GPT_Direct; }
		
		void send(CBytestream& bs, bool withTypeInfo = true);
		void read(const SmartPointer<NetControlIntern>& con, CBytestream& bs, bool withTypeInfo = true); // if withTypeInfo=false, set type before calling this!
	};
	
	typedef std::list<DataPackage> Packages;
	Packages packetsToSend; // These are the packages we have to send. They are processed in olxSend.
	Packages packetsReceived; // These are parsed via olxParse. They are handled in Net_processInput.
	
	DataPackage& pushPackageToSend() { packetsToSend.push_back(DataPackage()); return packetsToSend.back(); }

	// We handle new node registering in Net_processOutput. This is the queue for all new nodes.
	typedef std::pair< SmartPointer<NetNodeIntern>, Net_ConnID > NewNodeInfo;
	std::list<NewNodeInfo> newNodesInfo;
	
	
	// Gusanos node updates aren't handled pushed directly from packetsReceived.
	// They will be pushed into this structure and handled specially here -
	// the main addition is a bandwidth check.
	// Every connection/Net_ConnID has its own manager.
	struct NodeUpdateManager {		
		typedef std::list<DataPackage> Updates;
		Updates updates;
		typedef std::map<NetNodeIntern*,Updates::iterator> NodeMap;
		NodeMap nodeMap;
		
		void pushUpdate(const DataPackage& p) {
			NodeMap::iterator f = nodeMap.find(p.node.get());
			if(f != nodeMap.end()) {
				*f->second = p;
			}
			else {
				updates.push_back(p);
				Updates::iterator& last = nodeMap[p.node.get()] = updates.end(); --last;
			}
		}
		
		void remove(NetNodeIntern* node) {
			NodeMap::iterator f = nodeMap.find(node);
			if(f != nodeMap.end()) {
				updates.erase(f->second);
				nodeMap.erase(f);
			}
		}
		
		void clear() {
			updates.clear();
			nodeMap.clear();
		}
		
		bool send(const SmartPointer<NetControlIntern>& con, Net_ConnID target, size_t maxBytes);
	};
	std::map<Net_ConnID,NodeUpdateManager> nodeUpdateManager;

	
	// Every node has a specific class it is type of.
	struct Class {
		std::string name;
		Net_ClassID id;
		Net_ClassFlags flags;

		Class() : id(INVALID_CLASS_ID), flags(0) {}
		Class(const std::string& n, Net_ClassID i, Net_ClassFlags f) : name(n), id(i), flags(f) {}
	};

	typedef std::map<Net_ClassID, Class> Classes;
	Classes classes;
	
	typedef std::map<Net_NodeID, Net_Node*> Nodes;
	Nodes nodes;
	typedef std::map<Net_ClassID, Net_Node*> LocalNodes;
	LocalNodes localNodes;
		
	Net_NodeID getUnusedNodeId() {
		if(nodes.size() == 0) return 2;
		return nodes.rbegin()->first + 1;
	}
	
	Net_Node* getNode(Net_NodeID id) {
		if(id == INVALID_NODE_ID) return NULL;
		Nodes::iterator i = nodes.find(id);
		if(i != nodes.end()) return i->second;
		return NULL;
	}
	
	Class* getClass(Net_ClassID cid) {
		Classes::iterator i = classes.find(cid);
		if(i != classes.end()) return &i->second;
		return NULL;
	}
	
	Net_Node* getLocalNode(Net_ClassID cid) {
		LocalNodes::iterator i = localNodes.find(cid);
		if(i != localNodes.end()) return i->second;
		return NULL;
	}
	
	std::string debugClassName(Net_ClassID cid) {
		Classes::iterator i = classes.find(cid);
		if(i == classes.end()) return "INVALID-CLASS(" + itoa(cid) + ")";
		return "Class(" + itoa(cid) + ":" + i->second.name + ")";
	}
};

struct NetNodeIntern {
	NetNodeIntern() :
	publicOwner(NULL), control(NULL), classId(INVALID_CLASS_ID), nodeId(INVALID_NODE_ID), role(eNet_RoleUndefined),
	eventForInit(false), eventForRemove(false),
	ownerConn(NetConnID_server()),
	forthcomingReplicatorInterceptID(0), interceptor(NULL) {}
	~NetNodeIntern() { clearReplicationSetup(); }

	Net_Node* publicOwner;
	SmartPointer<NetControlIntern> control;	
	Net_ClassID classId;
	Net_NodeID nodeId;
	eNet_NodeRole role;
	bool eventForInit, eventForRemove;
	std::unique_ptr<BitStream> announceData;
	Net_ConnID ownerConn;
	
	typedef std::list< std::pair<Net_Replicator*,bool> > ReplicationSetup;
	ReplicationSetup replicationSetup;
	Net_InterceptID forthcomingReplicatorInterceptID;
	Net_NodeReplicationInterceptor* interceptor;
		
	void clearReplicationSetup() {
		for(ReplicationSetup::iterator i = replicationSetup.begin(); i != replicationSetup.end(); ++i)
			if(/* autodelete */ i->second)
				delete i->first;
		replicationSetup.clear();
	}
	
	struct Event {
		eNet_Event ev;
		eNet_NodeRole role;
		Net_ConnID cid;
		BitStream stream;
		Event() : ev(eNet_Event(-1)), role(eNet_RoleUndefined), cid(INVALID_CONN_ID) {}
		Event(eNet_Event _ev, eNet_NodeRole _r, Net_ConnID _cid, const BitStream& _s) : ev(_ev), role(_r), cid(_cid), stream(_s) {}
		static Event NodeInit(Net_ConnID c) { return Event(eNet_EventInit, eNet_RoleAuthority /* TODO? */, c, BitStream()); }
		static Event NodeRemoved(Net_ConnID c) { return Event(eNet_EventRemoved, eNet_RoleAuthority /* TODO? */, c, BitStream()); }
		static Event User(const BitStream& s, Net_ConnID c) { return Event(eNet_EventUser, eNet_RoleAuthority /* TODO? */, c, s); }
	};
	
	typedef std::list<Event> Events;
	Events incomingEvents;
	Event curIncomingEvent;

	std::string debugStr() {
		if(control.get() == NULL) return "Node-no-control";
		if(nodeId == INVALID_NODE_ID) return "Node-invalid";
		if(classId == INVALID_CLASS_ID) return "Node-no-ClassID";
		return "Node(" + itoa(nodeId) + "," + control->debugClassName(classId) + ")";
	}
	
	bool isRegistered() { return nodeId != INVALID_NODE_ID; }
};



template <> void SmartPointer_ObjectDeinit<NetControlIntern> ( NetControlIntern * obj )
{
	delete obj;
}

Net_Control::Net_Control(bool isServer) : intern(NULL) {
	intern = new NetControlIntern();
	intern->isServer = isServer;
}

Net_Control::~Net_Control() {
	intern = NULL;
}

void Net_Control::Shutdown() {}


static std::string rawFromBits(BitStream& bits) {
	size_t oldPos = bits.bitPos();
	bits.resetPos();
	std::string ret;
	ret.reserve((bits.bitSize() + 7) / 8);
	for(size_t i = 0; i < bits.bitSize() / 8; ++i)
		ret += getCharFromBits(bits);
	if(bits.bitSize() % 8 != 0)
		ret += (char) (unsigned char) bits.getInt(bits.bitSize() % 8);
	bits.setBitPos(oldPos);
	return ret;
}

static void writeEliasGammaNr(BitStream& bits, size_t n) {
	Encoding::encodeEliasGamma(bits, n + 1);
}

static size_t readEliasGammaNr(BitStream& bits) {
	size_t n = Encoding::decodeEliasGamma(bits) - 1;
	if(n == size_t(-1)) {
		errors << "readEliasGammaNr: stream reached end" << endl;
		return 0;
	}
	return n;
}

static void writeEliasGammaNr(CBytestream& bs, size_t n) {
	BitStream bits;
	writeEliasGammaNr(bits, n);
	bs.writeData(rawFromBits(bits));
}

static size_t readEliasGammaNr(CBytestream& bs) {
	BitStream bits(bs.data().substr(bs.GetPos()));
	size_t n = readEliasGammaNr(bits);
	bs.Skip( (bits.bitPos() + 7) / 8 );
	return n;
}

static size_t eliasGammaEncodedByteLen(size_t num) {
	const size_t bits =
		Encoding::bitsOf(num + 1) + // Encoding::encodeEliasGamma, bits
		Encoding::bitsOf(num + 1) - 1; // Encoding::encodeEliasGamma, num
	return (bits + 7) / 8;
}

void NetControlIntern::DataPackage::send(CBytestream& bs, bool withTypeInfo) {
	if(withTypeInfo) bs.writeByte(type);
	if(nodeMustBeSet()) {
		if(node.get() == NULL) {
			errors << "NetControlIntern::DataPackage::send: node was not set" << endl;
			writeEliasGammaNr(bs, INVALID_NODE_ID);
		}
		else
			writeEliasGammaNr(bs, node->nodeId);
	}
	writeEliasGammaNr(bs, (data.bitSize() + 7)/8);
	bs.writeData(rawFromBits(data));
}

void NetControlIntern::DataPackage::read(const SmartPointer<NetControlIntern>& con, CBytestream& bs, bool withTypeInfo) {
	if(withTypeInfo) type = (NetControlIntern::DataPackage::Type) bs.readByte();
	nodeId = nodeMustBeSet() ? readEliasGammaNr(bs) : INVALID_NODE_ID;
	node = NULL;
	size_t len = readEliasGammaNr(bs);
	data = BitStream( bs.getRawData( bs.GetPos(), bs.GetPos() + len ) );
	bs.Skip(len);
}


static bool composePackagesForConn(CBytestream& bs, const SmartPointer<NetControlIntern>& con, Net_ConnID connid) {
	assert(connid != INVALID_CONN_ID);
	
	typedef std::list<NetControlIntern::DataPackage*> Packages;
	Packages packages;
	
	for(NetControlIntern::Packages::iterator i = con->packetsToSend.begin(); i != con->packetsToSend.end(); ++i)
		if(i->connID == INVALID_CONN_ID || i->connID == connid) {
			if(!isServerNetConnID(connid)) {
				CServerConnection* cl = serverConnFromNetConnID(connid);
				if(!cl->gusLoggedIn()) {
					if(i->type == NetControlIntern::DataPackage::GPT_ConnectResult)
						cl->gusLoggedIn() = true;
					else
						continue;
				}
			}
			
			if(i->nodeMustBeSet() || i->node.get()) {
				if(i->node.get() == NULL) {
					errors << "composePackagesForConn: node must be set but is unset" << endl;
					continue;
				}
				
				if(!con->isServer) {
					if(i->repRules & Net_REPRULE_OWNER_2_AUTH) {
						if(i->node->ownerConn != con->myConnIdOnServer)
							continue;
					}
					else
						continue;
				}
				else { // server
					bool trgtIsOwner = i->node->ownerConn == connid;
					if(trgtIsOwner && !(i->repRules & Net_REPRULE_AUTH_2_OWNER))
						continue;
					if(!trgtIsOwner && !(i->repRules & Net_REPRULE_AUTH_2_PROXY))
						continue;
				}
			}
			else { // no node
				if(i->repRules != Net_REPRULE_NONE)
					warnings << "reprules should be none for gus package of type " << i->type << endl;
			}
			
			if(i->type == NetControlIntern::DataPackage::GPT_NodeUpdate)
				con->nodeUpdateManager[connid].pushUpdate(*i);
			else
				packages.push_back(&*i);
		}
	
	if(packages.size() == 0) return false;
	
	bs.writeByte(con->isServer ? (uchar)S2C_GUSANOS : (uchar)C2S_GUSANOS);
	writeEliasGammaNr(bs, packages.size() - 1);
	for(Packages::iterator i = packages.begin(); i != packages.end(); ++i)
		(*i)->send(bs);
	
	//bs.Dump();
	return true;
}

static std::vector<CServerConnection*> getConnsForId(Net_ConnID cid) {
	std::vector<CServerConnection*> ret;
	if(cid == INVALID_CONN_ID) {
		ret.reserve(MAX_CLIENTS);
		CServerConnection *cl = cServer->getClients();
		for(int c = 0; c < MAX_CLIENTS; c++, cl++) {
			if(cl->getStatus() == NET_DISCONNECTED || cl->getStatus() == NET_ZOMBIE) continue;
			if(cl->getNetEngine() == NULL) continue;
			if(cl->isLocalClient()) continue;
			if(cl->getClientVersion() < OLXBetaVersion(0,59,1)) continue;
			
			ret.push_back(cl);
		}
	}
	else {
		CServerConnection *cl = serverConnFromNetConnID(cid);
		if(cl) ret.push_back(cl);
	}
	return ret;
}

void Net_Control::olxSend(bool /* sendPendingOnly */) {
	if(intern->packetsToSend.size() == 0) return;
		
	if(game.isClient()) {
		if(cClient->getServerVersion() >= OLXBetaVersion(0,59,1)) {
			CBytestream bs;
			if(composePackagesForConn(bs, this->intern, NetConnID_server()))
				cClient->getChannel()->AddReliablePacketToSend(bs);
		}
	} else {
		// send to all except local client
		std::vector<CServerConnection*> conns = getConnsForId(INVALID_CONN_ID);
		for(size_t i = 0; i < conns.size(); ++i) {
			CBytestream bs;
			if(composePackagesForConn(bs, this->intern, NetConnID_conn(conns[i])))
				conns[i]->getNetEngine()->SendPacket(&bs);
		}
	}
	
	intern->packetsToSend.clear();
}

bool NetControlIntern::NodeUpdateManager::send(const SmartPointer<NetControlIntern>& con, Net_ConnID target, size_t maxBytes) {
	if(updates.size() == 0) return false;
	CBytestream tmpbs;

	size_t count = 0;
	while(updates.size() > 0) {
		CBytestream tmpbs2;
		updates.front().send(tmpbs2, false);
		if(tmpbs.GetLength() + tmpbs2.GetLength() + eliasGammaEncodedByteLen(count) + 1 > maxBytes)
			break;
		
		tmpbs.Append(&tmpbs2);
		remove(updates.front().node.get());
		count++;
	}
	if(count == 0) return false;
	
	CBytestream bs;
	bs.writeByte(con->isServer ? (uchar)S2C_GUSANOSUPDATE : (uchar)C2S_GUSANOSUPDATE);
	writeEliasGammaNr(bs, count - 1);
	bs.Append(&tmpbs);

	if(con->isServer)
		serverConnFromNetConnID(target)->getNetEngine()->SendPacket(&bs);
	else
		cClient->getChannel()->AddReliablePacketToSend(bs);
	
	return true;
}

bool Net_Control::olxSendNodeUpdates(Net_ConnID target, size_t maxBytes) {
	return intern->nodeUpdateManager[target].send(intern, target, maxBytes);
}

void Net_Control::olxParse(Net_ConnID src, CBytestream& bs) {
	//size_t bsStart = bs.GetPos();
	size_t len = readEliasGammaNr(bs) + 1;

	for(size_t i = 0; i < len; ++i) {
		intern->packetsReceived.push_back(NetControlIntern::DataPackage());
		NetControlIntern::DataPackage& p = intern->packetsReceived.back();
		p.read(this->intern, bs);
		p.connID = src;
	}
	
	//bs.Dump(PrintOnLogger(notes), std::set<size_t>(), bsStart, bs.GetPos() - bsStart);
}

static void olxParseWithFixedType(Net_Control* con, Net_ConnID src, CBytestream& bs, NetControlIntern::DataPackage::Type type) {
	size_t len = readEliasGammaNr(bs) + 1;
	
	for(size_t i = 0; i < len; ++i) {
		con->intern->packetsReceived.push_back(NetControlIntern::DataPackage());
		NetControlIntern::DataPackage& p = con->intern->packetsReceived.back();
		p.type = type;
		p.read(con->intern, bs, false);
		p.connID = src;
	}		
}

void Net_Control::olxParseUpdate(Net_ConnID src, CBytestream& bs) {
	olxParseWithFixedType(this, src, bs, NetControlIntern::DataPackage::GPT_NodeUpdate);
}

void Net_Control::olxHandleClientDisconnect(Net_ConnID cl) {
	// push node remove events for those nodes where we requested it
	for(NetControlIntern::Nodes::iterator i = intern->nodes.begin(); i != intern->nodes.end(); ++i) {
		Net_Node* node = i->second;
	
		if(node->intern->eventForRemove)
			node->intern->incomingEvents.push_back( NetNodeIntern::Event::NodeRemoved(cl) );
	}
	
	intern->nodeUpdateManager[cl].clear();
}

static void pushNodeUpdate(Net_Node* node, const std::vector<BitStream>& replData, Net_RepRules rule) {
	NetControlIntern::DataPackage p;
	p.connID = INVALID_CONN_ID;
	p.sendMode = eNet_ReliableOrdered; // TODO ?
	p.repRules = rule;
	p.type = NetControlIntern::DataPackage::GPT_NodeUpdate;
	p.node = node->intern;
	
	size_t count = 0;
	size_t k = 0;
	for(NetNodeIntern::ReplicationSetup::iterator j = node->intern->replicationSetup.begin(); j != node->intern->replicationSetup.end(); ++j, ++k) {
		if(replData[k].bitSize() > 0) {
			Net_ReplicatorBasic* replicator = dynamic_cast<Net_ReplicatorBasic*>(j->first);
			if(replicator->getSetup()->repRules & rule) {
				p.data.addBitStream(replData[k]);
				count++;
			}
			else
				p.data.addBool(false);				
		}
		else
			p.data.addBool(false);
	}
	
	if(count > 0)
		node->intern->control->pushPackageToSend() = p;
}

static void handleNodeForUpdate(Net_Node* node, bool forceUpdate) {
	if(node->intern->interceptor)
		if(!node->intern->interceptor->outPreUpdate(node, eNet_RoleProxy))
			return;

	std::vector<BitStream> replData;
	replData.resize(node->intern->replicationSetup.size());
	
	size_t count = 0;
	size_t k = 0;
	for(NetNodeIntern::ReplicationSetup::iterator j = node->intern->replicationSetup.begin(); j != node->intern->replicationSetup.end(); ++j, ++k) {
		Net_ReplicatorBasic* replicator = dynamic_cast<Net_ReplicatorBasic*>(j->first);
		if(replicator == NULL) {
			errors << "Replicator is not a basic replicator" << endl;
			continue;
		}
		
		if(!forceUpdate && !replicator->checkState())
			continue;
		
		//eNet_NodeRole remoterole = !con->intern->isServer ? eNet_RoleAuthority : (rule == Net_REPRULE_AUTH_2_OWNER) ? eNet_RoleOwner : eNet_RoleProxy;
		if(node->intern->interceptor) {
			if(!node->intern->interceptor->outPreUpdateItem(node, eNet_RoleProxy, replicator))
				continue;
		}
		
		replData[k].addBool(true);
		replicator->packData(&replData[k]);
		count++;
	}		
	
	if(count == 0) return;
	
	if(node->intern->role == eNet_RoleAuthority) {
		pushNodeUpdate(node, replData, Net_REPRULE_AUTH_2_PROXY);
		pushNodeUpdate(node, replData, Net_REPRULE_AUTH_2_OWNER);
	}
	else if(node->intern->role == eNet_RoleOwner) {						
		pushNodeUpdate(node, replData, Net_REPRULE_OWNER_2_AUTH);
	}	
}

static void pushNodeInit(const NetControlIntern::NewNodeInfo& info) {
	const SmartPointer<NetNodeIntern>& node = info.first;
	const Net_ConnID connid = info.second;
	
	if(node->publicOwner == NULL || !node->isRegistered())
		// Node was already deleted again in the meanwhile.
		// This can happen, nothing wrong then. Just ignore.
		return;
	
	if(node->interceptor)
		node->interceptor->outPreReplicateNode(node->publicOwner, (connid == node->ownerConn) ? eNet_RoleOwner : eNet_RoleProxy);
	
	NetControlIntern::DataPackage& p = node->control->pushPackageToSend();
	p.connID = connid;
	p.sendMode = eNet_ReliableOrdered;
	p.repRules = Net_REPRULE_AUTH_2_ALL;
	p.type = NetControlIntern::DataPackage::GPT_NodeInit;
	p.node = node;
	p.data.addInt(node->classId, 32);
	p.data.addInt(node->ownerConn, 32);
	
	bool announce = node->control->classes[node->classId].flags & Net_CLASSFLAG_ANNOUNCEDATA;
	if(node->announceData.get() && !announce)
		warnings << "node " << node->debugStr() << " has announce data but class doesnt have flag set" << endl;
	else if(!node->announceData.get() && announce)
		warnings << "node " << node->debugStr() << " has no announce data but class requests it" << endl;
	else if(node->announceData.get() && announce)
		p.data.addBitStream(*node->announceData.get());
	
	if(node->eventForInit) {
		std::vector<CServerConnection*> conns = getConnsForId(p.connID);
		for(size_t i = 0; i < conns.size(); ++i)
			node->incomingEvents.push_back( NetNodeIntern::Event::NodeInit(NetConnID_conn(conns[i])) );
	}
	
	handleNodeForUpdate(node->publicOwner, true);	
}

void Net_Control::Net_processOutput() {
	// send info about new nodes
	for(std::list<NetControlIntern::NewNodeInfo>::iterator i = intern->newNodesInfo.begin(); i != intern->newNodesInfo.end(); ++i)
		pushNodeInit(*i);
	intern->newNodesInfo.clear();
	
	// goes through the nodes and push Node-updates as needed
	for(NetControlIntern::Nodes::iterator i = intern->nodes.begin(); i != intern->nodes.end(); ++i) {
		Net_Node* node = i->second;
		handleNodeForUpdate(node, false);		
	}
}

static void doNodeUpdate(Net_Node* node, BitStream& bs, Net_ConnID cid) {
	size_t i = 0;
	for(NetNodeIntern::ReplicationSetup::iterator j = node->intern->replicationSetup.begin(); j != node->intern->replicationSetup.end(); ++j, ++i) {
		if( /* skip mark */ !bs.getBool()) continue;
		
		Net_ReplicatorBasic* replicator = dynamic_cast<Net_ReplicatorBasic*>(j->first);
		if(replicator == NULL) {
			errors << "Replicator " << i << " in update of node " << node->intern->debugStr() << " is not a basic replicator" << endl;
			break; // nothing else we can do
		}
				
		bool store = true;
		if(node->intern->interceptor) {
			BitStream peekStream(bs);
			replicator->peekStream = &peekStream;

			store = node->intern->interceptor->inPreUpdateItem(node, cid, eNet_RoleAuthority, replicator);

			replicator->clearPeekData();
			replicator->ptr = NULL;
			replicator->peekStream = NULL;
		}
		
		replicator->unpackData(&bs, store);
	}
}


static void tellClientAboutNode(Net_Node* node, Net_ConnID connid) {
	node->intern->control->newNodesInfo.push_back( std::make_pair(node->intern, connid) );
}

static bool unregisterNode(Net_Node* node);

static void tellClientAboutAllNodes(const SmartPointer<NetControlIntern>& con, Net_ConnID connid) {
	for(NetControlIntern::Nodes::iterator i = con->nodes.begin(); i != con->nodes.end(); ++i) {
		tellClientAboutNode(i->second, connid);
	}
}

void Net_Control::Net_processInput() {
	for(NetControlIntern::Packages::iterator i = intern->packetsReceived.begin(); i != intern->packetsReceived.end(); ++i) {
		switch(i->type) {
			case NetControlIntern::DataPackage::GPT_Direct:
				Net_cbDataReceived(i->connID, i->data);
				break;
				
			case NetControlIntern::DataPackage::GPT_ConnectRequest: {
				if(!intern->isServer) {
					warnings << "Net_processInput: got GPT_ConnectRequest as client" << endl;
					break;
				}
				
				CServerConnection* cl = serverConnFromNetConnID(i->connID);
				if(cl == NULL) {
					errors << "Net_processInput GPT_ConnectRequest: didn't found connection for id " << i->connID << endl;
					break;
				}
				
				if(cl->gusLoggedIn()) {
					warnings << "Net_processInput GPT_ConnectRequest: client " << i->connID << " was already logged in" << endl;
					break;
				}
				
				// in olxSend, when we handle this package, we set gusLoggedIn() = true
				NetControlIntern::DataPackage& p = intern->pushPackageToSend();
				p.connID = i->connID;
				p.sendMode = eNet_ReliableOrdered;
				p.repRules = Net_REPRULE_NONE;
				p.type = NetControlIntern::DataPackage::GPT_ConnectResult;	
				p.data.addInt(i->connID, 32); // we tell client about its connection ID
				
				tellClientAboutAllNodes(this->intern, i->connID);
								
				Net_cbConnectionSpawned(i->connID);
				break;
			}
			
			case NetControlIntern::DataPackage::GPT_ConnectResult: {
				if(intern->isServer) {
					warnings << "Net_processInput: got GPT_ConnectResult as server" << endl;
					break;
				}
				
				intern->myConnIdOnServer = i->data.getInt(32);
				
				Net_cbConnectResult(eNet_ConnAccepted);
				break;
			}
				
			case NetControlIntern::DataPackage::GPT_NodeInit: {
				if(intern->isServer) {
					warnings << "Net_processInput: got GPT_NodeInit as server" << endl;
					break;
				}
				
				Net_ClassID classId = i->data.getInt(32);
				Net_ConnID ownerConnId = i->data.getInt(32);

				NetControlIntern::Class* nodeClass = intern->getClass(classId);
				if(nodeClass == NULL) {
					warnings << "NodeInit for node " << i->nodeId << " with class " << classId << " failed because that class is unknown" << endl;
					break;
				}
				
				intern->cbNodeRequest_Dynamic = true;
				intern->cbNodeRequest_nodeId = i->nodeId;
				bool announce = nodeClass->flags & Net_CLASSFLAG_ANNOUNCEDATA;
				
				eNet_NodeRole role = (ownerConnId == intern->myConnIdOnServer) ? eNet_RoleOwner : eNet_RoleProxy;
				Net_cbNodeRequest_Dynamic(i->connID, classId, announce ? &i->data : NULL, role, i->nodeId);
				if(intern->cbNodeRequest_Dynamic) { // we didnt created the node - otherwise this would be false
					errors << "Net_cbNodeRequest_Dynamic callback didnt created the node " << i->nodeId << " with class " << classId << endl;
					intern->cbNodeRequest_Dynamic = false; // reset anyway
					break;
				}
				
				Net_Node* node = intern->getNode(i->nodeId);
				if(node == NULL) {
					errors << "NodeInit: node " << i->nodeId << " not found after dynamic creation" << endl;
					break;
				}
				
				node->intern->ownerConn = ownerConnId;
				node->intern->role = role;

				if(node->intern->classId != classId)
					warnings << "NodeInit requested a node of class " << classId << " but a node of class " << node->intern->classId << " was created" << endl;
								
				break;
			}
				
			case NetControlIntern::DataPackage::GPT_NodeRemove: {
				if(intern->isServer) {
					warnings << "Net_processInput: got GPT_NodeRemove as server" << endl;
					break;
				}

				Net_Node* node = intern->getNode(i->nodeId);
				if(node == NULL) {
					warnings << "NodeRemove: node " << i->nodeId << " not found" << endl;
					break;
				}
				
				// we are proxy or owner -> in any case, always push this event
				node->intern->incomingEvents.push_back( NetNodeIntern::Event::NodeRemoved(i->connID) );
				
				unregisterNode(node);
				break;
			}
				
			case NetControlIntern::DataPackage::GPT_NodeUpdate: {				
				Net_Node* node = intern->getNode(i->nodeId);
				if(node == NULL) {
					warnings << "NodeUpdate: node " << i->nodeId << " not found" << endl;
					break;
				}
				
				if(intern->isServer) {
					if(node->intern->ownerConn != i->connID) {
						warnings << "Net_processInput: got GPT_NodeUpdate as server from proxy" << endl;
						break;
					}
				}
					   
				doNodeUpdate(node, i->data, i->connID);
				break;
			}
				
			case NetControlIntern::DataPackage::GPT_NodeEvent: {
				Net_Node* node = intern->getNode(i->nodeId);
				if(node == NULL) {
					warnings << "NodeEvent: node " << i->nodeId << " not found" << endl;
					break;
				}
				
				if(intern->isServer) {
					if(node->intern->ownerConn != i->connID) {
						warnings << "NodeEvent: got event for node " << i->nodeId << " from non-owner " << i->connID << ", owner is " << node->intern->ownerConn << endl;
						break;
					}
				}
				
				node->intern->incomingEvents.push_back( NetNodeIntern::Event::User(i->data, i->connID) );
				break;
			}
			
			case NetControlIntern::DataPackage::GPT_UniqueNodeEvent: {
				Net_ClassID classId = readEliasGammaNr(i->data);
				NetControlIntern::Class* c = intern->getClass(classId);
				if(c == NULL) {
					warnings << "UniqueNodeEvent: class " << classId << " not found" << endl;
					break;
				}
				
				Net_Node* node = intern->getLocalNode(classId);
				if(node == NULL) {
					warnings << "UniqueNodeEvent: unique node of class " << c->name << " not found" << endl;
					break;
				}
				
				node->intern->incomingEvents.push_back( NetNodeIntern::Event::User(i->data, i->connID) );
				
				break;
			}
				
			default:
				warnings << "invalid Gusanos data package type" << endl;
		}
	}
	
	intern->packetsReceived.clear();
}

void Net_Control::Net_ConnectToServer() {
	NetControlIntern::DataPackage& p = intern->pushPackageToSend();
	p.connID = NetConnID_server();
	p.type = NetControlIntern::DataPackage::GPT_ConnectRequest;
	p.sendMode = eNet_ReliableOrdered;
	p.repRules = Net_REPRULE_NONE; // this is anyway ignored here
}

void Net_Control::Net_sendData(Net_ConnID id, BitStream* s, eNet_SendMode m) {
	NetControlIntern::DataPackage& p = intern->pushPackageToSend();
	p.connID = id;
	p.sendMode = m;
	p.repRules = Net_REPRULE_NONE; // anyway ignored
	p.type = NetControlIntern::DataPackage::GPT_Direct;
	p.data = *s;
	delete s;
}

Net_ClassID Net_Control::Net_registerClass(const std::string& classname, Net_ClassFlags flags) {
	Net_ClassID id = 1;
	if(intern->classes.size() > 0)
		id = intern->classes.rbegin()->first + 1;

	intern->classes[id] = NetControlIntern::Class(classname, id, flags);
	return id;
}


void Net_Control::Net_setControlID(int id) { intern->controlId = id; }
void Net_Control::Net_setDebugName(const std::string& n) { intern->debugName = n; }


void Net_Control::Net_requestNetMode(Net_ConnID, int) {
	// we silently ignore that because we dont have different netstream levels
}



static bool __unregisterNode(Net_Node* node) {
	if(node->intern->nodeId != INVALID_NODE_ID) {
		if(node->intern->control.get() == NULL) {
			errors << "Net_Node::unregisterNode: node was a valid id but no reference to Net_Control" << endl;
			return false;
		}
		
		if(node->intern->nodeId != UNIQUE_NODE_ID) { // dynamic node
			NetControlIntern::Nodes& nodes = node->intern->control->nodes;
			NetControlIntern::Nodes::iterator i = nodes.find(node->intern->nodeId);
			if(i == nodes.end()) {
				errors << "Net_Node::unregisterNode: dynamic node not found in node-list" << endl;
				return false;
			}
			
			nodes.erase(i);

			if(node->intern->role == eNet_RoleAuthority) {
				NetControlIntern::DataPackage& p = node->intern->control->pushPackageToSend();
				p.connID = 0;
				p.sendMode = eNet_ReliableOrdered;
				p.repRules = Net_REPRULE_AUTH_2_ALL;
				p.type = NetControlIntern::DataPackage::GPT_NodeRemove;
				p.node = node->intern;
			}
		}
		
		else { // unique node
			NetControlIntern::LocalNodes& nodes = node->intern->control->localNodes;
			NetControlIntern::LocalNodes::iterator i = nodes.find(node->intern->classId);
			if(i == nodes.end()) {
				errors << "Net_Node::unregisterNode: unique node not found in node-list" << endl;
				return false;
			}
			
			nodes.erase(i);
		}
		
		notes << "Node " << node->intern->nodeId << " with class " << node->intern->control->classes[node->intern->classId].name << " unregisters" << endl;
		return true;
	}
	return false;
}

static bool unregisterNode(Net_Node* node) {
	bool ret = __unregisterNode(node);
	return ret;
}

template <> void SmartPointer_ObjectDeinit<NetNodeIntern> ( NetNodeIntern * obj )
{
	delete obj;
}

Net_Node::Net_Node() {
	intern = new NetNodeIntern();
	intern->publicOwner = this;
}

Net_Node::~Net_Node() {
	unregisterNode(this);
	intern->publicOwner = NULL;
	intern = NULL;
}


eNet_NodeRole Net_Node::getRole() { return intern->role; }
void Net_Node::setOwner(Net_ConnID cid) { intern->ownerConn = cid; }
void Net_Node::setAnnounceData(BitStream* s) { intern->announceData = std::unique_ptr<BitStream>(s); }
Net_NodeID Net_Node::getNetworkID() { return intern->nodeId; }

static void tellAllClientsAboutNode(Net_Node* node) {
	for(int i = 0; i < MAX_CLIENTS; ++i) {
		if(cServer->getClients()[i].getStatus() != NET_CONNECTED) continue;
		if(cServer->getClients()[i].getNetEngine() == NULL) continue;
		tellClientAboutNode(node, NetConnID_conn(&cServer->getClients()[i]));
	}
}

static bool registerNode(Net_ClassID classid, Net_Node* node, Net_NodeID nid, eNet_NodeRole role, const SmartPointer<NetControlIntern>& con) {
	if(node->intern->nodeId != INVALID_NODE_ID) {
		errors << "Net_Node::registerNode " << node->intern->debugStr() << " trying to register node twice" << endl;
		return false;
	}

	if(con.get() == NULL) {
		errors << "Net_Node::registerNode without Net_Control" << endl;
		return false;
	}
	
	if(nid != UNIQUE_NODE_ID && con->getNode(nid)) {
		errors << "Net_Node::registerNode " << node->intern->debugStr() << " node id " << nid << " was already taken by " << con->getNode(nid)->intern->debugStr() << endl;
		return false;
	}
	
	node->intern->control = con;
	node->intern->classId = classid;
	node->intern->nodeId = nid;
	node->intern->role = role;
	
	if(nid == UNIQUE_NODE_ID) {
		if(con->getLocalNode(classid) != NULL)
			errors << "Net_Node::registerNode: unique node " << node->intern->debugStr() << " overwrites another unique node of the same class" << endl;
		con->localNodes[classid] = node;
	} else {
		con->nodes[nid] = node;
	
		if(role == eNet_RoleAuthority)
			tellAllClientsAboutNode(node);
	}
	
	notes << "Node " << nid << " registers with role " << role << " and class " << con->classes[classid].name << endl;
	return true;
}

bool Net_Node::registerNodeUnique(Net_ClassID cid, eNet_NodeRole role, Net_Control* con) {
	return registerNode(cid, this, UNIQUE_NODE_ID, role, con->intern);
}

bool Net_Node::registerNodeDynamic(Net_ClassID cid, Net_Control* con) {
	eNet_NodeRole role = con->intern->cbNodeRequest_Dynamic ? eNet_RoleProxy : eNet_RoleAuthority;
	
	if(con->intern->isServer && role != eNet_RoleAuthority) {
		errors << "registerNodeDynamic: want to register proxy node on server" << endl;
		return false; // stop because otherwise we probably would crash
	}

	if(!con->intern->isServer && role == eNet_RoleAuthority) {
		errors << "registerNodeDynamic: want to register authority node on client" << endl;
		return false; // stop because otherwise we probably would crash
	}
	
	Net_NodeID nid = con->intern->cbNodeRequest_Dynamic ? con->intern->cbNodeRequest_nodeId : con->intern->getUnusedNodeId();
	con->intern->cbNodeRequest_Dynamic = false; // only for first node
	return registerNode(cid, this, nid, role, con->intern);
}

bool Net_Node::registerRequestedNode(Net_ClassID cid, Net_Control* con) { return registerNodeDynamic(cid, con); }

void Net_Node::applyForNetLevel(int something) {}
void Net_Node::removeFromNetLevel(int something) {}

bool Net_Node::isNodeRegistered() {
	return intern->isRegistered();
}

Net_ConnID Net_Node::getOwner() {
	return intern->ownerConn;
}

bool Net_Node::areWeOwner() {
	if(isNodeRegistered()) {
		if(intern->control->isServer)
			return intern->ownerConn == NetConnID_server();
		else
			return intern->ownerConn == intern->control->myConnIdOnServer;
	}
	
	// node not registered
	return false;
}



void Net_Node::setEventNotification(bool eventForInit /* eNet_EventInit */, bool eventForRemove /* eNet_EventRemoved */) {
	intern->eventForInit = eventForInit;
	intern->eventForRemove = eventForRemove;
}

static void __sendNodeEvent(Net_ConnID connId, eNet_SendMode m, Net_RepRules rules, Net_Node* node, BitStream& s) {
	NetControlIntern::DataPackage& p = node->intern->control->pushPackageToSend();
	p.connID = connId;
	p.sendMode = m;
	p.repRules = rules;
	p.node = node->intern;
	if(node->intern->nodeId == UNIQUE_NODE_ID) {
		p.type = NetControlIntern::DataPackage::GPT_UniqueNodeEvent;
		writeEliasGammaNr(p.data, node->intern->classId);
	} else
		p.type = NetControlIntern::DataPackage::GPT_NodeEvent;
	p.data.addBitStream(s);	
}

void Net_Node::sendEvent(eNet_SendMode m, Net_RepRules rules, BitStream* s) {
	__sendNodeEvent(INVALID_CONN_ID, m, rules, this, *s);
	delete s;
}

void Net_Node::sendEventDirect(eNet_SendMode m, BitStream* s, Net_ConnID cid) {
	if(!intern->control->isServer) {
		errors << "Net_Node::sendEventDirect (node " << intern->nodeId << ") only works as server" << endl;
		return;
	}
	
	__sendNodeEvent(cid, m, Net_REPRULE_AUTH_2_ALL, this, *s);
	delete s;
}

bool Net_Node::checkEventWaiting() { return intern.get() && intern->incomingEvents.size() > 0; }

BitStream* Net_Node::getNextEvent(eNet_Event* e, eNet_NodeRole* r, Net_ConnID* cid) {
	if(intern->incomingEvents.size() == 0) return NULL;
	
	NetNodeIntern::Event& ev = intern->curIncomingEvent = intern->incomingEvents.front();
	intern->incomingEvents.pop_front();

	if(e) *e = ev.ev;
	if(r) *r = ev.role;
	if(cid) *cid = ev.cid;
	return &ev.stream;
}


void Net_Node::beginReplicationSetup() {
	intern->clearReplicationSetup();
}

void Net_Node::addReplicator(Net_Replicator* rep, bool autodelete) {
	intern->replicationSetup.push_back( std::make_pair(rep, autodelete) );
}

void Net_Node::addReplicationInt(Net_S32* n, int bits, bool, Net_RepFlags f, Net_RepRules r, Net_InterceptID id, int p2, int p3) {

	if((f & Net_REPFLAG_INTERCEPT) && id == 0)
		id = intern->forthcomingReplicatorInterceptID;

	struct IntReplicator : Net_ReplicatorBasic {
		Net_ReplicatorSetup setup;
		typedef Net_S32 Num;
		Num* n;
		Num old;
		int bits;
		
		IntReplicator(const Net_ReplicatorSetup& s) : Net_ReplicatorBasic(&setup), setup(s) {}
		Net_Replicator* Duplicate(Net_Replicator*) { return new IntReplicator(*this); }		
		
		void* peekData() { peekDataStore(new Num(peekStream->getInt(bits))); return peekDataRetrieve(); }
		void clearPeekData() { Num* p = (Num*)peekDataRetrieve(); if(p) delete p; }
		
		bool checkState() { bool diff = *n != old; old = *n; return diff;  }
		void packData(BitStream *_stream) { _stream->addInt(*n, bits); }
		void unpackData(BitStream *_stream, bool _store) {
			Num i = _stream->getInt(bits);
			if(_store) *n = i;
		}
	};
	
	IntReplicator* rep = new IntReplicator(Net_ReplicatorSetup(f, r, id, p2, p3));
	rep->n = n;
	rep->old = *n;
	rep->bits = bits;
	
	addReplicator(rep, true);
}

void Net_Node::addReplicationFloat(Net_Float* n, int bits, Net_RepFlags f, Net_RepRules r, Net_InterceptID id, int p2, int p3) {

	if((f & Net_REPFLAG_INTERCEPT) && id == 0)
		id = intern->forthcomingReplicatorInterceptID;

	struct FloatReplicator : Net_ReplicatorBasic {
		Net_ReplicatorSetup setup;
		typedef Net_Float Num;
		Num* n;
		Num old;
		int bits;
		
		FloatReplicator(const Net_ReplicatorSetup& s) : Net_ReplicatorBasic(&setup), setup(s) {}
		Net_Replicator* Duplicate(Net_Replicator*) { return new FloatReplicator(*this); }		
		
		void* peekData() { peekDataStore(new Num(peekStream->getFloat(bits))); return peekDataRetrieve(); }
		void clearPeekData() { Num* p = (Num*)peekDataRetrieve(); if(p) delete p; }
		
		bool checkState() { bool diff = *n != old; old = *n; return diff;  }
		void packData(BitStream *_stream) { _stream->addFloat(*n, bits); }
		void unpackData(BitStream *_stream, bool _store) {
			Num i = _stream->getFloat(bits);
			if(_store) *n = i;
		}
	};
	
	FloatReplicator* rep = new FloatReplicator(Net_ReplicatorSetup(f, r, id, p2, p3));
	rep->n = n;
	rep->old = *n;
	rep->bits = bits;
		
	addReplicator(rep, true);	
}

void Net_Node::endReplicationSetup() {}

void Net_Node::setInterceptID(Net_InterceptID id) { intern->forthcomingReplicatorInterceptID = id; }

void Net_Node::setReplicationInterceptor(Net_NodeReplicationInterceptor* inter) { intern->interceptor = inter; }

std::string Net_Node::debugName() {
	return intern->debugStr();
}







Net_ReplicatorSetup::Net_ReplicatorSetup(Net_RepFlags flags, Net_RepRules rules, Net_InterceptID id, int p2, int p3) {
	repFlags = flags;
	repRules = rules;
	interceptId = id;
}




Net_ConnID NetConnID_server() {
	return Net_ConnID(-1);
}

Net_ConnID NetConnID_conn(CServerConnection* cl) {
	for(int i = 0; i < MAX_CLIENTS; i++) {
		if(&cServer->getClients()[i] == cl) return i + 1;
	}
	
	errors << "NetConnID_conn: connection invalid" << endl;
	return INVALID_CONN_ID;
}

CServerConnection* serverConnFromNetConnID(Net_ConnID id) {
	if(!cServer->getClients() || !cServer->isServerRunning()) {
		errors << "serverConnFromNetConnID: server is not running" << endl;
		return NULL;
	}
	
	if(id >= 1 && id <= MAX_CLIENTS) return &cServer->getClients()[id - 1];
	
	errors << "serverConnFromNetConnID: id " << id << " is invalid" << endl;
	return NULL;
}

bool isServerNetConnID(Net_ConnID id) {
	return id == NetConnID_server();
}


