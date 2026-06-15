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

#include <string>
#include <vector>

#include "SmartPointer.h"

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

// Called by the input layer when a genuine (non-synthetic) touch event
// arrives. The "auto" value of Game.TouchscreenControls turns the touch
// controls on once this has happened — i.e. a touchscreen is actually in
// use — with no per-platform assumptions.
void NotifyRealTouch();

// If the active touch layout specifies a minimap position, write it
// to outX/outY (top-left in logical 640x480 coords) and return true.
// Callers (the HUD draw path) use this to reposition the minimap so it
// doesn't overlap the action buttons. Returns false when the layout
// hasn't specified one — the caller is then free to pick its own default.
bool GetMinimapPosition(int& outX, int& outY);

// Re-read the YAML layout file named by Game.TouchscreenLayout. Drops
// any held button / vjoy state so a leftover finger on the old layout
// doesn't synthesise events against the new one. Called when the user
// changes the layout selection in the options menu.
void ReloadLayout();

// True if SDL reports at least one touch input device. Used by the
// options menu to gate the "Touchscreen" sub-tab.
bool IsTouchDeviceAvailable();

// Metadata about one share/gamedir/touchscreen/<name>.yaml — enough
// for the options menu to render its preview tile.
struct LayoutInfo {
	std::string               fileName;     // basename without ".yaml" — what gets
	                                        // saved into Game.TouchscreenLayout.
	std::string               displayName;  // YAML `name:` field; falls back to
	                                        // fileName when absent.
	SmartPointer<SDL_Surface> preview;      // 640x480 thumbnail rendered live
	                                        // from the YAML layout data — no
	                                        // on-disk PNG. Null only when the
	                                        // surface allocation failed; the menu
	                                        // draws a "No preview" tile then.
};

// Every share/gamedir/touchscreen/*.yaml, sorted alphabetically by file
// name. Used by the options menu to build the layout-selection tiles.
std::vector<LayoutInfo> GetAvailableLayouts();

} // namespace TouchControls

#endif // __OLX_TOUCHCONTROLS_H__
