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

#include <curl/curl.h>

#include "ThreadPool.h"
#ifndef WIN32
#include <signal.h>
#endif

#include "CodeAttributes.h"
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



static std::string GetLastErrorAndReset() {
	std::string errMsg = GetLastErrorStr();
	ResetSocketError();
	return errMsg;
}


#ifdef _MSC_VER
#pragma warning(disable: 4786)
#endif

#include <map>

#include <nl.h>
// workaraound for bad named makros by nl.h
// macros are bad, esp the names (reserved/used by CBytestream)
// TODO: they seem to not work correctly!
// all use of it in CBytestream was removed
INLINE void nl_writeShort(char* x, int& y, NLushort z)		{ writeShort(x, y, z); }
INLINE void nl_writeLong(char* x, int& y, NLulong z)		{ writeLong(x, y, z); }
INLINE void nl_writeFloat(char* x, int& y, NLfloat z)		{ writeFloat(x, y, z); }
INLINE void nl_writeDouble(char* x, int& y, NLdouble z)		{ writeDouble(x, y, z); }
INLINE void nl_readShort(char* x, int& y, NLushort z)		{ readShort(x, y, z); }
INLINE void nl_readLong(char* x, int& y, NLulong z)			{ readLong(x, y, z); }
INLINE void nl_readFloat(char* x, int& y, NLfloat z)		{ readFloat(x, y, z); }
INLINE void nl_readDouble(char* x, int& y, NLdouble z)		{ readDouble(x, y, z); }
#undef writeByte
#undef writeShort
#undef writeFloat
#undef writeString
#undef readByte
#undef readShort
#undef readFloat
#undef readString

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
}

class NetAddrInternal
{
	public:
	NetAddrInternal() {}
	NetAddrInternal(const NetAddrInternal & other)
	{
		*this = other;
	}
	
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
	}

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



/*
 *
 * HawkNL Network wrapper
 *
 */


bool bNetworkInited = false;
ReadWriteLock nlSystemUseChangeLock;


typedef std::map<std::string, std::pair< NLaddress, AbsTime > > dnsCacheT; // Second parameter is expiration time of DNS record
ThreadVar<dnsCacheT>* dnsCache = NULL;

void AddToDnsCache(const std::string& name, const NetworkAddr& addr, TimeDiff expireTime ) {
	ScopedReadLock lock(nlSystemUseChangeLock);
	if(dnsCache == NULL) return;
	ThreadVar<dnsCacheT>::Writer dns( *dnsCache );
	dns.get()[name] = std::make_pair( *getNLaddr(addr), GetTime() + expireTime );
}

bool GetFromDnsCache(const std::string& name, NetworkAddr& addr) {
	ScopedReadLock lock(nlSystemUseChangeLock);
	if(dnsCache == NULL) return false;
	ThreadVar<dnsCacheT>::Writer dns( *dnsCache );
	dnsCacheT::iterator it = dns.get().find(name);
	if(it != dns.get().end()) {
		if( it->second.second < tLX->currentTime )
		{
			dns.get().erase(it);
			return false;
		}
		*getNLaddr(addr) = it->second.first;
		return true;
	} else
		return false;
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
	
	dnsCache = new ThreadVar<dnsCacheT>();

#if !defined(WIN32) && !defined(__ANDROID__)
	// Bionic libc on Android does not provide sigignore().
	sigignore(SIGPIPE);
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
	delete dnsCache; dnsCache = NULL;
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

// copied from sock.c from HawkNL

#include <memory.h>
#include <stdio.h>
#include <string.h>

#if defined (_WIN32_WCE)
#define EAGAIN          11
#define errno GetLastError()
#else
#include <errno.h>
#endif

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


// from nlinternal.h from HawkNL

/* number of buckets for average bytes/second */
#define NL_NUM_BUCKETS          8

/* number of packets stored for NL_LOOP_BACK */
#define NL_NUM_PACKETS          8
#define NL_MAX_ACCEPT           10


typedef struct
{
	NLlong      bytes;          /* bytes sent/received */
	NLlong      packets;        /* packets sent/received */
	NLlong      highest;        /* highest bytes/sec sent/received */
	NLlong      average;        /* average bytes/sec sent/received */
	time_t      stime;          /* the last time stats were updated */
	NLint       lastbucket;     /* the last bucket that was used */
	NLlong      curbytes;       /* current bytes sent/received */
	NLlong      bucket[NL_NUM_BUCKETS];/* buckets for sent/received counts */
	NLboolean   firstround;     /* is this the first round through the buckets? */
} nl_stats_t;

typedef struct
{
	/* info for NL_LOOP_BACK, NL_SERIAL, and NL_PARALLEL */
	NLbyte      *outpacket[NL_NUM_PACKETS];/* temp storage for packet data */
	NLbyte      *inpacket[NL_NUM_PACKETS];/* temp storage for packet data */
	NLint       outlen[NL_NUM_PACKETS];/* the length of each packet */
	NLint       inlen[NL_NUM_PACKETS];/* the length of each packet */
	NLint       nextoutused;    /* the next used packet */
	NLint       nextinused;     /* the next used packet */
	NLint       nextoutfree;    /* the next free packet */
	NLint       nextinfree;     /* the next free packet */
	NLsocket    accept[NL_MAX_ACCEPT];/* pending connects */
	NLsocket    consock;        /* the socket this socket is connected to */
} nl_extra_t;


/* the internal socket object */
typedef struct
{
	/* the current status of the socket */
	NLenum      driver;         /* the driver used with this socket */
	NLenum      type;           /* type of socket */
	NLboolean   inuse;          /* is in use */
	NLboolean   connecting;     /* a non-blocking TCP or UDP connection is in process */
	NLboolean   conerror;       /* an error occured on a UDP connect */
	NLboolean   connected;      /* is connected */
	NLboolean   reliable;       /* do we use reliable */
	NLboolean   blocking;       /* is set to blocking */
	NLboolean   listen;         /* can receive an incoming connection */
	NLint       realsocket;     /* the real socket number */
	NLushort    localport;      /* local port number */
	NLushort    remoteport;     /* remote port number */
	NLaddress   addressin;      /* address of remote system, same as the socket sockaddr_in structure */
	NLaddress   addressout;     /* the multicast address set by nlConnect or the remote address for unconnected UDP */
	NLmutex     readlock;       /* socket is locked to update data */
	NLmutex     writelock;      /* socket is locked to update data */

	/* the current read/write statistics for the socket */
	nl_stats_t  instats;        /* stats for received */
	nl_stats_t  outstats;       /* stats for sent */

	/* NL_RELIABLE_PACKETS info and storage */
	NLbyte      *outbuf;        /* temp storage for partially sent reliable packet data */
	NLint       outbuflen;      /* the length of outbuf */
	NLint       sendlen;        /* how much still needs to be sent */
	NLbyte      *inbuf;         /* temp storage for partially received reliable packet data */
	NLint       inbuflen;       /* the length of inbuf */
	NLint       reclen;         /* how much of the reliable packet we have received */
	NLboolean   readable;       /* a complete packet is in inbuf */
	NLboolean   message_end;    /* a message end error ocured but was not yet reported */
	NLboolean   packetsync;     /* is the reliable packet stream in sync */
	/* pointer to extra info needed for NL_LOOP_BACK, NL_SERIAL, and NL_PARALLEL */
	nl_extra_t   *ext;
} nl_socket_t;

typedef /*@only@*/ nl_socket_t *pnl_socket_t;

// -------------------------------------
// extern defs for HawkNL intern stuff
#ifdef _MSC_VER
extern "C"  {
	__declspec(dllimport) pnl_socket_t *nlSockets;
}
#else
#ifdef WIN32
extern "C" pnl_socket_t *nlSockets;	// For Dev-Cpp
#else
extern "C"  {
	NL_EXP pnl_socket_t *nlSockets;
}
#endif
#endif


#if defined(WIN32) && (! defined(_MSC_VER))
// TODO: why is that needed?
#define NL_EXP
#elif (_MSC_VER >= 1400) //MSVC 2005 hax
// TODO: why is that needed?
#undef NL_EXP
#undef NL_APIENTRY
#define NL_EXP
#define NL_APIENTRY
#endif

extern "C" {
	NL_EXP void NL_APIENTRY nlSetError(NLenum err);
	
	// can we use those? (at least we have to)
	NLboolean nlLockSocket(NLsocket socket, NLint which);
	NLboolean nlIsValidSocket(NLsocket socket);
	void nlUnlockSocket(NLsocket socket, NLint which);
}


/* for nlLockSocket and nlUnlockSocket */
#define NL_READ                 0x0001
#define NL_WRITE                0x0002
#define NL_BOTH                 (NL_READ|NL_WRITE)




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
			((struct sockaddr_in *)&sock->addressin)->sin_addr.s_addr = INADDR_BROADCAST;
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


static void _nlCustomClose(NLsocket socket) {
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
		struct ip_mreq  mreq;
		mreq.imr_multiaddr.s_addr = ((struct sockaddr_in *)&sock->addressout)->sin_addr.s_addr;
		mreq.imr_interface.s_addr = INADDR_ANY; //bindaddress;

		(void)setsockopt((SOCKET)sock->realsocket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		 (char *)&mreq, (int)sizeof(mreq));
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

	// nlClose() will try to close the socket again. If the filedesc was already
	// taken elsewhere, it would close it again! To avoid that, set some invalid
	// fd here.
	sock->realsocket = -1;
}

struct CloseSocketWorker : Task {
	NLsocket sock;
	Result handle() {
		ScopedReadLock lock(nlSystemUseChangeLock);
		if(bNetworkInited) { // only do that if we have the network system still up
			_nlCustomClose(sock);
			if(nlClose(sock) == NL_TRUE) return true;
			return GetLastErrorAndReset();
		}
		return "network not inited";
	}
};




struct NetworkSocket::EventHandler {
	struct SharedData : DontCopyTag {
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
				NLint ret = nlPollGroup(sharedData->nlGroup, pollType, &s, /* amount of sockets */ 1, /* timeout */ (NLint)max_frame_time.milliseconds());
				// if no error, ret is amount of sockets which triggered the event
				
				if(ret > 0) {
					switch(pollType) {
						case NL_READ_STATUS: if(!pushNewDataEvent()) return false; break;
						case NL_ERROR_STATUS: if(!pushErrorEvent()) return false; break;
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
		else
			localStr = "ERRORLOCALADDR(" + GetLastErrorAndReset() + ")";
		ret += " " + localStr;
	}
	if(m_state == NSS_CONNECTED) {
		ret += " connected to ";
		std::string remoteStr = "INVALIDREMOTE";
		NetworkAddr addr;
		if(nlGetRemoteAddr(m_socket->sock, getNLaddr(addr)) != NL_FALSE)
			NetAddrToString(remoteAddress(), remoteStr);
		else
			remoteStr = "ERRORREMOTEADDR(" + GetLastErrorAndReset() + ")";
		ret += remoteStr;
	}
	return ret;
}

Result NetworkSocket::OpenReliable(Port port) {
	if(isOpen()) {
		warnings << "NetworkSocket " << debugString() << ": OpenReliable: socket is already opened, reopening now" << endl;
		Close();
	}
	
	NLsocket ret = nlOpen(port, NL_RELIABLE);
	if (ret == NL_INVALID)
		return "OpenReliableSocket: " + GetLastErrorAndReset();
	m_socket->sock = ret;
	m_type = NST_TCP;
	m_state = NSS_NONE;
	checkEventHandling();
	return true;
}

Result NetworkSocket::OpenUnreliable(Port port) {
	if(isOpen()) {
		warnings << "NetworkSocket " << debugString() << ": OpenReliable: socket is already opened, reopening now" << endl;
		Close();
	}

	NLsocket ret = nlOpen(port, NL_UNRELIABLE);
	if (ret == NL_INVALID)
		return "OpenUnreliableSocket: " + GetLastErrorAndReset();
	m_socket->sock = ret;
	m_type = NST_UDP;
	m_state = NSS_NONE;
	checkEventHandling();
	return true;
}

Result NetworkSocket::OpenBroadcast(Port port) {
	if(isOpen()) {
		warnings << "NetworkSocket " << debugString() << ": OpenBroadcast: socket is already opened, reopening now" << endl;
		Close();
	}

	NLsocket ret = nlOpen(port, NL_BROADCAST);
	if (ret == NL_INVALID)
		return "OpenBroadcastSocket: " + GetLastErrorAndReset();
	m_socket->sock = ret;
	m_type = NST_UDPBROADCAST;
	m_state = NSS_NONE;
	checkEventHandling();
	return true;	
}

Result NetworkSocket::Connect(const NetworkAddr& addr) {
	if(!isOpen()) {
		errors << "NetworkSocket::Connect: socket is closed" << endl;
		return "closed";
	}
	
	if(m_type != NST_TCP) {
		errors << "NetworkSocket::Connect " << debugString() << ": connect only works with TCP" << endl;
		return "not TCP";
	}
	
	if(nlConnect(m_socket->sock, getNLaddr(addr)) == NL_FALSE)
		return "Connect: " + GetLastErrorAndReset();
	
	checkEventHandling();
	return true;
}

Result NetworkSocket::Listen() {
	if(!isOpen())
		return "NetworkSocket::Listen: socket is closed";
	
	if(nlListen(m_socket->sock) == NL_FALSE)
		return "Listen: " + GetLastErrorAndReset();
	
	checkEventHandling();
	return true;
}

void NetworkSocket::Close() {
	if(!isOpen()) {
		warnings << "NetworkSocket::Close: cannot close already closed socket" << endl;
		return;
	}
	
	if(m_type != NST_TCP) {
		nlClose(m_socket->sock);
	}
	else {
		// Do it in a separate thread because it might block for a while.
		CloseSocketWorker* worker = new CloseSocketWorker();
		worker->name = "close socket";
		worker->sock = m_socket->sock;
		taskManager->start(worker, TaskManager::QT_GlobalQueue);
	}

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
	if(nbytes < 0) {
		errors << "WriteSocket " << debugString() << ": nbytes < 0" << endl;
		return NL_INVALID;
	}
	
	ResetSocketError();
	NLint ret = nlWrite(m_socket->sock, buffer, nbytes);

	// Error checking
	if (ret == NL_INVALID)  {
#ifdef DEBUG
		std::string errStr = GetLastErrorStr(); // cache errStr that debugString will not overwrite it
		errors << "WriteSocket " << debugString() << ": " << errStr << endl;
#endif
		ResetSocketError();
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
#endif			
		}
		ResetSocketError();
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
		std::string errMsg = GetLastErrorAndReset();
		errors << "NetworkSocket::localAddress: cannot get local address (" << debugString() << "): " << errMsg << endl;
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
		std::string errMsg = GetLastErrorAndReset();
		errors << "NetworkSocket::remoteAddress: cannot get remote address" << "(" << debugString() << "): " << errMsg << endl;
		return addr;
	}
	
	return addr;
}

Result NetworkSocket::setRemoteAddress(const NetworkAddr& addr) {
	if(!isOpen())
		return "NetworkSocket::setRemoteAddress: socket is closed";
	
	if(getNLaddr(addr) == NULL)
		return "NetworkSocket::setRemoteAddress " + debugString() + ": given address is invalid";
	if( GetNetAddrPort(addr) == 0 )
		return "NetworkSocket::setRemoteAddress " + debugString() + ": port is set to 0";
	
	if(nlSetRemoteAddr(m_socket->sock, getNLaddr(addr)) == NL_FALSE) {
		std::string addrStr = "INVALIDADDR";
		NetAddrToString(addr, addrStr);
		std::string errMsg = GetLastErrorAndReset();
		return "NetworkSocket::setRemoteAddress " + debugString() + ": failed to set destination " + addrStr + ": " + errMsg;
	}
	
	return true;
}










int GetSocketErrorNr() {
	return (int)nlGetError();
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

static bool isStringValidIP(const std::string& str) {
	size_t p = 0;
	int numC = 0;
	int numLen = 0;
	bool readingPort = false;
	while(p < str.size()) {
		if(str[p] == '.') {
			if (numC >= 3)
				return false;
			if (!numLen)
				return false;
			if (readingPort)
				return false;

			numC++; numLen = 0;
			p++; continue;
		}
		
		if(str[p] == ':') {
			if (numC < 3)
				return false;
			if (!numLen)
				return false;
			if (readingPort)
				return false;

			readingPort = true;
			numC++; numLen = 0;
			p++; continue;
		}
		
		if(str[p] >= '0' && str[p] <= '9') {
			numLen++;
			if(numLen > (readingPort ? 5 : 3))
				return false;

			p++; continue;
		}
		
		return false;
	}
	
	return (numC >= 3 && numLen > 0);
}

// accepts "%i.%i.%i.%i[:%l]" as input
Result StringToNetAddr(const std::string& string, NetworkAddr& addr, std::string* errorStr) {
	if(getNLaddr(addr) == NULL) return false;
	
	if(!isStringValidIP(string)) {
		SetNetAddrValid(addr, false);
		return false;
	}
	
	if(nlStringToAddr(string.c_str(), getNLaddr(addr)) == NL_FALSE) {
		if(errorStr) *errorStr = GetLastErrorStr();
		return "StringToNetAddr: cannot use " + string + " as address: " + GetLastErrorAndReset();
	}
	
	return true;
}

Result NetAddrToString(const NetworkAddr& addr, std::string& string) {
	// TODO: safty here for buffer
	char buf[256];
	NLchar* res = nlAddrToString(getNLaddr(addr), buf);
	if(res) {
		fix_markend(buf);
		string = buf;
		return true;
	} else
		return false;
}

NetworkAddr StringToNetAddr(const std::string& string) { 
	NetworkAddr ret; 
	ResetNetAddr(ret); 
	StringToNetAddr(string, ret); 
	return ret; 
}

std::string NetAddrToString(const NetworkAddr& addr) { 
	std::string ret; 
	NetAddrToString(addr, ret); 
	return ret; 
}

unsigned short GetNetAddrPort(const NetworkAddr& addr) {
	if(getNLaddr(addr) == NULL)
		return 0;
	else
		return nlGetPortFromAddr(getNLaddr(addr));
}

Result SetNetAddrPort(NetworkAddr& addr, unsigned short port, std::string* errorStr) {
	if(getNLaddr(addr) == NULL)
		return "SetNetAddrPort: addr not set";
	else {
		if(nlSetAddrPort(getNLaddr(addr), port) == NL_FALSE) {
			if(errorStr) *errorStr = GetLastErrorStr();
			return "SetNetAddrPort: cannot set port " + itoa(port) + ": " + GetLastErrorAndReset();
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
static bool GetAddrFromNameAsync_Internal(const NLchar* name, NLaddress* address) {
	struct hostent *hostentry;
    NLushort    port = 0;
    int			pos;
    NLbyte      temp[NL_MAX_STRING_LENGTH];
	
#ifdef _UNICODE
    /* convert from wide char string to multibyte char string */
    (void)wcstombs(temp, (const NLchar *)name, NL_MAX_STRING_LENGTH);
#else
    strncpy(temp, name, NL_MAX_STRING_LENGTH);
#endif
    temp[NL_MAX_STRING_LENGTH - 1] = (NLbyte)'\0';
    pos = (int)strcspn(temp, (const char *)":");
    if(pos > 0)
    {
        NLbyte      *p = &temp[pos+1];
		
        temp[pos] = (NLbyte)'\0';
        (void)sscanf(p, "%hu", &port);
    }
    hostentry = gethostbyname((const char *)temp);
	
    if(hostentry != NULL)
    {
        ((struct sockaddr_in *)address)->sin_family = AF_INET;
        ((struct sockaddr_in *)address)->sin_port = htons(port);
        ((struct sockaddr_in *)address)->sin_addr.s_addr = (uint32_t) *(NLulong *)hostentry->h_addr_list[0];
        address->valid = NL_TRUE;
    }
    else
    {
        ((struct sockaddr_in *)address)->sin_family = AF_INET;
        ((struct sockaddr_in *)address)->sin_addr.s_addr = INADDR_NONE;
        ((struct sockaddr_in *)address)->sin_port = 0;
        nlSetError(NL_SYSTEM_ERROR);
        return false;
    }
    return true;
}

static std::set<std::string> PendingDnsQueries;
static Mutex PendingDnsQueriesMutex;

bool GetNetAddrFromNameAsync(const std::string& name, NetworkAddr& addr)
{
	// We don't use nlGetAddrFromNameAsync here because we have to use SmartPointers
	// The problem is, if you call this and then delete the memory of the network address
	// while the thread isn't ready, it will write after to deleted memory.

	if(getNLaddr(addr) == NULL)
		return false;

	if(name == "") {
		SetNetAddrValid(addr, false);
		return false;
	}
	
	if (StringToNetAddr(name, addr))  {
		return true;
	}
	ResetSocketError(); // Clear the bad address error
	
	if(GetFromDnsCache(name, addr)) {
		SetNetAddrValid(addr, true);
		return true;
	}

    getNLaddr(addr)->valid = NL_FALSE;

	{
		Mutex::ScopedLock l(PendingDnsQueriesMutex);
		if(PendingDnsQueries.find(name) != PendingDnsQueries.end())
			return true;
		PendingDnsQueries.insert(name);
	}

	struct GetAddrFromNameAsync_Executer : Task {
		std::string addr_name;
		NetAddrInternal::Ptr_t address;
		
		Result handle() {
			if(GetAddrFromNameAsync_Internal(addr_name.c_str(), address.get())) {
				// TODO: we use default DNS record expire time of 1 hour, we should include some DNS client to make it in correct way
				AddToDnsCache(addr_name, NetworkAddr(NetAddrInternal(*address.get())));
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
    data->address = NetworkAddrData(addr).getPtr();

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

