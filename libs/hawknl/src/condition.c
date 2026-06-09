/*
  HawkNL condition module
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

enum {
    SIGNAL = 0,
    BROADCAST = 1,
    MAX_EVENTS = 2
};

struct nl_cond_t
{
  HANDLE events_[MAX_EVENTS];
};

#ifndef __MINGW32__
struct timespec {
	long tv_sec;
	long tv_nsec;
};
#endif

#else /* !NL_WIN_THREADS */

#ifndef WINDOWS_APP
#include <sys/time.h>
#endif /* WINDOWS_APP */

#include <errno.h>
#include <pthread.h>
struct nl_cond_t
{
  pthread_cond_t    cond;
  pthread_mutex_t   mutex;
};

#endif /* !NL_WIN_THREADS */

NL_EXP NLboolean NL_APIENTRY nlCondInit(NLcond *cond)
{
	if(cond == NULL)
	{
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
	}
    else
    {
#ifdef NL_WIN_THREADS
        NLcond cv = NULL;

        cv = (NLcond)malloc(sizeof(struct nl_cond_t));
        if(cv == NULL)
        {
		    nlSetError(NL_OUT_OF_MEMORY);
            return NL_FALSE;
        }
        cv->events_[SIGNAL] = CreateEvent(NULL, FALSE, FALSE, NULL);
        cv->events_[BROADCAST] = CreateEvent(NULL, TRUE, FALSE, NULL);
#else
        int         result;
        NLcond   cv = NULL;

        cv = (NLcond)malloc(sizeof(struct nl_cond_t));
        if(cv == NULL)
        {
		    nlSetError(NL_OUT_OF_MEMORY);
            return NL_FALSE;
        }

        result = pthread_cond_init((pthread_cond_t *)&cv->cond, NULL);
        if(result != 0)
        {
            free(cv);
#ifdef WINDOWS_APP
            SetLastError((DWORD)result);
#endif
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
        result = pthread_mutex_init((pthread_mutex_t *)&cv->mutex, NULL);
        if(result != 0)
        {
            (void)pthread_cond_destroy((pthread_cond_t *)&cv->cond);
            free(cv);
#ifdef WINDOWS_APP
            SetLastError((DWORD)result);
#endif
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
#endif
        *cond = cv;
    }
    return NL_TRUE;
}

NL_EXP NLboolean NL_APIENTRY nlCondWait(NLcond *cond, NLint timeout)
{
    if(cond == NULL)
    {
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
    }
    if(*cond == NULL)
    {
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
    }
    if(timeout <= 0)
    {
        NLcond cv = *cond;

#ifdef NL_WIN_THREADS
        DWORD result;

        result = WaitForMultipleObjects (2,cv->events_, FALSE, INFINITE);
        if(result == WAIT_FAILED)
        {
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
#else
        int result = 0;

        (void)pthread_mutex_lock((pthread_mutex_t *)&cv->mutex);
        result = pthread_cond_wait((pthread_cond_t *)&cv->cond, (pthread_mutex_t *)&cv->mutex);
        if(result != 0)
        {
#ifdef WINDOWS_APP
            SetLastError((DWORD)result);
#endif
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
        (void)pthread_mutex_unlock((pthread_mutex_t *)&cv->mutex);
#endif
    }
    else
    {
        NLcond cv = *cond;

#ifdef NL_WIN_THREADS
        DWORD result;

        result = WaitForMultipleObjects (2, cv->events_, FALSE, (DWORD)timeout);
        if(result == WAIT_FAILED)
        {
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
        else if(result == WAIT_TIMEOUT)
        {
            nlSetError(NL_TIMED_OUT);
            return NL_FALSE;
        }
    }
#else
        int                 result = 0;
        struct timespec     tv;
        NLtime              t;
        NLlong              ms;

        /* convert timeout to an absolute time */
        (void)nlTime(&t);
        ms = t.mseconds + timeout;
        tv.tv_sec = t.seconds + (ms / 1000);
        tv.tv_nsec = (ms % 1000) * 1000;

        (void)pthread_mutex_lock((pthread_mutex_t *)&cv->mutex);
        result = pthread_cond_timedwait((pthread_cond_t *)&cv->cond,
                                            (pthread_mutex_t *)&cv->mutex, &tv);
        if(result == ETIMEDOUT)
        {
            nlSetError(NL_TIMED_OUT);
            (void)pthread_mutex_unlock((pthread_mutex_t *)&cv->mutex);
            return NL_FALSE;
        }
        else if(result != 0)
        {
#ifdef WINDOWS_APP
            SetLastError((DWORD)result);
#endif
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
        (void)pthread_mutex_unlock((pthread_mutex_t *)&cv->mutex);
    }
#endif
    return NL_TRUE;
}

NL_EXP NLboolean NL_APIENTRY nlCondSignal(NLcond *cond)
{
    NLcond cv;

    if(cond == NULL)
    {
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
    }
    if(*cond == NULL)
    {
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
    }
    else
    {
#ifdef NL_WIN_THREADS
        cv = *cond;
        if(PulseEvent(cv->events_[SIGNAL]) == 0)
        {
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
#else
        int result;

        cv = *cond;
        result = pthread_cond_signal((pthread_cond_t *)&cv->cond);
        if(result != 0)
        {
#ifdef WINDOWS_APP
            SetLastError((DWORD)result);
#endif
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
#endif
    }
    return NL_TRUE;
}

NL_EXP NLboolean NL_APIENTRY nlCondBroadcast(NLcond *cond)
{
    if(cond == NULL)
    {
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
    }
    if(*cond == NULL)
    {
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
    }
    else
    {
        NLcond cv = *cond;

#ifdef NL_WIN_THREADS
        if(PulseEvent(cv->events_[BROADCAST]) == 0)
        {
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
#else
        int result;

        result = pthread_cond_broadcast((pthread_cond_t *)&cv->cond);
        if(result != 0)
        {
#ifdef WINDOWS_APP
            SetLastError((DWORD)result);
#endif
            nlSetError(NL_SYSTEM_ERROR);
            return NL_FALSE;
        }
#endif
    }
    return NL_TRUE;
}

NL_EXP NLboolean NL_APIENTRY nlCondDestroy(NLcond *cond)
{
    if(cond == NULL)
    {
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
    }
    if(*cond == NULL)
    {
		nlSetError(NL_NULL_POINTER);
		return NL_FALSE;
    }
    else
    {
        NLcond cv = *cond;

#ifdef NL_WIN_THREADS
        (void)CloseHandle(cv->events_[SIGNAL]);
        (void)CloseHandle(cv->events_[BROADCAST]);
#else
        (void)pthread_cond_destroy((pthread_cond_t *)&cv->cond);
#endif
        free(*cond);
        *cond = NULL;
    }
    return NL_TRUE;
}


