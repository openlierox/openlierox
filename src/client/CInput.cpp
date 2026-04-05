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

	
	
#if defined(DEDICATED_ONLY) || defined(DISABLE_JOYSTICK)

void updateAxisStates() {}
void CInput::InitJoysticksTemp() {}
void CInput::UnInitJoysticksTemp() {}

#else

#define HAVE_JOYSTICK

// Analog stick deadzone (axes range -32768..32767)
static const int JOY_DEADZONE = 8000;
// Trigger threshold: triggers range 0..32767; fire at 75% travel
static const int JOY_TRIGGER_THRESHOLD = 24576;

// Binding table — maps config string to JOY_* flag and SDL constant (extra).
// Text names use SDL's naming: SDL_GameControllerGetStringForButton/Axis
// For axes:    extra = SDL_GameControllerAxis
// For buttons: extra = SDL_GameControllerButton
joystick_t Joysticks[] = {
	{ "joy1_lefty_up",       JOY_UP,             SDL_CONTROLLER_AXIS_LEFTY,            0},
	{ "joy1_lefty_down",     JOY_DOWN,           SDL_CONTROLLER_AXIS_LEFTY,            0},
	{ "joy1_leftx_left",     JOY_LEFT,           SDL_CONTROLLER_AXIS_LEFTX,            0},
	{ "joy1_leftx_right",    JOY_RIGHT,          SDL_CONTROLLER_AXIS_LEFTX,            0},
	{ "joy1_a",              JOY_BUTTON,         SDL_CONTROLLER_BUTTON_A,              0},
	{ "joy1_b",              JOY_BUTTON,         SDL_CONTROLLER_BUTTON_B,              0},
	{ "joy1_x",              JOY_BUTTON,         SDL_CONTROLLER_BUTTON_X,              0},
	{ "joy1_y",              JOY_BUTTON,         SDL_CONTROLLER_BUTTON_Y,              0},
	{ "joy1_back",           JOY_BUTTON,         SDL_CONTROLLER_BUTTON_BACK,           0},
	{ "joy1_guide",          JOY_BUTTON,         SDL_CONTROLLER_BUTTON_GUIDE,          0},
	{ "joy1_start",          JOY_BUTTON,         SDL_CONTROLLER_BUTTON_START,          0},
	{ "joy1_leftstick",      JOY_BUTTON,         SDL_CONTROLLER_BUTTON_LEFTSTICK,      0},
	{ "joy1_rightstick",     JOY_BUTTON,         SDL_CONTROLLER_BUTTON_RIGHTSTICK,     0},
	{ "joy1_leftshoulder",   JOY_BUTTON,         SDL_CONTROLLER_BUTTON_LEFTSHOULDER,   0},
	{ "joy1_rightshoulder",  JOY_BUTTON,         SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,  0},
	{ "joy1_rightx_left",    JOY_TURN_LEFT,      SDL_CONTROLLER_AXIS_RIGHTX,           0},
	{ "joy1_rightx_right",   JOY_TURN_RIGHT,     SDL_CONTROLLER_AXIS_RIGHTX,           0},
	{ "joy1_righty_up",      JOY_THROTTLE_LEFT,  SDL_CONTROLLER_AXIS_RIGHTY,           0},
	{ "joy1_righty_down",    JOY_THROTTLE_RIGHT, SDL_CONTROLLER_AXIS_RIGHTY,           0},
	{ "joy1_dpup",           JOY_HAT_UP,         SDL_CONTROLLER_BUTTON_DPAD_UP,        0},
	{ "joy1_dpdown",         JOY_HAT_DOWN,       SDL_CONTROLLER_BUTTON_DPAD_DOWN,      0},
	{ "joy1_dpleft",         JOY_HAT_LEFT,       SDL_CONTROLLER_BUTTON_DPAD_LEFT,      0},
	{ "joy1_dpright",        JOY_HAT_RIGHT,      SDL_CONTROLLER_BUTTON_DPAD_RIGHT,     0},
	{ "joy1_triggerleft",    JOY_TRIGGER_LEFT,   SDL_CONTROLLER_AXIS_TRIGGERLEFT,      0},
	{ "joy1_triggerright",   JOY_TRIGGER_RIGHT,  SDL_CONTROLLER_AXIS_TRIGGERRIGHT,     0},
	{ "joy2_lefty_up",       JOY_UP,             SDL_CONTROLLER_AXIS_LEFTY,            0},
	{ "joy2_lefty_down",     JOY_DOWN,           SDL_CONTROLLER_AXIS_LEFTY,            0},
	{ "joy2_leftx_left",     JOY_LEFT,           SDL_CONTROLLER_AXIS_LEFTX,            0},
	{ "joy2_leftx_right",    JOY_RIGHT,          SDL_CONTROLLER_AXIS_LEFTX,            0},
	{ "joy2_a",              JOY_BUTTON,         SDL_CONTROLLER_BUTTON_A,              0},
	{ "joy2_b",              JOY_BUTTON,         SDL_CONTROLLER_BUTTON_B,              0},
	{ "joy2_x",              JOY_BUTTON,         SDL_CONTROLLER_BUTTON_X,              0},
	{ "joy2_y",              JOY_BUTTON,         SDL_CONTROLLER_BUTTON_Y,              0},
	{ "joy2_back",           JOY_BUTTON,         SDL_CONTROLLER_BUTTON_BACK,           0},
	{ "joy2_guide",          JOY_BUTTON,         SDL_CONTROLLER_BUTTON_GUIDE,          0},
	{ "joy2_start",          JOY_BUTTON,         SDL_CONTROLLER_BUTTON_START,          0},
	{ "joy2_leftstick",      JOY_BUTTON,         SDL_CONTROLLER_BUTTON_LEFTSTICK,      0},
	{ "joy2_rightstick",     JOY_BUTTON,         SDL_CONTROLLER_BUTTON_RIGHTSTICK,     0},
	{ "joy2_leftshoulder",   JOY_BUTTON,         SDL_CONTROLLER_BUTTON_LEFTSHOULDER,   0},
	{ "joy2_rightshoulder",  JOY_BUTTON,         SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,  0},
	{ "joy2_rightx_left",    JOY_TURN_LEFT,      SDL_CONTROLLER_AXIS_RIGHTX,           0},
	{ "joy2_rightx_right",   JOY_TURN_RIGHT,     SDL_CONTROLLER_AXIS_RIGHTX,           0},
	{ "joy2_righty_up",      JOY_THROTTLE_LEFT,  SDL_CONTROLLER_AXIS_RIGHTY,           0},
	{ "joy2_righty_down",    JOY_THROTTLE_RIGHT, SDL_CONTROLLER_AXIS_RIGHTY,           0},
	{ "joy2_dpup",           JOY_HAT_UP,         SDL_CONTROLLER_BUTTON_DPAD_UP,        0},
	{ "joy2_dpdown",         JOY_HAT_DOWN,       SDL_CONTROLLER_BUTTON_DPAD_DOWN,      0},
	{ "joy2_dpleft",         JOY_HAT_LEFT,       SDL_CONTROLLER_BUTTON_DPAD_LEFT,      0},
	{ "joy2_dpright",        JOY_HAT_RIGHT,      SDL_CONTROLLER_BUTTON_DPAD_RIGHT,     0},
	{ "joy2_triggerleft",    JOY_TRIGGER_LEFT,   SDL_CONTROLLER_AXIS_TRIGGERLEFT,      0},
	{ "joy2_triggerright",   JOY_TRIGGER_RIGHT,  SDL_CONTROLLER_AXIS_TRIGGERRIGHT,     0},
};

static SDL_GameController* controllers[2] = {NULL, NULL};

// Called each frame by the event loop — nothing to snapshot with the GameController API
void updateAxisStates() {}

static int getControllerValue(int flag, int extra, int idx)
{
	SDL_GameController *gc = controllers[idx];
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
			return SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)extra);
		case JOY_BUTTON:
		case JOY_HAT_UP:
		case JOY_HAT_DOWN:
		case JOY_HAT_LEFT:
		case JOY_HAT_RIGHT:
			return SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)extra);
		default:
			warnings << "getControllerValue: unknown flag" << endl;
	}
	return 0;
}

static bool checkControllerState(int flag, int extra, int idx)
{
	if(!bJoystickSupport) return false;
	SDL_GameController *gc = controllers[idx];
	if(!gc) return false;

	switch(flag) {
		case JOY_UP:
		case JOY_LEFT:
		case JOY_TURN_LEFT:
		case JOY_THROTTLE_LEFT:
			return SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)extra) < -JOY_DEADZONE;
		case JOY_DOWN:
		case JOY_RIGHT:
		case JOY_TURN_RIGHT:
		case JOY_THROTTLE_RIGHT:
			return SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)extra) >  JOY_DEADZONE;
		case JOY_TRIGGER_LEFT:
		case JOY_TRIGGER_RIGHT:
			return SDL_GameControllerGetAxis(gc, (SDL_GameControllerAxis)extra) > JOY_TRIGGER_THRESHOLD;
		case JOY_BUTTON:
		case JOY_HAT_UP:
		case JOY_HAT_DOWN:
		case JOY_HAT_LEFT:
		case JOY_HAT_RIGHT:
			return SDL_GameControllerGetButton(gc, (SDL_GameControllerButton)extra) != 0;
		default:
			warnings << "checkControllerState: unknown flag" << endl;
	}
	return false;
}

static bool controllers_inited_temp[2] = {false, false};

static void initController(int i, bool isTemp) {
	assert(i == 0 || i == 1);
	if(!bJoystickSupport) return;
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
	notes << "Initing controllers temporary..." << endl;
	notes << "Available joysticks: " << SDL_NumJoysticks() << endl;
	initController(0, true);
	initController(1, true);
}

static void uninitTempController(int i) {
	if(controllers_inited_temp[i]) {
		notes << "Uninit temporary controller " << i << endl;
		SDL_GameControllerClose(controllers[i]);
		controllers[i] = NULL;
		controllers_inited_temp[i] = false;
	}
}

void CInput::UnInitJoysticksTemp() {
	uninitTempController(0);
	uninitTempController(1);
}

#endif // !DEDICATED_ONLY






CInput::CInput() {
	Type = INP_NOTUSED;
	Data = 0;
	Extra = 0;
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
	// controller
	// TODO: more controllers
	for(uint n = 0; n < sizeof(Joysticks) / sizeof(joystick_t); n++) {
		int i = Joysticks[n].text[3] - '1'; // at pos 3, there is the number ("joy1_...")

		if(controllers[i] != NULL && checkControllerState(Joysticks[n].value, Joysticks[n].extra, i)) {
			strText = Joysticks[n].text;
			return true;
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
	// Check if it's a joystick #1
	// TODO: allow more joysticks
	if(remaining.substr(0,5) == "joy1_") {
		Type = INP_JOYSTICK1;
		Data = 0;

		// Make sure there is a joystick present
		if(SDL_NumJoysticks()<=0) {
			SetError("Could not open joystick1");
			return false;
		}

		// Open the controller if it hasn't been already opened
		initController(0, false);

		// Go through the joystick list
		for(uint32_t n=0; n<sizeof(Joysticks) / sizeof(joystick_t); n++) {
			if(strcasecmp(Joysticks[n].text, remaining.c_str()) == 0) {
				Data = Joysticks[n].value;
				Extra = Joysticks[n].extra;
				return true;
			}
		}
	}

	// Check if it's a joystick #2
	if(remaining.substr(0,5) == "joy2_") {
		Type = INP_JOYSTICK2;
		Data = 0;

		// Make sure there is a joystick present
		if(SDL_NumJoysticks()<=1)
			return false;

		// Open the controller if it hasn't been already opened
		initController(1, false);

		// Go through the joystick list
		for(uint32_t n=0; n < sizeof(Joysticks) / sizeof(joystick_t); n++) {
			if(strcasecmp(Joysticks[n].text, remaining.c_str()) == 0) {
				Data = Joysticks[n].value;
				Extra = Joysticks[n].extra;
				return true;
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
	switch (Type)  {
	case INP_JOYSTICK1:
		return getControllerValue(Data, Extra, 0);
	case INP_JOYSTICK2:
		return getControllerValue(Data, Extra, 1);
	default:
		return 0;
	}
#endif
	return 0;
}

/////////////////////
// Returns true if this input is a joystick axis
bool CInput::isJoystickAxis()
{
#ifdef HAVE_JOYSTICK
	if (Type == INP_JOYSTICK1 || Type == INP_JOYSTICK2)
		return Data == JOY_UP || Data == JOY_DOWN
		    || Data == JOY_LEFT || Data == JOY_RIGHT
		    || Data == JOY_TURN_LEFT || Data == JOY_TURN_RIGHT
		    || Data == JOY_THROTTLE_LEFT || Data == JOY_THROTTLE_RIGHT;
#endif
	return false;
}

////////////////////
// Returns true if this joystick is a throttle
bool CInput::isJoystickThrottle()
{
#ifdef HAVE_JOYSTICK
	if (Type == INP_JOYSTICK1 || Type == INP_JOYSTICK2)
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
		case INP_JOYSTICK1:
		case INP_JOYSTICK2:
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
		case INP_JOYSTICK1:
			return checkControllerState(Data, Extra, 0);
		case INP_JOYSTICK2:
			return checkControllerState(Data, Extra, 1);
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
	case INP_JOYSTICK1:
	case INP_JOYSTICK2:
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
	case INP_JOYSTICK1:
	case INP_JOYSTICK2:
		counter = 0; // no other way at the moment
		break;
#endif
	}

	return counter;
}

void CInput::reset() {
	nDown = nDownOnce = nUp = 0;
}
