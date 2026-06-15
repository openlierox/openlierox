/*
  HawkNL cross platform network library
  Copyright (C) 2000-2003 Phil Frisbie, Jr. (phil@hawksoft.com)

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


#include <string.h>
#include <stdio.h>
#include <stdint.h>

#if defined (_WIN32_WCE)
#define errno GetLastError()
#else
#include <errno.h>
#endif

#include "nlinternal.h"

#ifdef WINDOWS_APP
/* Windows systems */

#include "wsock.h"

#else
/* Unix-style systems or macs with posix support */
#include <netinet/in.h> /* for ntohs and htons */
#endif

#ifdef NL_INCLUDE_LOOPBACK
#include "loopback.h"

static NLaddress loopback_ouraddress;
static NLint loopgroup;
static volatile NLboolean reuseaddress = NL_FALSE;

static NLmutex  portlock; /* In memory of my step-father, Don Portlock,
                                       who passed away Jan 12, 2001 - Phil */

static volatile uint16_t newport = 1024;

static NLushort loopback_getNextPort(void)
{
    (void)nlMutexLock(&portlock);
    ++newport;
    if(newport < 1024)
    {
        newport = 1024;
    }
    (void)nlMutexUnlock(&portlock);
    return newport;
}

static NLboolean loopback_ScanPort(NLushort port, NLenum type)
{
    NLint           numsockets = NL_MAX_GROUP_SOCKETS;
    NLsocket        temp[NL_MAX_GROUP_SOCKETS];

    if(reuseaddress == NL_TRUE)
    {
        return NL_TRUE;
    }
    if(nlGroupGetSockets(loopgroup, (NLsocket *)&temp, &numsockets) == NL_FALSE)
    {
        return NL_FALSE;
    }
    if(numsockets == 0)
    {
        return NL_TRUE;
    }
    while(numsockets-- > 0)
    {
        nl_socket_t *sock = nlSockets[temp[numsockets]];

        if(sock->type == type && sock->localport == port)
        {
            return NL_FALSE;
        }
    }
    return NL_TRUE;
}

static NLushort loopback_TryPort(NLushort port, NLenum type)
{
    NLint   ntries = 500; /* this is to prevent an infinite loop */

    if(port > 0)
    {
        if(loopback_ScanPort(port, type) == NL_TRUE)
        {
            return port;
        }
        else
        {
            return 0;
        }
    }
    /* let's find our own port number */
    while(ntries-- > 0)
    {
        port = loopback_getNextPort();
        if(loopback_ScanPort(port, type) == NL_TRUE)
        {
            return port;
        }
    }
    return 0;
}

NLboolean loopback_Init(void)
{
    loopgroup = nlGroupCreate();

    if(loopgroup == NL_INVALID)
    {
        return NL_FALSE;
    }
    return NL_TRUE;
}

void loopback_Shutdown(void)
{
    (void)nlGroupDestroy(loopgroup);
}

NLboolean loopback_Listen(NLsocket socket)
{
    nl_socket_t *sock = nlSockets[socket];
    NLint       i = NL_MAX_ACCEPT;

    if(sock->type == NL_BROADCAST)
    {
        nlSetError(NL_WRONG_TYPE);
        return NL_FALSE;
    }
    sock->listen = NL_TRUE;
    while(i-- > 0)
    {
        sock->ext->accept[i] = NL_INVALID;
    }
    return NL_TRUE;
}

NLsocket loopback_AcceptConnection(NLsocket socket)
{
    nl_socket_t *sock = nlSockets[socket];
    
    if(sock->listen == NL_FALSE)
    {
        nlSetError(NL_NOT_LISTEN);
        return NL_INVALID;
    }
    if(sock->inuse == NL_FALSE)
    {
        /* socket was closed by nlShutdown */
        nlSetError(NL_INVALID_SOCKET);
        return NL_INVALID;
    }
    if(sock->ext == NULL)
    {
        /* socket was closed on another thread */
        nlSetError(NL_INVALID_SOCKET);
        return NL_INVALID;
    }
    if(sock->ext->accept[0] != NL_INVALID)
    {
        NLsocket    newsocket;
        NLsocket    osock = sock->ext->accept[0];
        nl_socket_t *othersock = nlSockets[osock];
        
        /* make sure the other socket is valid */
        if(nlIsValidSocket(osock) == NL_FALSE)
        {
            NLint       i;

            for(i=1;i<NL_MAX_ACCEPT;i++)
            {
                sock->ext->accept[i-1] = sock->ext->accept[i];
            }
            sock->ext->accept[NL_MAX_ACCEPT-1] = NL_INVALID;
            return loopback_AcceptConnection(socket);
        }
        newsocket = loopback_Open(0, sock->type);
        if(newsocket != NL_INVALID)
        {
            NLint       i;
            nl_socket_t *newsock = nlSockets[newsocket];

            /* we must unlock the socket briefly or else nlConnect will deadlock */
            nlUnlockSocket(socket, NL_BOTH);
            if(nlLockSocket(osock, NL_READ) == NL_FALSE)
            {
                return NL_INVALID;
            }
            (void)nlLockSocket(socket, NL_BOTH);
            /* do the connecting */
            newsock->ext->consock = osock;
            newsock->remoteport = othersock->localport;
            othersock->ext->consock = newsocket;
            othersock->remoteport = newsock->localport;
            newsock->connected = NL_TRUE;
            loopback_SetAddrPort(&othersock->addressin, othersock->remoteport);
            loopback_SetAddrPort(&newsock->addressin, newsock->remoteport);
            othersock->connected = NL_TRUE;
            othersock->connecting = NL_FALSE;
            /* move the accept que down one */
            for(i=1;i<NL_MAX_ACCEPT;i++)
            {
                sock->ext->accept[i-1] = sock->ext->accept[i];
            }
            sock->ext->accept[NL_MAX_ACCEPT-1] = NL_INVALID;
            nlUnlockSocket(socket, NL_BOTH);
            nlUnlockSocket(osock, NL_READ);
            (void)nlLockSocket(socket, NL_BOTH);
            return newsocket;
        }
    }
    nlSetError(NL_NO_PENDING);
    return NL_INVALID;
}

NLsocket loopback_Open(NLushort port, NLenum type)
{
    nl_socket_t *newsock;
    NLsocket    newsocket;
    NLint       i;
    NLushort    lport;

    switch (type) {

    case NL_RELIABLE:
    case NL_UNRELIABLE:
    case NL_RELIABLE_PACKETS:
    case NL_BROADCAST:
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_INVALID;
    }

    lport = loopback_TryPort(port, type);
    if(lport == 0)
    {
        nlSetError(NL_INVALID_PORT);
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
    newsock->type = type;
    newsock->localport = lport;
    if(type == NL_BROADCAST)
    {
        newsock->remoteport = lport;
    }

    if((newsock->ext = (nl_extra_t *)malloc(sizeof(nl_extra_t))) == NULL)
    {
        nlSetError(NL_OUT_OF_MEMORY);
        nlUnlockSocket(newsocket, NL_BOTH);
        loopback_Close(newsocket);
        return NL_INVALID;
    }
    /* clear out the structure */
    memset(newsock->ext, 0, sizeof(nl_extra_t));
    for(i=0;i<NL_NUM_PACKETS;i++)
    {
        NLboolean err = NL_FALSE;

        /* malloc the max packet length plus two bytes for the port number */
        if((newsock->ext->inpacket[i] = (NLbyte *)malloc((size_t)(NL_MAX_PACKET_LENGTH + 2))) == NULL)
        {
            nlSetError(NL_OUT_OF_MEMORY);
            err = NL_TRUE;
        }
        if(err == NL_TRUE)
        {
            while(i-- > 0)
            {
                free(newsock->ext->inpacket[i]);
            }
            nlUnlockSocket(newsocket, NL_BOTH);
            loopback_Close(newsocket);
            return NL_INVALID;
        }
    }

    (void)nlGroupAddSocket(loopgroup, newsocket);
    nlUnlockSocket(newsocket, NL_BOTH);

    return newsocket;
}

NLboolean loopback_Connect(NLsocket socket, const NLaddress *address)
{
    nl_socket_t *sock = nlSockets[socket];
    NLushort    port;
    NLint       numsockets = NL_MAX_GROUP_SOCKETS;
    NLsocket    temp[NL_MAX_GROUP_SOCKETS];

    /* no need to connect a broadcast socket */
    if(sock->type == NL_BROADCAST)
    {
        nlSetError(NL_WRONG_TYPE);
    }
    port = loopback_GetPortFromAddr(address);
    /* make sure socket is not already connected */
    if(sock->connected == NL_TRUE || sock->connecting == NL_TRUE)
    {
        nlSetError(NL_CON_REFUSED);
        return NL_FALSE;
    }
    if(nlGroupGetSockets(loopgroup, (NLsocket *)&temp, &numsockets) == NL_FALSE)
    {
        return NL_FALSE;
    }
    if(numsockets == 0)
    {
        return NL_FALSE;
    }
    while(numsockets-- > 0)
    {
        NLsocket    s = temp[numsockets];
        nl_socket_t *othersock = nlSockets[s];

        if(sock->type == othersock->type && port == othersock->localport
            && othersock->listen == NL_TRUE && othersock->connected == NL_FALSE
            && othersock->connecting == NL_FALSE)
        {
            /* we found the right socket, so connect */
            NLint i;

            if(nlLockSocket(s, NL_BOTH) == NL_FALSE)
            {
                return NL_FALSE;
            }
            for(i=0;i<NL_MAX_ACCEPT;i++)
            {
                if(othersock->ext->accept[i] == NL_INVALID)
                {
                    othersock->ext->accept[i] = socket;
                    sock->connecting = NL_TRUE;
                    sock->ext->consock = s;
                    nlUnlockSocket(s, NL_BOTH);
                    if(sock->blocking == NL_TRUE)
                    {
                        nlUnlockSocket(socket, NL_BOTH);
                        /* wait for nlAccept to be called */
                        while(sock->connecting == NL_TRUE)
                        {
                            if(sock->inuse == NL_FALSE)
                            {
                                /* nlShutdown has been called */
                                nlSetError(NL_INVALID_SOCKET);
                                return NL_FALSE;
                            }
                            nlThreadSleep(NL_CONNECT_SLEEP);
                        }
                        (void)nlLockSocket(socket, NL_BOTH);
                    }
                    return NL_TRUE;
                }
            }
            nlUnlockSocket(s, NL_BOTH);
        }
    }
    nlSetError(NL_CON_REFUSED);
    return NL_FALSE;
}

void loopback_Close(NLsocket socket)
{
    nl_socket_t *sock = nlSockets[socket];
    int         i;

    if(sock->connected == NL_TRUE || sock->connecting == NL_TRUE)
    {
        /* break the connection */
        nl_socket_t *othersock = nlSockets[sock->ext->consock];

        if(othersock->ext != NULL)
        {
            othersock->ext->consock = NL_INVALID;
        }
        othersock->connected = NL_FALSE;
        sock->connected = NL_FALSE;
        sock->listen = NL_FALSE;
        if(sock->type != NL_BROADCAST)
        {
            /* this allows nlPollGroup to report socket is readable */
            /* so that the app can get the NL_SOCK_DISCONNECT message*/
            if(othersock->ext != NULL)
            {
                othersock->ext->inlen[othersock->ext->nextinused] = -1;
            }
        }
    }
    for(i=0;i<NL_NUM_PACKETS;i++)
    {
        void /*@owned@*/*t = sock->ext->inpacket[i];

        free(t);
        sock->ext->inpacket[i] = NULL;
    }
    free(sock->ext);
    sock->ext = NULL;
}

NLint loopback_Read(NLsocket socket, NLvoid *buffer, NLint nbytes)
{
    nl_socket_t *sock = nlSockets[socket];
    NLint       len = sock->ext->inlen[sock->ext->nextinused];
    NLint       c = 0;
    NLushort    port;
    
    if(sock->blocking == NL_TRUE)
    {
        while(len == 0)
        {
            nlUnlockSocket(socket, NL_READ);
            nlThreadSleep(10);
            if(sock->inuse == NL_FALSE)
            {
                /* nlShutdown has been called */
                nlSetError(NL_INVALID_SOCKET);
                return NL_INVALID;
            }
            (void)nlLockSocket(socket, NL_READ);
            len = sock->ext->inlen[sock->ext->nextinused];
        }
    }
    if(len > 0)
    {
        if(len > nbytes)
        {
            nlSetError(NL_BUFFER_SIZE);
            return NL_INVALID;
        }
        if(sock->connecting == NL_TRUE)
        {
            nlSetError(NL_CON_PENDING);
            return NL_INVALID;
        }
        /* get the port number */
        readShort(sock->ext->inpacket[sock->ext->nextinused], c, port);
        loopback_SetAddrPort(&sock->addressin, port);
        /* copy the packet */
        memcpy(buffer, sock->ext->inpacket[sock->ext->nextinused] + 2, (size_t)len);
        /* zero out length and set up for next packet */
        sock->ext->inlen[sock->ext->nextinused] = 0;
        sock->ext->nextinused++;
        if(sock->ext->nextinused >= NL_NUM_PACKETS)
        {
            sock->ext->nextinused = 0;
        }
    }
    /* check for broken connection */
    else if((len == -1) || (sock->connected == NL_TRUE && sock->ext->consock == NL_INVALID)
        || (sock->connected == NL_FALSE && sock->type != NL_BROADCAST
            && sock->type != NL_UNRELIABLE))
    {
        /* Unconnected NL_UNRELIABLE (UDP) is the normal pattern OLX
         * uses: bind a port, sendto/recvfrom by address. The original
         * driver treated "no data" on such sockets as DISCONNECT,
         * which made every OLX read fail. Treat it the same as
         * NL_BROADCAST and return len (== 0). */
        nlSetError(NL_SOCK_DISCONNECT);
        return NL_INVALID;
    }
    return len;
}

static NLint loopback_WritePacket(NLsocket to, const NLvoid *buffer, NLint nbytes, NLushort fromport)
{
    nl_socket_t *sock = nlSockets[to];
    NLint       i, j;
    NLint       c = 0;

    /* check the packet size */
    if(nbytes > NL_MAX_PACKET_LENGTH)
    {
        nlSetError(NL_PACKET_SIZE);
        return NL_INVALID;
    }
    if(nlLockSocket(to, NL_READ) == NL_FALSE)
    {
        return NL_INVALID;
    }
    /* make sure we have an empty packet buffer */
    if(sock->ext->nextinfree == NL_INVALID)
    {
        /* all buffers were filled by last write */
        /* check to see if any were emptied by a read */
        i = NL_NUM_PACKETS;
        j = sock->ext->nextinused;

        while(i-- > 0)
        {
            if(sock->ext->inlen[j] == 0)
            {
                /* found the first free */
                sock->ext->nextinfree = j;
                break;
            }
            j++;
            if(j >= NL_NUM_PACKETS)
            {
                j = 0;
            }
        }
        if(sock->ext->nextinfree == NL_INVALID)
        {
            nlUnlockSocket(to, NL_READ);
            /* none are free */
            if(sock->type == NL_RELIABLE || sock->type == NL_RELIABLE_PACKETS)
            {
                return 0;
            }
            else
            {
                /* silently fail */
                return nbytes;
            }
        }
    }
    /* write the port number */
    writeShort(sock->ext->inpacket[sock->ext->nextinfree], c, fromport);
    /* copy the packet buffer */
    memcpy(sock->ext->inpacket[sock->ext->nextinfree] + 2, buffer, (size_t)nbytes);
    sock->ext->inlen[sock->ext->nextinfree] = nbytes;
    sock->ext->nextinfree++;
    if(sock->ext->nextinfree >= NL_NUM_PACKETS)
    {
        sock->ext->nextinfree = 0;
    }
    /* check for full packet buffers */
    if(sock->ext->inlen[sock->ext->nextinfree] != 0)
    {
        sock->ext->nextinfree = NL_INVALID;
    }
    nlUnlockSocket(to, NL_READ);
    return nbytes;
}

NLint loopback_Write(NLsocket socket, const NLvoid *buffer, NLint nbytes)
{
    nl_socket_t *sock = nlSockets[socket];
    nl_socket_t *othersock;
    NLsocket    s[NL_MAX_GROUP_SOCKETS];
    NLint       number = NL_MAX_GROUP_SOCKETS;
    NLint       i;
    NLint       count;

    switch (sock->type) {

    case NL_RELIABLE:
    case NL_RELIABLE_PACKETS:
    default:
        {
            if(sock->connected == NL_TRUE)
            {
                /* check for broken connection */
                if(sock->ext->consock == NL_INVALID)
                {
                    nlSetError(NL_SOCK_DISCONNECT);
                    return NL_INVALID;
                }
                count = loopback_WritePacket(sock->ext->consock, buffer, nbytes, sock->localport);
            }
            else if(sock->connecting == NL_TRUE)
            {
                nlSetError(NL_CON_PENDING);
                return NL_INVALID;
            }
            else
            {
                nlSetError(NL_SOCK_DISCONNECT);
                return NL_INVALID;
            }
        }
        break;
    case NL_UNRELIABLE:
        {
            if(sock->connected == NL_TRUE)
            {
                /* check for broken connection */
                if(sock->ext->consock == NL_INVALID)
                {
                    nlSetError(NL_SOCK_DISCONNECT);
                    return NL_INVALID;
                }
                count = loopback_WritePacket(sock->ext->consock, buffer, nbytes, sock->localport);
            }
            else if(sock->connecting == NL_TRUE)
            {
                nlSetError(NL_CON_PENDING);
                return NL_INVALID;
            }
            /* unconnected UDP emulation */
            {
                /* OLX uses nlSetRemoteAddr() which only fills in
                 * sock->addressout. The original loopback driver only
                 * matched against sock->remoteport, which Open never
                 * sets for unconnected UDP — so the destination port
                 * was always 0 and packets went nowhere. Fall back to
                 * the addressout port when remoteport is unset. */
                NLushort dport = sock->remoteport;
                if(dport == 0)
                    dport = loopback_GetPortFromAddr(&sock->addressout);
                count = nbytes;
                (void)nlGroupGetSockets(loopgroup, (NLsocket *)s, &number);
                for(i=0;i<number;i++)
                {
                    if(nlIsValidSocket(s[i]) == NL_TRUE)
                    {
                        othersock = nlSockets[s[i]];

                        if(dport == othersock->localport &&
                            othersock->connected == NL_FALSE &&
                            sock->type == othersock->type)
                        {
                            (void)loopback_WritePacket(s[i], buffer, nbytes, sock->localport);
                        }
                    }
                }
            }
        }
        break;
    case NL_BROADCAST:
        {
            count = nbytes;
            (void)nlGroupGetSockets(loopgroup, (NLsocket *)s, &number);
            for(i=0;i<number;i++)
            {
                if(nlIsValidSocket(s[i]) == NL_TRUE)
                {
                    othersock = nlSockets[s[i]];

                    if(sock->localport == othersock->localport &&
                        sock->type == othersock->type)
                    {
                        (void)loopback_WritePacket(s[i], buffer, nbytes, sock->localport);
                    }
                }
            }
        }
    }

    return count;
}

NLchar *loopback_AddrToString(const NLaddress *address, NLchar *string)
{
    _stprintf(string, TEXT("127.0.0.1:%u"), loopback_GetPortFromAddr(address));

    return string;
}

NLboolean loopback_StringToAddr(const NLchar *string, NLaddress *address)
{
    NLchar      *st;
    NLint       port;

    memset(address, 0, sizeof(NLaddress));
    address->valid = NL_TRUE;
    /* check for a port number */
    st = _tcschr(string, TEXT(':'));
    if(st != NULL)
    {
        st++;
        port = _ttoi(st);
        if(port < 0 || port > 65535)
        {
            nlSetError(NL_BAD_ADDR);
            address->valid = NL_FALSE;
            return NL_FALSE;
        }
        loopback_SetAddrPort(address, (NLushort)port);
    }
    return NL_TRUE;
}

NLboolean loopback_GetLocalAddr(NLsocket socket, NLaddress *address)
{
    nl_socket_t *sock = nlSockets[socket];

    memset(address, 0, sizeof(NLaddress));
    loopback_SetAddrPort(address, sock->localport);
    address->valid = NL_TRUE;
    return NL_TRUE;
}

NLaddress *loopback_GetAllLocalAddr(NLint *count)
{
    *count = 1;
    memset(&loopback_ouraddress, 0, sizeof(NLaddress));
    loopback_ouraddress.valid = NL_TRUE;
    return &loopback_ouraddress;
}

NLboolean loopback_SetLocalAddr(const NLaddress *address)
{
    /* this is just to keep compilers happy */
    if(address == NULL)
    {
        return NL_FALSE;
    }
    return NL_TRUE;
}

NLchar *loopback_GetNameFromAddr(const NLaddress *address, NLchar *name)
{
    _stprintf(name, TEXT("%s:%u"), TEXT("localhost"), loopback_GetPortFromAddr(address));
    return name;
}

NLboolean loopback_GetNameFromAddrAsync(const NLaddress *address, NLchar *name)
{
    (void)loopback_GetNameFromAddr(address, name);
    return NL_TRUE;
}

NLboolean loopback_GetAddrFromName(const NLchar *name, NLaddress *address)
{
    return loopback_StringToAddr(name, address);
}

NLboolean loopback_GetAddrFromNameAsync(const NLchar *name, NLaddress *address)
{
    return loopback_GetAddrFromName(name, address);
}

NLboolean loopback_AddrCompare(const NLaddress *address1, const NLaddress *address2)
{
    if(*(NLushort *)(&address1->addr[0]) == *(NLushort *)(&address2->addr[0]))
    {
        return NL_TRUE;
    }
    return NL_FALSE;
}

NLushort loopback_GetPortFromAddr(const NLaddress *address)
{
    return *(NLushort *)(&address->addr[0]);
}

void loopback_SetAddrPort(NLaddress *address, NLushort port)
{
    *(NLushort *)(&address->addr[0]) = port;
}

NLint loopback_GetSystemError(void)
{
    return errno;
}

NLint loopback_PollGroup(NLint group, NLenum name, NLsocket *sockets, NLint number, NLint timeout)
{
    NLint           count = 0;
    NLint           numsockets = NL_MAX_GROUP_SOCKETS;
    NLsocket        temp[NL_MAX_GROUP_SOCKETS];
    NLtime          end, now;

    nlGroupLock();
    if(nlGroupGetSocketsINT(group, (NLsocket *)&temp, &numsockets) == NL_FALSE)
    {
        /* any error is set by nlGroupGetSockets */
        nlGroupUnlock();
        return NL_INVALID;
    }
    nlGroupUnlock();
    if(numsockets == 0)
    {
        return 0;
    }

    (void)nlTime(&now);
    end.seconds = now.seconds;
    end.mseconds = now.mseconds;
    if(timeout > 0)
    {
        end.mseconds += timeout;
        while(end.mseconds > 999)
        {
            end.mseconds -= 1000;
            end.seconds++;
        }
    }

    while(count == 0)
    {
        switch(name) {
            
        case NL_READ_STATUS:
            {
                NLint   i = 0;
                NLint   j = numsockets;
                
                while(j-- > 0)
                {
                    /* check for a packet */
                    nl_socket_t *sock;
                    
                    if(nlIsValidSocket(temp[i]) != NL_TRUE)
                    {
                        nlSetError(NL_INVALID_SOCKET);
                        return NL_INVALID;
                    }
                    sock = nlSockets[temp[i]];
                    
                    if(sock->ext->inlen[sock->ext->nextinused] != 0)
                    {
                        *sockets = temp[i];
                        sockets++;
                        count++;
                        if(count > number)
                        {
                            nlSetError(NL_BUFFER_SIZE);
                            return NL_INVALID;
                        }
                    }
                    i++;
                }
            }
            break;
            
        case NL_WRITE_STATUS:
            {
                NLint   i = 0;
                NLint   j = numsockets;
                
                while(j-- > 0)
                {
                    nl_socket_t *sock;
                    
                    if(nlIsValidSocket(temp[i]) != NL_TRUE)
                    {
                        nlSetError(NL_INVALID_SOCKET);
                        return NL_INVALID;
                    }
                    sock = nlSockets[temp[i]];
                    
                    /* check for a free packet if reliable and connected */
                    if((sock->type == NL_TCP || sock->type == NL_TCP_PACKETS)
                        && (sock->connecting == NL_TRUE || sock->connected == NL_TRUE))
                    {
                        nl_socket_t *othersock = nlSockets[sock->ext->consock];
                        
                        if(othersock->ext->nextinfree == NL_INVALID)
                        {
                            continue;
                        }
                    }
                    /* add the socket to the list */
                    *sockets = temp[i];
                    sockets++;
                    count++;
                    if(count > number)
                    {
                        nlSetError(NL_BUFFER_SIZE);
                        return NL_INVALID;
                    }
                    i++;
                }
            }
            break;
            
        case NL_ERROR_STATUS:
            {
                NLint   i = 0;
                NLint   j = numsockets;
                
                while(j-- > 0)
                {
                    nl_socket_t *sock;
                    
                    if(nlIsValidSocket(temp[i]) != NL_TRUE)
                    {
                        nlSetError(NL_INVALID_SOCKET);
                        return NL_INVALID;
                    }
                    sock = nlSockets[temp[i]];
                    
                    if(sock->connected == NL_FALSE && sock->type != NL_UDP_BROADCAST)
                    {
                        /* add the socket to the list */
                        *sockets = temp[i];
                        sockets++;
                        count++;
                        if(count > number)
                        {
                            nlSetError(NL_BUFFER_SIZE);
                            return NL_INVALID;
                        }
                        i++;
                    }
                }
            }
            break;

        default:
            nlSetError(NL_INVALID_ENUM);
            return NL_INVALID;
        }
        if(timeout != 0)
        {
            nlThreadSleep(1);
            (void)nlTime(&now);
            if(timeout > 0 && (now.seconds > end.seconds || (now.seconds == end.seconds && now.mseconds > end.mseconds)))
                break;
        }
        else
        {
            break;
        }
    }
    return count;
}

NLboolean loopback_Hint(NLenum name, NLint arg)
{
    switch (name) {

    case NL_REUSE_ADDRESS:
        reuseaddress = (NLboolean)(arg != 0 ? NL_TRUE : NL_FALSE);
        break;

    default:
        nlSetError(NL_INVALID_ENUM);
        return NL_FALSE;
    }
    return NL_TRUE;
}

#endif /* NL_INCLUDE_LOOPBACK */

