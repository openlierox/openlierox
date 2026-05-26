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

#include <assert.h>
#include <vector>


#include "LieroX.h"

#include "AuxLib.h"  // for GetConfig()
#include "Error.h"
#include "ConfigHandler.h"
#include "InputEvents.h"
#include "StringUtils.h"
#include "CInput.h"
#include "Debug.h"





keys_t Keys[] = {
	{ "a", SDLK_a },
	{ "b", SDLK_b },
	{ "c", SDLK_c },
	{ "d", SDLK_d },
	{ "e", SDLK_e },
	{ "f", SDLK_f },
	{ "g", SDLK_g },
	{ "h", SDLK_h },
	{ "i", SDLK_i },
	{ "j", SDLK_j },
	{ "k", SDLK_k },
	{ "l", SDLK_l },
	{ "m", SDLK_m },
	{ "n", SDLK_n },
	{ "o", SDLK_o },
	{ "p", SDLK_p },
	{ "q", SDLK_q },
	{ "r", SDLK_r },
	{ "s", SDLK_s },
	{ "t", SDLK_t },
	{ "u", SDLK_u },
	{ "v", SDLK_v },
	{ "w", SDLK_w },
	{ "x", SDLK_x },
	{ "y", SDLK_y },
	{ "z", SDLK_z },
	{ "1", SDLK_1 },
	{ "2", SDLK_2 },
	{ "3", SDLK_3 },
	{ "4", SDLK_4 },
	{ "5", SDLK_5 },
	{ "6", SDLK_6 },
	{ "7", SDLK_7 },
	{ "8", SDLK_8 },
	{ "9", SDLK_9 },
	{ "0", SDLK_0 },
	{ "kp 0", SDLK_KP_0},
	{ "kp 1", SDLK_KP_1},
	{ "kp 2", SDLK_KP_2},
	{ "kp 3", SDLK_KP_3},
	{ "kp 4", SDLK_KP_4},
	{ "kp 5", SDLK_KP_5},
	{ "kp 6", SDLK_KP_6},
	{ "kp 7", SDLK_KP_7},
	{ "kp 8", SDLK_KP_8},
	{ "kp 9", SDLK_KP_9},
	{ "kp .",SDLK_KP_PERIOD },
	{ "kp enter",SDLK_KP_ENTER },
	{ "kp /",SDLK_KP_DIVIDE },
	{ "kp *",SDLK_KP_MULTIPLY },
	{ "kp -",SDLK_KP_MINUS },
	{ "kp +",SDLK_KP_PLUS },
	{ "kp =",SDLK_KP_EQUALS },
	{ "[",SDLK_LEFTBRACKET },
	{ "]",SDLK_RIGHTBRACKET },
	{ "\\",SDLK_BACKSLASH },
	{ "/",SDLK_SLASH},
	{ ":", SDLK_COLON},
	{ ";", SDLK_SEMICOLON},
	{ "<", SDLK_LESS},
	{ "=", SDLK_EQUALS},
	{ ">", SDLK_GREATER},
	{ "?", SDLK_QUESTION},
	{ "@", SDLK_AT},
	{ "(", SDLK_LEFTPAREN},
	{ ")", SDLK_RIGHTPAREN},
	{ "*", SDLK_ASTERISK},
	{ "+", SDLK_PLUS},
	{ "-", SDLK_MINUS},
	{ "'", SDLK_QUOTE},
	{ ",", SDLK_COMMA},
	{ ".", SDLK_PERIOD},
	{ "#", SDLK_HASH},
	{ "%", SDLK_PERCENT},
	{ "$", SDLK_DOLLAR},
	{ "&", SDLK_AMPERSAND},
	{ "\"", SDLK_QUOTEDBL},
	{ "`", SDLK_BACKQUOTE},
	{ "^", SDLK_CARET},
	{ "enter", SDLK_RETURN},
	{ "tab", SDLK_TAB},
	{ "space",SDLK_SPACE },
	{ "up",SDLK_UP },
	{ "down",SDLK_DOWN },
	{ "right",SDLK_RIGHT },
	{ "left",SDLK_LEFT },
	{ "lctrl",SDLK_LCTRL },
	{ "lshift",SDLK_LSHIFT },
	{ "lalt",SDLK_LALT },
	{ "lmeta",SDLK_LGUI },
	{ "lsuper",SDLK_LGUI },
	{ "rctrl",SDLK_RCTRL },
	{ "rshift",SDLK_RSHIFT },
	{ "ralt",SDLK_RALT },
	{ "rmeta",SDLK_RGUI },
	{ "rsuper",SDLK_RGUI },
	{ "insert", SDLK_INSERT},
	{ "home", SDLK_HOME},
	{ "pg up", SDLK_PAGEUP},
	{ "end", SDLK_END},
	{ "pg Dn", SDLK_PAGEDOWN},
	{ "delete", SDLK_DELETE},
	{ "num lk", SDLK_NUMLOCKCLEAR},
	{ "caps", SDLK_CAPSLOCK},
	{ "scr lk", SDLK_SCROLLLOCK},
	{ "pause", SDLK_PAUSE},
	{ "F1", SDLK_F1 },
	{ "F2", SDLK_F2 },
	{ "F3", SDLK_F3 },
	{ "F4", SDLK_F4 },
	{ "F5", SDLK_F5 },
	{ "F6", SDLK_F6 },
	{ "F7", SDLK_F7 },
	{ "F8", SDLK_F8 },
	{ "F9", SDLK_F9 },
	{ "F10", SDLK_F10 },
	{ "F11", SDLK_F11 },
	{ "F12", SDLK_F12 },
	{ "F13", SDLK_F13 },
	{ "F14", SDLK_F14 },
	{ "F15", SDLK_F15 },
	{ "F16", SDLK_F16 },
	{ "F17", SDLK_F17 },
	{ "F18", SDLK_F18 },
	{ "F19", SDLK_F19 },
	{ "F20", SDLK_F20 },
	{ "F21", SDLK_F21 },
	{ "F22", SDLK_F22 },
	{ "F23", SDLK_F23 },
	{ "F24", SDLK_F24 }
	};

int keys_t::keySymFromName(const std::string & name)
{
	for(uint n = 0; n<sizeof(Keys) / sizeof(keys_t); n++)
		if( strcasecmp(Keys[n].text, name.c_str()) == 0 )
			return Keys[n].value;
			
	return 0;
}

	
	
#ifdef DEDICATED_ONLY

void updateAxisStates() {}
void CInput::InitJoysticksTemp() {}
void CInput::UnInitJoysticksTemp() {}
void CInput::OnControllerAdded(int) {}
void CInput::OnControllerRemoved(int) {}

#else

#define HAVE_JOYSTICK

// Analog stick deadzone (axes range -32768..32767)
static const int JOY_DEADZONE = 8000;
// Trigger threshold: triggers range 0..32767; fire at 75% travel
static const int JOY_TRIGGER_THRESHOLD = 24576;

// Binding table — maps the per-pad suffix to JOY_* flag and SDL constant.
// Config strings use the form "j<N>_<suffix>" where N is 1-based pad index
// (e.g. "j1_button_south", "j5_lefty_up"). Pad count is unbounded; the N is
// parsed at Setup() time and the suffix is matched against this table.
// Text names use SDL's naming: SDL_GameControllerGetStringForButton/Axis
// For axes:    sdl_index = SDL_GameControllerAxis
// For buttons: sdl_index = SDL_GameControllerButton
joystick_t Joysticks[] = {
	{ "lefty_up",      JOY_UP,             SDL_CONTROLLER_AXIS_LEFTY},
	{ "lefty_down",    JOY_DOWN,           SDL_CONTROLLER_AXIS_LEFTY},
	{ "leftx_left",    JOY_LEFT,           SDL_CONTROLLER_AXIS_LEFTX},
	{ "leftx_right",   JOY_RIGHT,          SDL_CONTROLLER_AXIS_LEFTX},
	{ "button_south",  JOY_BUTTON,         SDL_CONTROLLER_BUTTON_A},
	{ "button_east",   JOY_BUTTON,         SDL_CONTROLLER_BUTTON_B},
	{ "button_west",   JOY_BUTTON,         SDL_CONTROLLER_BUTTON_X},
	{ "button_north",  JOY_BUTTON,         SDL_CONTROLLER_BUTTON_Y},
	{ "back",          JOY_BUTTON,         SDL_CONTROLLER_BUTTON_BACK},
	{ "guide",         JOY_BUTTON,         SDL_CONTROLLER_BUTTON_GUIDE},
	{ "start",         JOY_BUTTON,         SDL_CONTROLLER_BUTTON_START},
	{ "leftstick",     JOY_BUTTON,         SDL_CONTROLLER_BUTTON_LEFTSTICK},
	{ "rightstick",    JOY_BUTTON,         SDL_CONTROLLER_BUTTON_RIGHTSTICK},
	{ "lshoulder",     JOY_BUTTON,         SDL_CONTROLLER_BUTTON_LEFTSHOULDER},
	{ "rshoulder",     JOY_BUTTON,         SDL_CONTROLLER_BUTTON_RIGHTSHOULDER},
	{ "rightx_left",   JOY_TURN_LEFT,      SDL_CONTROLLER_AXIS_RIGHTX},
	{ "rightx_right",  JOY_TURN_RIGHT,     SDL_CONTROLLER_AXIS_RIGHTX},
	{ "righty_up",     JOY_THROTTLE_LEFT,  SDL_CONTROLLER_AXIS_RIGHTY},
	{ "righty_down",   JOY_THROTTLE_RIGHT, SDL_CONTROLLER_AXIS_RIGHTY},
	{ "d_up",          JOY_HAT,            SDL_CONTROLLER_BUTTON_DPAD_UP},
	{ "d_down",        JOY_HAT,            SDL_CONTROLLER_BUTTON_DPAD_DOWN},
	{ "d_left",        JOY_HAT,            SDL_CONTROLLER_BUTTON_DPAD_LEFT},
	{ "d_right",       JOY_HAT,            SDL_CONTROLLER_BUTTON_DPAD_RIGHT},
	{ "triggerleft",   JOY_TRIGGER_LEFT,   SDL_CONTROLLER_AXIS_TRIGGERLEFT},
	{ "triggerright",  JOY_TRIGGER_RIGHT,  SDL_CONTROLLER_AXIS_TRIGGERRIGHT},
};

// Pad slots grow on demand; closed pads remain as NULL holes so existing
// jN_* bindings keep referring to the same physical position.
static std::vector<SDL_GameController*> controllers;
static std::vector<bool> controllers_inited_temp;

static SDL_GameController* getController(int idx) {
	if(idx < 0 || (size_t)idx >= controllers.size()) return NULL;
	return controllers[idx];
}

static void ensureControllerSlot(int idx) {
	if(idx < 0) return;
	if((size_t)idx >= controllers.size()) {
		controllers.resize(idx + 1, NULL);
		controllers_inited_temp.resize(idx + 1, false);
	}
}

// Called each frame by the event loop — nothing to snapshot with the GameController API
void updateAxisStates() {}

static int getControllerValue(int flag, int sdl_index, int idx)
{
	SDL_GameController *gc = getController(idx);
	if(!gc) return 0;

	switch(flag) {
		case JOY_UP:
		case JOY_DOWN:
		case JOY_LEFT:
		case JOY_RIGHT:
		case JOY_TURN_LEFT:
		case JOY_TURN_RIGHT:
		case JOY_THROTTLE_LEFT:
		case JOY_THROTTLE_RIGHT:
		case JOY_TRIGGER_LEFT:
		case JOY_TRIGGER_RIGHT:
			return SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)sdl_index);
		case JOY_BUTTON:
		case JOY_HAT:
			return SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)sdl_index);
		default:
			warnings << "getControllerValue: unknown flag" << endl;
	}
	return 0;
}

static bool checkControllerState(int flag, int sdl_index, int idx)
{
	if(!bJoystickSupport) return false;
	SDL_GameController *gc = getController(idx);
	if(!gc) return false;

	switch(flag) {
		case JOY_UP:
		case JOY_LEFT:
		case JOY_TURN_LEFT:
		case JOY_THROTTLE_LEFT:
			return SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)sdl_index) < -JOY_DEADZONE;
		case JOY_DOWN:
		case JOY_RIGHT:
		case JOY_TURN_RIGHT:
		case JOY_THROTTLE_RIGHT:
			return SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)sdl_index) >  JOY_DEADZONE;
		case JOY_TRIGGER_LEFT:
		case JOY_TRIGGER_RIGHT:
			return SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)sdl_index) > JOY_TRIGGER_THRESHOLD;
		case JOY_BUTTON:
		case JOY_HAT:
			return SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)sdl_index) != 0;
		default:
			warnings << "checkControllerState: unknown flag" << endl;
	}
	return false;
}

static void initController(int i, bool isTemp) {
	if(i < 0) return;
	if(!bJoystickSupport) return;
	ensureControllerSlot(i);
	if(controllers[i] != NULL) return;
	if(SDL_NumJoysticks() <= i) return;

	const char* name = SDL_GameControllerNameForIndex(i);
	notes << "opening controller " << i << " (\"" << (name ? name : "unknown") << "\")" << endl;

	if(!SDL_IsGameController(i)) {
		warnings << "  Device is not a recognised SDL GameController — skipping" << endl;
		return;
	}

	controllers[i] = SDL_GameControllerOpen(i);
	if(!controllers[i]) {
		warnings << "  Could not open game controller: " << SDL_GetError() << endl;
		return;
	}

	SDL_Joystick* joy = SDL_GameControllerGetJoystick(controllers[i]);
	notes << "  Axes: "    << SDL_JoystickNumAxes(joy)    << endl;
	notes << "  Buttons: " << SDL_JoystickNumButtons(joy) << endl;
	notes << "  Hats: "    << SDL_JoystickNumHats(joy)    << endl;

	if(isTemp) controllers_inited_temp[i] = true;

	// Small delay needed for the controller to initialise correctly (SDL bug workaround)
	SDL_Delay(40);
	SDL_GameControllerUpdate();
}

void CInput::InitJoysticksTemp() {
	if(!bJoystickSupport) return;
	const int n = SDL_NumJoysticks();
	notes << "Initing controllers temporary..." << endl;
	notes << "Available joysticks: " << n << endl;
	for(int i = 0; i < n; ++i) initController(i, true);
}

static void uninitTempController(int i) {
	if(i < 0 || (size_t)i >= controllers.size()) return;
	if(controllers_inited_temp[i]) {
		notes << "Uninit temporary controller " << i << endl;
		SDL_GameControllerClose(controllers[i]);
		controllers[i] = NULL;
		controllers_inited_temp[i] = false;
	}
}

void CInput::UnInitJoysticksTemp() {
	for(size_t i = 0; i < controllers.size(); ++i) uninitTempController((int)i);
}

void CInput::OnControllerAdded(int deviceIndex) {
	if(!bJoystickSupport) return;
	// SDL fires SDL_CONTROLLERDEVICEADDED for every controller present
	// during SDL_INIT_GAMECONTROLLER (not only true hot-plug events).
	// Skip if this device is already open in some slot — otherwise the
	// boot-time scan in InitJoysticksTemp() and this handler will both
	// open the same physical pad and we'll bind it to two slots.
	SDL_JoystickID newId = SDL_JoystickGetDeviceInstanceID(deviceIndex);
	if(newId >= 0) {
		for(size_t j = 0; j < controllers.size(); ++j) {
			if(!controllers[j]) continue;
			SDL_Joystick* joy = SDL_GameControllerGetJoystick(controllers[j]);
			if(joy && SDL_JoystickInstanceID(joy) == newId) return;
		}
	}
	if(!SDL_IsGameController(deviceIndex)) {
		warnings << "OnControllerAdded: device " << deviceIndex
		         << " is not a recognised SDL GameController — skipping" << endl;
		return;
	}
	// Fill the first free slot, or append a new one. Slots freed by
	// OnControllerRemoved stay as NULL holes so existing bindings keep
	// pointing at the same physical position.
	int slot = -1;
	for(size_t i = 0; i < controllers.size(); ++i) {
		if(controllers[i] == NULL) { slot = (int)i; break; }
	}
	if(slot < 0) {
		slot = (int)controllers.size();
		ensureControllerSlot(slot);
	}
	const char* name = SDL_GameControllerNameForIndex(deviceIndex);
	notes << "OnControllerAdded: opening device " << deviceIndex
	      << " (\"" << (name ? name : "unknown") << "\") in slot " << slot << endl;
	controllers[slot] = SDL_GameControllerOpen(deviceIndex);
	if(!controllers[slot]) {
		warnings << "  Could not open game controller: " << SDL_GetError() << endl;
		return;
	}
	SDL_Joystick* joy = SDL_GameControllerGetJoystick(controllers[slot]);
	notes << "  Axes: "    << SDL_JoystickNumAxes(joy)    << endl;
	notes << "  Buttons: " << SDL_JoystickNumButtons(joy) << endl;
	notes << "  Hats: "    << SDL_JoystickNumHats(joy)    << endl;
	// Hot-plugged controllers persist past the menu's temp-init
	// teardown — leave controllers_inited_temp[slot] false.
	SDL_GameControllerUpdate();
}

void CInput::OnControllerRemoved(int instanceId) {
	if(!bJoystickSupport) return;
	for(size_t i = 0; i < controllers.size(); ++i) {
		if(!controllers[i]) continue;
		SDL_Joystick* joy = SDL_GameControllerGetJoystick(controllers[i]);
		if(!joy) continue;
		if(SDL_JoystickInstanceID(joy) != (SDL_JoystickID)instanceId) continue;
		notes << "OnControllerRemoved: closing slot " << i
		      << " (instance " << instanceId << ")" << endl;
		SDL_GameControllerClose(controllers[i]);
		controllers[i] = NULL;
		controllers_inited_temp[i] = false;
		return;
	}
}

#endif // !DEDICATED_ONLY






CInput::CInput() {
	Type = INP_NOTUSED;
	Data = 0;
	SdlIndex = 0;
	JoystickIndex = 0;
	resetEachFrame = true;
	bDown = false;
	reset();

	RegisterCInput(this);
}

CInput::~CInput() {
	UnregisterCInput(this);
}



///////////////////
// Waits for any input (used in a loop)
int CInput::Wait(std::string& strText)
{
	mouse_t *Mouse = GetMouse();
	keyboard_t *kb = GetKeyboard();

	// First check the mouse
	for(uint n = 1; n <= MAX_MOUSEBUTTONS; n++) {
		uint i = n;
		if(Mouse->Up & SDL_BUTTON(n)) {
			// Swap rmb id wih mmb (mouse buttons)
			switch (n)  {
			case 2: i=3; break;
			case 3: i=2; break;
			}
			strText = "ms"+itoa(i);
			return true;
		}
	}

	// Keyboard
	for(int i = 0; i < kb->queueLength; ++i) {
		if(kb->keyQueue[i].down) continue;
		if(kb->keyQueue[i].sym == 0) continue;

		// Skip bare modifier key releases — they are not useful as standalone bindings
		// when combined with other modifiers (e.g. releasing Alt after Alt+Enter).
		// Pure modifier presses (Alt alone) still work since the state will show no other mods.
		SDL_Keycode sym = kb->keyQueue[i].sym;
		bool isModifierKey = (sym == SDLK_LALT || sym == SDLK_RALT ||
		                      sym == SDLK_LCTRL || sym == SDLK_RCTRL ||
		                      sym == SDLK_LSHIFT || sym == SDLK_RSHIFT ||
		                      sym == SDLK_LGUI || sym == SDLK_RGUI);
		const ModifiersState& evMods = kb->keyQueue[i].state;
		bool anyModHeld = evMods.bAlt || evMods.bCtrl || evMods.bShift || evMods.bGui;
		if(isModifierKey && anyModHeld)
			continue;

		// Build modifier prefix string from the event's modifier state
		std::string modPrefix;
		if(evMods.bAlt)   modPrefix += "alt+";
		if(evMods.bCtrl)  modPrefix += "ctrl+";
		if(evMods.bShift) modPrefix += "shift+";
		if(evMods.bGui)   modPrefix += "gui+";

		for(uint n = 0; n<sizeof(Keys) / sizeof(keys_t); n++) {
			if(kb->keyQueue[i].sym == Keys[n].value) {
#ifdef WIN32
				// TODO: does this hack also work for keyup?
				// TODO: remove this hack here
				// Workaround for right alt key which is reported as LCTRL + RALT on windib driver
				if (i+1 < kb->queueLength && kb->keyQueue[i].sym == SDLK_LCTRL)  {
					if (kb->keyQueue[i+1].sym == SDLK_RALT)  {
						strText = "ralt";
						return true;
					}
				}
#endif

				strText = modPrefix + Keys[n].text;
				return true;
			}
		}

		// Our description is not enough, let's call SDL for help
		// We use SDL only for the left unknown keys to stay backward and forward compatible.
		if (kb->keyQueue[i].sym != SDLK_ESCAPE)  {
			strText = modPrefix + SDL_GetKeyName(kb->keyQueue[i].sym);
			return true;
		}
	}

#ifdef HAVE_JOYSTICK
	for(size_t idx = 0; idx < controllers.size(); ++idx) {
		if(!controllers[idx]) continue;
		for(uint n = 0; n < sizeof(Joysticks) / sizeof(joystick_t); n++) {
			if(checkControllerState(Joysticks[n].value, Joysticks[n].sdl_index, (int)idx)) {
				strText = "j" + itoa((int)idx + 1) + "_" + Joysticks[n].text;
				return true;
			}
		}
	}
#endif
	
	return false;
}


///////////////////
// Setup
int CInput::Setup(const std::string& string)
{
	m_EventName = string;
	resetEachFrame = true;
	m_modifiers.clear();

	// Parse optional modifier prefixes: "alt+", "ctrl+", "shift+"
	std::string remaining = string;
	bool foundModifier = true;
	while(foundModifier && remaining.size() >= 4) {
		foundModifier = false;
		std::string lower = remaining.substr(0, 6);
		for(char& c : lower) c = tolower(c);
		if(lower.substr(0, 4) == "alt+") {
			m_modifiers.bAlt = true;
			remaining = remaining.substr(4);
			foundModifier = true;
		} else if(lower.substr(0, 5) == "ctrl+") {
			m_modifiers.bCtrl = true;
			remaining = remaining.substr(5);
			foundModifier = true;
		} else if(lower.substr(0, 6) == "shift+") {
			m_modifiers.bShift = true;
			remaining = remaining.substr(6);
			foundModifier = true;
		} else if(lower.substr(0, 4) == "gui+") {
			m_modifiers.bGui = true;
			remaining = remaining.substr(4);
			foundModifier = true;
		}
	}

	// Check if it's a mouse
	if(remaining.substr(0,2) == "ms") {
		Type = INP_MOUSE;
		Data = atoi(remaining.substr(2).c_str());
		if( Data == 3 ) Data = 2;
		else if( Data == 2 ) Data = 3;
		if(Data < 1 || Data > MAX_MOUSEBUTTONS) {
			warnings << "CInput::Setup " << string << ": mouse button index must be between 1 and " << MAX_MOUSEBUTTONS << endl;
			// those might be good fallbacks
			if(Data == 0) Data = 1;
			else Data = MAX_MOUSEBUTTONS;
		}
		return true;
	}
	
	{
		SDL_Keycode key = SDL_GetKeyFromName(remaining.c_str());
		if(key != SDLK_UNKNOWN) {
			Type = INP_KEYBOARD;
			Data = key;
			return true;
		}
	}

#ifdef HAVE_JOYSTICK
	// Joystick binding: "j<N>_<suffix>" — N is the 1-based pad index, any N.
	if(remaining.size() >= 3 && remaining[0] == 'j' && remaining[1] >= '1' && remaining[1] <= '9') {
		size_t digitsEnd = 1;
		while(digitsEnd < remaining.size() && remaining[digitsEnd] >= '0' && remaining[digitsEnd] <= '9')
			++digitsEnd;
		if(digitsEnd < remaining.size() && remaining[digitsEnd] == '_') {
			int padNumber = atoi(remaining.substr(1, digitsEnd - 1).c_str());
			int padIndex = padNumber - 1;
			Type = INP_JOYSTICK;
			JoystickIndex = padIndex;
			Data = 0;

			// Pad not connected — leave the binding unusable (Data stays 0).
			if(SDL_NumJoysticks() <= padIndex) return false;

			// Open the controller if it hasn't been already opened
			initController(padIndex, false);

			std::string suffix = remaining.substr(digitsEnd + 1);
			for(uint32_t n=0; n<sizeof(Joysticks) / sizeof(joystick_t); n++) {
				if(strcasecmp(Joysticks[n].text, suffix.c_str()) == 0) {
					Data = Joysticks[n].value;
					SdlIndex = Joysticks[n].sdl_index;
					return true;
				}
			}
		}
	}
#endif // HAVE_JOYSTICK


	// Must be a keyboard character
	Type = INP_KEYBOARD;
	Data = 0;

	// Go through the key list checking with piece of text it was
	for(uint32_t n=0; n < sizeof(Keys) / sizeof(keys_t); n++) {
		if(strcasecmp(Keys[n].text, remaining.c_str()) == 0) {
			Data = Keys[n].value;
			return true;
		}
	}

	return false;
}

////////////////////
// Returns the "force" value for a joystick axis
int CInput::getJoystickValue()
{
#ifdef HAVE_JOYSTICK
	if(Type == INP_JOYSTICK)
		return getControllerValue(Data, SdlIndex, JoystickIndex);
#endif
	return 0;
}


////////////////////
// Returns true if this joystick is a throttle
bool CInput::isJoystickThrottle()
{
#ifdef HAVE_JOYSTICK
	if (Type == INP_JOYSTICK)
		return (Data == JOY_THROTTLE_LEFT) || (Data == JOY_THROTTLE_RIGHT);
#endif
	return false;
}


///////////////////
// Returns if the input has just been released
bool CInput::isUp()
{

	switch(Type) {

		// Keyboard
		case INP_KEYBOARD:
			if(nUp > 0)
				return true;
			break;

		// Mouse
		case INP_MOUSE:
			if(GetMouse()->Up & SDL_BUTTON(Data))
				return true;
			break;

#ifdef HAVE_JOYSTICK
		// Joystick
		case INP_JOYSTICK:
			return nUp > 0;
#endif
	}

	return false;
}


///////////////////
// Returns if the input is down
bool CInput::isDown() const
{
	switch(Type) {

		// Keyboard
		case INP_KEYBOARD:
			if(wasDown()) return true; // in this case, we want to return true here to get it recognised at least once
			return bDown;

		// Mouse
		case INP_MOUSE:
			if(GetMouse()->Button & SDL_BUTTON(Data))
				return true;
			break;

#ifdef HAVE_JOYSTICK
		// Joystick
		case INP_JOYSTICK:
			return checkControllerState(Data, SdlIndex, JoystickIndex);
#endif
	}

	return false;
}


///////////////////
// Returns if the input was pushed down once
bool CInput::isDownOnce()
{
	return nDownOnce != 0;
}

int CInput::wasDown_withoutRepeats() const {
	return nDownOnce;
}

// goes through the event-signals and searches for the event
int CInput::wasDown() const {
	int counter = 0;

	switch(Type) {
	case INP_KEYBOARD:
		counter = nDown;
		break;

	case INP_MOUSE:
		// TODO: to make this possible, we need to go extend HandleNextEvent to save the mouse events
		counter = nDownOnce; // no other way at the moment
		break;

#ifdef HAVE_JOYSTICK
	case INP_JOYSTICK:
		counter = nDownOnce; // no other way at the moment
		break;
#endif
	}

	return counter;
}

// goes through the event-signals and searches for the event
int CInput::wasUp() {
	int counter = 0;

	switch(Type) {
	case INP_KEYBOARD:
		counter = nUp;
		break;

	case INP_MOUSE:
		// TODO: to make this possible, we need to go extend HandleNextEvent to save the mouse events
		counter = 0;  // no other way at the moment
		break;

#ifdef HAVE_JOYSTICK
	case INP_JOYSTICK:
		counter = 0; // no other way at the moment
		break;
#endif
	}

	return counter;
}

void CInput::reset() {
	nDown = nDownOnce = nUp = 0;
}
