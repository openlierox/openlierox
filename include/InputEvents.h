/*
	OpenLieroX

	input (keyboard, mouse, ...) events and related stuff
	
	code under LGPL
	created 01-05-2007
	by Albert Zeyer and Dark Charlie
*/

#ifndef __INPUTEVENTS_H__
#define __INPUTEVENTS_H__

#include <vector>
#include <SDL.h>
#include "Unicode.h"
#include "Event.h"
#include "EventQueue.h"


// application has focus
// These vars are like events, they got true once in a frame when the specific event occured.
// TODO: remove them
extern	bool			bActivated;
extern	bool			bDeactivated;


#define     MAX_KEYQUEUE        256
#define     MAX_MOUSEBUTTONS    8 // SDL_GetMouseState returns UInt8 bitmask

// State of modifier keys (ctrl, alt and shift)
struct ModifiersState  { 
	ModifiersState() { clear(); }
	void clear()  { bShift = bCtrl = bAlt = bGui = false; }

	bool bShift;
	bool bCtrl;
	bool bAlt;
	bool bGui;
};

struct KeyboardEvent {
	SDL_Keycode sym;
    UnicodeChar ch;
	bool down;
	ModifiersState state;
	KeyboardEvent() : sym(0), ch(0), down(false) {}
};

// Keyboard structure
struct keyboard_t {
    int     queueLength;
    KeyboardEvent keyQueue[MAX_KEYQUEUE];
};


// Mouse structure
struct mouse_t {
	int		X,Y;
	int		deltaX,deltaY;
	int		Button;

	// HINT: the following three variables cannot be bool
	//       because they contain info about the pressed button (left, right, ...)
	//		 as well
	// TODO: change this immediatly
	int		Up;
	int		Down;
    int     FirstDown;

	bool	WheelScrollUp;
	bool	WheelScrollDown;
		
};

enum MouseButton  {
	mbLeft,
	mbRight,
	mbMiddle,
	mbExtra1,
	mbExtra2
};

MouseButton SDLButtonToMouseButton(int sdlbut);
MouseButton SDLButtonStateToMouseButton(int sdlbut);

typedef Event<SDL_Event*> SDLEvent;

// All main SDL events. You can add your own global handlers here. Though that should
// not be needed in most cases.
// If you want to add a user event, DON'T add the handler here. Use SendSDLUserEvent()
// and your event will get called automatically. 
extern std::map<SDL_EventType, SDLEvent> sdlEvents;

extern bool processedEvent;

void 		InitEventSystem();
void		ShutdownEventSystem();
void		ProcessEvents(); // returns false if no new event

bool		EventSystemInited();
bool		IsWaitingForEvent();
void		WakeupIfNeeded();


keyboard_t	*GetKeyboard();
mouse_t		*GetMouse();
SDL_Event	*GetEvent();
ModifiersState *GetCurrentModstate();

// True while the player is using a mouse rather than touch (web build only;
// always false elsewhere). Used to show the software cursor in-game.
bool		InputUsingMouse();

bool		WasKeyboardEventHappening(SDL_Keycode key, bool down = true);

// Inject a synthetic keyboard event into the same pipeline real key events go
// through. Used by the on-screen touch controls so that a touch button press
// ends up in the player CInput state exactly like a real key press would.
// Must be called from the main thread / gameloop thread.
void		InjectSyntheticKey(SDL_Keycode sym, bool down);


class CInput;
void		RegisterCInput(CInput* input);
void		UnregisterCInput(CInput* input);

bool		ApplicationHasFocus();


extern Event<> onDummyEvent;


#endif
