/*
	OpenLieroX

	input (keyboard, mouse, ...) events and related stuff

	code under LGPL
	created 01-05-2007
	by Albert Zeyer and Dark Charlie
*/


#include <set>

#include "Clipboard.h"
#include "LieroX.h"

#include "EventQueue.h"
#include "InputEvents.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Menu.h"
#include "Timer.h"
#include "CInput.h"
#include "MathLib.h"
#include "NotifyUser.h"
#include "Event.h"
#include "MainLoop.h"

#include "gusanos/allegro.h"

#ifdef WIN32
#include <Windows.h>
#endif


// Keyboard, Mouse, & Event
static keyboard_t	Keyboard;
static mouse_t		Mouse;
static SDL_Event	sdl_event;
static ModifiersState evtModifiersState;

static bool         bHaveFocus = true;
bool		bActivated = false;
bool		bDeactivated = false;



///////////////////
// Returns the current state of the modifier keys (alt, ctrl etc.)
ModifiersState *GetCurrentModstate()
{
	return &evtModifiersState;
}

///////////////////
// Return the keyboard structure
keyboard_t *GetKeyboard()
{
	return &Keyboard;
}

///////////////////
// Return the mouse structure
mouse_t *GetMouse()
{
	return &Mouse;
}


///////////////////
// Return the event
SDL_Event *GetEvent()
{
	// TODO: this should not be used like it is atm because it only returns the last event
	// but in ProcessEvents() could be more than one Event get passed
	return &sdl_event;
}

bool bEventSystemInited = false;
bool bWaitingForEvent = false;

////////////////////
// Returns true if the event system is initialized
bool EventSystemInited()
{
	return bEventSystemInited;
}

// Whether the gameloop thread is currently waiting on an event.
bool IsWaitingForEvent() {
	return bWaitingForEvent;
}


///////////////////////
// Converts SDL button to a mouse button
MouseButton SDLButtonToMouseButton(int sdlbut)
{
	switch(sdlbut) {
		case SDL_BUTTON_LEFT: return mbLeft;
		case SDL_BUTTON_RIGHT: return mbRight;
		case SDL_BUTTON_MIDDLE: return mbMiddle;
		case SDL_BUTTON_X1: return mbExtra1;
		case SDL_BUTTON_X2: return mbExtra2;
		default: return mbLeft;
	}
}

// Returns just a single (the most important) button.
MouseButton SDLButtonStateToMouseButton(int sdlbut)
{
	if(sdlbut & SDL_BUTTON_LMASK) return mbLeft;
	if(sdlbut & SDL_BUTTON_RMASK) return mbRight;
	if(sdlbut & SDL_BUTTON_MMASK) return mbMiddle;
	if(sdlbut & SDL_BUTTON_X1MASK) return mbExtra1;
	if(sdlbut & SDL_BUTTON_X2MASK) return mbExtra2;
	return mbLeft; // default
}



std::map<SDL_EventType, SDLEvent> sdlEvents;

Event<> onDummyEvent;

static std::set<CInput*> cInputs;

void RegisterCInput(CInput* input) {
	cInputs.insert(input);
}

void UnregisterCInput(CInput* input) {
	cInputs.erase(input);
}

static void ResetCInputs() {
	for(std::set<CInput*>::iterator it = cInputs.begin(); it != cInputs.end(); it++) {
		if((*it)->getResetEachFrame())
			(*it)->reset();
	}
}

void HandleCInputs_KeyEvent(const KeyboardEvent& ev) {
	for(std::set<CInput*>::iterator it = cInputs.begin(); it != cInputs.end(); it++) {
		if(!(*it)->isKeyboard() || (*it)->getData() != ev.sym)
			continue;
		// If the binding requires specific modifiers, enforce an exact match.
		// If no modifiers are required, fire regardless of modifier state (backward compat).
		const ModifiersState& req = (*it)->m_modifiers;
		if(req.bAlt || req.bCtrl || req.bShift || req.bGui) {
			if(req.bAlt   != ev.state.bAlt)   continue;
			if(req.bCtrl  != ev.state.bCtrl)  continue;
			if(req.bShift != ev.state.bShift) continue;
			if(req.bGui   != ev.state.bGui)   continue;
		}
		if(ev.down) {
			(*it)->nDown++;
			if(!(*it)->bDown) {
				(*it)->nDownOnce++;
				(*it)->bDown = true;
			}
		} else {
			(*it)->bDown = false;
			(*it)->nUp++;
		}
	}
}

void HandleCInputs_UpdateDownOnceForNonKeyboard() {
	for(std::set<CInput*>::iterator it = cInputs.begin(); it != cInputs.end(); it++)
		if((*it)->isUsed() && !(*it)->isKeyboard()) {
			// HINT: It is possible that wasUp() and !Down (a case which is not covered in further code)
			if((*it)->wasUp() && !(*it)->bDown) {
				(*it)->nDownOnce++;
				continue;
			}

			// HINT: It's possible that wasDown() > 0 and !isDown().
			// That is the case when we press a key and release it directly after (in one frame).
			// Though wasDown() > 0 doesn't mean directly isDownOnce because it also counts keypresses.
			// HINT: It's also possible that wasDown() == 0 and isDown().
			// That is the case when we have pressed the key in a previous frame and we still hold it
			// and the keyrepeat-interval is bigger than FPS. (Rare case.)
			if((*it)->wasDown() || (*it)->isDown()) {
				// wasUp() > 0 always means that it was down once (though it is not down anymore).
				// Though the released key in wasUp() > 0 was probably already recognised before.
				if((*it)->wasUp()) {
					(*it)->bDown = (*it)->isDown();
					if((*it)->bDown) // if it is again down, there is another new press
						(*it)->nDownOnce++;
					continue;
				}
				// !Down means that we haven't recognised yet that it is down.
				if(!(*it)->bDown) {
					(*it)->bDown = (*it)->isDown();
					(*it)->nDownOnce++;
					continue;
				}
			}
			else
				(*it)->bDown = false;
		}
}

void HandleCInputs_UpdateUpForNonKeyboard() {
	for(std::set<CInput*>::iterator it = cInputs.begin(); it != cInputs.end(); it++)
		if((*it)->isUsed() && !(*it)->isKeyboard()) {
			if((*it)->isDown() && !(*it)->bDown) {
				(*it)->nUp++;
			}
		}
}

static void ResetCurrentEventStorage() {
	ResetCInputs();

    // Clear the queue
    Keyboard.queueLength = 0;

	// Reset mouse wheel
	Mouse.WheelScrollUp = false;
	Mouse.WheelScrollDown = false;

	// Reset the video mode changed flag here
	if (tLX)
		tLX->bVideoModeChanged = false;

//	for(int k=0;k<SDL_NUM_SCANCODES;k++) {
//		Keyboard.KeyUp[k] = false;
//	}

	bActivated = false;
	bDeactivated = false;
}


bool WasKeyboardEventHappening(SDL_Keycode sym, bool down) {
	for(int i = 0; i < Keyboard.queueLength; i++)
		if(Keyboard.keyQueue[i].sym == sym && Keyboard.keyQueue[i].down == down)
			return true;
	return false;
}


typedef void (*EventHandlerFct) (SDL_Event* ev);


static void EvHndl_WindowEvent(SDL_Event* ev) {
	switch(ev->window.event) {
		case SDL_WINDOWEVENT_EXPOSED:
			// We are redrawing anyway. (I hope.)
			break;
	
		case SDL_WINDOWEVENT_FOCUS_GAINED:
		case SDL_WINDOWEVENT_FOCUS_LOST: {
			bool hadFocusBefore = bHaveFocus;
			bHaveFocus = ev->window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
			bActivated = bHaveFocus;
			bDeactivated = !bHaveFocus;
			
			// HINT: Reset the mouse state - this should avoid the mouse staying pressed
			Mouse.Button = 0;
			Mouse.Down = 0;
			Mouse.FirstDown = 0;
			Mouse.Up = 0;
			
			if(!hadFocusBefore && bHaveFocus) {
				//notes << "OpenLieroX got the focus" << endl;
				ClearUserNotify();
			} else if(hadFocusBefore && !bHaveFocus) {
				//notes << "OpenLieroX lost the focus" << endl;
			}
			
			if(tLXOptions->bAutoFileCacheRefresh && bActivated)
				updateFileListCaches();
				
			break;
		}
	}
}

static void pushKeyboardEv(const KeyboardEvent& kbev) {
	// If we're going to over the queue length, shift the list down and remove the oldest key
	if(Keyboard.queueLength+1 >= MAX_KEYQUEUE) {
		for(int i=0; i<Keyboard.queueLength-1; i++)
			Keyboard.keyQueue[i] = Keyboard.keyQueue[i+1];
		Keyboard.queueLength--;
		warnings << "Keyboard queue full" << endl;
	}
	
	Keyboard.keyQueue[Keyboard.queueLength] = kbev;
	Keyboard.queueLength++;

	HandleCInputs_KeyEvent(kbev);
}

static void EvHndl_KeyDownUp(SDL_Event* ev) {
	// Check the characters
	if(ev->key.state == SDL_PRESSED || ev->key.state == SDL_RELEASED) {
		UnicodeChar input = 0;
		switch (ev->key.keysym.sym) {
			case SDLK_RETURN:
			case SDLK_RETURN2:
			case SDLK_KP_ENTER:
				input = '\r';
				break;
			case SDLK_TAB:
				input = '\t';
				break;
			default:
				// nothing
				break;
		}  // switch
		 
		KeyboardEvent kbev;

		// Key down
		if(ev->type == SDL_KEYDOWN)
			kbev.down = true;

		// Key up
		else if(ev->type == SDL_KEYUP || ev->key.state == SDL_RELEASED)
			kbev.down = false;

		else
			return; // don't save and handle it

		// save info
		kbev.ch = input;
		kbev.sym = ev->key.keysym.sym;

		// handle modifier state
		switch (kbev.sym)  {
		case SDLK_LALT: case SDLK_RALT:
			evtModifiersState.bAlt = kbev.down;
			break;
		case SDLK_LCTRL: case SDLK_RCTRL:
			evtModifiersState.bCtrl = kbev.down;
			break;
		case SDLK_LSHIFT: case SDLK_RSHIFT:
			evtModifiersState.bShift = kbev.down;
			break;
		case SDLK_LGUI: case SDLK_RGUI:
			evtModifiersState.bGui = kbev.down;
			break;
		}

		// copy it
		kbev.state = evtModifiersState;

		pushKeyboardEv(kbev);
		
	} else
		warnings << "Strange Event.key.state = " << ev->key.state << endl;

}

static void EvHndl_TextInput(SDL_Event* _ev) {
	SDL_TextInputEvent& ev = _ev->text;
	
	auto p = ev.text; // 0-terminated utf8
	auto end = p + sizeof(ev.text);
	while(p != end) {
		UnicodeChar ch = GetNextUnicodeFromUtf8(p, end);
		if(ch == 0) break;

		// This is somewhat hacky. SDL1 only supported key events and all
		// text input was via those. We keep the same interface and emulate
		// our text input as key events, key-down + key-up, char by char.
		KeyboardEvent kbev;
		kbev.down = true;
		kbev.ch = ch;
		pushKeyboardEv(kbev);
		kbev.down = false;
		pushKeyboardEv(kbev);
	}
}

static int mouseX, mouseY;

static void EvHndl_MouseMotion(SDL_Event* ev) {
/*	mouseX = CLAMP(mouseX, 0, VideoPostProcessor::get()->screenWidth());
	mouseY = CLAMP(mouseY, 0, VideoPostProcessor::get()->screenHeight());*/
	mouseX = ev->motion.x;
	mouseY = ev->motion.y;
}

static void EvHndl_MouseButtonDown(SDL_Event* ev) {}
static void EvHndl_MouseButtonUp(SDL_Event* ev) {}

static void EvHndl_MouseWheel(SDL_Event* ev) {
	if(ev->wheel.y > 0)
		Mouse.WheelScrollUp = true;
	else if(ev->wheel.y < 0)
		Mouse.WheelScrollDown = true;
}

static void EvHndl_Quit(SDL_Event*) {
	game.state = Game::S_Quit;
}

static void EvHndl_UserEvent(SDL_Event* ev) {
	if(ev->user.code == UE_CustomEventHandler) {
		((Action*)ev->user.data1)->handle();
		delete (Action*)ev->user.data1;
	}
}

void InitEventSystem() {	
	Mouse.Button = 0;
	Mouse.Down = 0;
	Mouse.FirstDown = 0;
	Mouse.Up = 0;

	sdlEvents[SDL_WINDOWEVENT].handler() = getEventHandler(&EvHndl_WindowEvent);
	sdlEvents[SDL_KEYDOWN].handler() = getEventHandler(&EvHndl_KeyDownUp);
	sdlEvents[SDL_KEYUP].handler() = getEventHandler(&EvHndl_KeyDownUp);
	sdlEvents[SDL_TEXTINPUT].handler() = getEventHandler(&EvHndl_TextInput);
	sdlEvents[SDL_MOUSEMOTION].handler() = getEventHandler(&EvHndl_MouseMotion);
	sdlEvents[SDL_MOUSEBUTTONDOWN].handler() = getEventHandler(&EvHndl_MouseButtonDown);
	sdlEvents[SDL_MOUSEBUTTONUP].handler() = getEventHandler(&EvHndl_MouseButtonUp);
	sdlEvents[SDL_MOUSEWHEEL].handler() = getEventHandler(&EvHndl_MouseWheel);
	sdlEvents[SDL_QUIT].handler() = getEventHandler(&EvHndl_Quit);
	sdlEvents[SDL_USEREVENT].handler() = getEventHandler(&EvHndl_UserEvent);
	// Note: SDL_SYSWMEVENT is handled directly on the main thread by handleSDLEvents().
	
	bEventSystemInited = true;
	bWaitingForEvent = false;
}

void ShutdownEventSystem()
{
	bEventSystemInited = false;
	bWaitingForEvent = false;
}


// main function for handling next event
// HINT: we are using the global variable Event here atm
// in both cases where we call this function this is correct
// TODO: though the whole architecture has to be changed later
// but then also GetEvent() has to be changed or removed
void HandleNextEvent() {
	auto it = sdlEvents.find((SDL_EventType)sdl_event.type);
	if(it != sdlEvents.end())
		it->second.occurred(&sdl_event);
}

static void HandleMouseState() {
	{
		// Mouse
		int oldX = Mouse.X;
		int oldY = Mouse.Y;

		// We don't use the coordinates from SDL_GetMouseState because
		// it ignores the render logical size via SDL_RenderSetLogicalSize().
		// The mouse coordinates are only translated in the mouse events.
		// That is why we track the coordinates in there.
		// However, that still has the problem that the mouse coordinates
		// can be in the letterboxing (black) area, i.e. outside
		// our logical screen.
		// SDL_SetRelativeMouseMode is also not really an option
		// because it is somewhat buggy (seems like the mouse is captured)
		// and it grabs the mouse in window mode which we don't want.
		// In fullscreen mode, we could warp the mouse back to the center
		// on each frame and then use this relative mouse mode.
		// See: https://forums.libsdl.org/viewtopic.php?t=10690
		// However, later, we could just remove the letterboxing area
		// by having a bigger virtual screen, so this problem goes away.
		
		Mouse.Button = SDL_GetMouseState(NULL,NULL); // Doesn't call libX11 funcs, so it's safe to call not from video thread
		Mouse.X = mouseX;
		Mouse.Y = mouseY;
		
		Mouse.deltaX = Mouse.X-oldX;
		Mouse.deltaY = Mouse.Y-oldY;
		Mouse.Up = 0;
		Mouse.FirstDown = 0;
	}

#ifdef FUZZY_ERROR_TESTING
	/*Mouse.Button = GetRandomInt(8);
	Mouse.deltaX = SIGN(GetRandomNum()) * GetRandomInt(655535);
	Mouse.deltaY = SIGN(GetRandomNum()) * GetRandomInt(655535);
	Mouse.Up = SIGN(GetRandomNum()) * GetRandomInt(655535);
	Mouse.FirstDown = SIGN(GetRandomNum()) * GetRandomInt(655535);
	return;*/
#endif

    for( int i=1; i<=MAX_MOUSEBUTTONS; i++ ) {
		if(!(Mouse.Button & SDL_BUTTON(i)) && Mouse.Down & SDL_BUTTON(i))
			Mouse.Up |= SDL_BUTTON(i);
        if( !(Mouse.Down & SDL_BUTTON(i)) && (Mouse.Button & SDL_BUTTON(i)) )
            Mouse.FirstDown |= SDL_BUTTON(i);
    }

	Mouse.Down = Mouse.Button;
}

// Declared in CInput.cpp
extern void updateAxisStates();

bool processedEvent = false;

///////////////////
// Process the events
void ProcessEvents()
{
	if(!isMainThread() && !isGameloopThread()) {
		errors << "ProcessEvents called from thread " << getCurThreadName() << endl;
		// Just ignore.
		// We really should not poll any events here, because the mainloop/gameloop could otherwise be confused.
		return;
	}

	ResetCurrentEventStorage();

	bool ret = false;
	if(game.allowedToSleepForEvent() && !mainQueue->hasItems()) {
		bWaitingForEvent = true;
		if(isMainThread())
			handleSDLEvents(true);
		// Note: We can only wait on the `mainQueue` if this is not the main thread.
		// Otherwise, SDL events could come but no-one would forward them to `mainQueue`.
		// If we are the main thread, we will at least poll the `mainQueue` below.
		else if(mainQueue->wait(sdl_event)) {
			bWaitingForEvent = false;
			HandleNextEvent();
			ret = true;
		}
		bWaitingForEvent = false;
	}

	if(isMainThread())
		handleSDLEvents(false);

	while(mainQueue->poll(sdl_event)) {
		HandleNextEvent();
		ret = true;
	}

#ifdef FUZZY_ERROR_TESTING
	/*Event.type = GetRandomInt(255);
	HandleNextEvent();*/

	/*Event.type = SDL_KEYDOWN;
	Event.key.keysym.sym = (SDLKey)GetRandomInt(65535);
	Event.key.keysym.mod = (SDLMod)GetRandomInt(65535);
	Event.key.keysym.scancode = GetRandomInt(65535);
	Event.key.keysym.unicode = GetRandomInt(65535);
	Event.key.which = GetRandomInt(65535);
	Event.key.state = GetRandomInt(50) > 25 ? SDL_PRESSED : SDL_RELEASED;
	HandleNextEvent();*/
#endif

	if (bDedicated)
	{
		#ifdef WIN32
		MSG msg;
		while( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
		{
			if( msg.message == WM_QUIT || msg.message == WM_CLOSE )
			{
				EventItem ev;
				ev.type = SDL_QUIT;
				mainQueue->push(ev);
			}
		}
		#endif
	}

	if (!bDedicated) {
		// If we don't have focus, don't update as often
		if(!bHaveFocus)
			SDL_Delay(14);

		HandleMouseState();
#ifndef DEDICATED_ONLY
#ifndef DISABLE_JOYSTICK
		if(bJoystickSupport)  {
			SDL_JoystickUpdate();
			updateAxisStates();
		}
#endif
#endif
		HandleCInputs_UpdateUpForNonKeyboard();
		HandleCInputs_UpdateDownOnceForNonKeyboard();
	}

	processedEvent = ret;
}

void WakeupIfNeeded() {
	SDL_Event ev;
	ev.type = SDL_USEREVENT;
	ev.user.code = UE_NopWakeup;
	// The main thread (SDL events) will just ignore it.
	// The game thread will also ignore it, but do another redraw.
	mainQueue->push(ev);
}

bool ApplicationHasFocus()
{
	return bHaveFocus;
}
