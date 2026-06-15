/*
 *  MainGlobals.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 01.12.09.
 *  code under LGPL
 *
 */

#include "LieroX.h"
#include "ConversationLogger.h"
#include "IpToCountryDB.h"
#include "FindFile.h"

lierox_t	*tLX = NULL;

bool        bDisableSound = false;
#ifdef DEDICATED_ONLY
bool		bDedicated = true;
bool		bJoystickSupport = false;
#else //DEDICATED_ONLY
bool		bDedicated = false;
bool		bJoystickSupport = true;
#endif //DEDICATED_ONLY
bool		bRestartGameAfterQuit = false;
TStartFunction startFunction = NULL;
void*		startFunctionData = NULL;
ConversationLogger *convoLogger = NULL;


IpToCountryDB *tIpToCountryDB = NULL;





char binaryfilename[2048] = {0};

const char* GetBinaryFilename() { return binaryfilename; }

static void saveSetBinFilename(const std::string& f) {
	if(f.size() < sizeof(binaryfilename) - 1)
		strcpy(binaryfilename, f.c_str());
}

void setBinaryDirAndName(char* argv0) {
	saveSetBinFilename(SystemNativeToUtf8(argv0)); // set system native binary filename
	binary_dir = SystemNativeToUtf8(argv0);
	size_t slashpos = findLastPathSep(binary_dir);
	if(slashpos != std::string::npos)  {
		binary_dir.erase(slashpos);
		
	} else {
		binary_dir = ".";
		
		// We where called somewhere and located in some PATH.
		// Search which one.
#ifdef WIN32
		static const char* PATHENTRYSEPERATOR = ";";
#else
		static const char* PATHENTRYSEPERATOR = ":";
#endif
		std::vector<std::string> paths = explode(getenv("PATH"), PATHENTRYSEPERATOR);
		for(std::vector<std::string>::iterator p = paths.begin(); p != paths.end(); ++p) {
			if(IsFileAvailable(SystemNativeToUtf8(*p) + "/" + binaryfilename, true)) {
				binary_dir = SystemNativeToUtf8(*p);
				saveSetBinFilename(binary_dir + "/" + binaryfilename);
				return;
			}
		}
		
		// Hm, nothing found. Nothing we can do about it...
	}
}

