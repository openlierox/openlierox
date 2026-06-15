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

/*
  Based on code supplied by Ryan Haksi <cryogen@infoserve.net>
*/

#if defined WIN32 || defined WIN64 || defined (_WIN32_WCE)
#if !defined (_WIN32_WCE)
#include <errno.h>
#endif
#include "wsock.h"
#elif macintosh
/* POSIX compat Mac systems ie pre OSX with GUSI2 installed */
#include <unistd.h>
#include <types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#else
/* POSIX systems */
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <netdb.h>
#include <string.h>
#endif

#include "nlinternal.h"


NL_EXP const /*@observer@*/  NLchar* NL_APIENTRY nlGetSystemErrorStr(NLint err)
{
    NLchar *lpszRetStr;
    switch(err) {
#ifdef WINDOWS_APP
    case WSABASEERR:
        lpszRetStr=(NLchar *)TEXT("No error.");
        break;
    case WSAVERNOTSUPPORTED:
        lpszRetStr=(NLchar *)TEXT("Version of WinSock not supported.");
        break;
    case WSASYSNOTREADY:
        lpszRetStr=(NLchar *)TEXT("WinSock not present or not responding.");
        break;
    case WSANOTINITIALISED:
        lpszRetStr=(NLchar *)TEXT("WinSock not initialized.");
        break;
#endif
        /* ELOOK is used on some Macs */
#ifdef ELOOK
    case ELOOK:
        lpszRetStr=(NLchar *)TEXT("Internal mapping for kOTLookErr, don't return to client.");
        break;
#endif
        /* EPROCLIM not defined in some UNIXs */
#ifdef EPROCLIM
    case EPROCLIM:
        lpszRetStr=(NLchar *)TEXT("Too many processes.");
        break;
#endif
        /* On some UNIXs, EINTR and NO_DATA have the same value */
#if (EINTR != NO_DATA)
    case EINTR:
        lpszRetStr=(NLchar *)TEXT("Interrupted function call.");
        break;
/* Emscripten/WASI: NO_DATA collides with EADDRNOTAVAIL (both = 4). */
#if defined(NO_DATA) && !defined(__EMSCRIPTEN__)
    case NO_DATA:
        lpszRetStr=(NLchar *)TEXT("Valid name, no data record for type.");
        break;
#endif
#else
    case NO_DATA:
        lpszRetStr=(NLchar *)TEXT("Interrupted function call or no data record for type.");
        break;
#endif
    case EBADF:
        lpszRetStr=(NLchar *)TEXT("Bad file descriptor.");
        break;
    case EFAULT:
        lpszRetStr=(NLchar *)TEXT("The namelen argument is incorrect.");
        break;
    case EMFILE:
        lpszRetStr=(NLchar *)TEXT("Too many open files.");
        break;
    case EINVAL:
        lpszRetStr=(NLchar *)TEXT("App version not supported by DLL.");
        break;
/* On Emscripten/WASI, h_errno values (TRY_AGAIN=2, NO_RECOVERY=3,
 * HOST_NOT_FOUND=4) collide with errno values (EACCES=2, EADDRINUSE=3,
 * EADDRNOTAVAIL=4) — both come from the same number space. Skip the
 * h_errno cases there; the errno cases below are the ones HawkNL
 * actually reports. */
#if defined(TRY_AGAIN) && !defined(__EMSCRIPTEN__)
    case TRY_AGAIN:
        lpszRetStr=(NLchar *)TEXT("Non-authoritive: host not found or server failure.");
        break;
#endif
#if defined(NO_RECOVERY) && !defined(__EMSCRIPTEN__)
    case NO_RECOVERY:
        lpszRetStr=(NLchar *)TEXT("Non-recoverable: refused or not implemented.");
        break;
#endif
#if defined(HOST_NOT_FOUND) && !defined(__EMSCRIPTEN__)
    case HOST_NOT_FOUND:
        lpszRetStr=(NLchar *)TEXT("Authoritive: Host not found.");
        break;
#endif
    case EACCES:
        lpszRetStr=(NLchar *)TEXT("Permission to access socket denied.");
        break;
    case ENETDOWN:
        lpszRetStr=(NLchar *)TEXT("Network subsystem failed.");
        break;
    case EAFNOSUPPORT:
        lpszRetStr=(NLchar *)TEXT("Address family not supported.");
        break;
    case ENOBUFS:
        lpszRetStr=(NLchar *)TEXT("No buffer space available.");
        break;
    case EPROTONOSUPPORT:
        lpszRetStr=(NLchar *)TEXT("Specified protocol not supported.");
        break;
    case EPROTOTYPE:
        lpszRetStr=(NLchar *)TEXT("Protocol wrong type for this socket.");
        break;
    case ESOCKTNOSUPPORT:
        lpszRetStr=(NLchar *)TEXT("Socket type not supported for address family.");
        break;
    case ENOTSOCK:
        lpszRetStr=(NLchar *)TEXT("Descriptor is not a socket.");
        break;
    case EWOULDBLOCK:
        lpszRetStr=(NLchar *)TEXT("Non-blocking socket would block.");
        break;
    case EADDRINUSE:
        lpszRetStr=(NLchar *)TEXT("Address already in use.");
        break;
    case ECONNABORTED:
        lpszRetStr=(NLchar *)TEXT("Connection aborted.");
        break;
    case ECONNRESET:
        lpszRetStr=(NLchar *)TEXT("Connection reset.");
        break;
    case ENOTCONN:
        lpszRetStr=(NLchar *)TEXT("Not connected.");
        break;
    case ETIMEDOUT:
        lpszRetStr=(NLchar *)TEXT("Connection timed out.");
        break;
    case ECONNREFUSED:
        lpszRetStr=(NLchar *)TEXT("Connection was refused.");
        break;
    case EHOSTDOWN:
        lpszRetStr=(NLchar *)TEXT("Host is down.");
        break;
    case ENETUNREACH:
        lpszRetStr=(NLchar *)TEXT("Network unreachable.");
        break;
    case EHOSTUNREACH:
        lpszRetStr=(NLchar *)TEXT("Host unreachable.");
        break;
    case EADDRNOTAVAIL:
        lpszRetStr=(NLchar *)TEXT("Address not available.");
        break;
    case EINPROGRESS:
#ifdef WINDOWS_APP
        lpszRetStr=(NLchar *)TEXT("A blocking sockets call is in progress.");
#else
        lpszRetStr=(NLchar *)TEXT("The socket is non-blocking and the connection could not be established immediately.");
#endif
        break;
    case EDESTADDRREQ:
        lpszRetStr=(NLchar *)TEXT("Destination address is required.");
        break;
    case EISCONN:
        lpszRetStr=(NLchar *)TEXT("Socket is already connected.");
        break;
    case ENETRESET:
        lpszRetStr=(NLchar *)TEXT("Connection has been broken due to the remote host resetting.");
        break;
    case EOPNOTSUPP:
        lpszRetStr=(NLchar *)TEXT("Operation not supported on socket");
        break;
    case ESHUTDOWN:
        lpszRetStr=(NLchar *)TEXT("Socket has been shut down.");
        break;
    case EMSGSIZE:
        lpszRetStr=(NLchar *)TEXT("The message was too large to fit into the specified buffer and was truncated.");
        break;
    case EALREADY:
        lpszRetStr=(NLchar *)TEXT("A non-blocking connect call is in progress on the specified socket.");
        break;
    case ENOPROTOOPT:
        lpszRetStr=(NLchar *)TEXT("Bad protocol option.");
        break;
    case EPFNOSUPPORT:
        lpszRetStr=(NLchar *)TEXT("Protocol family not supported.");
        break;
    case ETOOMANYREFS:
        lpszRetStr=(NLchar *)TEXT("Too many references; can't splice.");
        break;
    case ELOOP:
        lpszRetStr=(NLchar *)TEXT("Too many levels of symbolic links.");
        break;
    case ENAMETOOLONG:
        lpszRetStr=(NLchar *)TEXT("File name too long.");
        break;
    case ENOTEMPTY:
        lpszRetStr=(NLchar *)TEXT("Directory not empty.");
        break;
        
#if !defined(macintosh)
    case EUSERS:
        lpszRetStr=(NLchar *)TEXT("Too many users.");
        break;
    case EDQUOT:
        lpszRetStr=(NLchar *)TEXT("Disc quota exceeded.");
        break;
    case ESTALE:
        lpszRetStr=(NLchar *)TEXT("Stale NFS file handle.");
        break;
    case EREMOTE:
        lpszRetStr=(NLchar *)TEXT("Too many levels of remote in path.");
        break;
#endif        
    default:
#ifdef WINDOWS_APP
        {
            static NLchar temp[256];

            /* FormatMessage is unicode compliant */
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)temp, 256, NULL);
            lpszRetStr = temp;
        }
#else
        lpszRetStr=strerror(err);
#endif
        break;
  }
  return (const NLchar*)lpszRetStr;
}

NL_EXP const /*@observer@*/ NLchar* NL_APIENTRY nlGetErrorStr(NLenum err)
{
    NLchar *retStr;
    
    switch(err) {
    case NL_NO_ERROR:
        retStr = (NLchar *)TEXT("No HawkNL error.");
        break;
    case NL_NO_NETWORK:
        retStr = (NLchar *)TEXT("No network was found on init.");
        break;
    case NL_OUT_OF_MEMORY:
        retStr = (NLchar *)TEXT("Out of memory.");
        break;
    case NL_INVALID_ENUM:
        retStr = (NLchar *)TEXT("Invalid NLenum.");
        break;
    case NL_INVALID_SOCKET:
        retStr = (NLchar *)TEXT("Socket is not valid.");
        break;
    case NL_INVALID_PORT:
        retStr = (NLchar *)TEXT("Port could not be opened.");
        break;
    case NL_INVALID_TYPE:
        retStr = (NLchar *)TEXT("Network type is not available.");
        break;
    case NL_SYSTEM_ERROR:
        retStr = (NLchar *)TEXT("A system error occurred, call nlGetSystemError.");
        break;
    case NL_SOCK_DISCONNECT:
        retStr = (NLchar *)TEXT("Connection error: Close socket.");
        break;
    case NL_NOT_LISTEN:
        retStr = (NLchar *)TEXT("Socket has not been set to listen.");
        break;
    case NL_CON_REFUSED:
        retStr = (NLchar *)TEXT("Connection refused.");
        break;
    case NL_NO_PENDING:
        retStr = (NLchar *)TEXT("No pending connections to accept.");
        break;
    case NL_BAD_ADDR:
        retStr = (NLchar *)TEXT("The address or port are not valid.");
        break;
    case NL_MESSAGE_END:
        retStr = (NLchar *)TEXT("TCP message end.");
        break;
    case NL_NULL_POINTER:
        retStr = (NLchar *)TEXT("A NULL pointer was passed to a function.");
        break;
    case NL_INVALID_GROUP:
        retStr = (NLchar *)TEXT("The group is not valid.");
        break;
    case NL_OUT_OF_GROUPS:
        retStr = (NLchar *)TEXT("Out of groups.");
        break;
    case NL_OUT_OF_GROUP_SOCKETS:
        retStr = (NLchar *)TEXT("The group is full.");
        break;
    case NL_BUFFER_SIZE:
        retStr = (NLchar *)TEXT("The buffer is too small.");
        break;
    case NL_PACKET_SIZE:
        retStr = (NLchar *)TEXT("The packet is too large.");
        break;
    case NL_WRONG_TYPE:
        retStr = (NLchar *)TEXT("Wrong socket type.");
        break;
    case NL_CON_PENDING:
        retStr = (NLchar *)TEXT("A non-blocking connection is still pending.");
        break;
    case NL_SELECT_NET_ERROR:
        retStr = (NLchar *)TEXT("A network type is already selected.");
        break;
    case NL_PACKET_SYNC:
        retStr = (NLchar *)TEXT("The NL_RELIABLE_PACKET stream is out of sync.");
        break;
    case NL_TLS_ERROR:
        retStr = (NLchar *)TEXT("Thread local storage could not be created.");
        break;
    case NL_TIMED_OUT:
        retStr = (NLchar *)TEXT("The function timed out.");
        break;
    case NL_SOCKET_NOT_FOUND:
        retStr = (NLchar *)TEXT("The socket was not found in the group.");
        break;
    case NL_STRING_OVER_RUN:
        retStr = (NLchar *)TEXT("The string could cause a buffer over-run or corrupt memory.");
        break;
    case NL_MUTEX_RECURSION:
        retStr = (NLchar *)TEXT("The mutex was recursivly locked by a single thread.");
        break;
    case NL_MUTEX_OWNER:
        retStr = (NLchar *)TEXT("The mutex is not owned by the thread.");
        break;

    default:
        retStr = (NLchar *)TEXT("Undefined HawkNL error.");
        break;
    }
    return (const NLchar*)retStr;
}

