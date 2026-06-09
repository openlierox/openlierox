/*
 *  ServerList.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 22.04.10.
 *  code under LGPL
 *
 */

#include "ServerList.h"
#include "TaskManager.h"
#include "HTTP.h"
#include "CBytestream.h"
#include "Consts.h"
#include "DeprecatedGUI/Menu.h"
#include "DeprecatedGUI/CListview.h"
#include "Options.h"
#include "LieroX.h"
#include "Timer.h"
#include "StringUtils.h"
#include "Unicode.h"
#include "IpToCountryDB.h"
#include "FeatureList.h"
#include "FindFile.h"


// TODO: move this out here
// declare them only locally here as nobody really should use them explicitly
std::string Utf8String(const std::string &OldLxString);

// Maximum number of pings/queries before we ignore the server
static const int	MaxPings = 4;
static const int	MaxQueries = MAX_QUERIES;


// tSocket is an array and just has the sockets copied from tMenu->tSocket.
// Because there were some bugs where tMenu->tSocket was changed but our tSocket
// was not updated, I wrapped this to just use tMenu->tSocket.
// In the long term, it might make sense to move the socket ownage here.
struct SocketAccessWrapper {
	SmartPointer<NetworkSocket> operator[](int i) {
		assert(i >= 0 && i <= 2);
		assert(DeprecatedGUI::tMenu != NULL);
		SmartPointer<NetworkSocket>& sock = DeprecatedGUI::tMenu->tSocket[i];
		assert(sock.get() != NULL);
		return sock;
	}
};
SocketAccessWrapper tSocket;



ServerList::Ptr ServerList::m_instance = ServerList::Ptr((ServerList *)NULL);

ServerListNetwork::ResponseListener::Ptr ServerListNetwork::ResponseListener::null = ServerListNetwork::ResponseListener::Ptr((ServerListNetwork::ResponseListener *)NULL);

/////////////////////
// Serverlist network
//
/////////////////////

#define BUFFER_SIZE 4096

ServerListNetwork::ServerListNetwork()
{
	m_buffer = new char[BUFFER_SIZE];
	m_foo.OpenUnreliable(0);
	m_internet.OpenUnreliable(0);
	m_lan.OpenBroadcast(0);
	m_broadcastListener = ResponseListener::null;

	// Send some data to some random IP
	// Some weird routers seem to drop packets sent from the first newly open socket
	if (m_foo.isOpen())  {
		NetworkAddr a; 
		StringToNetAddr("1.2.3.4:5678", a);

		// For example, if no network is connected, you likely only have 127.* in your routing table.
		if(IsNetAddrAvailable(a)) {
			m_foo.setRemoteAddress(a);
			m_foo.Write("foo");
		}
	}
}

ServerListNetwork::~ServerListNetwork()
{
	Listeners::Writer l(m_listeners);
	l.get().clear();
	if (m_foo.isOpen())
		m_foo.Close();
	if (m_internet.isOpen())
		m_internet.Close();
	if (m_lan.isOpen())
		m_lan.Close();
	if (m_buffer)  {
		delete[] m_buffer;
		m_buffer = NULL;
	}
}

///////////////////
// Sends a packet to the Internet
bool ServerListNetwork::sendInternet(NetworkAddr& addr, const std::string& data, ServerListNetwork::ResponseListener::Ptr& response)
{
	if (!m_internet.isOpen())
		return false;

	m_internet.setRemoteAddress(addr);
	if (m_internet.Write(data) == (int)data.size())  {
		if (response.get())  {
			Listeners::Writer l(m_listeners);
			l.get()[addr] = response;
		}
		return true;
	}
	return false;
}

///////////////////
// Sends a packet to Local Network
bool ServerListNetwork::sendLAN(NetworkAddr& addr, const std::string& data, ServerListNetwork::ResponseListener::Ptr& response)
{
	if (!m_lan.isOpen())
		return false;

	m_lan.setRemoteAddress(addr);
	if (m_lan.Write(data) == (int)data.size())  {
		if (response.get())  {
			Listeners::Writer l(m_listeners);
			l.get()[addr] = response;
		}
		return true;
	}
	return false;
}

///////////////////
// Broadcasts a packet to Local Network
bool ServerListNetwork::broadcastLAN(const std::list<unsigned short>& ports, const std::string& data, ServerListNetwork::ResponseListener::Ptr& response)
{
	if (!m_lan.isOpen())
		return false;

	NetworkAddr broadcastAddr;
	StringToNetAddr("255.255.255.255", broadcastAddr);
	m_broadcastListener = response;
	bool success = true;
	for (std::list<unsigned short>::const_iterator it = ports.begin(); it != ports.end(); ++it)  {
		SetNetAddrPort(broadcastAddr, *it);
		success = success && sendLAN(broadcastAddr, data);
	}
	return success;
}

///////////////////
// Processes the server list network
void ServerListNetwork::process()
{
	NetworkSocket *socks[] = { &m_internet, &m_lan };
	for (uint i = 0; i < sizeof(socks)/sizeof(socks[0]); ++i)  {
		// Read any responses
		std::string response;
		while (true) {
			int ret = socks[i]->Read(m_buffer, BUFFER_SIZE);
			if (ret > 0)
				response.append(m_buffer, ret);
			else
				break;
		}

		if (!response.empty())  {
			NetworkAddr fromAddr = socks[i]->remoteAddress();

			// Fire the corresponding listener
			Listeners::Writer l(m_listeners);
			Listeners::type::iterator it = l.get().find(fromAddr);
			if (it != l.get().end()) {
				if (it->second.get())  {
					if (it->second->onResponse(response))
						l.get().erase(it);
				}

			// No unicast found, put it to broadcast listener if on LAN (and one is set)
			} else if (socks[i] == &m_lan) {
				BroadcastListener::Writer bl(m_broadcastListener);
				if (bl.get().get())  {
					if (bl.get()->onResponse(response))
						bl.get() = ResponseListener::null;
				}
			} else {
				warnings << "Got an unexpected packet from " << NetAddrToString(fromAddr) << ". The packet was dropped." << endl;
			}
		}
	}
}

///////////////////
// Serverlist
//
///////////////////

////////////////////
// Return singleton instance
ServerList::Ptr ServerList::get()
{
	if (!m_instance.get())
		m_instance = Ptr(new ServerList());
	return m_instance;
}

///////////////////
// Initialize the list
ServerList::ServerList() {
    loadList("cfg/svrlist.dat", SLFT_CustomSettings);
	loadList("cfg/favourites.dat", SLFT_Favourites);	
}

ServerList::~ServerList()
{
	shutdown();
}

///////////////////
// Clear the server list
void ServerList::clear(SvrListFilterType filterType)
{
	SvrList::Writer l(psServerList);
    for(SvrList::type::iterator it = l.get().begin(); it != l.get().end();) {
		if((*it)->matches(filterType))
			it = l.get().erase(it);
		else
			++it;
	}
}


///////////////////
// Clear any servers automatically added
void ServerList::clearAuto()
{
	SvrList::Writer l(psServerList);
    for(SvrList::type::iterator it = l.get().begin(); it != l.get().end();)
    {
        if(!(*it)->bManual && (*it)->matches(SLFT_CustomSettings)) 
        	it = l.get().erase(it);
		else
			it++;
    }
}


///////////////////
// Shutdown the server list
void ServerList::shutdown()
{
	SvrList::Writer l(psServerList);
	l.get().clear();
}


///////////////////
// Save the server list
void ServerList::saveList(const std::string& szFilename, SvrListFilterType filterType, SvrListSettingsFilter::Ptr settingsFilter)
{
    FILE *fp = OpenGameFile(szFilename,"wt");
    if( !fp )
        return;
	
	SvrList::Reader l(psServerList);
	for(SvrList::type::const_iterator i = l.get().begin(); i != l.get().end(); i++)
	{
		const SvrList::type::value_type& s = *i;
		if(!s->matches(filterType, settingsFilter)) continue;
		
		int UdpMasterServer = -1;
		for( size_t port = 0; s->bBehindNat && port < s->ports.size() && UdpMasterServer == -1; port++ )
			if( s->ports[port].second >= 0 )
				UdpMasterServer = s->ports[port].second;
		
        fprintf(fp, "%s, %s, %s",s->bManual ? "1" : "0", s->szName.c_str(), s->szAddress.c_str() );
        if( UdpMasterServer != -1 && !s->bManual )
        	fprintf(fp, ", %i", UdpMasterServer );
       	fprintf(fp, "\n" );
	}
	
    fclose(fp);
}

///////////////////
// Load the server list
void ServerList::loadList(const std::string& szFilename, SvrListFilterType filterType)
{
    FILE *fp = OpenGameFile(szFilename,"rt");
    if( !fp )
        return;
	
    // Go through every line
    while( !feof(fp) ) {
		std::string szLine = ReadUntil(fp);
        if( szLine == "" )
            continue;
		
		// explode and copy it
		std::vector<std::string> parsed = explode(szLine,",");
		
        if( parsed.size() >= 3 ) {
			TrimSpaces(parsed[0]);
			TrimSpaces(parsed[1]);
			TrimSpaces(parsed[2]); // Address
			
			int UdpMasterServer = -1;
			if( parsed.size() >= 4 )
				UdpMasterServer = atoi(parsed[3]);
			
			server_t::Ptr svr = addServer(parsed[2], parsed[0] == "1", parsed[1], UdpMasterServer);
			switch(filterType) {
				case SLFT_CustomSettings: break; // nothing
				case SLFT_Favourites:
					svr->isFavourite = true;
					break;
				case SLFT_Lan:
					svr->isLan = true;
					break;
			}
        }
    }
	
    fclose(fp);
}

void ServerList::save() {
	saveList("cfg/svrlist.dat", SLFT_CustomSettings);
	saveList("cfg/favourites.dat", SLFT_Favourites);
}


static void SendBroadcastPing(int port) {
	// Broadcast a ping on the LAN
	CBytestream bs;
	bs.writeInt(-1,4);
	bs.writeString("lx::ping");
	
	NetworkAddr a;
	StringToNetAddr("255.255.255.255", a);
	SetNetAddrPort(a,  port);
	tSocket[SCK_LAN]->setRemoteAddress(a);
	
	// Send the ping
	bs.Send(tSocket[SCK_LAN]);
}

///////////////////
// Send a ping out to the LAN (LAN menu)
void ServerList::pingLAN()
{
	SendBroadcastPing(LX_PORT);
	if(tLXOptions->iNetworkPort != LX_PORT)
		SendBroadcastPing(tLXOptions->iNetworkPort); // try also our own port
}


///////////////////
// Ping a server
void ServerList::pingServer(server_t::Ptr svr)
{
	// If not available, probably the network is not connected right now.
	if(!IsNetAddrAvailable(svr->sAddress)) return;
	
	if( svr->ports.size() == 0 )
	{
		errors << "svr->ports.size() == 0 at " << FILELINE << endl;
		return;
	}
	
	NetworkAddr addr = svr->sAddress;
	//hints << "Pinging server " << tmp << " real addr " << svr->szAddress << " name " << svr->szName << endl;
	svr->lastPingedPort++;
	if( svr->lastPingedPort >= (int)svr->ports.size() || svr->lastPingedPort < 0 )
		svr->lastPingedPort = 0;
	SetNetAddrPort(addr, svr->ports[svr->lastPingedPort].first);
	
	tSocket[SCK_NET]->setRemoteAddress(addr);
	
	CBytestream bs;
	bs.writeInt(-1,4);
	bs.writeString("lx::ping");
	bs.Send(tSocket[SCK_NET]);
	
	svr->bProcessing = true;
	svr->nPings++;
	svr->fLastPing = tLX->currentTime;
}

///////////////////
// Send Wants To Join message
void ServerList::wantsToJoin(const std::string& Nick, server_t::Ptr svr)
{
	tSocket[SCK_NET]->setRemoteAddress(svr->sAddress);
	
	CBytestream bs;
	bs.writeInt(-1,4);
	
	if( svr->bBehindNat )
	{
		UdpMasterserverInfo udpmasterserver = getUdpMasterserverForServer(svr->szAddress);
		if(udpmasterserver) {
			NetworkAddr masterserverAddr;
			SetNetAddrValid(masterserverAddr, false);
			NetworkAddr ignored;

			if( ! GetFromDnsCache( udpmasterserver.name, masterserverAddr, ignored ) ) {
				GetNetAddrFromNameAsync( udpmasterserver.name );
				return;
			}
			
			for( int count = 0; !IsNetAddrValid(masterserverAddr) && count < 5; count++ )
				SDL_Delay(20);
			
			if( !IsNetAddrValid(masterserverAddr) )
				return;
			
			tSocket[SCK_NET]->setRemoteAddress(masterserverAddr);
			bs.writeString("lx::traverse");
			bs.writeString(svr->szAddress);
		}
		else {
			errors << "SvrList_WantsJoin: server " << svr->szName << " (" << svr->szAddress << ") is behind NAT but no udpmasterserver found for it" << endl;			
		}
	}
	
	bs.writeString("lx::wantsjoin");
	bs.writeString(RemoveSpecialChars(Nick));
	bs.Send(tSocket[SCK_NET]);
}

///////////////////
// Get server info
void ServerList::getServerInfo(server_t::Ptr svr)
{
	// Send a getinfo request
	tSocket[SCK_NET]->setRemoteAddress(svr->sAddress);
	
	CBytestream bs;
	bs.writeInt(-1,4);
	
	if( svr->bBehindNat )
	{
		UdpMasterserverInfo udpmasterserver = getUdpMasterserverForServer(svr->szAddress);
		if(udpmasterserver) {
			NetworkAddr masterserverAddr;
			SetNetAddrValid(masterserverAddr, false);
			NetworkAddr ignored;

			if( ! GetFromDnsCache( udpmasterserver.name, masterserverAddr, ignored ) ) {
				GetNetAddrFromNameAsync( udpmasterserver.name );
				return;
			}
			
			for( int count = 0; !IsNetAddrValid(masterserverAddr) && count < 5; count++ )
				SDL_Delay(20);
			
			if( !IsNetAddrValid(masterserverAddr) )
				return;
			
			tSocket[SCK_NET]->setRemoteAddress(masterserverAddr);
			bs.writeString("lx::traverse");
			bs.writeString(svr->szAddress);
		}
		else {
			errors << "SvrList_GetServerInfo: server " << svr->szName << " (" << svr->szAddress << ") is behind NAT but no udpmasterserver found for it" << endl;
		}
	}
	
	bs.writeString("lx::getinfo");
	bs.Send(tSocket[SCK_NET]);
}

///////////////////
// Query a server
void ServerList::queryServer(server_t::Ptr svr)
{
	tSocket[SCK_NET]->setRemoteAddress(svr->sAddress);
	
	CBytestream bs;
	bs.writeInt(-1,4);
	bs.writeString("lx::query");
    bs.writeByte(svr->nQueries);
	bs.Send(tSocket[SCK_NET]);
    svr->fQueryTimes[svr->nQueries] = tLX->currentTime;
	
	svr->bProcessing = true;
	svr->nQueries++;
	svr->fLastQuery = tLX->currentTime;
}


///////////////////
// Refresh the server list (Internet menu)
void ServerList::refreshList()
{
	// Set all the servers to be pinged
	SvrList::Reader l(psServerList);
	for(SvrList::type::const_iterator it = l.get().begin(); it != l.get().end(); it++) 
	{
		refreshServer(*it, false);
	}
	
	// Update the GUI
	Timer("Menu_SvrList_RefreshList ping waiter", null, NULL, PingWait, true).startHeadless();
	
	//Menu_SvrList_UpdateUDPList(); // It adds duplicate server entries
}


///////////////////
// Refresh a single server
void ServerList::refreshServer(server_t::Ptr s, bool updategui)
{
	if (!tLX) return;
	
	// no refresh for NAT servers right now
	if(s->bBehindNat) return;
	
    s->bProcessing = s->bBehindNat ? false : true;
	s->bgotPong = false;
	s->bgotQuery = false;
	s->bIgnore = false;
	s->fLastPing = AbsTime();
	s->fLastQuery = AbsTime();
	s->nPings = 0;
	s->fInitTime = tLX->currentTime;
	s->nQueries = 0;
	s->nPing = s->bBehindNat ? -2 : -3; // unknown yet
	s->bAddrReady = false;
	s->lastPingedPort = 0;
	
	
	if(!StringToNetAddr(s->szAddress, s->sAddress)) {
		hints << "Menu_SvrList_RefreshServer(): cannot parse server addr " << s->szAddress << endl;
		int oldPort = LX_PORT; //GetNetAddrPort(s->sAddress);
		s->sAddress = NetworkAddr(); // assign new addr (needed to avoid problems with possible other still running thread)
		SetNetAddrPort(s->sAddress, oldPort);
		
		SetNetAddrValid(s->sAddress, false);
		size_t f = s->szAddress.rfind(":");
		GetNetAddrFromNameAsync(s->szAddress.substr(0, f));
	} else {
		s->bAddrReady = true;
		size_t f = s->szAddress.find(":");
		if(f != std::string::npos) {
			SetNetAddrPort(s->sAddress, from_string<int>(s->szAddress.substr(f + 1)));
		} else
			SetNetAddrPort(s->sAddress, LX_PORT);
		
		if (updategui)
			Timer("Menu_SvrList_RefreshServer ping waiter", null, NULL, PingWait, true).startHeadless();
	}
	
	if( s->ports.size() == 0 )
	{
		s->ports.push_back(std::make_pair((int)GetNetAddrPort(s->sAddress), -1));
	}
}


void ServerList::mergeWithNewInfo(server_t::Ptr found, const std::string& address, const std::string & name, int udpMasterserverIndex) {
	NetworkAddr ad;
	std::string tmp_address = address;
    TrimSpaces(tmp_address);
    int port = -1;
    if(StringToNetAddr(tmp_address, ad)) 
    {
    	port = GetNetAddrPort(ad);
    	if( port == 0 )
    		port = LX_PORT;
    }

	if( found->szName == "Untitled" )
		found->szName = name;
	//hints << "Menu_SvrList_AddServer(): merging duplicate " << found->szName << " " << found->szAddress << endl;
	
	if(port != -1 && port != 0) {
		for( size_t i = 0; i < found->ports.size(); i++ )
			if( found->ports[i].first == port )
				return;
		
		found->ports.push_back( std::make_pair( port, udpMasterserverIndex ) );	
	}
}

///////////////////
// Add a server onto the list (for list and manually)
server_t::Ptr ServerList::addServer(const std::string& address, bool bManual, const std::string & name, int udpMasterserverIndex)
{
    // Check if the server is already in the list
    // If it is, don't bother adding it
	NetworkAddr ad;
	std::string tmp_address = address;
    TrimSpaces(tmp_address);
    int port = -1;
    if(StringToNetAddr(tmp_address, ad)) 
    {
    	port = GetNetAddrPort(ad);
    	if( port == 0 )
    		port = LX_PORT;
    }
	
	server_t::Ptr found = findServerStr(tmp_address, name);
    if( found && port != -1 && port != 0 )
    {
		mergeWithNewInfo(found, tmp_address, name, udpMasterserverIndex);
		return found;
    }
	
    // Didn't find one, so create it
	server_t::Ptr svr(new server_t());
	
	// Fill in the details
    svr->bManual = bManual;
	svr->szAddress = tmp_address;
	ResetNetAddr(svr->sAddress);
	
	refreshServer(svr, bManual);
	
	if( svr->ports.size() > 0 )
		svr->ports[0].second = udpMasterserverIndex;
	
	// Default game details
	svr->szName = name;
	TrimSpaces(svr->szName);
	svr->nMaxPlayers = 0;
	svr->nNumPlayers = 0;
	svr->nState = 0;
	svr->nPing = -3; // Put it at the end of server list, after NAT servers
	if( udpMasterserverIndex >= 0 )
	{
		svr->bBehindNat = true;
		svr->nPing = -2;
	}
	else
		svr->bBehindNat = false;
	
	SvrList::Writer l(psServerList);
    l.get().push_back(svr);
	return svr;
}


///////////////////
// Remove a server from the server list
void ServerList::removeServer(const std::string& szAddress)
{
	SvrList::Writer l(psServerList);
	for(SvrList::type::iterator it = l.get().begin(); it != l.get().end(); )
		if( (*it)->szAddress == szAddress )
			it = l.get().erase( it );
		else
			it++;
}


///////////////////
// Find a server based on a string address
server_t::Ptr ServerList::findServerStr(const std::string& szAddress, const std::string & name)
{
	NetworkAddr addr;
	if( ! StringToNetAddr(szAddress, addr) )
		return server_t::Ptr((server_t*)NULL);
    
    return findServer(addr, name);
}


///////////////////
// Fill a listview box with the server list
void ServerList::fillList(DeprecatedGUI::CListview *lv, SvrListFilterType filterType, SvrListSettingsFilter::Ptr settingsFilter)
{
	if (!lv)
		return;
	
	std::string		addr;
	static const std::string states[] = {"Open", "Loading", "Playing", "Open/Loading", "Open/Playing"};
	
    // Store the ID of the currently selected item
    int curID = lv->getSelectedID();
	
	lv->SaveScrollbarPos();
	lv->Clear();
	
	SvrList::type serverList;
	{
		SvrList::Reader l(psServerList);
		serverList = l.get();
	}
	for(SvrList::type::const_iterator i = serverList.begin(); i != serverList.end(); i++)
	{
		const SvrList::type::value_type& s = *i;
		if(!s->matches(filterType, settingsFilter)) continue;
		
		bool processing = s->bProcessing && !getUdpMasterserverForServer( s->szAddress );
		
		// Ping Image
		int num = 3;
		if(s->nPing < 700)  num = 2;
		if(s->nPing < 400)  num = 1;
		if(s->nPing < 200)  num = 0;
		
		if(s->bIgnore || processing)
			num = 3;
		
		if(s->nPing == -2)	num = 4; // Server behind a NAT
		
		// Address
		//GetRemoteNetAddr(tSocket, &s->sAddress);
		//NetAddrToString(&s->sAddress, addr);
		
		// show port if special
		addr = s->szAddress;
		size_t p = addr.rfind(':');
		if(p != std::string::npos) {
			std::string sPort = addr.substr(p + 1);
			addr.erase(p);
			if(from_string<int>(sPort) != LX_PORT)
				addr += ":" + sPort;
		}
		
		// State
		int state = 0;
		if(s->nState >= 0 && s->nState < 3)
			state = s->nState;
		if( state != 0 && s->bAllowConnectDuringGame && s->nNumPlayers < s->nMaxPlayers )
			state += 2;
		
		// Colour
		Color colour = tLX->clListView;
		if(processing)
			colour = tLX->clDisabled;
		
		
		// Add the server to the list
		lv->AddItem(s->szAddress, 0, colour);
		lv->AddSubitem(DeprecatedGUI::LVS_IMAGE, itoa(num,10), DeprecatedGUI::tMenu->bmpConnectionSpeeds[num], NULL);
		lv->AddSubitem(DeprecatedGUI::LVS_TEXT, s->szName, (DynDrawIntf*)NULL, NULL);
        if(processing) {
			if(IsNetAddrValid(s->sAddress))
				lv->AddSubitem(DeprecatedGUI::LVS_TEXT, "Querying...", (DynDrawIntf*)NULL, NULL);
			else
				lv->AddSubitem(DeprecatedGUI::LVS_TEXT, "Lookup...", (DynDrawIntf*)NULL, NULL);
        } else if( num == 3 )
            lv->AddSubitem(DeprecatedGUI::LVS_TEXT, "Down", (DynDrawIntf*)NULL, NULL);
        else
		    lv->AddSubitem(DeprecatedGUI::LVS_TEXT, states[state], (DynDrawIntf*)NULL, NULL);
		
		bool unknownData = ( s->bProcessing || num == 3 ) && 
			!getUdpMasterserverForServer( s->szAddress );
		
		// Players
		lv->AddSubitem(DeprecatedGUI::LVS_TEXT,
					   unknownData ? "?" : (itoa(s->nNumPlayers,10)+"/"+itoa(s->nMaxPlayers,10)),
					   (DynDrawIntf*)NULL, NULL);
		
		if (s->nPing <= -2) // Server behind a NAT or not queried, it will add spaces if s->nPing == -3 so not queried servers will be below NAT ones
			lv->AddSubitem(DeprecatedGUI::LVS_TEXT, "N/A" + std::string(' ', -2 - s->nPing), (DynDrawIntf*)NULL, NULL);
		else
			lv->AddSubitem(DeprecatedGUI::LVS_TEXT, unknownData ? "∞" : itoa(s->nPing,10), (DynDrawIntf*)NULL, NULL); // TODO: the infinity symbol isn't shown correctly
		
		// Country
		if (tLXOptions->bUseIpToCountry) {
			IpInfo inf = tIpToCountryDB->GetInfoAboutIP(addr);
			if( tLXOptions->bShowCountryFlags )
			{
				SmartPointer<SDL_Surface> flag = tIpToCountryDB->GetCountryFlag(inf.countryCode);
				if (flag.get())
					lv->AddSubitem(DeprecatedGUI::LVS_IMAGE, "", flag, NULL, DeprecatedGUI::VALIGN_MIDDLE, inf.countryName);
				else
					lv->AddSubitem(DeprecatedGUI::LVS_TEXT, inf.countryCode, (DynDrawIntf*)NULL, NULL);
			}
			else
			{
				lv->AddSubitem(DeprecatedGUI::LVS_TEXT, inf.countryName, (DynDrawIntf*)NULL, NULL);
			}
		}
		
		// Address
		lv->AddSubitem(DeprecatedGUI::LVS_TEXT, addr, (DynDrawIntf*)NULL, NULL);
	}
	
	lv->ReSort();
    lv->setSelectedID(curID);
	lv->RestoreScrollbarPos();
}

static bool bUpdateFromUdpThread = false;
///////////////////
// Process the network connection
// Returns true if a server in the list was added/modified
bool ServerList::process()
{
	CBytestream		bs;
	bool			update = false;
	
	
	// Process any packets on the net socket
	while(bs.Read(tSocket[SCK_NET])) {
		
		if( parsePacket(&bs, tSocket[SCK_NET], false) )
			update = true;
		
	}
	
	// Process any packets on the LAN socket
	while(bs.Read(tSocket[SCK_LAN])) {
		
		if( parsePacket(&bs, tSocket[SCK_LAN], true) )
			update = true;
	}
	
	if( bUpdateFromUdpThread )
	{
		bUpdateFromUdpThread = false;
		update = true;
	}
	
	bool repaint = false;
	
	
	// Ping or Query any servers in the list that need it
	SvrList::Reader l(psServerList);
	for(SvrList::type::const_iterator i = l.get().begin(); i != l.get().end(); i++)
	{
		const SvrList::type::value_type& s = *i;
		
		// Ignore this server? (timed out)
		if(s->bIgnore)
			continue;
		
		if(!IsNetAddrValid(s->sAddress)) {
			if(tLX->currentTime - s->fInitTime >= DNS_TIMEOUT) {
				s->bIgnore = true; // timeout
				update = true;
			}
			continue;
		} else {
			if(!s->bAddrReady) {
				s->bAddrReady = true;
				update = true;
				
				size_t f = s->szAddress.find(":");
				if(f != std::string::npos) {
					SetNetAddrPort(s->sAddress, from_string<int>(s->szAddress.substr(f + 1)));
				} else
					SetNetAddrPort(s->sAddress, LX_PORT);
				
			}
		}
		
		// Need a pingin'?
		if(!s->bgotPong) {
			if(tLX->currentTime - s->fLastPing > (float)PingWait / 1000.0f) {
				
				if(s->nPings >= MaxPings) {
					s->bIgnore = true;
					
					update = true;
				}
				else  {
					// Ping the server
					pingServer(s);
					repaint = true;
				}
			}
		}
		
		// Need a querying?
		if(s->bgotPong && !s->bgotQuery) {
			if(tLX->currentTime - s->fLastQuery > (float)QueryWait / 1000.0f) {
				
				if(s->nQueries >= MaxQueries) {
					s->bIgnore = true;
					
					update = true;
				}
				else  {
					// Query the server
					queryServer(s);
					repaint = true;
				}
			}
		}
		
		// If we are ignoring this server now, set it to not processing
		if(s->bIgnore) {
			s->bProcessing = false;
			update = true;
		}
		
	}
	
	// Make sure the list repaints when the ping/query is received
	if (repaint)
		Timer("Menu_SvrList_Process ping waiter", null, NULL, PingWait + 100, true).startHeadless();
	
	return update;
}


///////////////////
// Parse a packet
// Returns true if we should update the list
bool ServerList::parsePacket(CBytestream *bs, const SmartPointer<NetworkSocket>& sock, bool isLan)
{
	NetworkAddr		adrFrom;
	bool			update = false;
	std::string cmd,buf;
	
	// Check for connectionless packet header
	if(bs->readInt(4) == -1) {
		cmd = bs->readString();
		
		adrFrom = sock->remoteAddress();
		
		// Check for a pong
		if(cmd == "lx::pong") {
			
			// Look the the list and find which server returned the ping
			server_t::Ptr svr = findServer(adrFrom);
			if( svr ) {
				
				// It pinged, so fill in the ping info so it will now be queried
				svr->bgotPong = true;
				svr->nQueries = 0;
				svr->bBehindNat = false;
				svr->lastPingedPort = 0;
				SetNetAddrPort(svr->sAddress, GetNetAddrPort(adrFrom));
				NetAddrToString(svr->sAddress, svr->szAddress);
				svr->ports.clear();
				svr->ports.push_back( std::make_pair( (int)GetNetAddrPort(adrFrom), -1 ) );
				
			} else {
				
				// If we didn't ping this server directly (eg, subnet), add the server to the list
				NetAddrToString( adrFrom, buf );
				svr = addServer(buf, false);
				
				if( svr ) {
					
					// Only update the list if this is the first ping
					if(!svr->bgotPong)
						update = true;
					
					// Set it the ponged
					svr->bgotPong = true;
					svr->nQueries = 0;
					svr->isLan = true;
					
					//Menu_SvrList_RemoveDuplicateNATServers(svr); // We don't know the name of server yet
				}
			}
		}
		
		// Check for a query return
		else if(cmd == "lx::queryreturn") {
			
			// Look the the list and find which server returned the ping
			server_t::Ptr svr = findServer(adrFrom);
			if( svr ) {
				
				// Only update the list if this is the first query
				if(!svr->bgotQuery)
					update = true;
				
				svr->bgotQuery = true;
				svr->bBehindNat = false;
				parseQuery(svr, bs);
				
			}
			
			// If we didn't query this server, then we should ignore it
		}
		
		else if(cmd == "lx::serverlist2") // This should not happen, we have another thread for polling UDP servers
		{
			parseUdpServerlist(bs, 0, false);
			update = true;
		}
		
		else if(cmd == "lx::serverlist3") // This should not happen, we have another thread for polling UDP servers
		{
			parseUdpServerlist(bs, 0, true);
			update = true;
		}
	}
	
	return update;
}


///////////////////
// Find a server from the list by address
server_t::Ptr ServerList::findServer(const NetworkAddr& addr, const std::string & name)
{
	SvrList::Reader l(psServerList);
	for(SvrList::type::const_iterator s = l.get().begin(); s != l.get().end(); s++)
	{
		if( AreNetAddrEqual( addr, (*s)->sAddress ) )
			return *s;
	}
	
    NetworkAddr addr1 = addr;
    SetNetAddrPort(addr1, LX_PORT);
	
	for(SvrList::type::const_iterator i = l.get().begin(); i != l.get().end(); i++)
	{
		const SvrList::type::value_type& s = *i;
		
		// Check if any port number match from the server entry
		NetworkAddr addr2 = s->sAddress;
		for( size_t i = 0; i < s->ports.size(); i++ )
		{
			SetNetAddrPort(addr2, s->ports[i].first);
			if( AreNetAddrEqual( addr, addr2 ) )
				return s;
		}
		
		// Check if IP without port and name match
		SetNetAddrPort(addr2, LX_PORT);
		if( AreNetAddrEqual( addr1, addr2 ) && name == s->szName && name != "Untitled" )
			return s;
	}
	
	/*
	 for(std::list<server_t>::iterator s = psServerList.begin(); s != psServerList.end(); s++)
	 {
	 // Check if just an IP without port match
	 NetworkAddr addr2 = s->sAddress;
	 SetNetAddrPort(addr2, LX_PORT);
	 if( AreNetAddrEqual( addr1, addr2 ) )
	 return &(*s);
	 }
	 */
	
	// None found
	return server_t::Ptr((server_t*)NULL);
}


///////////////////
// Parse the server query return packet
void ServerList::parseQuery(server_t::Ptr svr, CBytestream *bs)
{
	// TODO: move this net protocol stuff out here
	
	// Don't update the name in favourites
	std::string buf = Utf8String(bs->readString());
	if(!svr->isFavourite)
		svr->szName = buf;
	TrimSpaces(svr->szName);
	//hints << "Menu_SvrList_ParseQuery(): " << svr->szName << " " << svr->szAddress << endl;
	svr->nNumPlayers = bs->readByte();
	svr->nMaxPlayers = bs->readByte();
	svr->nState = bs->readByte();
    int num = bs->readByte();
	svr->bProcessing = false;
	svr->bAllowConnectDuringGame = false;
	svr->tVersion.reset();
	
    if(num < 0 || num >= MAX_QUERIES-1)
        num=0;
	
	svr->nPing = (int)( (tLX->currentTime - svr->fQueryTimes[num]).milliseconds() );
	
	if(svr->nPing < 0)
		svr->nPing = 999;
    if(svr->nPing > 999)
        svr->nPing = 999;
	
	if( !bs->isPosAtEnd() )
	{
		// Beta8+
		svr->tVersion.setByString( bs->readString(64) );
		svr->bAllowConnectDuringGame = bs->readBool();
	}
	
	// We got server name in a query. let's remove servers with the same name and IP, which we got from UDP masterserver
	SvrList::Writer l(psServerList);
	for(SvrList::type::iterator it = l.get().begin(); it != l.get().end(); )
	{
		NetworkAddr addr1 = (*it)->sAddress;
		SetNetAddrPort(addr1, LX_PORT);
		NetworkAddr addr2 = svr->sAddress;
		SetNetAddrPort(addr2, LX_PORT);
		if( (*it)->szName == svr->szName && AreNetAddrEqual(addr1, addr2) && svr != *it )
		{
			//Duplicate server - delete it
			//hints << "Menu_SvrList_ParseQuery(): removing duplicate " << it->szName << " " << it->szAddress << endl;
			it = l.get().erase(it);
		}
		else
			++it;
	}
}

/////////////////
// Performs a task on each server in the server list
void ServerList::each(Action& act)
{
	SvrList::Writer l(psServerList);
	for (SvrList::type::iterator svr = l.get().begin(); svr != l.get().end(); ++svr)  {
		SvrList::type::value_type& s = *svr;
		if (!act.handle(s))
			break;
	}
}

void ServerList::eachConst(Action& act) const
{
	SvrList::Reader l(const_cast<SvrList&>(psServerList));  // TODO: the const_cast here is ugly, Reader should take const reference
	for (SvrList::type::const_iterator svr = l.get().begin(); svr != l.get().end(); ++svr)  {
		const SvrList::type::value_type& s = *svr;
		if (!act.handleConst(s))
			break;
	}
}


/*************************
 *
 * UDP server list
 *
 ************************/

static std::list<std::string> getUdpMasterServerList() {
	// Open the masterservers file
	FILE *fp1 = OpenGameFile("cfg/udpmasterservers.txt", "rt");
	if(!fp1)  {
		warnings << "could not open udpmasterservers.txt file, NAT traversal will be inaccessible" << endl;
		return std::list<std::string>();
	}
	
	std::list<std::string> tUdpMasterServers;
	
	// Get the list of servers
	while( !feof(fp1) ) {
		std::string szLine = ReadUntil(fp1);
		TrimSpaces(szLine);
		
		if( szLine.length() == 0 )
			continue;
		
		tUdpMasterServers.push_back(szLine);
	}
	fclose(fp1);
	
	return tUdpMasterServers;
}


struct UdpUpdater : Task {
	std::string m_statusTxt;
	ServerList *m_list;

	UdpUpdater(ServerList *l) : m_list(l) { name = "udp serverlist updater"; }
	Result handle() { return SvrList_UpdaterFunc(); }
	std::string statusText() {
		Mutex::ScopedLock lock(*mutex);
		return m_statusTxt;
	}
	void setStatusText(const std::string& t) {
		Mutex::ScopedLock lock(*mutex);
		m_statusTxt = t;
	}
	Result SvrList_UpdaterFunc();
};

Result UdpUpdater::SvrList_UpdaterFunc()
{
	std::list<std::string> tUdpMasterServers = getUdpMasterServerList();
	if(breakSignal) return "break";
	
	// Open socket for networking
	NetworkSocket sock;
	if (!sock.OpenUnreliable(0)) 
		return "failed to open unreliable socket";
	
	// Get serverlist from all the servers in the file
	int UdpServerIndex = 0;
	for (std::list<std::string>::iterator it = tUdpMasterServers.begin(); it != tUdpMasterServers.end(); ++it, ++UdpServerIndex)  
	{
		std::string& server = *it;
		// Show the name exactly as written in udpmasterservers.txt (before the
		// default port gets appended below), so the status is easy to debug.
		setStatusText("Querying server " + server + " (" + itoa(UdpServerIndex + 1) + "/" + itoa((int)tUdpMasterServers.size()) + ")");
		NetworkAddr addr;
		NetworkAddr addr6;
		if (server.rfind(':') == std::string::npos)
			server += ":23450";  // Default port
		
		// Split to domain and port
		std::string domain = server.substr(0, server.rfind(':'));
		int port = atoi(server.substr(server.rfind(':') + 1));
		
		// Resolve the address
		if (!GetFromDnsCache(domain, addr, addr6)) {
			GetNetAddrFromNameAsync(domain);
			continue;
		}
		
		AbsTime start = GetTime();
		while (GetTime() - start <= 5.0f) {
			if(breakSignal) return "break";
			SDL_Delay(40);
			if(IsNetAddrValid(addr)) 
				break;
		}
		
		if( !IsNetAddrValid(addr) && !IsNetAddrValid(addr6) )
		{
			notes << "UDP masterserver failed: cannot resolve domain name " << domain << endl;
			continue;
		}
		
		// Setup the socket
		SetNetAddrPort(addr, port);
		SetNetAddrPort(addr6, port);
		for (int af = 0; af < 2; af++, addr = addr6)
		{
			if (!IsNetAddrValid(addr))
				continue;

			sock.setRemoteAddress(addr);

			// Send the getserverlist packet
			CBytestream bs;
			bs.writeInt(-1, 4);
			bs.writeString(af ? "lx::getserverlist3" : "lx::getserverlist2");
			if(!bs.Send(&sock)) {
				warnings << "error while sending data to UDP masterserver " << server << " " << NetAddrToString(addr) << ", ignoring" << endl;
				continue;
			}
			bs.Clear();

			notes << "Sent " << (af ? "lx::getserverlist3" : "lx::getserverlist2") << " to " << server << endl;

			// Wait for the reply
			AbsTime timeoutTime = GetTime() + 5.0f;
			bool firstPacket = true;
			while( true ) {
				while (GetTime() <= timeoutTime)  {
					if(breakSignal) return "break";
					
					SDL_Delay(40); // TODO: do it event based
					
					// Got a reply?
					if (bs.Read(&sock))  {
						//notes << "Got a reply from " << server << endl;
						break;
					}				
				}

				// Parse the reply
				std::string response;
				if (bs.GetLength() && bs.readInt(4) == -1 && (response = bs.readString()).find("lx::serverlist") == 0) {
					notes << "Got " << response << " from " << server << endl;
					std::string errStr = m_list->parseUdpServerlist(&bs, UdpServerIndex, response == "lx::serverlist3");
					if(errStr != "")
						errors << "Error reading data from UDP masterserver " << server << ": " << errStr << endl;
					timeoutTime = GetTime() + 0.5f;	// Check for another packet
					firstPacket = false;
				} else  {
					if( firstPacket )
						warnings << "Error getting serverlist from " << server << endl;
					break;
				}
			}
		}
	}
	
	DeprecatedGUI::Menu_Net_ServerList_Refresher();
	return true;
}

void ServerList::updateUDPList()
{
	taskManager->start(new UdpUpdater(this), TaskManager::QT_QueueToSameTypeAndBreakCurrent);
}

std::string ServerList::parseUdpServerlist(CBytestream *bs, int UdpMasterserverIndex, bool replyV3)
{
	// Look the the list and find which server returned the ping
	int amount = bs->readByte();
	//notes << "Menu_SvrList_ParseUdpServerlist " << amount << endl;
	for( int f=0; f<amount; f++ )
	{
		if(bs->isPosAtEnd())
			return "Package crippled";

		bUpdateFromUdpThread = true;

		std::string addr = bs->readString();
		std::string name = bs->readString();
		TrimSpaces(name);
		TrimSpaces(addr);
		//notes << "Menu_SvrList_ParseUdpServerlist(): " << name << " " << addr << endl;
		int players = bs->readByte();
		int maxplayers = bs->readByte();
		int state = bs->readByte();
		Version version = bs->readString(64);
		bool allowConnectDuringGame = bs->readBool();
		std::string v4addr;
		if( replyV3 )
			v4addr = bs->readString();
		TrimSpaces(v4addr);
		
		// UDP server info is updated once per 40 seconds, so if we have more recent entry ignore it
		server_t::Ptr svr = findServerStr(addr, name);
		if( svr ) {
			//hints << "Menu_SvrList_ParseUdpServerlist(): got duplicate " << name << " " << addr << " pong " << svr->bgotPong << " query " << svr->bgotQuery << endl;
			if( !svr->bgotPong ) {
				mergeWithNewInfo(svr, addr, name, UdpMasterserverIndex);
			}
		} else {
			svr = addServer(addr, false, name, UdpMasterserverIndex);
		}
		
		svr->nNumPlayers = players;
		svr->nMaxPlayers = maxplayers;
		svr->nState = state;
		svr->nPing = -2;
		svr->nQueries = 0;
		svr->bgotPong = false;
		svr->bgotQuery = false;
		svr->bProcessing = false;
		svr->tVersion = version;
		svr->bAllowConnectDuringGame = allowConnectDuringGame;
		svr->bBehindNat = true;

		if( v4addr != "" ) {
			svr = findServerStr(v4addr, name);
			if( svr ) {
				if( !svr->bgotPong ) {
					mergeWithNewInfo(svr, v4addr, name, UdpMasterserverIndex);
				}
			} else {
				svr = addServer(v4addr, false, name, UdpMasterserverIndex);
			}
			
			svr->nNumPlayers = players;
			svr->nMaxPlayers = maxplayers;
			svr->nState = state;
			svr->nPing = -2;
			svr->nQueries = 0;
			svr->bgotPong = false;
			svr->bgotQuery = false;
			svr->bProcessing = false;
			svr->tVersion = version;
			svr->bAllowConnectDuringGame = allowConnectDuringGame;
			svr->bBehindNat = true;
		}
	}
	
	return "";
}


///////////////////
// Add a favourite server
void ServerList::addFavourite(const std::string& szName, const std::string& szAddress)
{
	{
		server_t::Ptr svr = addServer(szAddress, true, szName, getUdpMasterserverForServer(szAddress).index);
		if(svr)
			svr->isFavourite = true;
	}
	
    FILE *fp = OpenGameFile("cfg/favourites.dat","a");  // We're appending
    if( !fp )  {
        fp = OpenGameFile("cfg/favourites.dat","wb");  // Try to create the file
		if (!fp)
			return;
	}
	
	// Append the server
    fprintf(fp,"%s, %s, %s\n","1", szName.c_str(), szAddress.c_str());
	
    fclose(fp);
}


UdpMasterserverInfo ServerList::getUdpMasterserverForServer(const std::string & addr)
{
	server_t::Ptr svr = findServerStr(addr);
	if( !svr )
		return UdpMasterserverInfo();
	if( !svr->bBehindNat )
		return UdpMasterserverInfo();
	
	std::list<std::string> tUdpMasterServers = getUdpMasterServerList();
	for( size_t port = 0; port < svr->ports.size(); port++ )
	{
		if( svr->ports[port].second < 0 )
			continue;
		int idx = 0;
		for( std::list<std::string>::iterator it = tUdpMasterServers.begin(); it != tUdpMasterServers.end(); ++it, ++idx )
			if( idx == svr->ports[port].second )
				return UdpMasterserverInfo(*it, idx);
	}
	
	return UdpMasterserverInfo();
}


struct ServerListUpdater : Task {
	std::string m_statusTxt;
	ServerList *m_list;
	ServerListUpdater(ServerList *l) : m_statusTxt("Updating server list ..."), m_list(l) { name = "server list updater"; }
	
	Result handle() {
		updateServerList();
		return true;
	}
	
	std::string statusText() {
		Mutex::ScopedLock lock(*mutex);
		return m_statusTxt;
	}
	void setStatusText(const std::string& t) {
		Mutex::ScopedLock lock(*mutex);
		m_statusTxt = t;
	}
	void updateServerList();
};

void ServerListUpdater::updateServerList() {
	int			http_result = 0;
	std::string szLine;
	
	// Clear the server list
	m_list->clearAuto();
	
	// UDP list
	m_list->updateUDPList();
	
	//
	// Get the number of master servers for a progress bar
	//
	int SvrCount = 0;
	int CurServer = 0;
	bool SentRequest = false;
	FILE *fp = OpenGameFile("cfg/masterservers.txt","rt");
	if( !fp )  {
		errors << "Cannot update list because there is no masterservers.txt file available\n" << endl;
		return;
	}
	
	// TODO: i don't understand it, why are we doing it so complicated here, why not just save it in a list?
	while( !feof(fp) ) {
		szLine = ReadUntil(fp);
		TrimSpaces(szLine);
		
		if( szLine.length() > 0 && szLine[0] != '#' )
			SvrCount++;
	}
	
	// Back to the start
	fseek(fp, 0, SEEK_SET);
	
	
	
	CHttp http;
	
	while(true) {
		// Do the HTTP requests of the master servers
		if( !SentRequest ) {
			
			// Have we gone through all the servers?
			if( CurServer >= SvrCount )
				break;
			
			// Get the next server in the list
			while( !feof(fp) ) {
				szLine = ReadUntil(fp);
				TrimSpaces(szLine);
				
				if( szLine.length() > 0 && szLine[0] != '#' ) {

					// Send the request
					//notes << "Getting serverlist from " + szLine + "..." << endl;
					// Show the master server name exactly as in masterservers.txt.
					setStatusText("Querying server " + szLine + " (" + itoa(CurServer + 1) + "/" + itoa(SvrCount) + ")");
					http.RequestData(szLine + LX_SVRLIST, tLXOptions->sHttpProxy);
					SentRequest = true;
					
					break;
				}
			}
		} else { // Process the http request
			http_result = http.ProcessRequest();
			
			// Parse the list if the request was successful
			if (http_result == HTTP_PROC_FINISHED) {
				m_list->HTTPParseList(http);
				
				// Other master servers could have more server so we process them anyway
				SentRequest = false;
				CurServer++;
				DeprecatedGUI::Menu_Net_ServerList_Refresher();
				
			} else if (http_result == HTTP_PROC_ERROR)  {
				if (http.GetError().iError != HTTP_NO_ERROR)
					errors << "HTTP ERROR: " << http.GetUrl() << " : " << http.GetError().sErrorMsg << endl;
				// Jump to next server
				SentRequest = false;
				CurServer++;
				http.CancelProcessing();
			}
		}
		
		SDL_Delay(10);
		if(breakSignal) break;
	}
	
	fclose(fp);
}


void ServerList::updateList() {
	taskManager->start(new ServerListUpdater(this), TaskManager::QT_QueueToSameTypeAndBreakCurrent);
}


/*************************
 *
 * Master server list files auto-update
 *
 * The lists of master servers themselves (cfg/masterservers.txt and
 * cfg/udpmasterservers.txt) are kept up to date by downloading the latest
 * versions from the OpenLieroX serverlist repository.
 *
 ************************/

struct MasterServerListFile {
	const char* url;
	const char* localFile;
};

static const MasterServerListFile masterServerListFiles[] = {
	{ "https://openlierox.github.io/serverlist/masterservers.txt", "cfg/masterservers.txt" },
	{ "https://openlierox.github.io/serverlist/udpmasterservers.txt", "cfg/udpmasterservers.txt" },
};

struct MasterServerListUpdater : Task {
	MasterServerListUpdater() { name = "masterserver list files updater"; }
	Result handle();
	// Shown in the menu status bar while the master server files download, before
	// ServerListUpdater takes over with its own "Updating server list" status.
	std::string statusText() { return "Updating server list..."; }
	void updateFile(const MasterServerListFile& f);
};

void MasterServerListUpdater::updateFile(const MasterServerListFile& f)
{
	// Show the full destination path so it's clear where the downloaded file ends up.
	notes << "Updating " << f.url << " to:\n" << GetWriteFullFileName(f.localFile) << endl;

	CHttp http;
	http.RequestData(f.url, tLXOptions->sHttpProxy);

	const AbsTime startTime = GetTime();
	HttpProc_t result = HTTP_PROC_PROCESSING;
	while(result == HTTP_PROC_PROCESSING) {
		if(breakSignal) {
			http.CancelProcessing();
			return;
		}
		if(GetTime() - startTime > 30.0f) {
			// Bound the wait so a stalled connection can't hang the task
			// (and block the next file) without leaving a trace.
			http.CancelProcessing();
			warnings << "Could not update " << f.localFile << ": download timed out" << endl;
			return;
		}
		SDL_Delay(20);
		result = http.ProcessRequest();
	}

	if(result == HTTP_PROC_ERROR) {
		warnings << "Could not update " << f.localFile << ": " << http.GetError().sErrorMsg << endl;
		return;
	}

	// Only accept a successful (2xx) HTTP response, so an error page (e.g. a
	// "404 Not Found" body) never overwrites a working file.
	const long status = http.GetHTTPStatusCode();
	if(status < 200 || status >= 300) {
		warnings << "Could not update " << f.localFile << ": server returned HTTP status " << status << endl;
		return;
	}

	const std::string& data = http.GetData();
	if(data.empty()) {
		// Don't overwrite a working file with an empty download.
		warnings << "Could not update " << f.localFile << ": downloaded file is empty" << endl;
		return;
	}

	FILE* fp = OpenGameFile(f.localFile, "wb");
	if(!fp) {
		warnings << "Could not update " << f.localFile << ": cannot open the file for writing" << endl;
		return;
	}
	fwrite(data.data(), 1, data.size(), fp);
	fclose(fp);
}

Result MasterServerListUpdater::handle()
{
	for(size_t i = 0; i < sizeof(masterServerListFiles) / sizeof(masterServerListFiles[0]); ++i) {
		if(breakSignal) return "break";
		updateFile(masterServerListFiles[i]);
	}
	if(breakSignal) return "break";

	// The master server lists are now up to date, so refresh the server list
	// shown in the UI. This must happen after the downloads above so that it
	// reads the freshly downloaded files, not the previous ones.
	ServerList::get()->updateList();
	return true;
}

void DownloadMasterServerListFiles()
{
	// Logged here on the main thread, so there is always a trace when Netplay
	// is clicked - even if the background task below never gets to run.
	notes << "Updating the master server list files..." << endl;
	taskManager->start(new MasterServerListUpdater(), TaskManager::QT_QueueToSameTypeAndBreakCurrent);
}


///////////////////
// Parse the downloaded server list
void ServerList::HTTPParseList(CHttp &http)
{
	const std::string& content = http.GetData();
	
	std::string addr, ptr;
	
	std::string::const_iterator it = content.begin();
	size_t i = 0;
	size_t startpos = 0;	
	for(; it != content.end(); it++, i++) {	
		if(*it != '\n') continue;
		std::vector<std::string> tokens = explode(content.substr(startpos, i-startpos), ",");
		startpos = i+1;
		
		// we need at least 2 items
		if(tokens.size() < 2) continue;
		
		addr = tokens[0];
		ptr = tokens[1];
		
		TrimSpaces(addr);
		TrimSpaces(ptr);
		
		// If the address, or port does NOT have quotes around it, the line must be mangled and cannot be used		
		if(addr.size() <= 2 || ptr.size() <= 2) continue;
		if(addr[0] != '\"' || ptr[0] != '\"') continue;
		if(addr[addr.size()-1] != '\"' || ptr[ptr.size()-1] != '\"') continue;
		
		StripQuotes(addr);
		StripQuotes(ptr);
		
		// Create the server address
		addServer(addr + ":" + ptr, false);
	}
	
	// Update the GUI
	Timer("Menu_Net_NETParseList ping waiter", null, NULL, PingWait, true).startHeadless();
}

bool ServerList::isProcessing() {
	if(taskManager->haveTaskOfType(typeid(ServerListUpdater)))
		return true;
	
	SvrList::type serverList;
	{
		SvrList::Reader l(psServerList);
		serverList = l.get();
	}
	for(SvrList::type::const_iterator i = serverList.begin(); i != serverList.end(); i++)
	{
		const SvrList::type::value_type& s = *i;		
		bool processing = s->bProcessing && !getUdpMasterserverForServer( s->szAddress );
		if(processing) return true;
	}
	
	// seems that we have them all ready
	return false;
}


bool SvrListSettingsFilter::loadFromFile(const std::string& cfgfile) {
	// TODO ...
	return true;
}

ScriptVar_t server_t::getSetting(const std::string& name) {
	{
		SettingsMap::iterator f = settings.find(name);
		if(f != settings.end())
			return f->second;
	}
	
	{
		Feature* f = featureByName(name);
		if(f)
			return f->unsetValue;
	}
	
	warnings << "server_t::getSetting: setting '" << name << "' not found" << endl;
	return ScriptVar_t();
}

bool server_t::matches(SvrListFilterType filterType, SvrListSettingsFilter::Ptr settingsFilter) {
	switch(filterType) {
		case SLFT_CustomSettings:
			// TODO ...
			return true;
		case SLFT_Favourites:
			return isFavourite;
		case SLFT_Lan:
			return isLan;
	}
	
	return true;
}

