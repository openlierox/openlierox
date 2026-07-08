/*
	OpenLieroX

	Mutex wrapper

	created 10-02-2009 by Karel Petranek
	code under LGPL
*/

#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <mutex>
#ifdef DEBUG
#include <thread>
#endif
#include "CodeAttributes.h"

// Mutex wrapper class with some extra debugging checks
class Mutex : DontCopyTag {
private:
	std::mutex m_mutex;

#ifdef DEBUG
	std::thread::id m_lockedThread;  // thread that holds the lock, or default-constructed if none

	void _lock_pre();
	void _lock_post();
	void _unlock_pre();
	void _unlock_post();
#endif

public:
#ifdef DEBUG
	~Mutex();
	void lock();
	void unlock();
#else
	void lock()		{ m_mutex.lock(); }
	void unlock()	{ m_mutex.unlock(); }
#endif

	struct ScopedLock : DontCopyTag {
		Mutex& mutex;
		ScopedLock(Mutex& m) : mutex(m) { mutex.lock(); }
		~ScopedLock() { mutex.unlock(); }
	};

	struct ScopedUnlock : DontCopyTag {
		Mutex& mutex;
		ScopedUnlock(Mutex& m) : mutex(m) { mutex.unlock(); }
		~ScopedUnlock() { mutex.lock(); }
	};
};

#endif // __MUTEX_H__
