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

#ifndef WSOCK_H
#define WSOCK_H

#include "../include/nl.h"

/* Windows systems */
#ifdef _MSC_VER
#pragma warning (disable:4201)
#pragma warning (disable:4214)
#pragma warning (disable:4127) /* bogus FD_SET warning */
#endif /* _MSC_VER */

#define WIN32_LEAN_AND_MEAN
#include <ws2tcpip.h>

#ifdef _MSC_VER
#pragma warning (default:4201)
#pragma warning (default:4214)
#endif /* _MSC_VER */

#define ioctl ioctlsocket

#undef  EBADF
#define EBADF                   WSAEBADF

#undef  EFAULT
#define EFAULT                  WSAEFAULT

#undef  EMFILE
#define EMFILE                  WSAEMFILE

#undef  EINTR
#define EINTR                   WSAEINTR

#undef  EINVAL
#define EINVAL                  WSAEINVAL

#undef  EACCES
#define EACCES                  WSAEACCES

#ifndef ENAMETOOLONG
#define ENAMETOOLONG            WSAENAMETOOLONG
#endif

#ifndef ENOTEMPTY
#define ENOTEMPTY               WSAENOTEMPTY
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT               WSAETIMEDOUT
#endif

#ifndef EREMOTE
#define EREMOTE                 WSAEREMOTE
#endif

#define TRY_AGAIN               WSATRY_AGAIN
#define NO_RECOVERY             WSANO_RECOVERY
#define NO_DATA                 WSANO_DATA
#define HOST_NOT_FOUND          WSAHOST_NOT_FOUND
#define EWOULDBLOCK             WSAEWOULDBLOCK
#define EINPROGRESS             WSAEINPROGRESS
#define EALREADY                WSAEALREADY
#define ENOTSOCK                WSAENOTSOCK
#define EDESTADDRREQ            WSAEDESTADDRREQ
#define EMSGSIZE                WSAEMSGSIZE
#define EPROTOTYPE              WSAEPROTOTYPE
#define ENOPROTOOPT             WSAENOPROTOOPT
#define EPROTONOSUPPORT         WSAEPROTONOSUPPORT
#define ESOCKTNOSUPPORT         WSAESOCKTNOSUPPORT
#define EOPNOTSUPP              WSAEOPNOTSUPP
#define EPFNOSUPPORT            WSAEPFNOSUPPORT
#define EAFNOSUPPORT            WSAEAFNOSUPPORT
#define EADDRINUSE              WSAEADDRINUSE
#define EADDRNOTAVAIL           WSAEADDRNOTAVAIL
#define ENETDOWN                WSAENETDOWN
#define ENETUNREACH             WSAENETUNREACH
#define ENETRESET               WSAENETRESET
#define ECONNABORTED            WSAECONNABORTED
#define ECONNRESET              WSAECONNRESET
#define ENOBUFS                 WSAENOBUFS
#define EISCONN                 WSAEISCONN
#define ENOTCONN                WSAENOTCONN
#define ESHUTDOWN               WSAESHUTDOWN
#define ETOOMANYREFS            WSAETOOMANYREFS
#define ECONNREFUSED            WSAECONNREFUSED
#define ELOOP                   WSAELOOP
#define EHOSTDOWN               WSAEHOSTDOWN
#define EHOSTUNREACH            WSAEHOSTUNREACH
#define EPROCLIM                WSAEPROCLIM
#define EUSERS                  WSAEUSERS
#define EDQUOT                  WSAEDQUOT
#define ESTALE                  WSAESTALE
#define sockerrno   WSAGetLastError()
/* get rid of some nasty LCLint messages */
#undef  FIONBIO
#define FIONBIO 0x8004667e

typedef int socklen_t;

typedef struct sockaddr_ipx
{
    short sa_family;
    char  sa_netnum[4];
    char  sa_nodenum[6];
    unsigned short sa_socket;
} SOCKADDR_IPX;

#define NSPROTO_IPX      1000
#define NSPROTO_SPX      1256
#define NSPROTO_SPXII    1257

/* These replacement inlined functions are replacements for */
/* the Winsock macros, but compile without errors :) */
#undef FD_CLR
#define FD_CLR      nlFD_CLR

NL_INLINE void nlFD_CLR(SOCKET fd, fd_set *set)
{
    u_int i;

    for(i=0;i<set->fd_count;i++)
    {
        if(set->fd_array[i] == fd)
        {
            while(i < set->fd_count-1)
            {
                set->fd_array[i] = set->fd_array[i+1];
                i++;
            }
            set->fd_count--;
            break;
        }
    }
}

#undef FD_SET
#define FD_SET      nlFD_SET

NL_INLINE void nlFD_SET(SOCKET fd, fd_set *set)
{
    if(set->fd_count < FD_SETSIZE)
        set->fd_array[set->fd_count++]=fd;
}

/* This function is inlined for speed over the Winsock function */

#undef FD_ISSET
#define FD_ISSET(fd, set)      nlWSAFDIsSet((SOCKET)(fd), set)

NL_INLINE int nlWSAFDIsSet(SOCKET fd, fd_set *set)
{
    int i = (int)set->fd_count;

    while(i-- != 0)
    {
        if (set->fd_array[i] == fd)
            return 1;
    }
    return 0;
}

#endif /* WSOCK_H */

