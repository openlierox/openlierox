/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Common networking routines to help us
// Created 18/12/02
// Jason Boettcher

#include <memory.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <curl/curl.h>

#include "ThreadPool.h"
#ifndef WIN32
#include <signal.h>
#endif

#include "LieroX.h"
#include "Debug.h"
#include "Options.h"
#include "Error.h"
#include "Networking.h"
#include "StringUtils.h"
#include "SmartPointer.h"
#include "Timer.h"
#include "ThreadVar.h"
#include "MathLib.h"
#include "InputEvents.h"
#include "TaskManager.h"
#include "ReadWriteLock.h"
#include "Mutex.h"


#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

#include <map>

#include <nl.h>
#include <nlinternal.h>
// workaraound for bad named makros by nl.h
// macros are bad, esp the names (reserved/used by CBytestream)
// TODO: they seem to not work correctly!
// all use of it in CBytestream was removed
inline void nl_writeShort(char* x, int& y, NLushort z)		{ writeShort(x, y, z); }
inline void nl_writeLong(char* x, int& y, NLulong z)		{ writeLong(x, y, z); }
inline void nl_writeFloat(char* x, int& y, NLfloat z)		{ writeFloat(x, y, z); }
inline void nl_writeDouble(char* x, int& y, NLdouble z)		{ writeDouble(x, y, z); }
inline void nl_readShort(char* x, int& y, NLushort z)		{ readShort(x, y, z); }
inline void nl_readLong(char* x, int& y, NLulong z)			{ readLong(x, y, z); }
inline void nl_readFloat(char* x, int& y, NLfloat z)		{ readFloat(x, y, z); }
inline void nl_readDouble(char* x, int& y, NLdouble z)		{ readDouble(x, y, z); }
#undef writeByte
#undef writeShort
#undef writeFloat
#undef writeString
#undef readByte
#undef readShort
#undef readFloat
#undef readString


#if defined WIN32 || defined WIN64 || defined (_WIN32_WCE)

#include "wsock.h"

#elif defined Macintosh

#include <Types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <sys/time.h>
#include <LowMem.h>
#define closesocket close
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKET int
#define sockerrno errno

/* define INADDR_NONE if not already */
#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

#else

 /* Unix-style systems */
#ifdef SOLARIS
#include <sys/filio.h> /* for FIONBIO */
#endif

#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/ioctl.h>
#define closesocket close
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SOCKET int
#define sockerrno errno

 /* define INADDR_NONE if not already */
#ifndef INADDR_NONE
#define INADDR_NONE ((unsigned long) -1)
#endif

/* SGI do not include socklen_t */
#if defined __sgi
typedef int socklen_t;
#endif

#endif /* WINDOWS_APP*/



class NetAddrIniter {
public:
	void operator()(SmartPointer<NLaddress, NetAddrIniter>* addr) {
		NLaddress* addrPtr = new NLaddress;
		memset(addrPtr, 0, sizeof(NLaddress));
		*addr = addrPtr;
	}
};

bool AreNetworkAddrEqual(const NetworkAddr& addr1, const NetworkAddr& addr2)
{
	return AreNetAddrEqual(addr1, addr2);
};

class NetAddrInternal
{
	public:
	NetAddrInternal() {};
	NetAddrInternal(const NetAddrInternal & other)
	{
		*this = other;
	};
	
	const NetAddrInternal & operator= (const NetAddrInternal & other)
	{
		*NetAddrSmartPtr.get() = *other.NetAddrSmartPtr.get();
		return *this;
	}

	const NetAddrInternal & operator= (const NLaddress & addr)
	{
		*NetAddrSmartPtr.get() = addr;
		return *this;
	}
	
	NetAddrInternal(const NLaddress & addr)
	{
		*this = addr;
	};

	typedef SmartPointer<NLaddress, NetAddrIniter> Ptr_t;
	
	const Ptr_t & getPtr() const
	{
		return NetAddrSmartPtr;
	}
	
	private:
	Ptr_t NetAddrSmartPtr;
};

DECLARE_INTERNDATA_CLASS( NetworkAddr, NetAddrInternal );

static NLaddress* getNLaddr(NetworkAddr& addr) {
	return NetworkAddrData(addr).getPtr().get();
}

static const NLaddress* getNLaddr(const NetworkAddr& addr) {
	return NetworkAddrData(addr).getPtr().get();
}


// ------------------------------------------------------------------------


#ifndef WIN32
static void sigpipe_handler(int i) {
	warnings << "got SIGPIPE, ignoring..." << endl;
	signal(SIGPIPE, sigpipe_handler);
}
#endif


/*
 *
 * HawkNL Network wrapper
 *
 */


bool bNetworkInited = false;
ReadWriteLock nlSystemUseChangeLock;


typedef std::map<std::string, std::pair< NLaddress, AbsTime > > dnsCacheT; // Second parameter is expiration time of DNS record
ThreadVar<dnsCacheT>* dnsCache4 = NULL;
ThreadVar<dnsCacheT>* dnsCache6 = NULL;

static void AddToDnsCache4(const std::string& name, const NetworkAddr& addr, TimeDiff expireTime = TimeDiff(600.0f)) {
	ScopedReadLock lock(nlSystemUseChangeLock);
	if(dnsCache4 == NULL) return;
	ThreadVar<dnsCacheT>::Writer dns( *dnsCache4 );
	dns.get()[name] = std::make_pair( *getNLaddr(addr), GetTime() + expireTime );
}

static void AddToDnsCache6(const std::string& name, const NetworkAddr& addr, TimeDiff expireTime = TimeDiff(600.0f)) {
	ScopedReadLock lock(nlSystemUseChangeLock);
	if(dnsCache6 == NULL) return;
	ThreadVar<dnsCacheT>::Writer dns6( *dnsCache6 );
	dns6.get()[name] = std::make_pair( *getNLaddr(addr), GetTime() + expireTime );
}

bool GetFromDnsCache(const std::string& name, NetworkAddr& addr4, NetworkAddr& addr6) {

	getNLaddr(addr4)->valid = NL_FALSE;
	getNLaddr(addr6)->valid = NL_FALSE;

	if (IsNetAddrV6(name) && StringToNetAddr(name, addr6)) {
		return true;
	}
	if (StringToNetAddr(name, addr4)) {
		return true;
	}

	ScopedReadLock lock(nlSystemUseChangeLock);
	if(dnsCache4 == NULL) return false;
	if(dnsCache6 == NULL) return false;
	bool v4 = false, v6 = false;
	ThreadVar<dnsCacheT>::Writer dns( *dnsCache4 );
	dnsCacheT::iterator it = dns.get().find(name);
	if(it != dns.get().end()) {
		if( it->second.second < tLX->currentTime )
		{
			dns.get().erase(it);
		}
		else
		{
			*getNLaddr(addr4) = it->second.first;
			v4 = true;
		}
	}
	ThreadVar<dnsCacheT>::Writer dns6( *dnsCache6 );
	it = dns6.get().find(name);
	if(it != dns6.get().end()) {
		if( it->second.second < tLX->currentTime )
		{
			dns6.get().erase(it);
		}
		else
		{
			*getNLaddr(addr6) = it->second.first;
			v6 = true;
		}
	}
	return v4 && v6; // Cache needs both address families to be present to consider an entry valid
}



/////////////////////
// Initializes network
bool InitNetworkSystem() {
	curl_global_init(CURL_GLOBAL_ALL);
	nlSystemUseChangeLock.startWriteAccess();
	bNetworkInited = false;

    if(!nlInit()) {
    	SystemError("nlInit failed");
		nlSystemUseChangeLock.endWriteAccess();
    	return false;
    }

    if(!nlSelectNetwork(NL_IP)) {
        SystemError("could not select IP-based network");
		nlSystemUseChangeLock.endWriteAccess();
		return false;
    }

	bNetworkInited = true;
	
	dnsCache4 = new ThreadVar<dnsCacheT>();
	dnsCache6 = new ThreadVar<dnsCacheT>();

#if !defined(WIN32) && !defined(__ANDROID__)
	//sigignore(SIGPIPE);
	signal(SIGPIPE, sigpipe_handler);
#endif
	
	nlSystemUseChangeLock.endWriteAccess();
	return true;
}

//////////////////
// Shutdowns the network system
bool QuitNetworkSystem() {
	nlSystemUseChangeLock.startWriteAccess();
	nlShutdown();
	bNetworkInited = false;
	delete dnsCache4; dnsCache4 = NULL;
	delete dnsCache6; dnsCache6 = NULL;
	nlSystemUseChangeLock.endWriteAccess();
	curl_global_cleanup();
	return true;
}


/*
Sadly, HawkNL lacks some support for specific things. For example we cannot get the real socket nr.

Atm., we are doing these:

nlUpdateState
nlPrepareClose

*/

// ---------------------------------------------------
// ----------- for state checking -------------------

// WARNING: both these function assume that we selected the IP-network driver in HawkNL

// modified sock_Write of sock.c from HawkNL
// returns true if socket is connected and data could be send
static bool nlUpdateState(NLsocket socket)
{
	if(nlIsValidSocket(socket) != NL_TRUE) return false;
	
	struct Unlocker {
		NLsocket socket;
		~Unlocker() {
			nlUnlockSocket(socket, NL_BOTH);
		}
		Unlocker(NLsocket s) : socket(s) {}
	};
	
	if(nlLockSocket(socket, NL_BOTH) == NL_FALSE)
	{
		return false;
	}
	
	Unlocker unlocker(socket);
	
	nl_socket_t *sock = nlSockets[socket];
	NLint       count = 0;

	if((sock->type == NL_RELIABLE) || (sock->type == NL_RELIABLE_PACKETS)) /* TCP */
	{
		if(sock->connecting == NL_TRUE)
		{
			fd_set          fdset;
			struct timeval  t = {0,0};
			int             serrval = -1;
			socklen_t       serrsize = (socklen_t)sizeof(serrval);


			FD_ZERO(&fdset);
			FD_SET((SOCKET)sock->realsocket, &fdset);
			if(select(sock->realsocket + 1, NULL, &fdset, NULL, &t) == 1)
			{
				/* Check the socket status */
				(void)getsockopt( sock->realsocket, SOL_SOCKET, SO_ERROR, (char *)&serrval, &serrsize );
				if(serrval != 0)
				{
					if(serrval == ECONNREFUSED)
					{
						nlSetError(NL_CON_REFUSED);
					}
					else if(serrval == EINPROGRESS || serrval == EWOULDBLOCK)
					{
						nlSetError(NL_CON_PENDING);
					}
					return false;
				}
				/* the connect has completed */
				sock->connected = NL_TRUE;
				sock->connecting = NL_FALSE;
			}
			else
			{
				/* check for a failed connect */
				FD_ZERO(&fdset);
				FD_SET((SOCKET)sock->realsocket, &fdset);
				if(select(sock->realsocket + 1, NULL, NULL, &fdset, &t) == 1)
				{
					nlSetError(NL_CON_REFUSED);
				}
				else
				{
					nlSetError(NL_CON_PENDING);
				}
				return false;
			}
		}
		/* check for reliable packets */
		if(sock->type == NL_RELIABLE_PACKETS)
		{
			return true;
		}
		return true;
	}
	else /* unconnected UDP */
	{
		/* check for a non-blocking connection pending */
		if(sock->connecting == NL_TRUE)
		{
			nlSetError(NL_CON_PENDING);
			return false;
		}
		/* check for a connection error */
		if(sock->conerror == NL_TRUE)
		{
			nlSetError(NL_CON_REFUSED);
			return false;
		}
		if(sock->type == NL_BROADCAST)
		{
			memcpy(&((struct sockaddr_in6 *)&sock->addressin)->sin6_addr, &in6addr_any, sizeof(in6addr_any));
		}
		if(sock->type == NL_UDP_MULTICAST)
		{
			return true;
		}
		else if(sock->connected == NL_TRUE)
		{
			return true;
		}
		else
		{
			return true;
		}
	}
	if(count == SOCKET_ERROR)
	{
		return false;
	}
	return false;
}


static void nlPrepareClose(NLsocket socket) {
	if(nlIsValidSocket(socket) != NL_TRUE) return;
	
	struct Unlocker {
		NLsocket socket;
		~Unlocker() {
			nlUnlockSocket(socket, NL_BOTH);
		}
		Unlocker(NLsocket s) : socket(s) {}
	};
	
	if(nlLockSocket(socket, NL_BOTH) == NL_FALSE)
	{
		return;
	}
	
	Unlocker unlocker(socket);
	
	// code copied&modified from sock_Close
	// The advantage we have here is that we don't lock the whole socket array.
	// nlClose is doing this, so if we would hang in nlClose, we block the
	// *whole* HawkNL system (or at least actions like opening new sockets etc.)!
	
	nl_socket_t     *sock = nlSockets[socket];
    
	if(sock->type == NL_UDP_MULTICAST)
	{
		/* leave the multicast group */
	}
	if(sock->type == NL_RELIABLE_PACKETS)
	{
		/* check for unsent data */
		if(sock->sendlen > 0)
		{
			int tries = 200;

			/* 200 * 50 ms = up to a 10 second delay to allow data to be sent */
			while(tries-- > 0 && sock->sendlen > 0)
			{
				SDL_Delay(50);
			}
		}
		
		// oh just fuck it
		sock->sendlen = 0;
	}
	if((sock->type == NL_RELIABLE_PACKETS || sock->type == NL_RELIABLE) && sock->listen == NL_FALSE)
	{
		struct linger l = {1, 10};

		(void)setsockopt((SOCKET)sock->realsocket, SOL_SOCKET, SO_LINGER, (const char *)&l, (int)sizeof(l));
	}
	(void)closesocket((SOCKET)sock->realsocket);
}





struct NetworkSocket::EventHandler {
	struct SharedData {
		Mutex mutex;
		bool quitSignal;
		NetworkSocket* sock;
		NLint nlGroup;
		
		SharedData(NetworkSocket* _s) : quitSignal(false), sock(_s) {
			nlGroup = nlGroupCreate();
			addSocketToGroup();
		}
		
		void addSocketToGroup();
		
		~SharedData() { nlGroupDestroy(nlGroup); nlGroup = NL_INVALID; }
	};
	SmartPointer<SharedData> sharedData;
	
	EventHandler(NetworkSocket* sock);
	
	void quit() {
		if(sharedData.get()) {
			Mutex::ScopedLock lock(sharedData->mutex);
			sharedData->quitSignal = true;
			sharedData->sock = NULL;
		}
	}
	
	~EventHandler() {
		// just to be sure
		quit();
		sharedData = NULL;
	}
};

struct NetworkSocket::InternSocket {
	NLsocket sock;
	SmartPointer<EventHandler> eventHandler;
	
	InternSocket() : sock(NL_INVALID) {}
	~InternSocket() {
		// just a double check - there really shouldn't be a case where this could be true
		if(eventHandler.get()) {
			errors << "NetworkSocket::~InternSocket: event handler was not unset" << endl;
			eventHandler->quit();
			eventHandler = NULL;
		}
	}
};

void NetworkSocket::EventHandler::SharedData::addSocketToGroup() {
	nlGroupAddSocket(nlGroup, sock->m_socket->sock);
}

NetworkSocket::EventHandler::EventHandler(NetworkSocket* sock) {
	if(!sock) {
		errors << "NetworkSocket::EventHandler: socket == NULL" << endl;
		return;
	}
	
	if(!sock->isOpen()) {
		errors << "NetworkSocket::EventHandler: socket is closed" << endl;
		return;
	}
	
	struct EventHandlerThread : Action {
		SmartPointer<SharedData> sharedData;
		uint pollType; // NL_READ_STATUS or NL_ERROR_STATUS
		
		EventHandlerThread(const SmartPointer<SharedData>& _data, uint _pollType) :
		sharedData(_data), pollType(_pollType) {}
		
		bool pushNewDataEvent() {
			Mutex::ScopedLock lock(sharedData->mutex);
			if(sharedData->sock) {
				sharedData->sock->OnNewData.pushToMainQueue(EventData(sharedData->sock));
				return true;
			}
			return false;
		}
		
		bool pushErrorEvent() {
			Mutex::ScopedLock lock(sharedData->mutex);
			if(sharedData->sock) {
				sharedData->sock->OnError.pushToMainQueue(EventData(sharedData->sock));
				return true;
			}
			return false;			
		}
		
		Result handle() {
			const int maxFPS = tLXOptions ? tLXOptions->nMaxFPS : 100;
			TimeDiff max_frame_time = TimeDiff(MAX(0.01f, (maxFPS > 0) ? 1.0f/(float)maxFPS : 0.0f));
			AbsTime lastTime = GetTime();
			
			while(!sharedData->quitSignal) {
				NLsocket s;
				NLint ret = nlPollGroup(sharedData->nlGroup, pollType, &s, /* amount of sockets */ 1, /* timeout */ max_frame_time.milliseconds());
				// if no error, ret is amount of sockets which triggered the event
				
				if(ret > 0) {
					switch(pollType) {
						case NL_READ_STATUS: if(!pushNewDataEvent()) return -1; break;
						case NL_ERROR_STATUS: if(!pushErrorEvent()) return -1; break;
					}
				}
				
				AbsTime curTime = GetTime();
				if(curTime - lastTime < max_frame_time) {
					SDL_Delay( (Uint32)( ( max_frame_time - (curTime - lastTime) ).milliseconds() ) );
				}
				lastTime = curTime;
			}
			
			return true;
		}
	};
	
	sharedData = new SharedData(sock);
	threadPool->start(new EventHandlerThread(sharedData, NL_READ_STATUS), "socket " + itoa(sock->m_socket->sock) + " read event checker", true);
	threadPool->start(new EventHandlerThread(sharedData, NL_ERROR_STATUS), "socket " + itoa(sock->m_socket->sock) + " error event checker", true);
}

NetworkSocket::NetworkSocket() : m_type(NST_INVALID), m_state(NSS_NONE), m_withEvents(false), m_socket(NULL) {
	m_socket = new InternSocket();
}

NetworkSocket::~NetworkSocket() {
	Clear();
	delete m_socket;
	m_socket = NULL;
}

void NetworkSocket::setWithEvents( bool v )
{
	if( v == m_withEvents ) return;
	m_withEvents = v;	
	checkEventHandling();
}

void NetworkSocket::checkEventHandling() {
	if(isOpen() && m_withEvents) {
		// we must have an event handler
		if(m_socket->eventHandler.get() == NULL) {
			m_socket->eventHandler = new EventHandler(this);
		}
	}
	else {
		// we must not have an event handler
		if(m_socket->eventHandler.get()) {
			m_socket->eventHandler->quit();
			m_socket->eventHandler = NULL;			
		}
	}
}

std::string NetworkSocket::debugString() const {
	if(!isOpen()) return "Closed";
	std::string ret = TypeStr(m_type) + "/" + StateStr(m_state);
	{
		std::string localStr = "INVALIDLOCAL";
		NetworkAddr addr;
		if(nlGetLocalAddr(m_socket->sock, getNLaddr(addr)) != NL_FALSE)
			NetAddrToString(addr, localStr);
		else {
			localStr = "ERRORLOCALADDR(" + GetLastErrorStr() + ")";
			ResetSocketError();
		}
		ret += " " + localStr;
	}
	if(m_state == NSS_CONNECTED) {
		ret += " connected to ";
		std::string remoteStr = "INVALIDREMOTE";
		NetworkAddr addr;
		if(nlGetRemoteAddr(m_socket->sock, getNLaddr(addr)) != NL_FALSE)
			NetAddrToString(remoteAddress(), remoteStr);
		else {
			remoteStr = "ERRORREMOTEADDR(" + GetLastErrorStr() + ")";
			ResetSocketError();
		}
		ret += remoteStr;
	}
	return ret;
}

bool NetworkSocket::OpenReliable(Port port) {
	if(isOpen()) {
		warnings << "NetworkSocket " << debugString() << ": OpenReliable: socket is already opened, reopening now" << endl;
		Close();
	}
	
	NLsocket ret = nlOpen(port, NL_RELIABLE);
	if (ret == NL_INVALID)  {
#ifdef DEBUG
		errors << "OpenReliableSocket: " << GetLastErrorStr() << endl;
#endif
		ResetSocketError();
		return false;
	}
	m_socket->sock = ret;
	m_type = NST_TCP;
	m_state = NSS_NONE;
	checkEventHandling();
	return true;
}

bool NetworkSocket::OpenUnreliable(Port port) {
	if(isOpen()) {
		warnings << "NetworkSocket " << debugString() << ": OpenReliable: socket is already opened, reopening now" << endl;
		Close();
	}

	NLsocket ret = nlOpen(port, NL_UNRELIABLE);
	if (ret == NL_INVALID)  {
#ifdef DEBUG
		errors << "OpenUnreliableSocket: " << GetLastErrorStr() << endl;
#endif
		ResetSocketError();
		return false;
	}
	m_socket->sock = ret;
	m_type = NST_UDP;
	m_state = NSS_NONE;
	checkEventHandling();
	return true;
}

bool NetworkSocket::OpenBroadcast(Port port) {
	if(isOpen()) {
		warnings << "NetworkSocket " << debugString() << ": OpenBroadcast: socket is already opened, reopening now" << endl;
		Close();
	}

	NLsocket ret = nlOpen(port, NL_BROADCAST);
	if (ret == NL_INVALID)  {
#ifdef DEBUG
		errors << "OpenBroadcastSocket: " << GetLastErrorStr() << endl;
#endif
		ResetSocketError();
		return false;
	}
	m_socket->sock = ret;
	m_type = NST_UDPBROADCAST;
	m_state = NSS_NONE;
	checkEventHandling();
	return true;	
}

bool NetworkSocket::Connect(const NetworkAddr& addr) {
	if(!isOpen()) {
		errors << "NetworkSocket::Connect: socket is closed" << endl;
		return false;
	}
	
	if(m_type != NST_TCP) {
		errors << "NetworkSocket::Connect " << debugString() << ": connect only works with TCP" << endl;
		return false;
	}
	
	if(nlConnect(m_socket->sock, getNLaddr(addr)) == NL_FALSE) {
#ifdef DEBUG
		errors << "Connect: " << GetLastErrorStr() << endl;
#endif
		ResetSocketError();
		return false;
	}
	
	checkEventHandling();
	return true;
}

bool NetworkSocket::Listen() {
	if(!isOpen()) {
		errors << "NetworkSocket::Listen: socket is closed" << endl;
		return false;
	}
	
	if(nlListen(m_socket->sock) == NL_FALSE) {
#ifdef DEBUG
		errors << "Listen: " << GetLastErrorStr() << endl;
#endif
		ResetSocketError();
		return false;
	}
	
	checkEventHandling();
	return true;
}

void NetworkSocket::Close() {
	if(!isOpen()) {
		warnings << "NetworkSocket::Close: cannot close already closed socket" << endl;
		return;
	}
	
	struct CloseSocketWorker : Task {
		NLsocket sock;
		Result handle() {
			nlSystemUseChangeLock.startReadAccess();
			int ret = -1;
			if(bNetworkInited) { // only do that if we have the network system still up
				// this should already close the socket but not lock other parts in HawkNL
				nlPrepareClose(sock);
				// hopefully this does not block anymore
				ret = (nlClose(sock) != NL_FALSE) ? 0 : -1;
			}
			nlSystemUseChangeLock.endReadAccess();
			return ret == 0 ? true : false;
		}
	};
	CloseSocketWorker* worker = new CloseSocketWorker();
	worker->name = "close socket";
	worker->sock = m_socket->sock;
	taskManager->start(worker, TaskManager::QT_GlobalQueue);
	
	m_socket->sock = NL_INVALID;
	m_type = NST_INVALID;
	m_state = NSS_NONE;
	
	checkEventHandling();
}

int NetworkSocket::Write(const void* buffer, int nbytes) {
	if(!isOpen()) {
		errors << "NetworkSocket::Write: cannot write on closed socket" << endl;
		return NL_INVALID;
	}
	
	ResetSocketError();
	NLint ret = nlWrite(m_socket->sock, buffer, nbytes);

	// Error checking
	if (ret == NL_INVALID)  {
#ifdef DEBUG
		std::string errStr = GetLastErrorStr(); // cache errStr that debugString will not overwrite it
		warnings << "WriteSocket " << debugString() << ": " << errStr << endl;
#endif // DEBUG
		return NL_INVALID;
	}

	/*if (ret == 0) {  // HINT: this is not an error, it is one of the ways to find out if a socket is writeable
		errors << "WriteSocket: Could not send the packet, network buffers are full." << endl;
	}*/

	return ret;
}


int NetworkSocket::Read(void* buffer, int nbytes) {
	if(!isOpen()) {
		errors << "NetworkSocket::Read: cannot read on closed socket" << endl;
		return NL_INVALID;
	}

	ResetSocketError();
	NLint ret = nlRead(m_socket->sock, buffer, nbytes);
	
	// Error checking
	if (ret == NL_INVALID)  {
		// messageend-error is just that there is no data; we can ignore that
		if (!IsMessageEndSocketErrorNr(GetSocketErrorNr())) {
#ifdef DEBUG
			std::string errStr = GetLastErrorStr(); // cache errStr that debugString will not overwrite it
			errors << "ReadSocket " << debugString() << ": " << errStr << endl;
#endif // DEBUG
			
			// Is this perhaps the solution for the Bad file descriptor error?
			//Close();
		}
		return NL_INVALID;
	}

	return ret;
}





bool NetworkSocket::isReady() const {
	return isOpen() && nlUpdateState(m_socket->sock);
}


// TODO: Remove the following functions because they are blocking.

/////////////////////
// Wait until the socket is writable
// TODO: remove this function, don't use it!
void NetworkSocket::WaitForSocketWrite(int timeout)
{
	if(!isOpen()) {
		errors << "WaitForSocketWrite: socket closed" << endl;
		return;
	}
	
	NLint group = nlGroupCreate();
	nlGroupAddSocket(group, m_socket->sock);
	NLsocket s;
	nlPollGroup(group, NL_WRITE_STATUS, &s, 1, (NLint)timeout);
	nlGroupDestroy(group);
}

//////////////////////
// Wait until the socket contains some data to read
// TODO: remove this function, don't use it!
void NetworkSocket::WaitForSocketRead(int timeout)
{
	if(!isOpen()) {
		errors << "WaitForSocketRead: socket closed" << endl;
		return;
	}

	NLint group = nlGroupCreate();
	nlGroupAddSocket(group, m_socket->sock);
	NLsocket s;
	nlPollGroup(group, NL_READ_STATUS, &s, 1, (NLint)timeout);
	nlGroupDestroy(group);
}

/////////////////////
// Wait until the socket contains some data or is writeable
// TODO: remove this function, don't use it!
void NetworkSocket::WaitForSocketReadOrWrite(int timeout)
{
	if(!isOpen()) {
		errors << "WaitForSocketReadOrWrite: socket closed" << endl;
		return;
	}

	NLint group = nlGroupCreate();
	nlGroupAddSocket(group, m_socket->sock);
	NLsocket s;

	if (timeout < 0)  {
		// Infinite timeout
		while (true)  {
			if (nlPollGroup(group, NL_READ_STATUS, &s, 1, 0))
				break;
			if (nlPollGroup(group, NL_WRITE_STATUS, &s, 1, 0))
				break;
			SDL_Delay(2);
		}
	} else if (timeout > 0)  {
		// Blocking, with a timeout
		Uint32 start = SDL_GetTicks();
		while (SDL_GetTicks() - start <= (Uint32)timeout)  {
			if (nlPollGroup(group, NL_READ_STATUS, &s, 1, 0))
				break;
			if (nlPollGroup(group, NL_WRITE_STATUS, &s, 1, 0))
				break;
			SDL_Delay(2);
		}
	}

	nlGroupDestroy(group);
}

NetworkAddr NetworkSocket::localAddress() const {
	NetworkAddr addr;
	
	if(!isOpen()) {
		errors << "NetworkSocket::localAddress: socket is closed" << endl;
		return addr;
	}
	
	if(nlGetLocalAddr(m_socket->sock, getNLaddr(addr)) == NL_FALSE) {
		errors << "NetworkSocket::localAddress: cannot get local address (" << debugString() << "): " << GetLastErrorStr() << endl;
		ResetSocketError();
		return addr;
	}
	
	return addr;
}

NetworkAddr NetworkSocket::remoteAddress() const {
	NetworkAddr addr;
	
	if(!isOpen()) {
		errors << "NetworkSocket::remoteAddress: socket is closed" << endl;
		return addr;
	}
	
	if(nlGetRemoteAddr(m_socket->sock, getNLaddr(addr)) == NL_FALSE) {
		errors << "NetworkSocket::remoteAddress: cannot get remote address" << "(" << debugString() << "): " << GetLastErrorStr() << endl;
		ResetSocketError();
		return addr;
	}
	
	return addr;
}

bool NetworkSocket::setRemoteAddress(const NetworkAddr& addr) {
	if(!isOpen()) {
		errors << "NetworkSocket::setRemoteAddress: socket is closed" << endl;
		return false;
	}
	
	if(getNLaddr(addr) == NULL) {
		errors << "NetworkSocket::setRemoteAddress " << debugString() << ": given address is invalid" << endl;
		return false;
	}
	if( GetNetAddrPort(addr) == 0 )
	{
		errors << "NetworkSocket::setRemoteAddress " << debugString() << ": port is set to 0" << endl;
	}
	
	if(nlSetRemoteAddr(m_socket->sock, getNLaddr(addr)) == NL_FALSE) {
		std::string addrStr = "INVALIDADDR";
		NetAddrToString(addr, addrStr);
		errors << "NetworkSocket::setRemoteAddress " << debugString() << ": failed to set destination " << addrStr << ": " << GetLastErrorStr() << endl;
		ResetSocketError();
		return false;
	}
	
	return true;
}

int GetSocketErrorNr() {
	return nlGetError();
}

std::string GetSocketErrorStr(int errnr) {
	return GetLastErrorStr();
}

std::string GetLastErrorStr()  {
	if (nlGetError() != NL_SYSTEM_ERROR)
		return std::string(nlGetErrorStr(nlGetError()));
	else
		return std::string(nlGetSystemErrorStr(nlGetSystemError()));
}

bool IsMessageEndSocketErrorNr(int errnr) {
	return (errnr == NL_MESSAGE_END);
}

void ResetSocketError()  {
	if (!bNetworkInited)
		return;
	
	nlSetError(NL_NO_ERROR);
}




bool IsNetAddrValid(const NetworkAddr& addr) {
	if(getNLaddr(addr))
		return (getNLaddr(addr)->valid != NL_FALSE);
	else
		return false;
}

bool SetNetAddrValid(NetworkAddr& addr, bool valid) {
	if(!getNLaddr(addr)) return false;
	getNLaddr(addr)->valid = valid ? NL_TRUE : NL_FALSE;
	return true;
}

void ResetNetAddr(NetworkAddr& addr) {
	if(!getNLaddr(addr)) return;
	// TODO: is this the best way?
	memset(getNLaddr(addr), 0, sizeof(NLaddress));
	SetNetAddrValid(addr, false);
}

bool IsNetAddrV6(const std::string& s)
{
	// Any IPv6 address will be in a format [XXXX:XXXX:...:XXXX]:XXXX or [XXXX:XXXX:...:XXXX]
	return s.size() > 0 && s[0] == '[';
}

bool IsNetAddrV6(const NetworkAddr& addr)
{
	std::string s;
	NetAddrToString(addr, s);
	return IsNetAddrV6(s);
}

static bool isStringValidIP(const std::string& str) {
	NetworkAddr addr;
	return (nlStringToAddr(str.c_str(), getNLaddr(addr)) == NL_TRUE);
}

// accepts "%i.%i.%i.%i[:%l]" as input
bool StringToNetAddr(const std::string& string, NetworkAddr& addr, std::string* errorStr) {
	if(getNLaddr(addr) == NULL) return false;
	
	if(!isStringValidIP(string)) {
		SetNetAddrValid(addr, false);
		return false;
	}
	
	if(nlStringToAddr(string.c_str(), getNLaddr(addr)) == NL_FALSE) {
		errors << "StringToNetAddr: cannot use " << string << " as address: " << GetLastErrorStr() << endl;
		if(errorStr) *errorStr = GetLastErrorStr();
		ResetSocketError();
		return false;
	}
	
	return true;
}

bool NetAddrToString(const NetworkAddr& addr, std::string& string) {
	// TODO: safty here for buffer
	char buf[256];
	NLchar* res = nlAddrToString(getNLaddr(addr), buf);
	if(res) {
		fix_markend(buf);
		string = buf;
		return true;
	} else {
		string = "";
		return false;
	}
}

NetworkAddr StringToNetAddr(const std::string& string) { 
	NetworkAddr ret;
	ResetNetAddr(ret);
	StringToNetAddr(string, ret);
	return ret;
};

std::string NetAddrToString(const NetworkAddr& addr) { 
	std::string ret;
	NetAddrToString(addr, ret);
	return ret;
};

unsigned short GetNetAddrPort(const NetworkAddr& addr) {
	if(getNLaddr(addr) == NULL)
		return 0;
	else
		return nlGetPortFromAddr(getNLaddr(addr));
}

bool SetNetAddrPort(NetworkAddr& addr, unsigned short port, std::string* errorStr) {
	if(getNLaddr(addr) == NULL)
		return false;
	else {
		if(nlSetAddrPort(getNLaddr(addr), port) == NL_FALSE) {
			errors << "SetNetAddrPort: cannot set port " << port << ": " << GetLastErrorStr() << endl;
			if(errorStr) *errorStr = GetLastErrorStr();
			ResetSocketError();
			return false;
		}
		return true;
	}
}

bool AreNetAddrEqual(const NetworkAddr& addr1, const NetworkAddr& addr2) {
	if(getNLaddr(addr1) == getNLaddr(addr2))
		return true;
	else
		return nlAddrCompare(getNLaddr(addr1), getNLaddr(addr2)) != NL_FALSE;
}




Event<> onDnsReady;

// copied from HawkNL sock.c and modified to not use nlStringToNetAddr
static bool GetAddrFromNameAsync_Internal(const NLchar* name, NLaddress* address4, NLaddress* address6) {
    NLushort    port = 0;
    char        *pos;
    int         status;
    NLbyte      temp[NL_MAX_STRING_LENGTH];
	struct      addrinfo hints;
	struct      addrinfo *result, *rp;

	address4->valid = NL_FALSE;
	address6->valid = NL_FALSE;
#ifdef _UNICODE
    /* convert from wide char string to multibyte char string */
    (void)wcstombs(temp, (const NLchar *)name, NL_MAX_STRING_LENGTH);
#else
    strncpy(temp, name, NL_MAX_STRING_LENGTH);
#endif
    temp[NL_MAX_STRING_LENGTH - 1] = (NLbyte)'\0';
    pos = strrchr(temp, ':');
    if(pos != NULL)
    {
        pos[0] = (NLbyte)'\0';
        (void)sscanf(pos+1, "%hu", &port);
    }

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = 0;
    hints.ai_flags = 0;    /* For wildcard IP address */
    hints.ai_protocol = 0;          /* Any protocol */
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;

    status = getaddrinfo(temp, NULL, &hints, &result);
    if(status != 0 || result == NULL)
    {
        nlSetError(NL_SYSTEM_ERROR);
        return false;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        if(rp->ai_family == AF_INET && !address4->valid)
        {
            ((struct sockaddr_in6 *)address4)->sin6_family = AF_INET6;
            ((struct sockaddr_in6 *)address4)->sin6_port = htons(port);
            struct in_addr v4addr;
            v4addr = ((struct sockaddr_in *)rp->ai_addr)->sin_addr;
            char addr6[32] = "::ffff:";
            strcat(addr6, inet_ntoa(v4addr));
            inet_pton(AF_INET6, addr6, &((struct sockaddr_in6 *)address4)->sin6_addr);
            address4->valid = NL_TRUE;
        }
        if(rp->ai_family == AF_INET6 && !address6->valid)
        {
            ((struct sockaddr_in6 *)address6)->sin6_family = AF_INET6;
            ((struct sockaddr_in6 *)address6)->sin6_port = htons(port);
            memcpy(&((struct sockaddr_in6 *)address6)->sin6_addr, &((struct sockaddr_in6 *)rp->ai_addr)->sin6_addr, sizeof(struct in6_addr));
            address6->valid = NL_TRUE;
        }
    }

    freeaddrinfo(result);

    return address4->valid || address6->valid;
}

static std::set<std::string> PendingDnsQueries;
static Mutex PendingDnsQueriesMutex;

bool GetNetAddrFromNameAsync(const std::string& name)
{
	// We don't use nlGetAddrFromNameAsync here because we have to use SmartPointers
	// The problem is, if you call this and then delete the memory of the network address
	// while the thread isn't ready, it will write after to deleted memory.

	NetworkAddr addr4, addr6;

	if(name == "") {
		return false;
	}
	if (StringToNetAddr(name, addr4)) {
		return true;
	}

	ResetSocketError(); // Clear the bad address error
	
	if(GetFromDnsCache(name, addr4, addr6)) {
		return true;
	}

	{
		Mutex::ScopedLock l(PendingDnsQueriesMutex);
		if(PendingDnsQueries.find(name) != PendingDnsQueries.end())
			return true;
		PendingDnsQueries.insert(name);
	}

	struct GetAddrFromNameAsync_Executer : Task {
		std::string addr_name;
		NetworkAddr address4, address6;
		
		Result handle() {
			getNLaddr(address4)->valid = NL_FALSE;
			getNLaddr(address6)->valid = NL_FALSE;
			if(GetAddrFromNameAsync_Internal(addr_name.c_str(), getNLaddr(address4), getNLaddr(address6))) {
				// TODO: we use default DNS record expire time of 1 hour, we should include some DNS client to make it in correct way
				// Cache both valid and invalid addersses, one server may have IPv4 but no IPv6 address
				notes << "DNS: resolved " << addr_name << " to " << NetAddrToString(address4) << " IPv6 " << NetAddrToString(address6) << endl;
				AddToDnsCache4(addr_name, address4);
				AddToDnsCache6(addr_name, address6);
			}
			
			// TODO: handle failures here? there should be, but we only have the valid field
			// For now, we leave it at false, so the timeout handling will just occur.
			
			// push a net event
			onDnsReady.pushToMainQueue(EventData());

			Mutex::ScopedLock l(PendingDnsQueriesMutex);
			PendingDnsQueries.erase(addr_name);
			
			return true;
		}
	};
	GetAddrFromNameAsync_Executer* data = new GetAddrFromNameAsync_Executer();
	if(data == NULL) return false;
	data->name = "GetNetAddrFromNameAsync for " + name;
	data->addr_name = name;

	taskManager->start(data, TaskManager::QT_NoQueue);
    return true;
}


bool NetworkSocket::isDataAvailable() {
	NLint group = nlGroupCreate();
	nlGroupAddSocket( group, m_socket->sock );
	NLsocket sock_out[2];
	int ret = nlPollGroup( group, NL_READ_STATUS, sock_out, 1, 0 );
	nlGroupDestroy(group);
	return ret > 0;
}

// In some cases, e.g. if the network is not connected, some IPs are
// not available. Most likely, without any network, you just have
// 127.* in the routing table.
bool IsNetAddrAvailable(const NetworkAddr& addr) {
	// TODO: implement...
	return true;
}


void NetworkSocket::reapplyRemoteAddress() {
	if(m_type == NST_UDP || m_type == NST_UDPBROADCAST)
		// TODO: comment this, why we need that in some cases
		setRemoteAddress(remoteAddress());
	else
		errors << "NetworkSocket::reapplyRemoteAddress cannot be done as " << TypeStr(m_type) << endl;
}

