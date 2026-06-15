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


// Input class
// Created 10/12/01
// By Jason Boettcher


#ifndef __CINPUT_H__
#define __CINPUT_H__

#include <string>
#include <vector>
#include "InputEvents.h"


// Input variable types
#define		INP_NOTUSED			-1
#define		INP_KEYBOARD		0
#define		INP_MOUSE			1
#define		INP_JOYSTICK		2


// Joystick data
#define		JOY_UP				0
#define		JOY_DOWN			1
#define		JOY_LEFT			2
#define		JOY_RIGHT			3
#define		JOY_BUTTON			4
#define		JOY_TURN_LEFT		5
#define		JOY_TURN_RIGHT		6
#define		JOY_THROTTLE_LEFT	7
#define		JOY_THROTTLE_RIGHT	8
#define		JOY_HAT				9
#define		JOY_TRIGGER_LEFT	10
#define		JOY_TRIGGER_RIGHT	11

struct KeyboardEvent;

class CInput {
public:
	CInput();
	~CInput();

private:
	// A single physical binding (one key, mouse button or joystick control).
	// A CInput can hold several of these so that one game action may be bound
	// to multiple inputs (e.g. a keyboard key and a gamepad button); the
	// action is considered active when any of the bindings is active.
	struct Binding {
		int		Type; // keyboard, mouse or joystick
		int		Data;
		int		SdlIndex;
		int		JoystickIndex; // 0-based pad index for INP_JOYSTICK
		ModifiersState m_modifiers; // required modifier keys for keyboard bindings

		// HINT: currently these are only used for keyboard exept nDownOnce
		int		nDown;
		int		nDownOnce; // this is also updated for non-keyboards with currently the old code
		int		nUp;
		bool	bDown;

		Binding() : Type(INP_NOTUSED), Data(0), SdlIndex(0), JoystickIndex(0),
					nDown(0), nDownOnce(0), nUp(0), bDown(false) {}

		bool	setup(const std::string& text); // parse a single binding token

		bool	isUsed() const { return Type >= 0; }
		bool	isJoystick() const { return Type == INP_JOYSTICK; }
		bool	isKeyboard() const { return Type == INP_KEYBOARD; }

		bool	isUp() const;
		bool	isDown() const;
		int		wasDown() const;
		int		wasDown_withoutRepeats() const { return nDownOnce; }
		int		wasUp() const;
		int		getJoystickValue() const;
		bool	isJoystickThrottle() const;
		void	reset() { nDown = nDownOnce = nUp = 0; }
	};

	std::vector<Binding>	m_bindings;
	std::string				m_EventName; // the original (possibly comma-separated) config string
	bool					resetEachFrame;

public:
	// Methods

	int		Setup(const std::string& text);
	static void InitJoysticksTemp(); // call this if CInput::Wait shall recognise joystick events
	static void UnInitJoysticksTemp();
	// SDL_CONTROLLERDEVICEADDED / SDL_CONTROLLERDEVICEREMOVED hotplug
	// handlers. Without these, controllers plugged in after startup
	// (or surfaced late by the platform — e.g. browser Gamepad API
	// only exposing pads after the user's first button press) are
	// invisible to the engine.
	static void OnControllerAdded(int deviceIndex);
	static void OnControllerRemoved(int instanceId);
	static int Wait(std::string& strText); // TODO: change this name. this function doesn't realy wait, it just checks the event-state
	bool	isUsed() const;
	bool	isJoystick() const;
	bool	isJoystickDown() const;     // true if any joystick binding is currently down
	bool	isJoystickDownOnce() const; // true if any joystick binding was just pressed
	bool	isKeyboard() const;
	bool	usesKeyboardKey(int sym) const; // true if any keyboard binding is bound to this key sym
	void	setResetEachFrame(bool r)	{ resetEachFrame = r; }
	bool	getResetEachFrame()			{ return resetEachFrame; }
	int		getJoystickValue();
	bool	isJoystickThrottle();

	bool	isUp();
	bool	isDown() const;
	bool	isDownOnce();
	int		wasDown(bool withRepeats) const;
	int		wasDown() const; // checks if there was such an event in the queue; returns the count of presses (down-events)
	int		wasUp(); // checks if there was an keyup-event; returns the count of up-events

	std::string getEventName() { return m_EventName; }

	// event handling, called from the global input-event dispatchers
	void	handleKeyEvent(const KeyboardEvent& ev);
	void	updateDownOnceForNonKeyboard();
	void	updateUpForNonKeyboard();

	// resets the current state
	// this is called automatically if resetEachFrame
	void	reset();
};



// Keyboard structure
struct keys_t {
	char	text[16];	// Keep string local to speed up lookup.
	int		value;
	
	static int keySymFromName(const std::string & name);
};


// Joystick structure
struct joystick_t {
	char	text[16];	// Keep string local to speed up lookup.
	int		value;
	int		sdl_index;
};




#endif  //  __CINPUT_H__

