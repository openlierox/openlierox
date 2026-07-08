/*
  HawkNL thread module
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

#include "nlinternal.h"

#ifdef NL_WIN_THREADS

typedef struct {
    void *(*func) (void *);
    void *arg;
} ThreadParms;

static unsigned  __stdcall threadfunc(void *arg)
{
    void *(*func) (void *);
    void *args;
    
    func = ((ThreadParms *)arg)->func;
    args = ((ThreadParms *)arg)->arg;
    free(arg);
    
    return (unsigned)((*func)(args));
}
#if defined (_WIN32_WCE)
#define _beginthreadex(security, \
		       stack_size, \
		       start_proc, \
		       arg, \
		       flags, \
		       pid) \
	CreateThread(security, \
		     stack_size, \
		     (LPTHREAD_START_ROUTINE) start_proc, \
		     arg, \
		     flags, \
		     pid)
#else /* !(_WIN32_WCE) */
#include <process.h>
#endif /* !(_WIN32_WCE) */

#else /* !NL_WIN_THREADS */
/* POSIX systems */
#include <pthread.h>
#include <sched.h>
#endif /* !NL_WIN_THREADS */

#ifndef WINDOWS_APP
#include <unistd.h>
#endif /* WINDOWS_APP */

NL_EXP NLthreadID NL_APIENTRY nlThreadCreate(NLThreadFunc func, void *data, NLboolean joinable)
{
    /* Windows threads */
#ifdef NL_WIN_THREADS
    HANDLE          h;
    unsigned        tid;
    ThreadParms     *p;

    p = (ThreadParms *)malloc(sizeof(ThreadParms));
    if(p == NULL)
    {
        nlSetError(NL_OUT_OF_MEMORY);
        return (NLthreadID)NL_INVALID;
    }
    p->func = func;
    p->arg = data;
    h = (HANDLE)_beginthreadex(NULL, 0, threadfunc, p, 0, &tid);
    if(h == (HANDLE)(0))
    {
        nlSetError(NL_SYSTEM_ERROR);
        return (NLthreadID)NL_INVALID;
    }
    if(joinable == NL_FALSE)
    {
        (void)CloseHandle(h);
        return NULL;
    }
    return (NLthreadID)h;
    
    /* POSIX systems */
#else
    pthread_attr_t  attr;
    pthread_t       tid;
    int             result;
    
    (void)pthread_attr_init(&attr);
    if(joinable == NL_FALSE)
    {
        (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    }
    else
    {
        (void)pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    }
    result = pthread_create(&tid, &attr, func, data);
    (void)pthread_attr_destroy(&attr);
    if(result != 0)
    {
#ifdef WINDOWS_APP
            SetLastError((DWORD)result);
#endif
            nlSetError(NL_SYSTEM_ERROR);
            return (NLthreadID)NL_INVALID;
    }
    if(joinable == NL_FALSE)
    {
        return NULL;
    }
    return (NLthreadID)tid;
#endif
}

NL_EXP void NL_APIENTRY nlThreadYield(void)
{
    /* Windows threads */
#ifdef NL_WIN_THREADS
    Sleep((DWORD)0);

    /* POSIX systems */
#else
    (void)sched_yield();
    
#endif
}

NL_EXP NLboolean NL_APIENTRY nlThreadJoin(NLthreadID threadID, void **status)
{
    /* Windows threads */
#ifdef NL_WIN_THREADS
    if(WaitForSingleObject((HANDLE)threadID, INFINITE) == WAIT_FAILED)
    {
        nlSetError(NL_SYSTEM_ERROR);
        return NL_FALSE;
    }
    if(status != NULL)
    {
        (void)GetExitCodeThread((HANDLE)threadID, (LPDWORD)status);
    }
    (void)CloseHandle((HANDLE)threadID);

    /* POSIX systems */
#else
    int     result;

    result = pthread_join((pthread_t)threadID, status);
    if(result != 0)
    {
#ifdef WINDOWS_APP
            SetLastError((DWORD)result);
#endif
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
    }
#endif
    return NL_TRUE;
}

void nlThreadSleep(NLint mseconds)
{
#ifdef WINDOWS_APP
    Sleep((DWORD)mseconds);
#else /* !WINDOWS_APP */
    struct timespec     tv;

    tv.tv_sec = mseconds / 1000;
    tv.tv_nsec = (mseconds % 1000) * 1000000;

    (void)nanosleep(&tv, NULL);
#endif /* !WINDOWS_APP */
    /* can use usleep if nanosleep is not supported on your platform */
/*    (void)usleep(mseconds*1000); */
}


