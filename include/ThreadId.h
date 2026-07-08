/*
 *  ThreadId.h
 *  OpenLieroX
 *
 *  code under LGPL
 *
 */

#ifndef __OLX__THREADID_H__
#define __OLX__THREADID_H__

#include <stdint.h>

// Under Win, it's HANDLE (which is void*).
// Otherwise, it's pthread_t, which is also a ptr-type.
typedef uintptr_t ThreadId;

#endif
