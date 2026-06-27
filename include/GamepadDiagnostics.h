/////////////////////////////////////////
//
//             OpenLieroX
//
//   code under LGPL
//
/////////////////////////////////////////


// Gamepad diagnostics
//
// Collects information about the bundled SDL game controller mapping
// database (gamecontrollerdb.txt) and the controllers SDL currently
// recognises. Surfaced in the Options -> Controls -> "Gamepad diagnostics"
// sub-tab so users can quickly tell whether the mapping database loaded and
// which pads were picked up.


#ifndef __GAMEPAD_DIAGNOSTICS_H__
#define __GAMEPAD_DIAGNOSTICS_H__

#include <string>
#include <vector>


// Result of trying to load the bundled SDL game controller mapping database.
// Filled in once at startup (see InitializeAuxLib) and read back by the
// diagnostics page.
struct GamepadDbStatus {
	bool		attempted = false;	// did we try to load the bundled db at all?
	bool		fileFound = false;	// was gamecontrollerdb.txt located in the gamedir?
	bool		loaded = false;		// did SDL accept the file (added >= 0)?
	int			mappingsAdded = -1;	// number of mappings SDL added (>= 0 on success)
	size_t		fileSize = 0;		// size of the db file in bytes
	std::string	path;				// resolved full path of the db file
	std::string	checksum;			// CRC32 checksum of the db file (hex), empty if unknown
	std::string	error;				// SDL error message if loading failed
};


// One currently-connected controller as seen by SDL, with the info that
// tells us whether the mapping database recognised it.
struct GamepadControllerInfo {
	int			deviceIndex = -1;		// SDL joystick device index
	std::string	name;					// human-readable name
	std::string	guid;					// joystick GUID string
	bool		isGameController = false;	// true if SDL has a GameController mapping for it
	bool		hasMapping = false;		// true if a mapping string is available
};


// Record the outcome of loading the bundled mapping database. Called from
// InitializeAuxLib; safe to call once.
void SetGamepadDbStatus(const GamepadDbStatus& status);

// Read back the recorded database load status.
const GamepadDbStatus& GetGamepadDbStatus();

// Compute a CRC32 checksum (lower-case hex) of the file at the given path.
// Returns an empty string if the file can't be read.
std::string ComputeFileCrc32(const std::string& path, size_t* outSize = NULL);

// Enumerate the controllers SDL currently sees. This is a live query, so it
// reflects hot-plugged pads each time it is called.
std::vector<GamepadControllerInfo> GetDetectedControllers();


#endif  //  __GAMEPAD_DIAGNOSTICS_H__
