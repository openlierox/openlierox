/////////////////////////////////////////
//
//   OpenLieroX
//
//   event queue
//
//   based on the work of JasonB
//   enhanced by Dark Charlie and Albert Zeyer
//
//   code under LGPL
//
/////////////////////////////////////////

#ifndef __EVENTQUEUE_H__
#define __EVENTQUEUE_H__

#include <cassert>
#include "ThreadPool.h" // for Action

enum SDLUserEvent {
	UE_CustomEventHandler = 0,
	UE_QuitEventThread,
	UE_DoVideoFrame,
	UE_DoSetVideoMode,
	UE_DoActionInMainThread,
	UE_NopWakeup
};


// bDedicated must be set before we can call this
void InitEventQueue();
void ShutdownEventQueue();
void ProcessPendingQuitSignal();



union SDL_Event;
typedef SDL_Event EventItem; // for now, we can change that later

class _Event;
template< typename _Data > class Event;

class CustomEventHandler : public Action {
public:
	virtual Result handle() = 0;
	virtual const _Event* owner() const = 0;
	virtual CustomEventHandler* copy(_Event* newOwner = NULL) const = 0;
	virtual ~CustomEventHandler() {}
};

template< typename _Data >
class EventThrower : public CustomEventHandler {
public:
	Event<_Data>* m_event;
	_Data m_data;
	EventThrower(Event<_Data>* e, _Data d) : m_event(e), m_data(d) {}
	virtual Result handle() {
		m_event->occurred( m_data );
		return true;
	}
	virtual const _Event* owner() const { return m_event; }
	virtual CustomEventHandler* copy(_Event* newOwner) const {
		EventThrower* th = new EventThrower(*this);
		if(newOwner) th->m_event =  (Event<_Data>*) newOwner;
		return th;
	}
};


struct EventQueueIntern;

class EventQueue {
private:
	EventQueueIntern* data;
public:
	EventQueue();
	~EventQueue();
	
	bool hasItems();

	// Polls for currently pending events.
	bool poll(EventItem& e);
	
	// Waits indefinitely for the next available event.
	bool wait(EventItem& e);
	
	/* Add an event to the event queue.
	 * This function returns true on success
	 * or false if there was some error.
	 */
	bool push(const EventItem& e);
	bool push(Action* eh);
	
	// goes through all CustomEventHandler and copies them if oldOwner is matching
	void copyCustomEvents(const _Event* oldOwner, _Event* newOwner);
	
	// removes all CustomEventHandler with owner
	void removeCustomEvents(const _Event* owner);
};

extern EventQueue* mainQueue;



#endif  //  __EVENTQUEUE_H__
