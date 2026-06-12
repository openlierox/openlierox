/*
 *  TouchScreenInput.h
 *  OpenLieroX
 *
 *  Touch-screen action events, decoupled from key bindings.
 *
 *  The TouchControls visual layer produces high-level action events
 *  (Fire / Jump / PreviousWeapon / ...) tagged with a player number and
 *  a state change (Down / Up). These events are queued and drained
 *  once per frame; the game logic reads the resulting per-action state
 *  directly, with no dependency on what the player's keyboard or
 *  gamepad is currently bound to.
 *
 *  Producer side  (TouchControls.cpp):
 *    TouchScreenInput::pushEvent(1, Action::Fire, StateChange::Down);
 *
 *  Consumer side  (CWormHuman.cpp, CClient_Game.cpp):
 *    TouchScreenInput::processFrame();          // once per gameloop frame
 *    if(cShoot.isDown() || TouchScreenInput::isActionDown(1, Action::Fire)) ...
 *    if(TouchScreenInput::wasActionTapped(1, Action::OpenMenu)) ...
 *
 *  code under LGPL
 */

#ifndef __TOUCHSCREEN_INPUT_H__
#define __TOUCHSCREEN_INPUT_H__

namespace TouchScreenInput {

// Named, binding-independent game actions that the touch screen can
// drive. Keep the order/values stable — they index into internal arrays.
enum class Action {
	// Held while a finger is on a direction (vjoy) or directional button.
	Left,
	Right,
	Up,
	Down,

	// Held while a finger is on the corresponding combat button.
	Fire,
	Jump,
	Rope,
	SelectWeapon,
	Strafe,

	// Edge-triggered taps.
	PreviousWeapon,
	NextWeapon,
	RandomizeWeapons,
	ConfirmWeapons,
	OpenMenu,

	__Count
};

enum class StateChange {
	Down,
	Up,
};

struct Event {
	int          playerNumber;   // 1-based; touch currently only drives player 1
	Action       action;
	StateChange  stateChange;
};

// Producer: queue one transition. Called from the touchscreen layer
// (TouchControls) whenever a button is pressed/released or a vjoy
// direction crosses its threshold.
void pushEvent(int playerNumber, Action action, StateChange stateChange);

// Consumer: drain the queue and apply pending state changes. Call once
// per gameloop frame, before any isActionDown / wasActionTapped read.
void processFrame();

// Continuous state: returns true while a finger is currently holding the
// action down. Reading does not consume the state.
bool isActionDown(int playerNumber, Action action);

// Edge-triggered state: the number of leading-edge (Down) taps that have
// accumulated for this action since it was last read. processFrame()
// adds to this count and never clears it, so a tap survives a slow
// gameloop tick and a frame on which the consumer didn't run. The read
// consumes the count (resets it to 0), so each tap is applied exactly
// once -- the same contract as CInput::isDownOnce() / wasDown().
int  tapCount(int playerNumber, Action action);

// Convenience wrapper around tapCount(): true if at least one tap was
// pending. Also consumes the pending taps.
bool wasActionTapped(int playerNumber, Action action);

// Reset every action to "not pressed" and clear pending taps. Used on
// layout transitions where holding fingers must not leak across modes.
void releaseAll();

} // namespace TouchScreenInput

#endif // __TOUCHSCREEN_INPUT_H__
