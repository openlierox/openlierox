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

#ifndef INTERNAL_H
#define INTERNAL_H

#include "../include/nl.h"

/* for malloc and free */
#include <stdlib.h>

/* Windows CE does not have time.h functions */
#if defined (_WIN32_WCE)
extern time_t time(time_t *timer);
#else
#include <time.h>
#endif


#ifdef NL_LITTLE_ENDIAN
#define NL_SWAP_TRUE (nlState.nl_big_endian_data == NL_TRUE)
#else
#define NL_SWAP_TRUE (nlState.nl_big_endian_data != NL_TRUE)
#endif /* NL_LITTLE_ENDIAN */

#ifdef WINDOWS_APP
/* Windows systems */
#ifdef _MSC_VER
#pragma warning (disable:4201)
#pragma warning (disable:4214)
#endif /* _MSC_VER */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#ifdef _MSC_VER
#pragma warning (default:4201)
#pragma warning (default:4214)
#endif /* _MSC_VER */

#endif

/* part of portable unicode support */
#if !defined _TCHAR_DEFINED && !(defined _WCHAR_T_DEFINED && defined (__LCC__))
#ifdef _UNICODE
#define TEXT(x)    L##x
#define _tcsncat    wcsncat
#define _stprintf   swprintf
#define _sntprintf  snwprintf
#define _stscanf    swscanf
#define _tcsncpy    wcsncpy
#define _tcscspn    wcscspn
#define _tcschr     wcschr
#define _tcslen     wcslen
#define _tcsrchr    wcsrchr
#ifdef WINDOWS_APP
#define _ttoi       _wtoi
#else /* !WINDOWS_APP*/
#define _ttoi       wtoi
#endif /* !WINDOWS_APP*/
#else /* !UNICODE */
#define TEXT(x)    x
#define _tcsncat    strncat
#define _stprintf   sprintf
#define _sntprintf  snprintf
#define _stscanf    sscanf
#define _tcsncpy    strncpy
#define _tcscspn    strcspn
#define _tcschr     strchr
#define _tcslen     strlen
#define _ttoi       atoi
#define _tcsrchr    strrchr
#endif /* !UNICODE */
#endif /* _INC_TCHAR */


#define NL_FIRST_GROUP          (200000 + 1)

/* the minumum number of sockets that will be allocated */
#define NL_MIN_SOCKETS          16

/* number of buckets for average bytes/second */
#define NL_NUM_BUCKETS          8

/* number of packets stored for NL_LOOP_BACK */
#define NL_NUM_PACKETS          8
#define NL_MAX_ACCEPT           10

/* for nlLockSocket and nlUnlockSocket */
#define NL_READ                 0x0001
#define NL_WRITE                0x0002
#define NL_BOTH                 (NL_READ|NL_WRITE)

/* time in milliseconds that unreliable connect/accepts sleep while waiting */
#define NL_CONNECT_SLEEP    50

#ifdef __cplusplus
extern "C" {
#endif

/* the driver object */
typedef struct
{
    const NLchar /*@observer@*/*name;
    const NLchar /*@observer@*/*connections;
    NLenum      type;
    NLboolean   initialized;
    NLboolean   (*Init)(void);
    void        (*Shutdown)(void);
    NLboolean   (*Listen)(NLsocket socket);
    NLsocket    (*AcceptConnection)(NLsocket socket);
    NLsocket    (*Open)(NLushort port, NLenum type);
    NLboolean   (*Connect)(NLsocket socket, const NLaddress *address);
    void        (*Close)(NLsocket socket);
    NLint       (*Read)(NLsocket socket, /*@out@*/ NLvoid *buffer, NLint nbytes);
    NLint       (*Write)(NLsocket socket, const NLvoid *buffer, NLint nbytes);
    NLchar      *(*AddrToString)(const NLaddress *address, /*@returned@*/ /*@out@*/ NLchar *string);
    NLboolean   (*StringToAddr)(const NLchar *string, /*@out@*/ NLaddress *address);
    NLboolean   (*GetLocalAddr)(NLsocket socket, /*@out@*/ NLaddress *address);
    NLaddress   *(*GetAllLocalAddr)(/*@out@*/ NLint *count);
    NLboolean   (*SetLocalAddr)(const NLaddress *address);
    NLchar      *(*GetNameFromAddr)(const NLaddress *address, /*@returned@*/ /*@out@*/ NLchar *name);
    NLboolean   (*GetNameFromAddrAsync)(const NLaddress *address, /*@out@*/ NLchar *name);
    NLboolean   (*GetAddrFromName)(const NLchar *name, /*@out@*/ NLaddress *address);
    NLboolean   (*GetAddrFromNameAsync)(const NLchar *name, /*@out@*/ NLaddress *address);
    NLboolean   (*AddrCompare)(const NLaddress *address1, const NLaddress *address2);
    NLushort    (*GetPortFromAddr)(const NLaddress *address);
    void        (*SetAddrPort)(NLaddress *address, NLushort port);
    NLint       (*GetSystemError)(void);
    NLint       (*PollGroup)(NLint group, NLenum name, /*@out@*/ NLsocket *sockets,
                                NLint number, NLint timeout);
    NLboolean   (*Hint)(NLenum name, NLint arg);
} nl_netdriver_t;

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

typedef struct
{
    NLboolean   socketStats;    /* enable collection of socket read/write statistics, default disabled */
    NLboolean   nl_big_endian_data; /* is the packet data big endian? */
} nl_state_t;

/* used by the drivers to allocate and free socket objects */
NLsocket nlGetNewSocket(void);

/* other functions */
NLboolean nlGroupInit(void);
void nlGroupShutdown(void);
void nlGroupLock(void);
void nlGroupUnlock(void);
NLboolean nlGroupGetSocketsINT(NLint group, /*@out@*/ NLsocket *socket, /*@in@*/ NLint *number);
NLboolean nlIsValidSocket(NLsocket socket);
NLboolean nlLockSocket(NLsocket socket, NLint which);
void nlUnlockSocket(NLsocket socket, NLint which);
void nlSetError(NLenum err);
void nlThreadSleep(NLint mseconds);

/* globals (as few as possible) */
extern volatile nl_state_t nlState;

typedef /*@only@*/ nl_socket_t *pnl_socket_t;
extern /*@only@*/ pnl_socket_t *nlSockets;

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* INTERNAL_H */

