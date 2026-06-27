/////////////////////////////////////////
//
//             OpenLieroX
//
//   code under LGPL
//
/////////////////////////////////////////


// Gamepad diagnostics — see GamepadDiagnostics.h


#include <SDL.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost/crc.hpp>

#include "GamepadDiagnostics.h"
#include "LieroX.h"		// bJoystickSupport


// Database load status, filled in once at startup by InitializeAuxLib.
static GamepadDbStatus gGamepadDbStatus;


void SetGamepadDbStatus(const GamepadDbStatus& status)
{
	gGamepadDbStatus = status;
}

const GamepadDbStatus& GetGamepadDbStatus()
{
	return gGamepadDbStatus;
}


std::string ComputeFileCrc32(const std::string& path, size_t* outSize)
{
	if(outSize) *outSize = 0;

	std::ifstream f(path.c_str(), std::ios::binary);
	if(!f.is_open())
		return "";

	boost::crc_32_type crc;
	char buf[16 * 1024];
	size_t total = 0;
	while(f) {
		f.read(buf, sizeof(buf));
		const std::streamsize got = f.gcount();
		if(got <= 0)
			break;
		crc.process_bytes(buf, (size_t)got);
		total += (size_t)got;
	}

	if(outSize) *outSize = total;

	std::ostringstream ss;
	ss << std::hex << std::setw(8) << std::setfill('0') << crc.checksum();
	return ss.str();
}


std::string ComputeBufferCrc32(const void* data, size_t len)
{
	if(!data || len == 0)
		return "";

	boost::crc_32_type crc;
	crc.process_bytes(data, len);

	std::ostringstream ss;
	ss << std::hex << std::setw(8) << std::setfill('0') << crc.checksum();
	return ss.str();
}


std::vector<GamepadControllerInfo> GetDetectedControllers()
{
	std::vector<GamepadControllerInfo> result;

	if(!bJoystickSupport)
		return result;

	const int n = SDL_NumJoysticks();
	for(int i = 0; i < n; ++i) {
		GamepadControllerInfo info;
		info.deviceIndex = i;

		const char* name = SDL_GameControllerNameForIndex(i);
		if(!name) name = SDL_JoystickNameForIndex(i);
		info.name = name ? name : "(unknown)";

		char guidStr[64] = {0};
		SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
		SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
		info.guid = guidStr;

		info.isGameController = SDL_IsGameController(i) == SDL_TRUE;

		// A controller is "known" to the mapping database (bundled or
		// SDL built-in) when a mapping string is available for its GUID.
		char* mapping = SDL_GameControllerMappingForGUID(guid);
		info.hasMapping = (mapping != NULL);
		if(mapping)
			SDL_free(mapping);

		result.push_back(info);
	}

	return result;
}
