/*
  HawkNL cross platform network library
  Copyright (C) 2000-2002 Phil Frisbie, Jr. (phil@hawksoft.com)

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
  
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Library General Public License for more details.
    
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA  02111-1307, USA.
      
  Or go to http://www.gnu.org/copyleft/lgpl.html
*/

#define FD_SETSIZE      8192
//#define NL_DEBUG        DEBUG

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


#include "nlinternal.h"
#include "sock.h"

/* internally for TCP packets and UDP connections, all data is big endien,
   so we force it so here using these macros */
#undef writeShort
#define writeShort(x, y, z)     {*((NLushort *)((NLbyte *)&x[y])) = htons(z); y += 2;}
#undef readShort
#define readShort(x, y, z)      {z = ntohs(*(NLushort *)((NLbyte *)&x[y])); y += 2;}


#ifndef IN_MULTICAST
#define IN_MULTICAST(i) (((unsigned long)(i) & 0xF0000000) == (unsigned long)0xE0000000)
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN      NL_MAX_STRING_LENGTH
#endif

#define NL_CONNECT_STRING   "HawkNL request connection."
#define NL_REPLY_STRING     "HawkNL connection OK."
#define NL_HEADER_LEN       4

/*
* Portable NLMAX() function macro
*/
#define NLMAX( a, b )             ( ( a ) > ( b ) ? ( a ) : ( b ) )
#define NLMIN( a, b )             ( ( a ) < ( b ) ? ( a ) : ( b ) )

static volatile int backlog = SOMAXCONN;
static volatile int multicastTTL = 1;
static volatile NLboolean reuseaddress = NL_FALSE;
static volatile NLboolean nlTCPNoDelay = NL_FALSE;

static NLaddress *alladdr = NULL;
static struct in6_addr bindaddress;
static struct in6_addr in6addr_multicast;
static struct in6_addr in6addr_ipv4mapped;
static struct in6_addr in6addr_ipv4broadcast;

typedef struct
{
    NLaddress   /*@temp@*/*address;
    NLchar      /*@temp@*/*name;
    NLsocket    socket;
} NLaddress_ex_t;

extern SOCKET nlGroupGetFdset(NLint group, /*@out@*/ fd_set *fd);

/*
This is a Winsock work around to be able to bind() to more than 3976 ports
*/

#ifdef WINDOWS_APP

static NLmutex  portlock; /* In memory of my step-father, Don Portlock,
who passed away Jan 12, 2001 - Phil */

static volatile NLushort nextport = 1024;

static NLushort sock_getNextPort(void)
{
    NLlong temp;

    (void)nlMutexLock(&portlock);
    temp = (NLlong)nextport;
    if(++temp > 65535)
    {
        /* skip the well known ports */
        temp = 1024;
    }
    nextport = (NLushort)temp;
    (void)nlMutexUnlock(&portlock);
    return nextport;
}

static NLint sock_bind(SOCKET socket, const struct sockaddr *a, int len)
{
    struct sockaddr_in6 *addr = (struct sockaddr_in6 *)a;
    int                 ntries = 500; /* this is to prevent an infinite loop */
    NLboolean           found = NL_FALSE;
    
    /* check to see if the port is already specified */
    if(addr->sin6_port != 0)
    {
        /* do the normal bind */
        return bind(socket, a, len);
    }
    
    /* let's find our own port number */
    while(ntries-- > 0)
    {
        addr->sin6_port = htons(sock_getNextPort());
        if(bind(socket, (struct sockaddr *)addr, len) != SOCKET_ERROR)
        {
            found = NL_TRUE;
            break;
        }
    }
    if(found == NL_TRUE)
    {
        return 0;
    }
    /* could not find a port, restore the port number back to 0 */
    addr->sin6_port = 0;
    /*  return error */
    return SOCKET_ERROR;
}

static int sock_connect(SOCKET socket, const struct sockaddr* a, int len )
{
    struct sockaddr_in6  addr;

    memset(&addr, 0, sizeof(addr));

    addr.sin6_family = AF_INET6;
    memcpy(&addr.sin6_addr, &in6addr_any, sizeof(in6addr_any));
    addr.sin6_port = 0;

    if(sock_bind(socket, (struct sockaddr *)&addr, (int)sizeof(addr)) == SOCKET_ERROR)
    {
        return SOCKET_ERROR;
    }
    return connect(socket, a, len);
}

#else
#define sock_bind bind
#define sock_connect connect
#endif /* WINDOWS_APP */

/*
helper functions for NL_RELIABLE_PACKETS
*/

static NLint    rpGroup;                /* the group to hold all the NL_RELIABLE_PACKETS sockets */
static NLmutex  rpMutex;                /* mutex to lock the functions */
static NLboolean needThread = NL_TRUE;  /* do we need to spawn a thread? */
static NLint    rpSocketCount = 0;      /* total count of NL_RELIABLE_PACKETS sockets */
static volatile NLint rpBufferedCount = 0;/* count of sockets that are buffering data */

static void sock_WritePacketCheckPending(NLsocket socket)
{
    nl_socket_t *sock = nlSockets[socket];
    
    /* first check for data waiting to be sent */
    if(sock->sendlen > 0)
    {
        NLint       count, size = NL_MAX_PACKET_LENGTH + NL_HEADER_LEN;

        if(size > sock->sendlen)
        {
            size = sock->sendlen;
        }
        count = send((SOCKET)sock->realsocket, (char *)sock->outbuf, size, 0);
        if(count > 0)
        {
            sock->sendlen -= count;
            if(sock->sendlen > 0)
            {
                /* move remaining data to beginning of outbuf */
                memmove(sock->outbuf, (sock->outbuf + count), (size_t)sock->sendlen);
            }
            else
            {
                rpBufferedCount--;
            }
       }
    }
}

static void *sock_rpThread(void *p)
{
    static NLsocket *sockets = NULL;
    static NLint    maxsockets = 0;

    /* allocate memory */
    if(sockets == NULL)
    {
        sockets = (NLsocket *)malloc(sizeof(NLsocket *) * 16);
        if(sockets == NULL)
        {
            needThread = NL_TRUE;
            return NULL;
        }
        maxsockets = 16;
    }
    while(rpGroup != NL_INVALID)
    {
        NLint count = maxsockets;

        /* make sure there is something to do */
        if(rpSocketCount == 0 || rpBufferedCount == 0)
        {
            goto loopend;
        }
        /* see if we need to allocate more memory */
        if(maxsockets < rpSocketCount)
        {
            NLsocket *temp;
            
            while(maxsockets < rpSocketCount)
            {
                maxsockets *= 2;
            }
            temp = (NLsocket *)realloc((void *)sockets, sizeof(NLsocket *) * maxsockets);
            if(temp == NULL)
            {
                goto cleanup;
            }
            sockets = temp;
        }
        /* we can now get the sockets */
        if(nlGroupGetSockets(rpGroup, sockets, &count) == NL_FALSE)
        {
            goto cleanup;
        }
        while(count-- > 0 && rpBufferedCount > 0)
        {
            NLsocket socket = sockets[count];

            (void)nlLockSocket(socket, NL_WRITE);
            sock_WritePacketCheckPending(socket);
            nlUnlockSocket(socket, NL_WRITE);
        }
loopend:
        nlThreadSleep(50);
    }
cleanup:
    free(sockets);
    sockets = NULL;
    maxsockets = 0;
    needThread = NL_TRUE;
    return p;
}

static void sock_AddSocket(NLsocket socket)
{
    (void)nlMutexLock(&rpMutex);
    (void)nlGroupAddSocket(rpGroup, socket);
    rpSocketCount++;
    if(needThread == NL_TRUE)
    {
        (void)nlThreadCreate(sock_rpThread, NULL, NL_FALSE);
        needThread = NL_FALSE;
    }
    (void)nlMutexUnlock(&rpMutex);
}

static void sock_DeleteSocket(NLsocket socket)
{
    (void)nlMutexLock(&rpMutex);
    (void)nlGroupDeleteSocket(rpGroup, socket);
    rpSocketCount--;
    (void)nlMutexUnlock(&rpMutex);
}

/*
handle some common connection errors so the app knows when a connection has been dropped
*/

static NLint sock_Error(void)
{
    switch (sockerrno) {
        
#ifdef WINDOWS_APP
    case WSABASEERR:
        return 0;
#endif
        
    case EWOULDBLOCK:
        return 0;
        
    case ENETRESET:
    case EHOSTUNREACH:
    case ECONNABORTED:
    case ECONNRESET:
    case ENETUNREACH:
    case ETIMEDOUT:
        nlSetError(NL_SOCK_DISCONNECT);
        break;
        
    default:
        nlSetError(NL_SYSTEM_ERROR);
        break;
    }
    
    return NL_INVALID;
}

static NLboolean sock_SetNonBlocking(SOCKET socket)
{
    int rc;
    unsigned long i = 1;
    
    rc = ioctl(socket, FIONBIO, &i);
    if(rc == SOCKET_ERROR)
    {
        return NL_FALSE;
    }
    return NL_TRUE;
}

static NLboolean sock_SetBroadcast(SOCKET realsocket)
{
    int i = 1;
    int default_iface = 0;

    // Needed for IPv4 broadcast socket to work, for both IPv4 and dial-stack sockets
    // I'm not sure whether it will work for IPv6 sockets on different OSes, but it's required on Linux
#ifdef NL_DEBUG
    printf("%s: sock %d setsockopt(SO_BROADCAST)\n", __FUNCTION__, realsocket);
#endif
    if(setsockopt(realsocket, SOL_SOCKET, SO_BROADCAST, (char *)&i, (int)sizeof(i)) == SOCKET_ERROR)
    {
#ifdef NL_DEBUG
        printf("%s: sock %d setsockopt(SO_BROADCAST) failed\n", __FUNCTION__, realsocket);
#endif
        //nlSetError(NL_SYSTEM_ERROR);
        //return NL_FALSE;
    }

#ifdef NL_DEBUG
    printf("%s: sock %d setsockopt(IPV6_MULTICAST_IF)\n", __FUNCTION__, realsocket);
#endif
    // Enable IPv6 multicast
    if(setsockopt(realsocket, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char*)&default_iface, sizeof(default_iface)) == SOCKET_ERROR)
    {
#ifdef NL_DEBUG
        printf("%s: sock %d setsockopt(IPV6_MULTICAST_IF) failed\n", __FUNCTION__, realsocket);
#endif
        //nlSetError(NL_SYSTEM_ERROR);
        //return NL_FALSE;
    }

    return NL_TRUE;
}

static void sock_SetReuseAddr(SOCKET socket)
{
    int i = 1;
    
    if(reuseaddress == NL_FALSE)
    {
        return;
    }
    if(setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&i, (int)sizeof(i)) == SOCKET_ERROR)
    {
        nlSetError(NL_SYSTEM_ERROR);
    }
}

static void sock_SetTCPNoDelay(SOCKET socket)
{
    int i = 1;
    
    if(setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, (char *)&i, (int)sizeof(i)) == SOCKET_ERROR)
    {
        nlSetError(NL_SYSTEM_ERROR);
    }
}

static NLsocket sock_SetSocketOptions(NLsocket s)
{
    nl_socket_t     *sock = nlSockets[s];
    NLenum          type = sock->type;
    SOCKET          realsocket = (SOCKET)sock->realsocket;
    NLint           v6only = 0;
    
    if(setsockopt(realsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only)) == SOCKET_ERROR)
    {
        nlSetError(NL_SYSTEM_ERROR);
        return INVALID_SOCKET;
    }
    
    if(type == NL_RELIABLE || type == NL_RELIABLE_PACKETS)
    {
        sock->reliable = NL_TRUE;
        if(nlTCPNoDelay == NL_TRUE)
        {
            sock_SetTCPNoDelay(realsocket);
        }
        if(type == NL_RELIABLE_PACKETS)
        {
            sock->packetsync = NL_TRUE;
        }
    }
    else
    {
        sock->reliable = NL_FALSE;
    }
    
    sock_SetReuseAddr(realsocket);
    
    if(sock->blocking == NL_FALSE)
    {
        if(sock_SetNonBlocking(realsocket) == NL_FALSE)
        {
            nlSetError(NL_SYSTEM_ERROR);
            return NL_INVALID;
        }
    }
#ifdef NL_DEBUG
    printf("%s: new socket %d type %d udp %d broadcast %d\n", __FUNCTION__, realsocket, (int)type, !sock->reliable, sock->type == NL_BROADCAST);
#endif
    
    return s;
}

static void sock_SetSocketOptionsMulticast(NLsocket s)
{
    nl_socket_t     *sock = nlSockets[s];
    NLenum          type = sock->type;
    SOCKET          realsocket = (SOCKET)sock->realsocket;
    // Join IPv6 multicast group, so we can receive IPv4 broadcast and IPv6 multicast packets
    struct ipv6_mreq multicast;

    if(type == NL_RELIABLE || type == NL_RELIABLE_PACKETS)
    {
        return;
    }

    memset(&multicast, 0, sizeof(multicast));
    memcpy(&multicast.ipv6mr_multiaddr, &in6addr_multicast, sizeof(in6addr_multicast));
    multicast.ipv6mr_interface = 0; // Any interface

#ifdef NL_DEBUG
    char tmp[128];
    inet_ntop(AF_INET6, &multicast.ipv6mr_multiaddr, tmp, sizeof(tmp));
    printf("%s: sock %d setsockopt(IPV6_JOIN_GROUP) addr %s\n", __FUNCTION__, realsocket, tmp);
#endif

    if(setsockopt(realsocket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&multicast, sizeof(multicast)) == SOCKET_ERROR)
    {
#ifdef NL_DEBUG
        printf("%s: sock %d setsockopt(IPV6_JOIN_GROUP) failed, trying again with in6addr_any\n", __FUNCTION__, realsocket);
#endif
        // Attempt again with wildcard address
        memcpy(&multicast.ipv6mr_multiaddr, &in6addr_any, sizeof(in6addr_any));
        multicast.ipv6mr_interface = 0; // Any interface
        if(setsockopt(realsocket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&multicast, sizeof(multicast)) == SOCKET_ERROR)
        {
#ifdef NL_DEBUG
            printf("%s: sock %d setsockopt(IPV6_JOIN_GROUP) with in6addr_any failed\n", __FUNCTION__, realsocket);
#endif
            //nlSetError(NL_SYSTEM_ERROR);
            //return NL_INVALID;
        }
    }
}

static NLushort sock_GetPort(SOCKET socket)
{
    struct sockaddr_in6  addr;
    socklen_t            len;
    
    len = (socklen_t)sizeof(struct sockaddr_in6);
    if(getsockname(socket, (struct sockaddr *) &addr, &len) == SOCKET_ERROR)
    {
        return 0;
    }
    
    return ntohs(addr.sin6_port);
}

NLboolean sock_Init(void)
{
#ifdef WINDOWS_APP
    WSADATA libmibWSAdata;
    
    /* first try Winsock 2.0 */
    if(WSAStartup(MAKEWORD(2, 0),&libmibWSAdata) != 0)
    {
        /* Winsock 2.0 failed, so now try 1.1 */
        if(WSAStartup(MAKEWORD(1, 1),&libmibWSAdata) != 0)
        {
            return NL_FALSE;
        }
    }
    if(nlMutexInit(&portlock) == NL_FALSE)
    {
        return NL_FALSE;
    }
#endif
    if(nlMutexInit(&rpMutex) == NL_FALSE)
    {
        return NL_FALSE;
    }
    if((rpGroup = nlGroupCreate()) == NL_INVALID)
    {
        return NL_FALSE;
    }
    
    memcpy(&bindaddress, &in6addr_any, sizeof(in6addr_any));
    inet_pton(AF_INET6, "ff04::4f70:656e:4c69:6572:6f58", &in6addr_multicast); // OpenLieroX IPv6 multicast address
    inet_pton(AF_INET6, "::ffff:0.0.0.0", &in6addr_ipv4mapped);
    inet_pton(AF_INET6, "::ffff:255.255.255.255", &in6addr_ipv4broadcast);

    return NL_TRUE;
}

void sock_Shutdown(void)
{
#ifdef WINDOWS_APP
    (void)WSACleanup();
    (void)nlMutexDestroy(&portlock);
    (void)nlMutexDestroy(&rpMutex);
    (void)nlGroupDestroy(rpGroup);
    rpGroup = NL_INVALID;
#endif
    if(alladdr != NULL)
    {
        free(alladdr);
        alladdr = NULL;
    }
}

NLboolean sock_Listen(NLsocket socket)
{
    nl_socket_t *sock = nlSockets[socket];

    if(sock->listen == NL_TRUE)
    {
        return NL_TRUE;
    }
    if(sock->reliable == NL_TRUE) /* TCP */
    {
        /* check for unbound socket */
        if(sock->localport == 0)
        {
            /* bind socket */
            ((struct sockaddr_in6 *)&sock->addressin)->sin6_family = AF_INET6;
            memcpy(&((struct sockaddr_in6 *)&sock->addressin)->sin6_addr, &bindaddress, sizeof(bindaddress));
            ((struct sockaddr_in6 *)&sock->addressin)->sin6_port = 0;

            if(sock_bind((SOCKET)sock->realsocket, (struct sockaddr *)&sock->addressin, (int)sizeof(struct sockaddr_in6)) == SOCKET_ERROR)
            {
                nlSetError(NL_SYSTEM_ERROR);
                return NL_FALSE;
            }
        }
        if(listen((SOCKET)sock->realsocket, backlog) == SOCKET_ERROR)
        {
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
    }

    sock->listen = NL_TRUE;
    return NL_TRUE;
}

static SOCKET sock_AcceptUDP(NLsocket nlsocket, /*@out@*/struct sockaddr_in6 *newaddr)
{
    nl_socket_t         *sock = nlSockets[nlsocket];
    struct sockaddr_in6 ouraddr;
    SOCKET              newsocket;
    NLushort            localport;
    NLbyte              buffer[NL_MAX_STRING_LENGTH];
    socklen_t           len = (socklen_t)sizeof(struct sockaddr_in6);
    NLint               slen = (NLint)sizeof(NL_CONNECT_STRING);
    NLbyte              reply = (NLbyte)0x00;
    NLint               count = 0;
    NLint               v6only = 0;
    
    /* Get the packet and remote host address */
    if(recvfrom((SOCKET)sock->realsocket, buffer, (int)sizeof(buffer), 0,
        (struct sockaddr *)newaddr, &len) < (int)sizeof(NL_CONNECT_STRING))
    {
        nlSetError(NL_NO_PENDING);
        return INVALID_SOCKET;
    }
    /* Let's check for the connection string */
    buffer[slen - 1] = (NLbyte)0; /* null terminate for peace of mind */
    if(strcmp(buffer, NL_CONNECT_STRING) != 0)
    {
        nlSetError(NL_NO_PENDING);
        return INVALID_SOCKET;
    }
    /* open up a new socket on this end */
    newsocket = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
    if(newsocket == INVALID_SOCKET)
    {
        nlSetError(NL_SYSTEM_ERROR);
        (void)closesocket(newsocket);
        return INVALID_SOCKET;
    }

    if(setsockopt(newsocket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only)) == SOCKET_ERROR)
    {
        nlSetError(NL_SYSTEM_ERROR);
        (void)closesocket(newsocket);
        return INVALID_SOCKET;
    }
    
    memset(&ouraddr, 0, sizeof(ouraddr));
    ouraddr.sin6_family = AF_INET6;
    memcpy(&ouraddr.sin6_addr, &bindaddress, sizeof(bindaddress));
    /* system assigned port number */
    ouraddr.sin6_port = 0;
    
    if(sock_bind(newsocket, (struct sockaddr *)&ouraddr, len) == SOCKET_ERROR)
    {
        nlSetError(NL_SYSTEM_ERROR);
        (void)closesocket(newsocket);
        return INVALID_SOCKET;
    }
    sock_SetSocketOptionsMulticast(nlsocket);
    /* get the new port */
    localport = sock_GetPort(newsocket);
#ifdef NL_DEBUG
    char tmp[128];
    NLaddress tmpaddr;
    memcpy(&tmpaddr, &ouraddr, sizeof(struct sockaddr_in6));
    tmpaddr.valid = NL_TRUE;
    nlAddrToString(&tmpaddr, tmp);
    printf("%s: sock %d bound to addr %s port %d\n", __FUNCTION__, newsocket, tmp, localport);
#endif
    
    /* create the return message */
    writeShort(buffer, count, localport);
    writeString(buffer, count, (NLchar *)TEXT(NL_REPLY_STRING));
    
    /* send back the reply with our new port */
    if(sendto((SOCKET)sock->realsocket, buffer, count, 0, (struct sockaddr *)newaddr,
        (int)sizeof(struct sockaddr_in6)) < count)
    {
        nlSetError(NL_SYSTEM_ERROR);
        (void)closesocket(newsocket);
        return INVALID_SOCKET;
    }
    /* send back a 0 length packet from our new port, needed for firewalls */
    if(sendto(newsocket, &reply, 0, 0,
        (struct sockaddr *)newaddr,
        (int)sizeof(struct sockaddr_in6)) < 0)
    {
        nlSetError(NL_SYSTEM_ERROR);
        (void)closesocket(newsocket);
        return INVALID_SOCKET;
    }
    /* connect the socket */
    if(connect(newsocket, (struct sockaddr *)newaddr,
        (int)sizeof(struct sockaddr_in6)) == SOCKET_ERROR)
    {
        nlSetError(NL_SYSTEM_ERROR);
        (void)closesocket(newsocket);
        return INVALID_SOCKET;
    }
    
    return newsocket;
}

NLsocket sock_AcceptConnection(NLsocket socket)
{
    nl_socket_t         *sock = nlSockets[socket];
    nl_socket_t         *newsock = NULL;
    NLsocket            newsocket = NL_INVALID;
    SOCKET              realsocket = INVALID_SOCKET;
    struct sockaddr_in6 newaddr;
    socklen_t           len = (socklen_t)sizeof(newaddr);
    
    memset(&newaddr, 0, sizeof(newaddr));
    if(sock->listen != NL_TRUE)
    {
        nlSetError(NL_NOT_LISTEN);
        return NL_INVALID;
    }
    
    if(sock->type == NL_RELIABLE || sock->type == NL_RELIABLE_PACKETS) /* TCP */
    {
        SOCKET s = 101;
        
        /* !@#$% metrowerks compiler, try to get it to actually produce some code */
        s = accept((SOCKET)(sock->realsocket), (struct sockaddr *)&newaddr, &len);
        
        realsocket = s;
        if(realsocket == INVALID_SOCKET)
        {
            if(sockerrno == EWOULDBLOCK || errno == EAGAIN)/* yes, we need to use errno here */
            {
                nlSetError(NL_NO_PENDING);
            }
            else
            {
                nlSetError(NL_SYSTEM_ERROR);
            }
            return NL_INVALID;
        }
        
    }
    else if(sock->type == NL_UNRELIABLE)/* UDP*/
    {
        realsocket = sock_AcceptUDP(socket, &newaddr);
        
        if(realsocket == INVALID_SOCKET)
        {
            /* error is already set in sock_AcceptUDP */
            return NL_INVALID;
        }
    }
    else /* broadcast or multicast */
    {
        nlSetError(NL_WRONG_TYPE);
        return NL_INVALID;
    }
    
    newsocket = nlGetNewSocket();
    if(newsocket == NL_INVALID)
    {
        return NL_INVALID;
    }
    if(nlLockSocket(newsocket, NL_BOTH) == NL_FALSE)
    {
        return NL_INVALID;
    }
    newsock = nlSockets[newsocket];
    
    /* update the remote address */
    memcpy((char *)&newsock->addressin, (char *)&newaddr, sizeof(struct sockaddr_in6));
    newsock->realsocket = (NLint)realsocket;
    newsock->localport = sock_GetPort(realsocket);
    newsock->remoteport = sock_GetPortFromAddr((NLaddress *)&newsock->addressin);
    
    newsock->type = sock->type;
    newsock->connected = NL_TRUE;
    if(newsock->type == NL_RELIABLE_PACKETS)
    {
        sock_AddSocket(newsocket);
    }
    return sock_SetSocketOptions(newsocket);
}

NLsocket sock_Open(NLushort port, NLenum type)
{
    nl_socket_t *newsock;
    NLsocket    newsocket;
    SOCKET      realsocket;
    
    switch (type) {
        
    case NL_RELIABLE: /* TCP */
    case NL_RELIABLE_PACKETS: /* TCP packets */
        realsocket = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
        break;
        
    case NL_UNRELIABLE: /* UDP */
    case NL_BROADCAST:  /* UDP broadcast */
    case NL_UDP_MULTICAST:  /* UDP multicast */
        realsocket = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        break;
        
    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_INVALID;
    }
    
    if(realsocket == INVALID_SOCKET)
    {
        nlSetError(NL_SYSTEM_ERROR);
        return NL_INVALID;
    }
    
    newsocket = nlGetNewSocket();
    if(newsocket == NL_INVALID)
    {
        return NL_INVALID;
    }
    if(nlLockSocket(newsocket, NL_BOTH) == NL_FALSE)
    {
        return NL_INVALID;
    }
    newsock = nlSockets[newsocket];
    newsock->realsocket = (NLint)realsocket;
    newsock->type = type;
    
    if(sock_SetSocketOptions(newsocket) == NL_INVALID)
    {
        nlUnlockSocket(newsocket, NL_BOTH);
        (void)sock_Close(newsocket);
        return NL_INVALID;
    }
    /* do not bind a TCP socket here if the port is 0; let connect assign the port */
    if((type == NL_RELIABLE || type == NL_RELIABLE_PACKETS) && port == 0)
    {
        newsock->localport = 0;
    }
    else
    {
        memset(((struct sockaddr_in6 *)&newsock->addressin), 0, sizeof(struct sockaddr_in6));
        ((struct sockaddr_in6 *)&newsock->addressin)->sin6_family = AF_INET6;
        memcpy(&(((struct sockaddr_in6 *)&newsock->addressin)->sin6_addr), &bindaddress, sizeof(bindaddress));
        ((struct sockaddr_in6 *)&newsock->addressin)->sin6_port = htons((unsigned short)port);
        newsock->addressin.valid = NL_TRUE;
        
        if(sock_bind(realsocket, (struct sockaddr *)&newsock->addressin, (int)sizeof(struct sockaddr_in6)) == SOCKET_ERROR)
        {
            nlSetError(NL_SYSTEM_ERROR);
            nlUnlockSocket(newsocket, NL_BOTH);
            (void)sock_Close(newsocket);
            return NL_INVALID;
        }
        newsock->localport = sock_GetPort(realsocket);

#ifdef NL_DEBUG
        char tmp[128];
        nlAddrToString(&newsock->addressin, tmp);
        printf("%s: sock %d bound to addr %s port %d\n", __FUNCTION__, realsocket, tmp, newsock->localport);
#endif

        sock_SetSocketOptionsMulticast(newsocket);
        if(type == NL_BROADCAST)
        {
            if(sock_SetBroadcast(realsocket) == NL_FALSE)
            {
                nlSetError(NL_SYSTEM_ERROR);
                nlUnlockSocket(newsocket, NL_BOTH);
                (void)sock_Close(newsocket);
                return NL_INVALID;
            }
            memset(((struct sockaddr_in6 *)&newsock->addressout), 0, sizeof(struct sockaddr_in6));
            ((struct sockaddr_in6 *)&newsock->addressout)->sin6_family = AF_INET6;
            memcpy(&((struct sockaddr_in6 *)&newsock->addressout)->sin6_addr, &in6addr_ipv4broadcast, sizeof(in6addr_ipv4broadcast));
            ((struct sockaddr_in6 *)&newsock->addressout)->sin6_port = htons((unsigned short)port);
            newsock->addressout.valid = NL_TRUE;
        }
    }
    if(type == NL_RELIABLE_PACKETS)
    {
        sock_AddSocket(newsocket);
    }
    nlUnlockSocket(newsocket, NL_BOTH);
    return newsocket;
}

static NLboolean sock_ConnectUDP(NLsocket socket, const NLaddress *address)
{
    nl_socket_t     *sock = nlSockets[socket];
    time_t          begin, t;
    
    if(sendto((SOCKET)sock->realsocket, (char *)NL_CONNECT_STRING, (NLint)sizeof(NL_CONNECT_STRING),
        0, (struct sockaddr *)address, (int)sizeof(struct sockaddr_in6))
        == SOCKET_ERROR)
    {
        if(sock->blocking == NL_TRUE)
        {
            nlSetError(NL_SYSTEM_ERROR);
        }
        else
        {
            sock->conerror = NL_TRUE;
        }
        return NL_FALSE;
    }
    
    (void)time(&begin);
    
    /* try for six seconds */
    while((time(&t) - begin) < 6)
    {
        NLbyte              buffer[NL_MAX_STRING_LENGTH];
        NLbyte              *pbuffer = buffer;
        NLushort            newport;
        NLint               slen = (NLint)(sizeof(NL_REPLY_STRING) + sizeof(newport));
        NLint               received;
        NLbyte              reply = (NLbyte)0;
        socklen_t           len = (socklen_t)sizeof(struct sockaddr_in6);
        
        received = recvfrom((SOCKET)sock->realsocket, (char *)buffer, (int)sizeof(buffer), 0,
            (struct sockaddr *)&sock->addressin, &len);
        
        if(received == SOCKET_ERROR)
        {
            if(sockerrno != EWOULDBLOCK)
            {
                if(sock->blocking == NL_TRUE)
                {
                    nlSetError(NL_CON_REFUSED);
                }
                else
                {
                    sock->conerror = NL_TRUE;
                }
                return NL_FALSE;
            }
        }
        if(received >= slen)
        {
            NLint count = 0;
            
            /* retrieve the port number */
            readShort(buffer, count, newport);
            ((struct sockaddr_in6 *)&sock->addressin)->sin6_port = htons(newport);
            /* Lets check for the reply string */
            pbuffer[slen - 1] = (NLbyte)0; /* null terminate for peace of mind */
            pbuffer += sizeof(newport);
            if(strcmp(pbuffer, NL_REPLY_STRING) == 0)
            {
                if(connect((SOCKET)sock->realsocket, (struct sockaddr *)&sock->addressin,
                    (int)sizeof(struct sockaddr_in6)) == SOCKET_ERROR)
                {
                    if(sock->blocking == NL_TRUE)
                    {
                        nlSetError(NL_SYSTEM_ERROR);
                    }
                    else
                    {
                        sock->conerror = NL_TRUE;
                    }
                    return NL_FALSE;
                }
                /* send back a 0 length packet to the new port, needed for firewalls */
                if(send((SOCKET)sock->realsocket, &reply, 0, 0) == SOCKET_ERROR)
                {
                    if(sock->blocking == NL_TRUE)
                    {
                        nlSetError(NL_SYSTEM_ERROR);
                    }
                    else
                    {
                        sock->conerror = NL_TRUE;
                    }
                    return NL_FALSE;
                }
                /* success! */
                sock->localport = sock_GetPort((SOCKET)sock->realsocket);
                sock->remoteport = sock_GetPortFromAddr((NLaddress *)&sock->addressin);
                sock->connected = NL_TRUE;
                sock->connecting = NL_FALSE;
                return NL_TRUE;
            }
        }
        nlThreadSleep(NL_CONNECT_SLEEP);
    }
    
    if(sock->blocking == NL_TRUE)
    {
        nlSetError(NL_CON_REFUSED);
    }
    else
    {
        sock->conerror = NL_TRUE;
    }
    return NL_FALSE;
}

static void *sock_ConnectUDPAsynchInt(void /*@owned@*/*addr)
{
    NLaddress_ex_t *address = (NLaddress_ex_t *)addr;
    
    (void)sock_ConnectUDP(address->socket, address->address);
    free(addr);
    return NULL;
}

static NLboolean sock_ConnectUDPAsynch(NLsocket socket, const NLaddress *address)
{
    NLaddress_ex_t  /*@dependent@*/*addr;
    nl_socket_t     *sock = nlSockets[socket];
    
    addr = (NLaddress_ex_t *)malloc(sizeof(NLaddress_ex_t));
    if(addr == NULL)
    {
        nlSetError(NL_OUT_OF_MEMORY);
        return NL_FALSE;
    }
    addr->address = (NLaddress *)address;
    addr->socket = socket;
    sock->connecting = NL_TRUE;
    sock->conerror = NL_FALSE;
    if(nlThreadCreate(sock_ConnectUDPAsynchInt, (void *)addr, NL_FALSE) == (NLthreadID)NL_INVALID)
    {
        return NL_FALSE;
    }
    return NL_TRUE;
}

NLboolean sock_Connect(NLsocket socket, const NLaddress *address)
{
    nl_socket_t *sock = nlSockets[socket];
    
    memcpy((char *)&sock->addressin, (char *)address, sizeof(NLaddress));
    
    if(sock->type == NL_RELIABLE || sock->type == NL_RELIABLE_PACKETS)
    {
        
        if(sock_connect((SOCKET)sock->realsocket, (struct sockaddr *)&sock->addressin,
            (int)sizeof(struct sockaddr_in6)) == SOCKET_ERROR)
        {
            if(sock->blocking == NL_FALSE &&
                (sockerrno == EWOULDBLOCK || sockerrno == EINPROGRESS))
            {
                sock->connecting = NL_TRUE;
            }
            else
            {
                nlSetError(NL_SYSTEM_ERROR);
                return NL_FALSE;
            }
        }
        sock->localport = sock_GetPort((SOCKET)sock->realsocket);
        sock->remoteport = sock_GetPortFromAddr((NLaddress *)&sock->addressin);
        sock->connected = NL_TRUE;
        return NL_TRUE;
    }
    else if(sock->type == NL_UDP_MULTICAST)
    {
        nlSetError(NL_SYSTEM_ERROR);
        return NL_FALSE;
    }
    else if(sock->type == NL_UNRELIABLE)
    {
        if(sock->blocking == NL_TRUE)
        {
            return sock_ConnectUDP(socket, &sock->addressin);
        }
        else
        {
            return sock_ConnectUDPAsynch(socket, &sock->addressin);
        }
    }
    else
    {
        nlSetError(NL_WRONG_TYPE);
    }
    return NL_FALSE;
}

void sock_Close(NLsocket socket)
{
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
                nlThreadSleep(50);
            }
        }
        sock_DeleteSocket(socket);
    }
    if((sock->type == NL_RELIABLE_PACKETS || sock->type == NL_RELIABLE) && sock->listen == NL_FALSE)
    {
        struct linger l = {1, 10};

        (void)setsockopt((SOCKET)sock->realsocket, SOL_SOCKET, SO_LINGER, (const char *)&l, (int)sizeof(l));
    }
    (void)closesocket((SOCKET)sock->realsocket);
}

/* internal function to read reliable packets from TCP stream */
static NLint sock_ReadPacket(NLsocket socket, NLvoid /*@out@*/ *buffer, NLint nbytes,
                             NLboolean checkonly)
{
    nl_socket_t *sock = nlSockets[socket];
    
    /* skip error reporting if checkonly is TRUE */
    /* check for sync */
    if(sock->packetsync == NL_FALSE)
    {
        if(checkonly == NL_FALSE)
        {
            nlSetError(NL_PACKET_SYNC);
        }
        else
        {
            sock->readable = NL_TRUE;
        }
        return NL_INVALID;
    }
    
    if(sock->message_end == NL_TRUE && checkonly == NL_FALSE && sock->reclen == 0)
    {
        sock->message_end = NL_FALSE;
        nlSetError(NL_MESSAGE_END);
        return NL_INVALID;
    }
    /* allocate some temp storage the first time through */
    if(sock->inbuf == NULL)
    {
        sock->inbuflen = (nbytes + nbytes / 4 + NL_HEADER_LEN);
        
        if(sock->inbuflen < 1024)
        {
            sock->inbuflen = 1024;
        }
        else if(sock->inbuflen > NL_MAX_PACKET_LENGTH + NL_MAX_PACKET_LENGTH/4)
        {
            sock->inbuflen = NL_MAX_PACKET_LENGTH + NL_MAX_PACKET_LENGTH/4;
        }
        sock->inbuf = (NLbyte *)malloc((size_t)sock->inbuflen);
        if(sock->inbuf == NULL)
        {
            if(checkonly == NL_FALSE)
            {
                sock->inbuflen = 0;
                nlSetError(NL_OUT_OF_MEMORY);
            }
            return NL_INVALID;
        }
    }
    /* if inbuf is empty, get some data */
    if(sock->reclen < NL_HEADER_LEN)
    {
        NLint count;
        
        count = recv((SOCKET)sock->realsocket, (char *)(sock->inbuf + sock->reclen), (sock->inbuflen  - sock->reclen), 0);
        if(count == SOCKET_ERROR)
        {
            if(checkonly == NL_FALSE)
            {
                return sock_Error();
            }
            else
            {
                sock->readable = NL_TRUE;
                return NL_INVALID;
            }
        }
        if(count == 0)
        {
            if(checkonly == NL_FALSE)
            {
                nlSetError(NL_MESSAGE_END);
            }
            else
            {
                sock->message_end = NL_TRUE;
                sock->readable = NL_TRUE;
            }
            return NL_INVALID;
        }
        sock->reclen += count;
    }
    /* start parsing the packet */
    if(sock->reclen >= NL_HEADER_LEN)
    {
        NLboolean   done = NL_FALSE;
        NLushort    len;
        NLint       c = 2;
        
        /* check for valid packet */
        if(sock->inbuf[0] != 'N' || sock->inbuf[1] != 'L')
        {
        /* packet is not valid, we are somehow out of sync,
            or we are talking to a regular TCP stream */
            if(checkonly == NL_FALSE)
            {
                nlSetError(NL_PACKET_SYNC);
            }
            else
            {
                sock->readable = NL_TRUE;
            }
            sock->packetsync = NL_FALSE;
            return NL_INVALID;
        }
        
        /* read the length of the packet */
        readShort(sock->inbuf, c, len);
        if(len > NL_MAX_PACKET_LENGTH)
        {
            /* packet is not valid, or we are talking to a regular TCP stream */
            if(checkonly == NL_FALSE)
            {
                nlSetError(NL_PACKET_SYNC);
            }
            else
            {
                sock->readable = NL_TRUE;
            }
            sock->packetsync = NL_FALSE;
            return NL_INVALID;
        }
        /* check to see if we need to make the inbuf storage larger */
        if((NLint)len > sock->inbuflen)
        {
            NLint   newbuflen;
            NLbyte  *temp;

            newbuflen = (len + len / 4 + NL_HEADER_LEN);
            temp = (NLbyte *)realloc(sock->inbuf, (size_t)newbuflen);
            if(temp == NULL)
            {
                if(checkonly == NL_FALSE)
                {
                    nlSetError(NL_OUT_OF_MEMORY);
                }
                return NL_INVALID;
            }
            sock->inbuf = temp;
            sock->inbuflen = newbuflen;
        }
        if(checkonly == NL_FALSE)
        {
            if(len > (NLushort)nbytes)
            {
                nlSetError(NL_BUFFER_SIZE);
                return NL_INVALID;
            }
        }
        /* see if we need to get more of the packet */
        if(len > (NLushort)(sock->reclen - c))
        {
            done = NL_FALSE;
            while(done == NL_FALSE)
            {
                NLint count;
                
                if(checkonly == NL_FALSE)
                {
                    count = recv((SOCKET)sock->realsocket,
                        (char *)(sock->inbuf + sock->reclen),
                        (sock->inbuflen - sock->reclen), 0);
                }
                else
                {
                    /* we are calling this from PollGroup, so it cannot block */
                    fd_set          fdset;
                    struct timeval  t = {0,0};
                    
                    FD_ZERO(&fdset);
                    FD_SET((SOCKET)sock->realsocket, &fdset);
                    if(select(sock->realsocket + 1, &fdset, NULL, NULL, &t) == 1)
                    {
                        count = recv((SOCKET)sock->realsocket,
                            (char *)(sock->inbuf + sock->reclen),
                            (sock->inbuflen - sock->reclen), 0);
                    }
                    else
                    {
                        /* socket would block, so break */
                        break;
                    }
                }
                if(count == SOCKET_ERROR)
                {
                    if(checkonly == NL_FALSE)
                    {
                        /* check to see if we already have all the packet */
                        if(len <= (NLushort)(sock->reclen - c))
                        {
                            done = NL_TRUE;
                        }
                        else
                        {
                            /* report the error */
                            return sock_Error();
                        }
                    }
                    else
                    {
                        done = NL_TRUE;
                    }
                }
                else if(count == 0)
                {
                    sock->message_end = NL_TRUE;
                    sock->readable = NL_TRUE;
                    done = NL_TRUE;
                }
                else
                {
                    sock->reclen += count;
                    if(len <= (NLushort)(sock->reclen - c))
                    {
                        done = NL_TRUE;
                    }
                }
            }
        }
        /* see if we now have all of the packet */
        if(len <= (NLushort)(sock->reclen - c))
        {
            sock->readable = NL_TRUE;
            
            if(checkonly == NL_FALSE)
            {
                /* copy the packet */
                memcpy(buffer, (sock->inbuf + c), (size_t)len);
                
                /* check for another packet */
                sock->reclen -= (len + c);
                if(sock->reclen > 0)
                {
                    /* move it down to the beginning of inbuf */
                    memmove(sock->inbuf, (sock->inbuf + c + len), (size_t)sock->reclen);
                }
                /* quick check to see if we have another complete packet buffered */
                if(sock->reclen >= NL_HEADER_LEN)
                {
                    NLushort templen;
                    
                    /* read the length of the packet */
                    c = 2;
                    readShort(sock->inbuf, c, templen);
                    
                    /* check the length */
                    if(templen <= (NLushort)(sock->reclen - c))
                    {
                        /* we have another complete packet, so mark as readable for PollGroup */
                        sock->readable = NL_TRUE;
                    }
                    else
                    {
                        sock->readable = NL_FALSE;
                    }
                }
                else
                {
                    sock->readable = NL_FALSE;
                }
                return (NLint)len;
            }
        }
        else
        {
            return 0;
        }
    }
    return 0;
}

NLint sock_Read(NLsocket socket, NLvoid *buffer, NLint nbytes)
{
    nl_socket_t *sock = nlSockets[socket];
    NLint       count;
    
    if(nbytes < 0)
    {
        return 0;
    }
    if(sock->type == NL_RELIABLE || sock->type == NL_RELIABLE_PACKETS) /* TCP */
    {
        /* check for a non-blocking connection pending */
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
                    return NL_INVALID;
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
                return NL_INVALID;
            }
        }
        /* check for reliable packets */
        if(sock->type == NL_RELIABLE_PACKETS)
        {
            return sock_ReadPacket(socket, buffer, nbytes, NL_FALSE);
        }
        count = recv((SOCKET)sock->realsocket, (char *)buffer, nbytes, 0);
        if(count == 0)
        {
            /* end of message */
            nlSetError(NL_MESSAGE_END);
            return NL_INVALID;
        }
    }
    else /* UDP */
    {
        /* check for a non-blocking connection pending */
        if(sock->connecting == NL_TRUE)
        {
            nlSetError(NL_CON_PENDING);
            return NL_INVALID;
        }
        /* check for a connection error */
        if(sock->conerror == NL_TRUE)
        {
            nlSetError(NL_CON_REFUSED);
            return NL_INVALID;
        }
        if(sock->connected == NL_TRUE)
        {
            count = recv((SOCKET)sock->realsocket, (char *)buffer, nbytes, 0);
        }
        else
        {
            socklen_t   len = (socklen_t)sizeof(struct sockaddr_in6);
            
            count = recvfrom((SOCKET)sock->realsocket, (char *)buffer, nbytes, 0,
                (struct sockaddr *)&sock->addressin, &len);
        }
    }
    if(count == SOCKET_ERROR)
    {
        return sock_Error();
    }
    return count;
}

static NLboolean allocateBuffer(nl_socket_t *sock, NLint nbytes)
{
    NLint size = NLMIN(NLMAX((nbytes + NL_HEADER_LEN) * 2, 1024), (NL_MAX_PACKET_LENGTH + NL_HEADER_LEN));

    /* first call */
    if(sock->outbuf == NULL)
    {
        sock->outbuf = (NLbyte *)malloc((size_t)size);
        if(sock->outbuf == NULL)
        {
            sock->outbuflen = 0;
            nlSetError(NL_OUT_OF_MEMORY);
            return NL_FALSE;
        }
        sock->outbuflen = size;
    }
    else
    {
        if(size > sock->outbuflen)
        {
            NLbyte *temp;

            temp = (NLbyte *)realloc(sock->outbuf, (size_t)size);
            if(temp == NULL)
            {
                sock->outbuflen = 0;
                nlSetError(NL_OUT_OF_MEMORY);
                return NL_FALSE;
            }
            sock->outbuf = temp;
            sock->outbuflen = size;
        }
    }
    return NL_TRUE;
}

static NLint sock_WritePacket(NLsocket socket, const NLvoid *buffer, NLint nbytes)
{
    nl_socket_t *sock = nlSockets[socket];
    NLint       count;
    NLbyte      temp[NL_HEADER_LEN];
    NLint       c = 0;
    
    
    /* allocate memory for outbuf */
    if(sock->outbuf == NULL)
    {
        if(allocateBuffer(sock, nbytes) == NL_FALSE)
        {
            return NL_INVALID;
        }
    }
    else
    {
        /* send any unsent data from last packet */
        sock_WritePacketCheckPending(socket);
    }
    
    /* check to see if we already have some pending data */
    if(sock->sendlen > 0)
    {
        /* to comply with the way UDP packets act */
        /* if the send buffer has unsent data return 0 */
        return 0;
    }

    /* ID for packets is 'NL'*/
    writeByte(temp, c, 'N');
    writeByte(temp, c, 'L');
    /* add the packet length */
    writeShort(temp, c, (NLushort)nbytes);
    
    count = send((SOCKET)sock->realsocket, (char *)temp, c, 0);

    if(count == SOCKET_ERROR)
    {
        if(sockerrno == EWOULDBLOCK)
        {
            count = 0;
        }
        else
        {
            return sock_Error();
        }
    }
    if(count < c)
    {
        int dif = c - count;
        
        /* check outbuf size */
        if(allocateBuffer(sock, nbytes) == NL_FALSE)
        {
            return NL_INVALID;
        }
        /* store it */
        memcpy((sock->outbuf + sock->sendlen), (temp + count), (size_t)(dif));
        sock->sendlen += (dif);
        memcpy((sock->outbuf + sock->sendlen), ((NLbyte *)buffer), (size_t)(nbytes));
        sock->sendlen += (nbytes);
        rpBufferedCount++;
    }
    else
    {
        count = send((SOCKET)sock->realsocket, (char *)buffer, nbytes, 0);
        if(count == SOCKET_ERROR)
        {
            if(sockerrno == EWOULDBLOCK)
            {
                count = 0;
            }
            else
            {
                return sock_Error();
            }
        }
        /* make sure all was sent */
        if(count < nbytes)
        {
            int dif = nbytes - count;
            
            /* check outbuf size */
            if(allocateBuffer(sock, nbytes) == NL_FALSE)
            {
                return NL_INVALID;
            }
            /* store it */
            memcpy((sock->outbuf + sock->sendlen), ((NLbyte *)buffer + count), (size_t)(dif));
            sock->sendlen += dif;
            rpBufferedCount++;
        }
    }
    count = nbytes;
    return count;
}

NLint sock_Write(NLsocket socket, const NLvoid *buffer, NLint nbytes)
{
    nl_socket_t *sock = nlSockets[socket];
    NLint       count;
    
    if(nbytes < 0)
    {
        return 0;
    }
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
                    return NL_INVALID;
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
                return NL_INVALID;
            }
        }
        /* check for reliable packets */
        if(sock->type == NL_RELIABLE_PACKETS)
        {
            if(nbytes > NL_MAX_PACKET_LENGTH)
            {
                nlSetError(NL_PACKET_SIZE);
                return NL_INVALID;
            }
            return sock_WritePacket(socket, buffer, nbytes);
        }
        count = send((SOCKET)sock->realsocket, (char *)buffer, nbytes, 0);
    }
    else /* unconnected UDP */
    {
        /* check for a non-blocking connection pending */
        if(sock->connecting == NL_TRUE)
        {
            nlSetError(NL_CON_PENDING);
            return NL_INVALID;
        }
        /* check for a connection error */
        if(sock->conerror == NL_TRUE)
        {
            nlSetError(NL_CON_REFUSED);
            return NL_INVALID;
        }
        if(nbytes > NL_MAX_PACKET_LENGTH)
        {
            nlSetError(NL_PACKET_SIZE);
            return NL_INVALID;
        }
        if(sock->type == NL_BROADCAST)
        {
            //memcpy(&((struct sockaddr_in6 *)&sock->addressin)->sin6_addr, &in6addr_multicast, sizeof(in6addr_multicast));
        }
        if(sock->type == NL_UDP_MULTICAST)
        {
            count = SOCKET_ERROR;
        }
        else if(sock->connected == NL_TRUE)
        {
            count = send((SOCKET)sock->realsocket, (char *)buffer, nbytes, 0);
        }
        else
        {
#ifdef NL_DEBUG
            char tmp[128];
            nlAddrToString(&sock->addressout, tmp);
            printf("%s: sock %d len %d addr %s\n", __FUNCTION__, sock->realsocket, (int)nbytes, tmp);
#endif
            count = sendto((SOCKET)sock->realsocket, (char *)buffer, nbytes, 0,
                (struct sockaddr *)&sock->addressout, (int)sizeof(struct sockaddr_in6));

            if(sock->type == NL_BROADCAST)
            {
                struct sockaddr_in6 broadcast;
                memcpy(&broadcast, (struct sockaddr_in6 *)&sock->addressout, sizeof(broadcast));
                memcpy(&broadcast.sin6_addr, &in6addr_multicast, sizeof(in6addr_multicast));
                sendto((SOCKET)sock->realsocket, (char *)buffer, nbytes, 0,
                    (struct sockaddr *)&broadcast, (int)sizeof(struct sockaddr_in6));
#ifdef NL_DEBUG
                NLaddress tmpaddr;
                memcpy(&tmpaddr, &broadcast, sizeof(struct sockaddr_in6));
                tmpaddr.valid = NL_TRUE;
                nlAddrToString(&tmpaddr, tmp);
                printf("%s: sock %d len %d addr %s\n", __FUNCTION__, sock->realsocket, (int)nbytes, tmp);
#endif
            }
        }
    }
    if(count == SOCKET_ERROR)
    {
        return sock_Error();
    }
    return count;
}

NLchar *sock_AddrToString(const NLaddress *address, NLchar *string)
{
    if(((struct sockaddr_in6 *)address)->sin6_family != AF_INET6 || !address->valid)
    {
        _stprintf(string, TEXT("%s"), "");
        return NULL;
    }

    if(memcmp(&in6addr_ipv4mapped, &(((struct sockaddr_in6 *)address)->sin6_addr), 12) != 0)
    {
        NLushort port = ntohs(((struct sockaddr_in6 *)address)->sin6_port);
        strcpy(string, "[");
        inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)address)->sin6_addr), string + 1, INET6_ADDRSTRLEN);
        _stprintf(string + strlen(string), TEXT("]"));
        if (port != 0)
        {
            _stprintf(string + strlen(string), TEXT(":%u"), port);
        }
    }
    else
    {
        NLushort    port;
        port = ntohs(((struct sockaddr_in6 *)address)->sin6_port);
        if(port == 0)
        {
            _stprintf(string, TEXT("%u.%u.%u.%u"),
                ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[12],
                ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[13],
                ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[14],
                ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[15]);
        }
        else
        {
            _stprintf(string, TEXT("%u.%u.%u.%u:%u"),
                ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[12],
                ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[13],
                ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[14],
                ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[15],
                port);
        }
    }
    return string;
}

NLboolean sock_StringToAddr(const NLchar *string, NLaddress *address)
{
    memset(address, 0, sizeof(struct sockaddr_in6));
    ((struct sockaddr_in6 *)address)->sin6_family = AF_INET6;

    if(string[0] == '[')
    {
        NLchar addrPart[INET6_ADDRSTRLEN];
        const NLchar *addrPartEnd;
        NLulong port = 0;

        string += 1;
        addrPartEnd = strchr(string, ']');
        if(addrPartEnd == NULL || addrPartEnd - string >= INET6_ADDRSTRLEN || addrPartEnd == string)
        {
            nlSetError(NL_BAD_ADDR);
            address->valid = NL_FALSE;
            return NL_FALSE;
        }
        strncpy(addrPart, string, addrPartEnd - string);
        addrPart[addrPartEnd - string] = '\0';
        if(inet_pton(AF_INET6, addrPart, &(((struct sockaddr_in6 *)address)->sin6_addr)) != 1)
        {
            nlSetError(NL_BAD_ADDR);
            address->valid = NL_FALSE;
            return NL_FALSE;
        }
        _stscanf(addrPartEnd, (const NLchar *)TEXT("]:%lu"), &port);
        ((struct sockaddr_in6 *)address)->sin6_port = htons((NLushort)port);
        address->valid = NL_TRUE;
        return NL_TRUE;
    }
    else
    {
        NLulong     a1, a2, a3, a4;
        NLulong     port = 0;
        int         ret;

        ret = _stscanf((const NLchar *)string, (const NLchar *)TEXT("%lu.%lu.%lu.%lu:%lu"), &a1, &a2, &a3, &a4, &port);

        if(a1 > 255 || a2 > 255 || a3 > 255 || a4 > 255 || port > 65535 || ret < 4)
        {
            /* bad address */
            ((struct sockaddr_in6 *)address)->sin6_port = 0;
            nlSetError(NL_BAD_ADDR);
            address->valid = NL_FALSE;
            return NL_FALSE;
        }
        else
        {
            memcpy(&(((struct sockaddr_in6 *)address)->sin6_addr), &in6addr_ipv4mapped, sizeof(in6addr_ipv4mapped));
            ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[12] = a1;
            ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[13] = a2;
            ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[14] = a3;
            ((struct sockaddr_in6 *)address)->sin6_addr.s6_addr[15] = a4;
            ((struct sockaddr_in6 *)address)->sin6_port = htons((NLushort)port);
            address->valid = NL_TRUE;
            return NL_TRUE;
        }
    }
}

NLboolean sock_GetLocalAddr(NLsocket socket, NLaddress *address)
{
    nl_socket_t *sock = nlSockets[socket];
    socklen_t   len;
    
    memset(address, 0, sizeof(NLaddress));
    ((struct sockaddr_in6 *)address)->sin6_family = AF_INET6;
    address->valid = NL_TRUE;
    len = (socklen_t)sizeof(struct sockaddr_in6);
    /* if the socket is connected, this will get us
    the correct address on a multihomed system*/
    if(getsockname((SOCKET)sock->realsocket, (struct sockaddr *)address, &len) == SOCKET_ERROR)
    {
        /* ignore error if socket has not been bound or connected yet */
        if(sockerrno != EINVAL)
        {
            nlSetError(NL_SYSTEM_ERROR);
            address->valid = NL_FALSE;
            return NL_FALSE;
        }
    }
    sock_SetAddrPort(address, sock->localport);
    return NL_TRUE;
}

NLboolean sock_SetLocalAddr(const NLaddress *address)
{
    /* should we check against all the local addresses? */
    bindaddress = ((struct sockaddr_in6 *)address)->sin6_addr;
    return NL_TRUE;
}

NLboolean sock_AddrCompare(const NLaddress *address1, const NLaddress *address2)
{
    if(((struct sockaddr_in6 *)address1)->sin6_family != ((struct sockaddr_in6 *)address2)->sin6_family)
        return NL_FALSE;
    
    if(memcmp(&((struct sockaddr_in6 *)address1)->sin6_addr,
              &(((struct sockaddr_in6 *)address2)->sin6_addr),
              sizeof(struct in6_addr)) != 0)
        return NL_FALSE;
    
    if(((struct sockaddr_in6 *)address1)->sin6_port
        != ((struct sockaddr_in6 *)address2)->sin6_port)
        return NL_FALSE;
    
    return NL_TRUE;
}

NLushort sock_GetPortFromAddr(const NLaddress *address)
{
    return ntohs(((struct sockaddr_in6 *)address)->sin6_port);
}

void sock_SetAddrPort(NLaddress *address, NLushort port)
{
    ((struct sockaddr_in6 *)address)->sin6_port = htons((NLushort)port);
}

NLint sock_GetSystemError(void)
{
    NLint err = sockerrno;
    
#ifdef WINDOWS_APP
    if(err < WSABASEERR)
    {
        if(errno > 0)
        {
            err = errno;
        }
    }
#endif
    return err;
}

NLint sock_PollGroup(NLint group, NLenum name, NLsocket *sockets, NLint number, NLint timeout)
{
    NLint           numselect, count = 0;
    NLint           numsockets = NL_MAX_GROUP_SOCKETS;
    NLsocket        temp[NL_MAX_GROUP_SOCKETS];
    NLboolean       reliable[NL_MAX_GROUP_SOCKETS];
    NLboolean       result;
    NLsocket        *ptemp = temp;
    int             i, found = 0;
    fd_set          fdset;
    SOCKET          highest;
    struct timeval  t = {0,0}; /* {seconds, useconds}*/
    struct timeval  *tp = &t;
    
    nlGroupLock();
    highest = nlGroupGetFdset(group, &fdset);
    
    if(highest == INVALID_SOCKET)
    {
        /* error is set by nlGroupGetFdset */
        nlGroupUnlock();
        return NL_INVALID;
    }
    
    result = nlGroupGetSocketsINT(group, ptemp, &numsockets);
    nlGroupUnlock();
    
    if(result == NL_FALSE)
    {
        /* any error is set by nlGroupGetSockets */
        return NL_INVALID;
    }
    if(numsockets == 0)
    {
        return 0;
    }
    
    if(name == NL_READ_STATUS)
    {
        /* check for buffered reliable packets */
        for(i=0;i<numsockets;i++)
        {
            nl_socket_t *s = nlSockets[ptemp[i]];
            
            if(s->type == NL_RELIABLE_PACKETS && s->readable == NL_TRUE)
            {
                /* mark as readable */
                reliable[i] = NL_TRUE;
                found++;
                /* change the timeout to 0, or non-blocking since we */
                /* have at least one reliable packet to read */
                timeout = 0;
            }
            else
            {
                reliable[i] = NL_FALSE;
            }
        }
    }
    
    /* check for full blocking call */
    if(timeout < 0)
    {
        tp = NULL;
    }
    else /* set t values */
    {
        t.tv_sec = timeout/1000;
        t.tv_usec = (timeout%1000) * 1000;
    }
    
    /* call select to check the status */
    switch(name) {
        
    case NL_READ_STATUS:
        numselect = select((int)highest, &fdset, NULL, NULL, tp);
        break;
        
    case NL_WRITE_STATUS:
        numselect = select((int)highest, NULL, &fdset, NULL, tp);
        break;
        
    case NL_ERROR_STATUS:
        numselect = select((int)highest, NULL, NULL, &fdset, tp);
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_INVALID;
    }
    if(numselect == SOCKET_ERROR)
    {
        if(sockerrno == ENOTSOCK)
        {
            /* one of the sockets has been closed */
            nlSetError(NL_INVALID_SOCKET);
        }
        else if(sockerrno == EINTR)
        {
            /* select was interrupted by the system, maybe because the app is exiting */
            return 0;
        }
        else
        {
            nlSetError(NL_SYSTEM_ERROR);
        }
        return NL_INVALID;
    }
    
    if(numselect > number)
    {
        nlSetError(NL_BUFFER_SIZE);
        return NL_INVALID;
    }
    /* fill *sockets with a list of the sockets ready to be read */
    numselect += found;
    i = 0;
    while(numsockets-- > 0 && numselect > count)
    {
        nl_socket_t *s = nlSockets[*ptemp];
        
        if((reliable[i] == NL_TRUE ) || (FD_ISSET(s->realsocket, &fdset) != 0))
        {
            /* if checking for read status, must check for a complete packet */
            if(s->type == NL_RELIABLE_PACKETS && s->listen == NL_FALSE && name == NL_READ_STATUS)
            {
                (void)nlLockSocket(*ptemp, NL_READ);
                if(s->readable != NL_TRUE)
                {
                    if(s->inuse == NL_TRUE)
                    {
                        (void)sock_ReadPacket(*ptemp, NULL, 0, NL_TRUE);
                    }
                    else
                    {
                        s->readable = NL_FALSE;
                    }
                }
                if(s->readable == NL_TRUE)
                {
                    /* we do have a complete packet */
                    *sockets = *ptemp;
                    sockets++;
                    count++;
                }
                nlUnlockSocket(*ptemp, NL_READ);
            }
            else
            {
                *sockets = *ptemp;
                sockets++;
                count ++;
            }
        }
        i++;
        ptemp++;
    }
    return count;
}

NLboolean sock_Hint(NLenum name, NLint arg)
{
    switch(name) {
        
    case NL_LISTEN_BACKLOG:
        backlog = arg;
        break;
        
    case NL_MULTICAST_TTL:
        if(arg < 1)
        {
            arg = 1;
        }
        else if(arg > 255)
        {
            arg = 255;
        }
        multicastTTL = arg;
        break;
        
    case NL_REUSE_ADDRESS:
        reuseaddress = (NLboolean)(arg != 0 ? NL_TRUE : NL_FALSE);
        break;
        
    case NL_TCP_NO_DELAY:
        nlTCPNoDelay = (NLboolean)(arg != 0 ? NL_TRUE : NL_FALSE);
        break;
        
    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_FALSE;
    }
    return NL_TRUE;
}
