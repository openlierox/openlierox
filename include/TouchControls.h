/*
 *  TouchControls.h
 *  OpenLieroX
 *
 *  On-screen touch controls (virtual D-pad + action buttons) that turn
 *  finger taps into the same player key bindings used by the keyboard
 *  input handler. Cross-platform (any SDL2 platform with touch); enabled
 *  via Game.TouchscreenControls in options.cfg.
 *
 *  code under LGPL
 */

#ifndef __OLX_TOUCHCONTROLS_H__
#define __OLX_TOUCHCONTROLS_H__

#include <SDL.h>

struct SDL_Surface;

namespace TouchControls {

// Initialise the on-screen control layout. Safe to call multiple times.
void Init();

// Tear down: release any held synthetic key state.
void Shutdown();

// Returns true if touch controls are currently active (option enabled and
// the game is in a state where they should be shown / accept input).
bool IsActive();

// Handle SDL_FINGERDOWN / SDL_FINGERUP / SDL_FINGERMOTION events.
// Returns true if the event was consumed (i.e. landed on a button).
bool HandleFingerEvent(const SDL_TouchFingerEvent& ev);

// Render the on-screen buttons onto the given surface.
// Should be called once per frame, after the rest of the HUD.
void Render(SDL_Surface* dst);

// Called once per frame to release synthetic keys for buttons that no
// longer have any finger on them. (Most state changes happen in
// HandleFingerEvent, but a per-frame sweep keeps things consistent.)
void Update();

// Called by the input layer when a non-touch input event (keyboard,
// gamepad button or axis past deadzone, mouse click) is received during
// gameplay. Hides the touch UI and releases any held touch actions so
// the player can use the other input device without the on-screen
// controls in the way. The next SDL_FINGERDOWN re-enables the UI.
void NotifyExternalInput();

// If the active touch layout specifies a minimap position, write it
// to outX/outY (top-left in logical 640x480 coords) and return true.
// Callers (the HUD draw path) use this to reposition the minimap so it
// doesn't overlap the action buttons. Returns false when the layout
// hasn't specified one — the caller is then free to pick its own default.
bool GetMinimapPosition(int& outX, int& outY);

} // namespace TouchControls

#endif // __OLX_TOUCHCONTROLS_H__
