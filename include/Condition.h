/*
	Condition wrapper

	OpenLieroX

	code under LGPL
	created 11-05-2009
*/

#ifndef __OLX__CONDITION_H__
#define __OLX__CONDITION_H__

#include <condition_variable>
#include <chrono>
#include "Mutex.h"

// Thin wrapper over std::condition_variable_any,
// which can wait on any lock()/unlock() type -- here our Mutex.
class Condition {
private:
	std::condition_variable_any cond;
public:
	void signal() { cond.notify_one(); }
	void broadcast() { cond.notify_all(); }

	// mutex must be held by the caller;
	// it is released while waiting and reacquired before returning.
	void wait(Mutex& mutex) { cond.wait(mutex); }
	bool wait(Mutex& mutex, unsigned int ms) {
		return cond.wait_for(mutex, std::chrono::milliseconds(ms)) == std::cv_status::no_timeout;
	}
};

#endif
