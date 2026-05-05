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
#include "InputEvents.h"


// Input variable types
#define		INP_NOTUSED			-1
#define		INP_KEYBOARD		0
#define		INP_MOUSE			1
#define		INP_JOYSTICK1		2
#define		INP_JOYSTICK2		3


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
	friend void HandleNextEvent();
	friend void HandleCInputs_UpdateDownOnceForNonKeyboard();
	friend void HandleCInputs_UpdateUpForNonKeyboard();
	friend void HandleCInputs_KeyEvent(const KeyboardEvent& ev);

public:
	CInput();
	~CInput();

private:
	// Attributes

	int		Type; // keyboard, mouse or joystick
	int		Data;
	int		SdlIndex;
	std::string m_EventName;
	ModifiersState m_modifiers; // required modifier keys for keyboard bindings
	bool	resetEachFrame;

	// HINT: currently these are only used for keyboard exept nDownOnce
	// TODO: change this in HandleNextEvent() and here
	int		nDown;
	int		nDownOnce; // this is also updated for non-keyboards with currently the old code
	int		nUp;
	bool	bDown;

private:
	int		wasDown_withoutRepeats() const;

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
	bool	isUsed() { return Type >= 0; }
	int		getData() { return Data; }
	int		getType() { return Type; }
	bool	isJoystick() { return Type == INP_JOYSTICK1 || Type == INP_JOYSTICK2; }
	bool	isKeyboard() { return Type == INP_KEYBOARD; }
	void	setResetEachFrame(bool r)	{ resetEachFrame = r; }
	bool	getResetEachFrame()			{ return resetEachFrame; }
	int		getJoystickValue();
	bool	isJoystickThrottle();

	bool	isUp();
	bool	isDown() const;
	bool	isDownOnce();
	int wasDown(bool withRepeats) const {
		if(withRepeats) return wasDown();
		else return wasDown_withoutRepeats();
	}
	int		wasDown() const; // checks if there was such an event in the queue; returns the count of presses (down-events)
	int		wasUp(); // checks if there was an keyup-event; returns the count of up-events

	std::string getEventName() { return m_EventName; }

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

