/*
 *  TouchControls.cpp
 *  OpenLieroX
 *
 *  See TouchControls.h for the design overview.
 *
 *  Two layouts:
 *
 *    LM_PLAYING (default in-game)
 *      - Most of the screen (x < gVJoyAreaRight logical, flush against
 *        the right-edge action-button column) is an INVISIBLE virtual joystick.
 *        First finger landing there sets an anchor; dragging from the
 *        anchor presses Left/Right (movement) and Up/Down (aim).
 *        Both axes use the same generous threshold (~2x worm width):
 *        |dx| > gWalkSensitivity for Left/Right, |dy| > gAimSensitivity
 *        for Up/Down.
 *      - Right half hosts: Jump, Rope, Fire, plus Prev/Next-Weapon
 *        (one-shot taps that synthesise SelectWeapon + Left/Right).
 *
 *    LM_WEAPON_SELECT (during the per-round weapon picker — i.e. while
 *      the first local human worm has bWeaponsReady == false)
 *      - Joystick is OFF.
 *      - Visible D-pad drives the menu; Random + DONE on the right edge.
 *
 *  Button positions and the vjoy tunables are loaded from
 *  share/gamedir/touchscreen/<Game.TouchscreenLayout>.yaml at Init().
 *  If that file is missing or malformed, hardcoded defaults take over
 *  so the build stays self-contained.
 *
 *  code under LGPL
 */

#include "TouchControls.h"

#include <SDL.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "AuxLib.h"
#include "CClient.h"
#include "CInput.h"
#include "Color.h"
#include "CWormHuman.h"
#include "Debug.h"
#include "FindFile.h"
#include "GfxPrimitives.h"
#include "InputEvents.h"
#include "LieroX.h"
#include "Options.h"
#include "SmartPointer.h"
#include "StringUtils.h"
#include "TouchScreenInput.h"
#include "game/CWorm.h"
#include "game/Game.h"
#include "Iter.h"


namespace TouchControls {

namespace {

// --- Layout types ------------------------------------------------------

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
	std::string label;
	// Optional artwork. `image` is the filename inside
	// share/gamedir/touchscreen/images/; when set, the renderer blits
	// the surface (scaled to the button rect) instead of drawing the
	// fill+border+label. `imagePressed` is loaded automatically from
	// "<basename>_pressed<.ext>" if that file exists and is rendered
	// while the button is held.
	std::string image;
	SmartPointer<SDL_Surface> imageNormal;
	SmartPointer<SDL_Surface> imagePressed;
};

// Upper bound on buttons per layout — sizes the per-button finger
// counter. Layouts that try to exceed it are rejected by the loader.
constexpr int kMaxButtons = 32;


// --- Runtime layout state ----------------------------------------------
//
// Populated by Init() from the YAML layout file, or — if loading fails —
// from applyHardcodedDefaults(). All gameplay reads go through these.

static int  gVJoyAreaRight    = 525;
static int  gWalkSensitivity  = 40;
static int  gAimSensitivity   = 40;
static bool gShowButtonBorder = true;

// Optional minimap-position override. When the active layout specifies
// `minimap: { x, y }`, the HUD draw path reads it through
// GetMinimapPosition() and uses those coords instead of its default.
static bool gMinimapOverride  = false;
static int  gMinimapX         = 0;
static int  gMinimapY         = 0;

static std::vector<Button> gPlayingButtons;
static std::vector<Button> gWeaponSelectButtons;


// --- Hardcoded defaults ------------------------------------------------
//
// Mirrors share/gamedir/touchscreen/rightside.yaml. Used when the YAML
// file is missing, unreadable, or malformed.

static void applyHardcodedDefaults() {
	gVJoyAreaRight    = 525;
	gWalkSensitivity  = 40;
	gAimSensitivity   = 40;
	gShowButtonBorder = true;
	gMinimapOverride  = false;
	gMinimapX         = 0;
	gMinimapY         = 0;

	// In-game:
	//   MENU(560,5) | ROPE(525, 50, 110x110)
	//   PREV(525,165, 55x50) + NEXT(580,165, 55x50)
	//   FIRE(525,220, 110x180) | JUMP(525,405, 110x70)
	gPlayingButtons = {
		{ 560,   5,  75,  40, 0,         BA_TAP_ESCAPE,    "MENU" },
		{ 525, 165,  55,  50, 0,         BA_TAP_PREV_WEAP, "<<"   },
		{ 580, 165,  55,  50, 0,         BA_TAP_NEXT_WEAP, ">>"   },
		{ 525,  50, 110, 110, SIN_ROPE,  BA_HOLD,          "ROPE" },
		{ 525, 220, 110, 180, SIN_SHOOT, BA_HOLD,          "FIRE" },
		{ 525, 405, 110,  70, SIN_JUMP,  BA_HOLD,          "JUMP" },
	};
	// Weapon-select: D-pad lower-left, Random + DONE on the right edge.
	gWeaponSelectButtons = {
		{ 560,   5, 75, 40, 0,         BA_TAP_ESCAPE,    "MENU"   },
		{  60, 240, 80, 70, SIN_UP,    BA_HOLD,          "^"      },
		{  60, 400, 80, 70, SIN_DOWN,  BA_HOLD,          "v"      },
		{   0, 320, 80, 70, SIN_LEFT,  BA_HOLD,          "<"      },
		{ 140, 320, 80, 70, SIN_RIGHT, BA_HOLD,          ">"      },
		{ 525, 240,110, 95, 0,         BA_TAP_RANDOMIZE, "Random" },
		{ 525, 345,110, 95, 0,         BA_TAP_DONE,      "DONE"   },
	};
}


// --- Image loading -----------------------------------------------------

// Return <basename>_pressed<.ext> for a path like
// "touchscreen/images/jump.png" → "touchscreen/images/jump_pressed.png".
static std::string pressedVariantPath(const std::string& path) {
	const size_t dot   = path.find_last_of('.');
	const size_t slash = path.find_last_of('/');
	if(dot == std::string::npos || (slash != std::string::npos && dot < slash))
		return path + "_pressed";
	return path.substr(0, dot) + "_pressed" + path.substr(dot);
}

// Cache surfaces by relPath so the same image used on multiple buttons
// (and across re-Inits) only hits disk once. A cached null is kept for
// genuinely-missing files so we don't re-try them every frame.
static SmartPointer<SDL_Surface> loadCachedImage(const std::string& relPath) {
	static std::map<std::string, SmartPointer<SDL_Surface>> cache;
	auto it = cache.find(relPath);
	if(it != cache.end()) return it->second;
	SmartPointer<SDL_Surface> surf = LoadGameImage(relPath, true);
	cache[relPath] = surf;
	return surf;
}


// --- YAML loader -------------------------------------------------------

static bool parseAction(const std::string& s, ButtonAction& outAction, bool& outNeedsBinding) {
	static const std::map<std::string, ButtonAction> actionMap = {
		{"hold",        BA_HOLD},
		{"menu",        BA_TAP_ESCAPE},
		{"prev_weapon", BA_TAP_PREV_WEAP},
		{"next_weapon", BA_TAP_NEXT_WEAP},
		{"randomize",   BA_TAP_RANDOMIZE},
		{"done",        BA_TAP_DONE},
	};
	auto it = actionMap.find(s);
	if(it == actionMap.end()) return false;
	outAction = it->second;
	outNeedsBinding = (it->second == BA_HOLD);
	return true;
}

static bool parseBinding(const std::string& s, int& outSin) {
	static const std::map<std::string, int> bindingMap = {
		{"up", SIN_UP}, {"down", SIN_DOWN},
		{"left", SIN_LEFT}, {"right", SIN_RIGHT},
		{"shoot", SIN_SHOOT}, {"fire", SIN_SHOOT},  // 'fire' = alias
		{"jump", SIN_JUMP}, {"rope", SIN_ROPE},
		{"selweap", SIN_SELWEAP}, {"select_weapon", SIN_SELWEAP},
		{"strafe", SIN_STRAFE},
		{"weapon1", SIN_WEAPON1}, {"weapon2", SIN_WEAPON2},
		{"weapon3", SIN_WEAPON3}, {"weapon4", SIN_WEAPON4},
		{"weapon5", SIN_WEAPON5},
	};
	auto it = bindingMap.find(s);
	if(it == bindingMap.end()) return false;
	outSin = it->second;
	return true;
}

static bool parseButtons(const YAML::Node& node, const std::string& section, std::vector<Button>& out) {
	if(!node.IsSequence()) {
		warnings << "TouchControls: '" << section << "' is not a sequence" << endl;
		return false;
	}
	out.clear();
	int i = 0;
	for(const auto& bn : node) {
		if(!bn.IsMap()) {
			warnings << "TouchControls: " << section << "[" << i << "] is not a map" << endl;
			return false;
		}
		Button b{};
		try {
			b.x = bn["x"].as<int>();
			b.y = bn["y"].as<int>();
			b.w = bn["w"].as<int>();
			b.h = bn["h"].as<int>();
		} catch(const YAML::Exception&) {
			warnings << "TouchControls: " << section << "[" << i << "] missing or bad x/y/w/h" << endl;
			return false;
		}
		if(bn["label"])
			b.label = bn["label"].as<std::string>();
		if(!bn["action"]) {
			warnings << "TouchControls: " << section << "[" << i << "] missing 'action'" << endl;
			return false;
		}
		std::string actionStr = bn["action"].as<std::string>();
		bool needsBinding = false;
		if(!parseAction(actionStr, b.action, needsBinding)) {
			warnings << "TouchControls: " << section << "[" << i << "] unknown action '"
			         << actionStr << "'" << endl;
			return false;
		}
		if(needsBinding) {
			if(!bn["binding"]) {
				warnings << "TouchControls: " << section << "[" << i
				         << "] action=hold needs 'binding'" << endl;
				return false;
			}
			std::string bindStr = bn["binding"].as<std::string>();
			if(!parseBinding(bindStr, b.sinIndex)) {
				warnings << "TouchControls: " << section << "[" << i
				         << "] unknown binding '" << bindStr << "'" << endl;
				return false;
			}
		} else {
			b.sinIndex = 0;
		}
		// Optional image: relative to share/gamedir/touchscreen/images/.
		// The pressed-state variant is loaded automatically if it exists.
		if(bn["image"]) {
			b.image = bn["image"].as<std::string>();
			const std::string normalPath = "touchscreen/images/" + b.image;
			b.imageNormal = loadCachedImage(normalPath);
			if(!b.imageNormal.get()) {
				warnings << "TouchControls: " << section << "[" << i
				         << "] image not found: " << normalPath
				         << " (button will render as text)" << endl;
			} else {
				b.imagePressed = loadCachedImage(pressedVariantPath(normalPath));
				// Missing pressed variant is fine; renderer falls back to normal.
			}
		}
		out.push_back(std::move(b));
		i++;
	}
	return true;
}

// Load layout from share/gamedir/touchscreen/<name>.yaml. Returns true
// if the file parsed cleanly and the runtime state has been replaced.
// On any failure, leaves the existing runtime state untouched and logs
// the reason.
static bool loadLayoutFromYaml(const std::string& name) {
	const std::string relPath = "touchscreen/" + name + ".yaml";
	std::ifstream f;
	if(!OpenGameFileR(f, relPath)) {
		notes << "TouchControls: layout '" << relPath << "' not found, using defaults" << endl;
		return false;
	}
	std::stringstream buf;
	buf << f.rdbuf();
	try {
		YAML::Node root = YAML::Load(buf.str());
		if(!root.IsMap()) {
			warnings << "TouchControls: YAML root is not a map in " << relPath << endl;
			return false;
		}
		// Parse into locals first; only commit if everything succeeds.
		int  vjoyAreaRight    = gVJoyAreaRight;
		int  walkSensitivity  = gWalkSensitivity;
		int  aimSensitivity   = gAimSensitivity;
		bool showButtonBorder = gShowButtonBorder;
		if(root["vjoy"]) {
			const YAML::Node& v = root["vjoy"];
			if(v["area_right"])       vjoyAreaRight   = v["area_right"].as<int>();
			if(v["walk_sensitivity"]) walkSensitivity = v["walk_sensitivity"].as<int>();
			if(v["aim_sensitivity"])  aimSensitivity  = v["aim_sensitivity"].as<int>();
		}
		if(root["button-border"]) showButtonBorder = root["button-border"].as<bool>();
		bool minimapOverride = false;
		int  minimapX        = 0;
		int  minimapY        = 0;
		if(root["minimap"]) {
			const YAML::Node& m = root["minimap"];
			if(m["x"] && m["y"]) {
				minimapX = m["x"].as<int>();
				minimapY = m["y"].as<int>();
				minimapOverride = true;
			} else {
				warnings << "TouchControls: 'minimap' needs both 'x' and 'y'" << endl;
				return false;
			}
		}
		std::vector<Button> playing;
		std::vector<Button> weaponSel;
		const bool hasPlaying  = (bool)root["playing"];
		const bool hasWeaponSel = (bool)root["weapon_select"];
		if(hasPlaying && !parseButtons(root["playing"], "playing", playing))
			return false;
		if(hasWeaponSel && !parseButtons(root["weapon_select"], "weapon_select", weaponSel))
			return false;
		if((int)playing.size() > kMaxButtons || (int)weaponSel.size() > kMaxButtons) {
			warnings << "TouchControls: too many buttons in " << relPath
			         << " (max " << kMaxButtons << " per layout)" << endl;
			return false;
		}
		// Commit.
		gVJoyAreaRight    = vjoyAreaRight;
		gWalkSensitivity  = walkSensitivity;
		gAimSensitivity   = aimSensitivity;
		gShowButtonBorder = showButtonBorder;
		gMinimapOverride  = minimapOverride;
		gMinimapX         = minimapX;
		gMinimapY         = minimapY;
		if(hasPlaying)    gPlayingButtons      = std::move(playing);
		if(hasWeaponSel)  gWeaponSelectButtons = std::move(weaponSel);
		notes << "TouchControls: loaded layout '" << name << "' ("
		      << gPlayingButtons.size() << " playing buttons, "
		      << gWeaponSelectButtons.size() << " weapon-select buttons)" << endl;
		return true;
	} catch(const YAML::Exception& e) {
		warnings << "TouchControls: YAML parse error in " << relPath
		         << ": " << e.what() << endl;
		return false;
	}
}


// --- Runtime state -----------------------------------------------------

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

static const std::vector<Button>& currentButtons() {
	return (gMode == LM_WEAPON_SELECT) ? gWeaponSelectButtons : gPlayingButtons;
}

static LayoutMode detectLayoutMode() {
	// Touch controls only drive the first local human worm (the "touch
	// player"). The layout follows that worm's picker state: as soon as
	// they confirm their loadout, switch back to the gameplay layout
	// even if a split-screen second player is still picking on the
	// keyboard.
	if(game.state < Game::S_Preparing) return LM_PLAYING;
	for_each_iterator(CWorm*, w, game.localWorms()) {
		if(dynamic_cast<CWormHumanInputHandler*>(w->get()->inputHandler())) {
			return w->get()->bWeaponsReady ? LM_PLAYING : LM_WEAPON_SELECT;
		}
	}
	return LM_PLAYING;
}


// --- SIN_* → Action mapping --------------------------------------------

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
	const auto& btns = currentButtons();
	const int n = (int)btns.size();
	for(int i = 0; i < n; i++) {
		if(lx >= btns[i].x && lx < btns[i].x + btns[i].w
		&& ly >= btns[i].y && ly < btns[i].y + btns[i].h)
			return i;
	}
	return -1;
}

static void setButtonPressed(int i, bool pressed) {
	const auto& btns = currentButtons();
	if(i < 0 || i >= (int)btns.size() || i >= kMaxButtons) return;
	const Button& b = btns[i];
	using TouchScreenInput::Action;
	using TouchScreenInput::StateChange;

	// Tap actions emit a single Down event on the leading edge of the first
	// finger. The game logic reads them via wasActionTapped(); no Up event
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
	const int dx = lx - gVJoy.anchorX;
	const int dy = ly - gVJoy.anchorY;
	setVJoyDir(gVJoy.heldL, dx < -gWalkSensitivity, A::Left);
	setVJoyDir(gVJoy.heldR, dx >  gWalkSensitivity, A::Right);
	setVJoyDir(gVJoy.heldU, dy < -gAimSensitivity,  A::Up);
	setVJoyDir(gVJoy.heldD, dy >  gAimSensitivity,  A::Down);
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
	const auto& btns = currentButtons();
	const int n = (int)btns.size();
	for(int i = 0; i < kMaxButtons; i++) {
		// Only HOLD buttons need a synthetic Up; tap actions self-balance.
		if(gFingerCount[i] > 0 && i < n && btns[i].action == BA_HOLD) {
			Action a;
			if(sinToAction(btns[i].sinIndex, a))
				TouchScreenInput::pushEvent(kTouchPlayer, a, StateChange::Up);
		}
		gFingerCount[i] = 0;
	}
	gFingerToButton.clear();
}

static void releaseAll() {
	releaseAllButtons();
	vjoyRelease();
}


// --- Render helpers ----------------------------------------------------

// Visualise the invisible joystick: anchor dot, line to the current
// finger position, and a small arrowhead at the tip.
static void renderVJoy(SDL_Surface* dst) {
	if(!gVJoy.active) return;

	const int ax = gVJoy.anchorX;
	const int ay = gVJoy.anchorY;
	const int cx = gVJoy.curX;
	const int cy = gVJoy.curY;

	// Red once the horizontal component exceeds gWalkSensitivity —
	// signals that the worm will actually move, not just aim.
	const int hdx = cx - ax;
	const bool willWalk = (hdx > gWalkSensitivity) || (hdx < -gWalkSensitivity);
	const Color stickColor  = willWalk ? Color(255,  90,  60, 230)
	                                   : Color(255, 220,  80, 220);
	const Color anchorColor (255, 255, 255, 200);

	DrawRectFillA(dst, ax - 3, ay - 3, ax + 4, ay + 4, anchorColor, anchorColor.a);

	const float dx = (float)(cx - ax);
	const float dy = (float)(cy - ay);
	const float len = std::sqrt(dx * dx + dy * dy);
	if(len < 4.0f) return;

	const float nx = dx / len;
	const float ny = dy / len;

	const float px = -ny;
	const float py =  nx;
	for(int o = -1; o <= 1; o++) {
		int ox = (int)(px * o + 0.5f * (o > 0 ? 1 : (o < 0 ? -1 : 0)));
		int oy = (int)(py * o + 0.5f * (o > 0 ? 1 : (o < 0 ? -1 : 0)));
		DrawLine(dst, ax + ox, ay + oy, cx + ox, cy + oy, stickColor);
	}

	const float kHeadLen = 14.0f;
	const float cos30 = 0.8660254f;
	const float sin30 = 0.5f;
	const float lx = -nx * cos30 - (-ny) * sin30;
	const float ly = -nx * sin30 + (-ny) * cos30;
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
	// Image-based button: blit the artwork scaled into the rect; swap to
	// the pressed-state image when the button is held (and a pressed
	// variant was loaded). Skip the fill/border/label fall-through.
	if(b.imageNormal.get()) {
		const SmartPointer<SDL_Surface>& img =
			(pressed && b.imagePressed.get()) ? b.imagePressed : b.imageNormal;
		DrawImageResampledAdv(dst, img, 0, 0, b.x, b.y,
		                      img.get()->w, img.get()->h, b.w, b.h);
		return;
	}

	Color fill   = pressed ? Color(255, 220, 80) : Color(40, 40, 40);
	Uint8 alpha  = pressed ? 180 : 90;
	Color border = Color(220, 220, 220);

	DrawRectFillA(dst, b.x, b.y, b.x + b.w, b.y + b.h, fill, alpha);
	if(gShowButtonBorder)
		DrawRect(dst, b.x, b.y, b.x + b.w - 1, b.y + b.h - 1, border);

	if(tLX && !b.label.empty()) {
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
	// Start with defaults so the rest of the code always has a usable
	// layout to query — even if option lookup or YAML loading fails.
	applyHardcodedDefaults();
	if(tLXOptions && !tLXOptions->sTouchscreenLayout.empty()) {
		if(!loadLayoutFromYaml(tLXOptions->sTouchscreenLayout)) {
			// Loader already logged the reason; reset to defaults in case
			// a partial parse modified some runtime state mid-way.
			applyHardcodedDefaults();
		}
	}
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
// Resolve the tri-state Game.TouchscreenControls option to an effective
// bool. "true"/"yes"/"on"/"1" → force on (use this on the Linux client
// to capture layout previews). "false"/"no"/"off"/"0" → force off.
// Anything else (including the default "auto") → platform default:
// Android = on, everything else = off.
static bool resolveTouchscreenEnabled(const std::string& setting) {
	if(stringcaseequal(setting, "true")  || stringcaseequal(setting, "yes")
	|| stringcaseequal(setting, "on")    || setting == "1")
		return true;
	if(stringcaseequal(setting, "false") || stringcaseequal(setting, "no")
	|| stringcaseequal(setting, "off")   || setting == "0")
		return false;
#ifdef __ANDROID__
	return true;
#else
	return false;
#endif
}

static bool isTouchAllowed() {
	if(!tLXOptions) return false;
	if(!resolveTouchscreenEnabled(tLXOptions->sTouchscreenControls)) return false;
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
	if(!isTouchAllowed()) return;
	// "true" forces the touch UI to stay visible regardless of other
	// input. This is what the desktop client uses to capture layout
	// preview screenshots: navigating with the keyboard / mouse must
	// not dismiss the touch UI we're trying to photograph.
	if(tLXOptions && stringcaseequal(tLXOptions->sTouchscreenControls, "true"))
		return;
	gUIActive = false;
	releaseAll();
}

bool GetMinimapPosition(int& outX, int& outY) {
	if(!gMinimapOverride) return false;
	outX = gMinimapX;
	outY = gMinimapY;
	return true;
}

void ReloadLayout() {
	applyHardcodedDefaults();
	if(tLXOptions && !tLXOptions->sTouchscreenLayout.empty()) {
		if(!loadLayoutFromYaml(tLXOptions->sTouchscreenLayout)) {
			// Loader logged the reason; reset cleanly in case a partial
			// parse modified state mid-way.
			applyHardcodedDefaults();
		}
	}
	// Drop any held button / joystick state — a button index from the old
	// layout could otherwise leak into the new one.
	for(int i = 0; i < kMaxButtons; i++)
		gFingerCount[i] = 0;
	gFingerToButton.clear();
	gVJoy = VJoy{false, 0, 0, 0, 0, 0, false, false, false, false};
	TouchScreenInput::releaseAll();
	gMode = LM_PLAYING;
}

bool IsTouchDeviceAvailable() {
	if(SDL_GetNumTouchDevices() > 0) return true;
	// "true" forces the touch UI visible (used for desktop screenshot
	// sessions); show the Controls → Touch screen tab too so the layout
	// can still be picked from a box that has no real touch device.
	if(tLXOptions && stringcaseequal(tLXOptions->sTouchscreenControls, "true"))
		return true;
	return false;
}

// Render the LM_PLAYING view of `layoutName` onto a freshly-allocated
// 640x480 surface and return it. Used by the options menu to populate
// each layout-selection tile — there is no on-disk preview PNG, the
// thumbnail comes straight out of whatever the YAML currently says.
//
// Backs up + restores the runtime layout state around the call so the
// active layout (held in the gXxx globals) is unaffected.
static SmartPointer<SDL_Surface> renderLayoutPreviewSurface(const std::string& layoutName) {
	SmartPointer<SDL_Surface> surf = gfxCreateSurfaceAlpha(640, 480);
	if(!surf.get()) return surf;

	// Snapshot active layout state.
	const int  bakVJoy   = gVJoyAreaRight;
	const int  bakWalk   = gWalkSensitivity;
	const int  bakAim    = gAimSensitivity;
	const bool bakBorder = gShowButtonBorder;
	const bool bakMmOv   = gMinimapOverride;
	const int  bakMmX    = gMinimapX;
	const int  bakMmY    = gMinimapY;
	std::vector<Button> bakPlaying     = gPlayingButtons;
	std::vector<Button> bakWeaponSel   = gWeaponSelectButtons;

	// Swap in the target layout. If the file is missing or malformed,
	// fall back to the hardcoded defaults so the tile still shows
	// something usable.
	applyHardcodedDefaults();
	if(!loadLayoutFromYaml(layoutName))
		applyHardcodedDefaults();

	// Background dark slate so the buttons read against it. A faint
	// outline of the vjoy zone and a box for the minimap give spatial
	// context without dominating the thumbnail.
	SDL_Surface* dst = surf.get();
	DrawRectFill(dst, 0, 0, dst->w, dst->h, Color(30, 38, 48));
	DrawRect(dst, 0, 0, gVJoyAreaRight - 1, dst->h - 1, Color(60, 80, 100));
	if(gMinimapOverride) {
		const int mmX = gMinimapX;
		const int mmY = gMinimapY;
		const int mmW = 128;
		const int mmH = 96;
		DrawRectFillA(dst, mmX, mmY, mmX + mmW, mmY + mmH, Color(80, 180, 80), 160);
		DrawRect    (dst, mmX, mmY, mmX + mmW - 1, mmY + mmH - 1, Color(180, 230, 180));
		// OLX ships only one font size, so to make "MAP" legible after the
		// 640x480 preview gets scaled down into the menu tile, render the
		// label to a tiny alpha surface and scale-blit it 3x.
		if(tLX) {
			const std::string label = "MAP";
			const int natW = tLX->cOutlineFont.GetWidth(label);
			const int natH = tLX->cOutlineFont.GetHeight();
			SmartPointer<SDL_Surface> tmp = gfxCreateSurfaceAlpha(natW, natH);
			if(tmp.get()) {
				SDL_FillRect(tmp.get(), NULL,
				             SDL_MapRGBA(tmp.get()->format, 0, 0, 0, 0));
				tLX->cOutlineFont.Draw(tmp.get(), 0, 0, tLX->clWhite, label);
				const int kScale  = 3;
				const int scaledW = natW * kScale;
				const int scaledH = natH * kScale;
				DrawImageResampledAdv(dst, tmp, 0, 0,
				                      mmX + (mmW - scaledW) / 2,
				                      mmY + (mmH - scaledH) / 2,
				                      natW, natH, scaledW, scaledH);
			}
		}
	}
	for(const Button& b : gPlayingButtons)
		renderButton(dst, b, /*pressed*/ false);

	// Restore.
	gVJoyAreaRight    = bakVJoy;
	gWalkSensitivity  = bakWalk;
	gAimSensitivity   = bakAim;
	gShowButtonBorder = bakBorder;
	gMinimapOverride  = bakMmOv;
	gMinimapX         = bakMmX;
	gMinimapY         = bakMmY;
	gPlayingButtons      = std::move(bakPlaying);
	gWeaponSelectButtons = std::move(bakWeaponSel);

	return surf;
}

std::vector<LayoutInfo> GetAvailableLayouts() {
	// First collect the filenames (basenames without ".yaml").
	struct Collector {
		std::vector<std::string>* out;
		bool operator()(const std::string& f) {
			if(stringcaseequal(GetFileExtension(f), "yaml"))
				out->push_back(GetBaseFilenameWithoutExt(f));
			return true;
		}
	};
	std::vector<std::string> fileNames;
	Collector c{&fileNames};
	FindFiles(c, "touchscreen", false, FM_REG);
	std::sort(fileNames.begin(), fileNames.end());
	fileNames.erase(std::unique(fileNames.begin(), fileNames.end()), fileNames.end());

	// For each YAML: read `name:` for the display label, and render a
	// live preview surface from the layout data. No on-disk PNG file —
	// the thumbnail always reflects the current YAML.
	std::vector<LayoutInfo> result;
	result.reserve(fileNames.size());
	for(const std::string& fname : fileNames) {
		LayoutInfo info;
		info.fileName    = fname;
		info.displayName = fname;  // fallback if YAML has no `name:`
		std::ifstream f;
		if(OpenGameFileR(f, "touchscreen/" + fname + ".yaml")) {
			std::stringstream buf;
			buf << f.rdbuf();
			try {
				YAML::Node root = YAML::Load(buf.str());
				if(root.IsMap() && root["name"])
					info.displayName = root["name"].as<std::string>();
			} catch(const YAML::Exception&) {
				// Malformed YAML — keep the fallback display name; the
				// renderer below will use hardcoded defaults.
			}
		}
		info.preview = renderLayoutPreviewSurface(fname);
		result.push_back(std::move(info));
	}
	return result;
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

	if(!gUIActive) return false;

	const int lx = (int)(ev.x * 640.0f);
	const int ly = (int)(ev.y * 480.0f);

	if(ev.type == SDL_FINGERDOWN) {
		// In playing mode, the left half is the joystick.
		if(gMode == LM_PLAYING && lx < gVJoyAreaRight && !gVJoy.active) {
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

	// Drain any events pushed during this ProcessEvents tick.
	TouchScreenInput::processFrame();
}

void Render(SDL_Surface* dst) {
	if(!IsActive() || dst == NULL) return;
	const auto& btns = currentButtons();
	const int n = (int)btns.size();
	for(int i = 0; i < n && i < kMaxButtons; i++)
		renderButton(dst, btns[i], gFingerCount[i] > 0);
	if(gMode == LM_PLAYING)
		renderVJoy(dst);
}

} // namespace TouchControls
