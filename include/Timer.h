/////////////////////////////////////////
//
//   OpenLieroX
//
//   Auxiliary Software class library
//
//   based on the work of JasonB
//   enhanced by Dark Charlie and Albert Zeyer
//
//   code under LGPL
//
/////////////////////////////////////////


// Timer
// Created 12/11/01
// Jason Boettcher


#ifndef __TIMER_H__
#define __TIMER_H__

#include <string>
#include <SDL.h>
#include <cassert>
#include "Event.h"
#include "olx-types.h"
#include "CodeAttributes.h"



struct TimeCounter {
	SDL_mutex* mutex;
	AbsTime time;
	Uint32 lastTicks;
	
	TimeCounter() : time(0), lastTicks(0) { mutex = SDL_CreateMutex(); lastTicks = SDL_GetTicks(); }
	~TimeCounter() { SDL_DestroyMutex(mutex); mutex = NULL; }
	AbsTime update() {
		if(mutex) SDL_mutexP(mutex);
		Uint32 curTicks = SDL_GetTicks();
		if(curTicks < lastTicks) {
			AbsTime t = time;
			lastTicks = curTicks; // ignore (that should only happen once every ~49 days or when SDL gets reinited)
			if(mutex) SDL_mutexV(mutex);
			return t;
		}
		TimeDiff td = TimeDiff((Uint64)(curTicks - lastTicks));
		time += td;
		AbsTime t = time;
		lastTicks = curTicks;
		if(mutex) SDL_mutexV(mutex);
		return t;
	}
};
extern TimeCounter timeCounter;

INLINE AbsTime GetTime() { return timeCounter.update(); }


int				GetFPS();
int				GetMinFPS();
std::string		GetDateTime();



/*
	---------------------------------
	event-driven Timer class
*/

/*
	Timer class

	After start(), there is a thread which will push frequently events to the
	SDL event queue. It will use the settings at the time of starting the timer.
	All later changes are ignored. If you hit start again, the current thread
	will stop and a new one with the new settings will be started. stop() will
	stop the timer.
	
	After stop() returns, no more events belonging to this thread
	will be handled (this is guaranteed). Though it's possible that there
	is one last event handled exactly at the time of calling stop() when
	calling it from another thread than the main thread. If you call stop()
	from the main-thread, there will be no more timer-event for sure.
	
	The events itself will be handled in the main thread
	(in the thread that calls ProcessEvents()).

	If the callback-functions returns false, the thread will also stop.

	You can also use startHeadless() which will run independently from the
	object. That means that stop() has no effect on the thread. The only
	possibility to break the timer is to return false from within the callback.
	
	userData can be used to point to some additional data. It's just a pointer,
	the timer itself just forwards it but ignores the content behind. So you
	have to care yourself about memory management.
	
	WARNING: don't set a too short interval because else the SDL event queue get flooded

	TODO: a CleanupCallback could also be saved to be called at the very last time
	after the last Timer-event to cleanup the userData for example.
	if somebody needs this, feel free to implement
*/
struct TimerData;

class Timer {
public:
	struct EventData {
		Timer* sender;
		void* userData;
		bool& shouldContinue;
		EventData(Timer* s, void* d, bool& c) : sender(s), userData(d), shouldContinue(c) {}
	};

	Timer();
	Timer(const std::string& nam, Null, void* dat = NULL, Uint32 t = 1000, bool o = false);
	Timer(const std::string& nam, void (*fct)(EventData dat), void* dat = NULL, Uint32 t = 1000, bool o = false);
	Timer(const std::string& nam, Ref<Event<EventData>::Handler> hndl, void* dat = NULL, Uint32 t = 1000, bool o = false);
	Timer(const std::string& nam, const Timer& t) { (*this) = t; }
	Timer& operator=(const Timer& t) { name = t.name; onTimer = t.onTimer; userData = t.userData; interval = t.interval; once = t.once; return *this; }
	~Timer();

	std::string name;
	Event<EventData> onTimer;
	void* userData; // forwarded to the event-function
	Uint32 interval; // how often an event is pushed in the SDL queue
	bool once; // the event is only pushed once and then it quits
	bool running(); // this timer is currently running
	
	bool start();
	bool startHeadless(); // start independent timer
	void stop();

	// Internal: the event handler calls this when a TimerData is freed,
	// so a finished/stopped timer never dereferences it.
	void detachData(TimerData* d);

private:
	bool m_running;
	TimerData* m_lastData;	// it's TimerData* intern
};

void InitializeTimers();
void ShutdownTimers();


// A class for profiling - measures the time between being constructed and destructed
// Just put it in a scope/function you want to profile, it will print the result to console
class StopWatch  {
private:
	AbsTime start;
	std::string name;

public:
	StopWatch(const std::string& name);
	~StopWatch();
};




#endif  //  __TIMER_H__
