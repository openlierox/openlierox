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


#include <list>
#include "ThreadPool.h"
#include "ReadWriteLock.h"
#include <time.h>
#include <cassert>
#include "Timer.h"
#include "Debug.h"
#include "InputEvents.h"
#include "game/Game.h"


TimeCounter timeCounter;

int		Frames = 0;
AbsTime	OldFPSTime = AbsTime();
int		Fps = 0;


///////////////////
// Get the frames per second count
// Should be called once per frame
int GetFPS()
{
	Frames++;

	TimeDiff dt = GetTime() - OldFPSTime;
	if(dt >= 1.0f) {
		OldFPSTime = GetTime();
		Fps = (int)( (float)Frames / dt.seconds() );
		Frames = 0;
	}

	return Fps;
}

int		Frames_MinFPS = 0;
AbsTime	OldFPSTime_MinFPS = AbsTime();
AbsTime	PrevFrameTime_MinFPS = AbsTime();
TimeDiff	MaxFrameTime_MinFPS = TimeDiff();
int		Fps_MinFPS = 0;

///////////////////
// Get the minimal frames per second count ( the slowest frame )
// Should be called once per frame
int GetMinFPS()
{
	Frames_MinFPS++;
	AbsTime ms = GetTime();
	if( ms - OldFPSTime_MinFPS >= 1.0f ) 
	{
		OldFPSTime_MinFPS = ms;

		if( MaxFrameTime_MinFPS > 0.00000001f )
			Fps_MinFPS = (int)( 1.0f / MaxFrameTime_MinFPS.seconds() );
		if( Fps_MinFPS > Fps )
			Fps_MinFPS = Fps;

		MaxFrameTime_MinFPS = ms - PrevFrameTime_MinFPS;
		Frames_MinFPS = 0;
	}

	if( MaxFrameTime_MinFPS < ms - PrevFrameTime_MinFPS )
		MaxFrameTime_MinFPS = ms - PrevFrameTime_MinFPS;

	PrevFrameTime_MinFPS = ms;

	return Fps_MinFPS;
}

///////////////////
// Get the actual time
std::string GetDateTime()
{
	char cTime[64];
	cTime[0]= '\0';
	time_t t;
	time(&t);
	struct tm* tp;
	tp = localtime(&t);
	if (tp)
		strftime(cTime, 26, "%Y-%m-%d-%a-%H-%M-%S", tp);
	cTime[63] = '\0';
	return cTime;
}








// -----------------------------------
// Timer class
// HINT: the timer is not exact, do not use it for any exact timing, like ingame simulation



struct InternTimerEventData {
	TimerData* data; // for intern struct TimerData
	bool lastEvent;
	InternTimerEventData(TimerData* d, bool le) : data(d), lastEvent(le) {}
};
Event<InternTimerEventData> onInternTimerSignal;
static bool timerSystem_inited = false;


static void Timer_handleEvent(InternTimerEventData data);

static void InitTimerSystem() {
	if(timerSystem_inited) return;
	onInternTimerSignal.handler() = getEventHandler(&Timer_handleEvent);
	timerSystem_inited = true;
}

struct TimerData;
static void RemoveTimerFromGlobalList(TimerData *data);


// Timer data, contains almost the same info as the timer class
struct TimerData {
	Timer*				timer;
	std::string			name;
	Event<Timer::EventData>::HandlerList onTimerHandler;
	void*				userData;
	Uint32				interval;
	bool				once;
	bool				quitSignal;
	SDL_cond*			quitCond;
	SDL_mutex*			mutex;
	ThreadPoolItem*		thread;
	
	TimerData() : timer(NULL), userData(NULL), interval(0), once(false), quitSignal(false), quitCond(NULL), mutex(NULL), thread(NULL) {
		mutex = SDL_CreateMutex();
		quitCond = SDL_CreateCond();
		// TODO: not threadsafe
		//timers.push_back(data); // Add it to the global timer array
	}
	~TimerData() {
		breakThread();
		if(thread) threadPool->wait(thread, NULL);
		thread = NULL;
		SDL_DestroyMutex(mutex); mutex = NULL;
		SDL_DestroyCond(quitCond); quitCond = NULL;
		RemoveTimerFromGlobalList(this);
	}
	
	void breakThread() {
		ScopedLock lock(mutex);
		quitSignal = true;
		SDL_CondSignal(quitCond);
	}

	void startThread() {
		assert(thread == NULL);
		
		struct TimerHandler : Action {
			TimerData* data;
			
			// thread function
			Result handle() {
	
				SDL_mutexP(data->mutex);
				while(true) {
					SDL_CondWaitTimeout(data->quitCond, data->mutex, data->interval);
					
					bool lastEvent = data->once || data->quitSignal || game.state == Game::S_Quit;
					onInternTimerSignal.pushToMainQueue(InternTimerEventData(data, lastEvent));
	
					if(lastEvent) {
						SDL_mutexV(data->mutex);
						// we have to return it here to ensure that this callback is never called again
						// we also have to ensure that there is only *one* event with lastEvent=true
						// (and this event has to be of course the last event for this timer in the queue)
						return true;
					}
				}
		
				return "should not happen";
			}
		
			TimerHandler(TimerData* d) : data(d) {}
		};
		
		thread = threadPool->start(new TimerHandler(this), name + " timer");
	}
	
};

// Global list that holds info about headless timers
// Used to make sure there are no memory leaks
// TODO: not threadsafe
// TODO: why not std::set?
std::list<TimerData *> timers;

//////////////////
// Initialize working with timers
void InitializeTimers()
{

}

///////////////////////
// Shut down and free all running timers
void ShutdownTimers()
{
// TODO: not threadsafe
// TODO: why not std::set?
	//assert(!EventSystemInited()); // Make sure the event system is shut down (to avoid double freed memory)

	/*
	// Stop and free all the running timers
	for (std::list<TimerData *>::iterator it = timers.begin(); it != timers.end(); it++)  {

		// Call the user function, because it might free some data
		Event<Timer::EventData>::Handler& handler = (*it)->timer ? (*it)->timer->onTimer.handler().get() : (*it)->onTimerHandler.get();
		bool cont = true;
		(*it)->breakThread();
		if (&handler) // Make sure it exists
			handler(Timer::EventData(NULL, (*it)->userData, cont));

		delete (*it);
	}

	timers.clear();*/
}

/////////////////////////
// Removes the timer from the global list (called when the timer is stopped/destroyed)
static void RemoveTimerFromGlobalList(TimerData *data)
{
// TODO: not threadsafe
// TODO: why not std::set?
	/*
	for (std::list<TimerData *>::iterator it = timers.begin(); it != timers.end(); it++)  {
		if (*it == data)  {
			timers.erase(it);
			break;
		}
	}*/
}


////////////////
// Constructors
Timer::Timer() : 
	name(""), userData(NULL), interval(1000),
	once(false), m_running(false), m_lastData(NULL) {
	InitTimerSystem();
}

Timer::Timer(const std::string& nam, Null, void* dat, Uint32 t, bool o) :
	name(nam), userData(dat), interval(t),
	once(o), m_running(false), m_lastData(NULL) {
	InitTimerSystem();
}

Timer::Timer(const std::string& nam, void (*fct)(EventData dat), void* dat, Uint32 t, bool o) :
	name(nam), userData(dat), interval(t),
	once(o), m_running(false), m_lastData(NULL) {
	onTimer.handler() = getEventHandler(fct);
	InitTimerSystem();
}

Timer::Timer(const std::string& nam, Ref<Event<EventData>::Handler> hndl, void* dat, Uint32 t, bool o) :
	name(nam), userData(dat), interval(t),
	once(o), m_running(false), m_lastData(NULL) {
	onTimer.handler() = hndl;
	InitTimerSystem();
}


////////////////
// Destructor
Timer::~Timer()
{
	// TODO: because of this, headless timers will cause memleaks if still running when quitting
	// (because they can't get any quit signal and won't be freed here). In handleEvent 
	// there should be check for tLX->bQuitGame && timer_data->timer == NULL (headless timer) and the timer should
	// be stopped if that condition is met (it is a bit dirty solution, if you think of a better one, implement it
	// and remove this)
	
	// Stop the timer if running and not headless
	stop();
}




/////////////////
// Returns true if this timer is running
bool Timer::running() { return m_running; }

////////////////
// Starts the timer
bool Timer::start() 
{
	// Stop if running
	if (m_running)
		stop();
	
	// Copy the info to timer data structure and run the timer
	TimerData* data = new TimerData;
	m_lastData = data;
	
	data->timer = this;
	data->name = name;
	data->userData = userData;
	data->interval = interval;
	data->once = once;
	data->quitSignal = false;
	
	if(data->name == "") {
		warnings << "unnamed timer is started" << endl;
		data->name = "unnamed";
	}
	
	data->startThread();

	m_running = true;
	return true;
}

//////////////////
// Starts the timer without pointing at "this" object (it means the timer object can be a temporary local variable)
bool Timer::startHeadless()
{
	// Copy the info to timer data structure and run the timer
	TimerData* data = new TimerData;
	data->timer = NULL;
	data->name = name;
	data->onTimerHandler = onTimer.handler().get();
	data->userData = userData;
	data->interval = interval;
	data->once = once;
	data->quitSignal = false;
	
	if(data->name == "") {
		warnings << "unnamed timer is started headless" << endl;
		data->name = "unnamed headless";
	}
	
	if(!data->once) {
		// It's hard to debug headless timers with once=false, thus we just disable them.
		warnings << "headless timer " << name << " has once=false, it is forced once=true now" << endl;
		data->once = true;
	}
	
	data->startThread();
	
	return true;
}

////////////////
// Stops the timer
void Timer::stop()
{
	// Already stopped
	if(!m_running) return;
	
	SDL_mutexP(m_lastData->mutex);
	m_lastData->breakThread(); // it will be removed in the last event
	m_lastData->timer = NULL;
	SDL_mutexV(m_lastData->mutex);
	
	m_lastData = NULL;
	m_running = false;
}



///////////////
// Handle the timer event, called from HandleNextEvent
static void Timer_handleEvent(InternTimerEventData data)
{
	TimerData* timer_data = data.data;
	if(timer_data == NULL) {
		errors << "Timer_handleEvent: timer_data unset" << endl;
		return;
	}
	
	// Run the client function (if no quitSignal) and quit the timer if it returns false
	// Also quit if we got last event signal
	SDL_mutexP(timer_data->mutex);
	if( !timer_data->quitSignal ) {
		Event<Timer::EventData>::HandlerList handlers = timer_data->timer ? timer_data->timer->onTimer.handler().get() : timer_data->onTimerHandler;
		bool shouldContinue = true;
		Timer::EventData eventData = Timer::EventData(timer_data->timer, timer_data->userData, shouldContinue);
		SDL_mutexV(timer_data->mutex);

		Event<Timer::EventData>::callHandlers(handlers, eventData);
		
		SDL_mutexP(timer_data->mutex);
		if( !timer_data->quitSignal && !shouldContinue ) {
			if(timer_data->timer) { // No headless timer => call stop() to handle intern state correctly
				SDL_mutexV(timer_data->mutex);
				timer_data->timer->stop();
				SDL_mutexP(timer_data->mutex);
			}
			
			// just to be sure; does not hurt
			timer_data->breakThread();
		}
	}
	SDL_mutexV(timer_data->mutex);
	
	if(data.lastEvent)  { // last-event-signal
		// we can delete here as we have ensured that this is realy the last event
		delete timer_data;
	}
}

//
// StopWatch class
//

StopWatch::StopWatch(const std::string& name)
{
	this->name = name;
	start = GetTime();
}

StopWatch::~StopWatch()
{
	hints << name << " took " << (GetTime() - start).milliseconds() << " ms" << endl;
}
