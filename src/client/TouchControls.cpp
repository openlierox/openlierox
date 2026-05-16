/*
 *  TouchControls.cpp
 *  OpenLieroX
 *
 *  See TouchControls.h for the design overview.
 *
 *  Two layouts:
 *
 *    LM_PLAYING (default in-game)
 *      - Most of the screen (x < kVJoyAreaRight logical, flush against
 *        the right-edge action-button column) is an INVISIBLE virtual joystick.
 *        First finger landing there sets an anchor; dragging from the
 *        anchor presses Left/Right (movement) and Up/Down (aim).
 *        Both axes use the same generous threshold (≈ 2x worm width):
 *        |dx| > walk_sensitivity for Left/Right, |dy| > aim_sensitivity
 *        for Up/Down. That way the player can hold the stick slightly
 *        off-center without unintended walking or aim-creep. The
 *        motion-vector arrow turns red once the horizontal component
 *        crosses walk_sensitivity, otherwise it's yellow. The anchor
 *        is FIXED at the touch-down point for the entire lifetime of
 *        the touch — release and re-engage if you want a new anchor.
 *      - Right half hosts: Jump, Rope, Fire, plus Prev-Weapon and
 *        Next-Weapon (one-shot taps that synthesise SelectWeapon +
 *        Left/Right).
 *
 *    LM_WEAPON_SELECT (during the per-round weapon picker — i.e. while
 *      the first local human worm has bWeaponsReady == false; in
 *      split-screen we ignore player 2's readiness since touch only
 *      drives player 1)
 *      - Joystick is OFF (the menu wants discrete taps, not a continuous
 *        axis).
 *      - Visible Up/Down/Left/Right arrow buttons drive the menu, and a
 *        Fire button confirms ('Done').
 *
 *  Touch presses are turned into synthetic SDL key events for the
 *  player-1 bindings in options.cfg, so the existing CInput pipeline
 *  picks them up exactly like real keypresses.
 *
 *  code under LGPL
 */

#include "TouchControls.h"

#include <SDL.h>
#include <cmath>
#include <map>

#include "AuxLib.h"
#include "CClient.h"
#include "CInput.h"
#include "Color.h"
#include "CWormHuman.h"
#include "GfxPrimitives.h"
#include "InputEvents.h"
#include "LieroX.h"
#include "Options.h"
#include "TouchScreenInput.h"
#include "game/CWorm.h"
#include "game/Game.h"
#include "Iter.h"


namespace TouchControls {

namespace {

// --- Layout (logical 640x480) -------------------------------------------

constexpr int kVJoyAreaRight = 525;   // right edge of invisible joystick zone — flush against the action-button column

// Drag distance from the anchor (logical pixels) needed to fire each
// direction. About 2x the 20-px worm sprite width — the worm character
// length the user perceives — so small wobbles in either axis don't
// nudge the worm or the aim.
constexpr int walk_sensitivity = 40;  // |dx| past this → Left/Right (move)
constexpr int aim_sensitivity  = 40;  // |dy| past this → Up/Down (aim)

enum LayoutMode {
	LM_PLAYING,
	LM_WEAPON_SELECT,
};

enum ButtonAction {
	BA_HOLD,           // sinIndex bound key is held while finger is on the button
	BA_TAP_PREV_WEAP,  // one-shot: synthesise SelWeap+Left tap
	BA_TAP_NEXT_WEAP,  // one-shot: synthesise SelWeap+Right tap
	BA_TAP_ESCAPE,     // one-shot: synthesise an Escape keypress (opens game menu)
	BA_TAP_RANDOMIZE,  // one-shot: randomise weapons for every local worm in the picker
	BA_TAP_DONE,       // one-shot: confirm the weapon picker for every local worm
};

struct Button {
	int x, y, w, h;
	int sinIndex;          // for BA_HOLD: SIN_* index. Ignored for tap actions.
	ButtonAction action;
	const char* label;
};

// In-game: invisible joystick + visible action buttons + menu in the corner.
// Right-edge column groups the most-used actions for the right thumb:
// ROPE at top, FIRE in the middle, JUMP at the bottom. Prev/Next weapon
// share a row in the gap between ROPE and FIRE (PREV left half, NEXT right).
//   MENU(560,5)
//                ROPE(525, 50, 110x110)
//   PREV(525,165, 55x50) | NEXT(580,165, 55x50)
//                FIRE(525,220, 110x180)
//                JUMP(525,405, 110x70)
static const Button gPlayingButtons[] = {
	{ 560,   5,  75,  40, 0,         BA_TAP_ESCAPE,    "MENU" },
	{ 525, 165,  55,  50, 0,         BA_TAP_PREV_WEAP, "<<"   },
	{ 580, 165,  55,  50, 0,         BA_TAP_NEXT_WEAP, ">>"   },
	{ 525,  50, 110, 110, SIN_ROPE,  BA_HOLD,          "ROPE" },
	{ 525, 220, 110, 180, SIN_SHOOT, BA_HOLD,          "FIRE" },
	{ 525, 405, 110,  70, SIN_JUMP,  BA_HOLD,          "JUMP" },
};
constexpr int kPlayingButtonCount = sizeof(gPlayingButtons) / sizeof(Button);

// Weapon-select: visible D-pad on the lower-left; on the right edge,
// RANDOM stacked above DONE.
static const Button gWeaponSelectButtons[] = {
	{ 560,   5, 75, 40, 0,         BA_TAP_ESCAPE,    "MENU"   },
	{  60, 240, 80, 70, SIN_UP,    BA_HOLD,          "^"      },
	{  60, 400, 80, 70, SIN_DOWN,  BA_HOLD,          "v"      },
	{   0, 320, 80, 70, SIN_LEFT,  BA_HOLD,          "<"      },
	{ 140, 320, 80, 70, SIN_RIGHT, BA_HOLD,          ">"      },
	{ 525, 240,110, 95, 0,         BA_TAP_RANDOMIZE, "Random" },
	{ 525, 345,110, 95, 0,         BA_TAP_DONE,      "DONE"   },
};
constexpr int kWeaponSelectButtonCount = sizeof(gWeaponSelectButtons) / sizeof(Button);

// Maximum buttons across layouts — sizes the per-button state arrays.
constexpr int kMaxButtons =
	(kPlayingButtonCount > kWeaponSelectButtonCount) ? kPlayingButtonCount : kWeaponSelectButtonCount;

// --- State -------------------------------------------------------------

// Touch is currently always wired to local player 1.
constexpr int kTouchPlayer = 1;

static int  gFingerCount[kMaxButtons] = {0};

// Per-finger: which button index the finger is on (-1 = empty).
// VJoy uses gVJoy and is not in this map.
static std::map<SDL_FingerID, int> gFingerToButton;

struct VJoy {
	bool active;
	SDL_FingerID finger;
	int anchorX, anchorY;
	int curX, curY;
	bool heldL, heldR, heldU, heldD;
};
static VJoy gVJoy = {false, 0, 0, 0, 0, 0, false, false, false, false};

static LayoutMode gMode      = LM_PLAYING;
static bool       gInited    = false;

// Touch UI auto-hide: any non-touch input event (keyboard / gamepad /
// real mouse) flips this off and releases any held touch actions; the
// next finger-down on the screen flips it back on. Lets a gamepad
// player avoid having buttons clutter the screen without disabling
// touch in options.
static bool       gUIActive  = true;


// --- Layout selection --------------------------------------------------

static const Button* currentButtons(int* outCount) {
	if(gMode == LM_WEAPON_SELECT) {
		*outCount = kWeaponSelectButtonCount;
		return gWeaponSelectButtons;
	}
	*outCount = kPlayingButtonCount;
	return gPlayingButtons;
}

static LayoutMode detectLayoutMode() {
	// Touch controls only drive the first local human worm (the "touch
	// player"). The layout follows that worm's picker state: as soon as
	// they confirm their loadout, switch back to the gameplay layout
	// even if a split-screen second player is still picking on the
	// keyboard. Other local worms' readiness is irrelevant to the touch
	// UI.
	if(game.state < Game::S_Preparing) return LM_PLAYING;
	for_each_iterator(CWorm*, w, game.localWorms()) {
		if(dynamic_cast<CWormHumanInputHandler*>(w->get()->inputHandler())) {
			return w->get()->bWeaponsReady ? LM_PLAYING : LM_WEAPON_SELECT;
		}
	}
	return LM_PLAYING;
}


// --- SIN_* → Action mapping --------------------------------------------
//
// Every BA_HOLD button references a SIN_* index (so the existing button
// table format keeps working). Translating it to a binding-independent
// TouchScreenInput::Action is a one-line lookup.

static bool sinToAction(int sinIndex, TouchScreenInput::Action& outAction) {
	using A = TouchScreenInput::Action;
	switch(sinIndex) {
	case SIN_LEFT:    outAction = A::Left;          return true;
	case SIN_RIGHT:   outAction = A::Right;         return true;
	case SIN_UP:      outAction = A::Up;            return true;
	case SIN_DOWN:    outAction = A::Down;          return true;
	case SIN_SHOOT:   outAction = A::Fire;          return true;
	case SIN_JUMP:    outAction = A::Jump;          return true;
	case SIN_ROPE:    outAction = A::Rope;          return true;
	case SIN_SELWEAP: outAction = A::SelectWeapon;  return true;
	case SIN_STRAFE:  outAction = A::Strafe;        return true;
	}
	return false;
}


// --- Button state machine ----------------------------------------------

static int hitTestButton(int lx, int ly) {
	int n;
	const Button* b = currentButtons(&n);
	for(int i = 0; i < n; i++) {
		if(lx >= b[i].x && lx < b[i].x + b[i].w
		&& ly >= b[i].y && ly < b[i].y + b[i].h)
			return i;
	}
	return -1;
}

static void setButtonPressed(int i, bool pressed) {
	int n;
	const Button* btns = currentButtons(&n);
	if(i < 0 || i >= n) return;
	const Button& b = btns[i];
	using TouchScreenInput::Action;
	using TouchScreenInput::StateChange;

	// Tap actions emit a single Down event on the leading edge of the first
	// finger. The game logic reads it via wasActionTapped(); no Up event
	// is needed because the consumer treats taps as edge-triggered.
	if(b.action == BA_TAP_PREV_WEAP || b.action == BA_TAP_NEXT_WEAP
	|| b.action == BA_TAP_ESCAPE   || b.action == BA_TAP_RANDOMIZE
	|| b.action == BA_TAP_DONE) {
		const bool wasPressed = (gFingerCount[i] > 0);
		if(pressed) gFingerCount[i]++;
		else if(gFingerCount[i] > 0) gFingerCount[i]--;
		if(pressed && !wasPressed) {
			Action a = Action::OpenMenu;
			switch(b.action) {
			case BA_TAP_PREV_WEAP: a = Action::PreviousWeapon;   break;
			case BA_TAP_NEXT_WEAP: a = Action::NextWeapon;       break;
			case BA_TAP_ESCAPE:    a = Action::OpenMenu;         break;
			case BA_TAP_RANDOMIZE: a = Action::RandomizeWeapons; break;
			case BA_TAP_DONE:      a = Action::ConfirmWeapons;   break;
			default: return;
			}
			// Tap = leading edge only. Down+Up in the same frame so
			// isActionDown(<tap action>) never latches "true" between
			// frames; wasActionTapped is the right read for these.
			TouchScreenInput::pushEvent(kTouchPlayer, a, StateChange::Down);
			TouchScreenInput::pushEvent(kTouchPlayer, a, StateChange::Up);
		}
		return;
	}

	// BA_HOLD: emit Down on the first finger landing, Up when the last
	// finger leaves the button.
	const bool wasPressed = (gFingerCount[i] > 0);
	if(pressed) gFingerCount[i]++;
	else if(gFingerCount[i] > 0) gFingerCount[i]--;
	const bool nowPressed = (gFingerCount[i] > 0);

	Action action;
	if(!sinToAction(b.sinIndex, action)) return;

	if(!wasPressed && nowPressed)
		TouchScreenInput::pushEvent(kTouchPlayer, action, StateChange::Down);
	else if(wasPressed && !nowPressed)
		TouchScreenInput::pushEvent(kTouchPlayer, action, StateChange::Up);
}


// --- VJoy --------------------------------------------------------------

static void setVJoyDir(bool& slot, bool want, TouchScreenInput::Action action) {
	using TouchScreenInput::StateChange;
	if(want && !slot) {
		slot = true;
		TouchScreenInput::pushEvent(kTouchPlayer, action, StateChange::Down);
	} else if(!want && slot) {
		TouchScreenInput::pushEvent(kTouchPlayer, action, StateChange::Up);
		slot = false;
	}
}

static void vjoyUpdate(int lx, int ly) {
	using A = TouchScreenInput::Action;
	gVJoy.curX = lx;
	gVJoy.curY = ly;

	// Anchor is fixed at touch-down for the entire lifetime of the touch
	// — no recentering. Direction is derived from the raw delta against
	// a small deadzone.
	const int dx = lx - gVJoy.anchorX;
	const int dy = ly - gVJoy.anchorY;

	// Both axes use a generous threshold (~2x worm width) so the player
	// can hold the joystick slightly off-center without unintended walking
	// or aim-creep.
	setVJoyDir(gVJoy.heldL, dx < -walk_sensitivity, A::Left);
	setVJoyDir(gVJoy.heldR, dx >  walk_sensitivity, A::Right);
	setVJoyDir(gVJoy.heldU, dy < -aim_sensitivity,  A::Up);
	setVJoyDir(gVJoy.heldD, dy >  aim_sensitivity,  A::Down);
}

static void vjoyRelease() {
	using A = TouchScreenInput::Action;
	using TouchScreenInput::StateChange;
	if(!gVJoy.active) return;
	if(gVJoy.heldL) { TouchScreenInput::pushEvent(kTouchPlayer, A::Left,  StateChange::Up); gVJoy.heldL = false; }
	if(gVJoy.heldR) { TouchScreenInput::pushEvent(kTouchPlayer, A::Right, StateChange::Up); gVJoy.heldR = false; }
	if(gVJoy.heldU) { TouchScreenInput::pushEvent(kTouchPlayer, A::Up,    StateChange::Up); gVJoy.heldU = false; }
	if(gVJoy.heldD) { TouchScreenInput::pushEvent(kTouchPlayer, A::Down,  StateChange::Up); gVJoy.heldD = false; }
	gVJoy.active = false;
}

static void releaseAllButtons() {
	using TouchScreenInput::Action;
	using TouchScreenInput::StateChange;
	for(int i = 0; i < kMaxButtons; i++) {
		// Only HOLD buttons need a synthetic Up; tap actions self-balance.
		if(gFingerCount[i] > 0) {
			int n;
			const Button* btns = currentButtons(&n);
			if(i < n && btns[i].action == BA_HOLD) {
				Action a;
				if(sinToAction(btns[i].sinIndex, a))
					TouchScreenInput::pushEvent(kTouchPlayer, a, StateChange::Up);
			}
		}
		gFingerCount[i] = 0;
	}
	gFingerToButton.clear();
}

static void releaseAll() {
	releaseAllButtons();
	vjoyRelease();
}


// --- Render helper -----------------------------------------------------

// Visualise the invisible joystick: anchor dot, line to the current
// finger position, and a small arrowhead at the tip — so the player can
// see the motion vector they're producing.
static void renderVJoy(SDL_Surface* dst) {
	if(!gVJoy.active) return;

	const int ax = gVJoy.anchorX;
	const int ay = gVJoy.anchorY;
	const int cx = gVJoy.curX;
	const int cy = gVJoy.curY;

	// Red once the horizontal component exceeds walk_sensitivity —
	// signals that the worm will actually move, not just aim.
	const int hdx = cx - ax;
	const bool willWalk = (hdx > walk_sensitivity) || (hdx < -walk_sensitivity);
	const Color stickColor  = willWalk ? Color(255,  90,  60, 230)
	                                   : Color(255, 220,  80, 220);
	const Color anchorColor (255, 255, 255, 200);

	// Anchor: 6x6 marker at the touch-down point.
	DrawRectFillA(dst, ax - 3, ay - 3, ax + 4, ay + 4, anchorColor, anchorColor.a);

	const float dx = (float)(cx - ax);
	const float dy = (float)(cy - ay);
	const float len = std::sqrt(dx * dx + dy * dy);
	if(len < 4.0f) return;  // too short to draw an arrow

	const float nx = dx / len;
	const float ny = dy / len;

	// Draw a thick shaft as 3 parallel lines, offset perpendicular by 1px.
	const float px = -ny;
	const float py =  nx;
	for(int o = -1; o <= 1; o++) {
		int ox = (int)(px * o + 0.5f * (o > 0 ? 1 : (o < 0 ? -1 : 0)));
		int oy = (int)(py * o + 0.5f * (o > 0 ? 1 : (o < 0 ? -1 : 0)));
		DrawLine(dst, ax + ox, ay + oy, cx + ox, cy + oy, stickColor);
	}

	// Arrowhead: two lines from the tip, rotated ±30° from the
	// reverse-direction vector (-nx,-ny).
	const float kHeadLen = 14.0f;
	const float cos30 = 0.8660254f;
	const float sin30 = 0.5f;
	// rotate (-n) by +30°
	const float lx = -nx * cos30 - (-ny) * sin30;
	const float ly = -nx * sin30 + (-ny) * cos30;
	// rotate (-n) by -30°
	const float rx = -nx * cos30 + (-ny) * sin30;
	const float ry =  nx * sin30 + (-ny) * cos30;
	const int hlx = cx + (int)(lx * kHeadLen);
	const int hly = cy + (int)(ly * kHeadLen);
	const int hrx = cx + (int)(rx * kHeadLen);
	const int hry = cy + (int)(ry * kHeadLen);
	DrawLine(dst, cx, cy, hlx, hly, stickColor);
	DrawLine(dst, cx, cy, hrx, hry, stickColor);
}

static void renderButton(SDL_Surface* dst, const Button& b, bool pressed) {
	Color fill   = pressed ? Color(255, 220, 80) : Color(40, 40, 40);
	Uint8 alpha  = pressed ? 180 : 90;
	Color border = Color(220, 220, 220);

	DrawRectFillA(dst, b.x, b.y, b.x + b.w, b.y + b.h, fill, alpha);
	DrawRect    (dst, b.x, b.y, b.x + b.w - 1, b.y + b.h - 1, border);

	if(tLX && b.label) {
		int tw = tLX->cFont.GetWidth(b.label);
		int th = tLX->cFont.GetHeight();
		int tx = b.x + (b.w - tw) / 2;
		int ty = b.y + (b.h - th) / 2;
		tLX->cFont.Draw(dst, tx, ty, pressed ? tLX->clBlack : tLX->clWhite, b.label);
	}
}

} // namespace


// --- Public API --------------------------------------------------------

void Init() {
	if(gInited) return;
	for(int i = 0; i < kMaxButtons; i++)
		gFingerCount[i] = 0;
	gFingerToButton.clear();
	gVJoy = VJoy{false, 0, 0, 0, 0, 0, false, false, false, false};
	TouchScreenInput::releaseAll();
	gMode = LM_PLAYING;
	gUIActive = true;
	gInited = true;
}

void Shutdown() {
	releaseAll();
	TouchScreenInput::releaseAll();
	gInited = false;
}

// Internal predicate — would touch be allowed if the player were using
// it? Used by HandleFingerEvent so a finger-down can re-enable the UI
// even when IsActive() is currently false because of auto-hide.
static bool isTouchAllowed() {
	if(!tLXOptions) return false;
	if(!tLXOptions->bTouchscreenControls) return false;
	if(game.state != Game::S_Playing && game.state != Game::S_Preparing) return false;
	// In-game menu (Esc / MENU button) is a mouse-driven overlay — hide the
	// touch UI and let SDL's native finger→mouse synth drive the menu.
	if(cClient && cClient->isGameMenu()) return false;
	return true;
}

bool IsActive() {
	return isTouchAllowed() && gUIActive;
}

void NotifyExternalInput() {
	if(!gUIActive) return;
	if(!isTouchAllowed()) return;  // nothing to hide outside gameplay
	gUIActive = false;
	releaseAll();  // pushes Up events for any held touch actions
}

bool HandleFingerEvent(const SDL_TouchFingerEvent& ev) {
	if(!isTouchAllowed()) {
		releaseAll();
		return false;
	}

	// Any finger landing on the screen wakes the touch UI back up. The
	// same touch then falls through to the normal button/joystick logic
	// below so the player's tap also counts as a press.
	if(ev.type == SDL_FINGERDOWN && !gUIActive)
		gUIActive = true;

	if(!gUIActive) return false;  // FINGERMOTION/UP while hidden — drop

	const int lx = (int)(ev.x * 640.0f);
	const int ly = (int)(ev.y * 480.0f);

	if(ev.type == SDL_FINGERDOWN) {
		// In playing mode, the left half is the joystick.
		if(gMode == LM_PLAYING && lx < kVJoyAreaRight && !gVJoy.active) {
			gVJoy.active   = true;
			gVJoy.finger   = ev.fingerId;
			gVJoy.anchorX  = lx;
			gVJoy.anchorY  = ly;
			gVJoy.curX     = lx;
			gVJoy.curY     = ly;
			gVJoy.heldL = gVJoy.heldR = gVJoy.heldU = gVJoy.heldD = false;
			return true;
		}
		int idx = hitTestButton(lx, ly);
		gFingerToButton[ev.fingerId] = idx;
		if(idx >= 0) {
			setButtonPressed(idx, true);
			return true;
		}
		return false;

	} else if(ev.type == SDL_FINGERUP) {
		if(gVJoy.active && gVJoy.finger == ev.fingerId) {
			vjoyRelease();
			return true;
		}
		auto it = gFingerToButton.find(ev.fingerId);
		int prev = (it != gFingerToButton.end()) ? it->second : -1;
		if(it != gFingerToButton.end()) gFingerToButton.erase(it);
		if(prev >= 0) {
			setButtonPressed(prev, false);
			return true;
		}
		return false;

	} else if(ev.type == SDL_FINGERMOTION) {
		if(gVJoy.active && gVJoy.finger == ev.fingerId) {
			vjoyUpdate(lx, ly);
			return true;
		}
		int idx = hitTestButton(lx, ly);
		auto it = gFingerToButton.find(ev.fingerId);
		int prev = (it != gFingerToButton.end()) ? it->second : -1;
		if(idx == prev) return prev >= 0;
		if(prev >= 0) setButtonPressed(prev, false);
		if(idx >= 0)  setButtonPressed(idx,  true);
		gFingerToButton[ev.fingerId] = idx;
		return idx >= 0 || prev >= 0;
	}
	return false;
}

void Update() {
	// Re-evaluate the layout each frame; if it changed, drop any held state
	// so an arrow held in the picker doesn't leak into the playing layout.
	const LayoutMode want = IsActive() ? detectLayoutMode() : LM_PLAYING;
	if(want != gMode) {
		releaseAll();
		gMode = want;
	}

	if(!IsActive() && gInited) {
		bool anyHeld = gVJoy.active;
		for(int i = 0; !anyHeld && i < kMaxButtons; i++)
			if(gFingerCount[i] > 0) anyHeld = true;
		if(anyHeld) releaseAll();
	}

	// Drain any events pushed during this ProcessEvents tick (touch finger
	// events that arrived this frame, plus any Up events emitted by
	// releaseAll above). The game-logic readers downstream will see the
	// up-to-date held state and pending taps.
	TouchScreenInput::processFrame();
}

void Render(SDL_Surface* dst) {
	if(!IsActive() || dst == NULL) return;
	int n;
	const Button* btns = currentButtons(&n);
	for(int i = 0; i < n; i++)
		renderButton(dst, btns[i], gFingerCount[i] > 0);
	if(gMode == LM_PLAYING)
		renderVJoy(dst);
}

} // namespace TouchControls
