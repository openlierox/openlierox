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

#ifndef SOCKETS_H
#define SOCKETS_H

#ifdef __cplusplus
extern "C" {
#endif

NLboolean sock_Init(void);
void sock_Shutdown(void);
NLboolean sock_Listen(NLsocket socket);
NLsocket sock_AcceptConnection(NLsocket socket);
NLsocket sock_Open(NLushort port, NLenum type);
NLboolean sock_Connect(NLsocket socket, const NLaddress *address);
void sock_Close(NLsocket socket);
NLint sock_Read(NLsocket socket, /*@out@*/ NLvoid *buffer, NLint nbytes);
NLint sock_Write(NLsocket socket, const NLvoid *buffer, NLint nbytes);
NLchar *sock_AddrToString(const NLaddress *address, /*@returned@*/ /*@out@*/ NLchar *string);
NLboolean sock_StringToAddr(const NLchar *string, /*@out@*/ NLaddress *address);
NLboolean sock_GetLocalAddr(NLsocket socket, /*@out@*/ NLaddress *address);
NLboolean sock_SetLocalAddr(const NLaddress *address);
NLboolean sock_AddrCompare(const NLaddress *address1, const NLaddress *address2);
NLushort sock_GetPortFromAddr(const NLaddress *address);
void sock_SetAddrPort(NLaddress *address, NLushort port);
NLint sock_GetSystemError(void);
NLint sock_PollGroup(NLint group, NLenum name, /*@out@*/ NLsocket *sockets, NLint number, NLint timeout);
NLboolean sock_Hint(NLenum name, NLint arg);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* SOCKETS_H */

