/*
	OpenLieroX

	Mutex wrapper implementation

	created 10-02-2009 by Karel Petranek
	code under LGPL
*/

#include <thread>

#include "Mutex.h"
#include "Debug.h"

#ifdef DEBUG

void Mutex::_lock_pre() {
	// If the current thread already holds the lock,
	// lock() was called twice from the same thread, which deadlocks.
	if (m_lockedThread == std::this_thread::get_id())  {
		errors << "Called mutex lock twice from the same thread, this will cause a deadlock" << endl;
		assert(false);
	}
}

void Mutex::_lock_post() {
	m_lockedThread = std::this_thread::get_id();  // we hold the lock now
}

void Mutex::_unlock_pre() {
	// The mutex must be released from the same thread that locked it.
	if (m_lockedThread != std::this_thread::get_id())  {
		errors << "Releasing the mutex in other thread than locking it, this will cause a deadlock" << endl;
		assert(false);
	}
	m_lockedThread = std::thread::id();  // lock released
}

void Mutex::_unlock_post() {
	// Nothing to do.
}

Mutex::~Mutex()
{
	// If a thread still holds the lock, we're destroying a locked mutex.
	if (m_lockedThread != std::thread::id())  {
		errors << "Destroyed a locked mutex" << endl;
		assert(false);
	}
}

void Mutex::lock()
{
	_lock_pre();
	m_mutex.lock();
	_lock_post();
}

void Mutex::unlock()
{
	_unlock_pre();
	m_mutex.unlock();
	_unlock_post();
}

#endif
