/*
 *  Debug_WriteCoreDump.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 06.04.12.
 *  code under LGPL
 *
 */

#include "Debug.h"

// not for MingW as it seems it has some different dbghelp version (or none at all)
#if defined(WIN32) && !defined(__MINGW32__)

#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>

#include "AuxLib.h"
#include "LieroX.h"
#include "CClient.h"
#include "CServer.h"
#include "DedicatedControl.h"
#include "StringUtils.h"
#include "ConversationLogger.h"
#include "CGameMode.h"
#include "game/Mod.h"
#include "game/Level.h"

#define itoa _itoa

void *ReadGameStateForReport(char *buffer, size_t bufsize)
{
	memset(buffer, 0, bufsize);
	__try {
		if (cClient)  {
			strncat(buffer, "Game state:\n", bufsize);
			if (cClient->getStatus() == NET_CONNECTED)  {
				if (game.state == Game::S_Preparing)
					strncat(buffer, "In game, selecting weapons.", bufsize);
				else
					strncat(buffer, "In lobby.", bufsize);
			} else if (cClient->getStatus() == NET_CONNECTING)  {
				strncat(buffer, "Connecting to a server.", bufsize);
			} else if (cClient->getStatus() == NET_DISCONNECTED)  {
				strncat(buffer, "Disconnected.\n", bufsize);
			} else {
				strncat(buffer, "Unknown state.\n", bufsize);
			}
		}
		buffer[bufsize - 1] = '\0';
	} __except (EXCEPTION_EXECUTE_HANDLER)
	{ return buffer; }
	
	return buffer;
}

void *ReadGameInfoForReport(char *buffer, size_t bufsize)
{
	memset(buffer, 0, bufsize);
	if (!tLXOptions || !tLX)
		return buffer;
	char tmp[32];
	try  {
		// Game type
		strncat(buffer, "iGameType = ", bufsize);
		if(game.isLocalGame())
			strncat(buffer, "GME_LOCAL", bufsize);
		if(game.isServer())
			strncat(buffer, "GME_HOST", bufsize);
		if(game.isClient())
			strncat(buffer, "GME_JOIN", bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Game mode
		strncat(buffer, "GameMode = ", bufsize);
		//char tmp[16];
		itoa(gameSettings[FT_GameMode].as<GameModeInfo>()->mode->GeneralGameType(), tmp, 10);
		fix_markend(tmp);
		strncat(buffer, tmp, bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Mod name
		strncat(buffer, "sModName = ", bufsize);
		if (gameSettings[FT_Mod].as<ModInfo>()->name.get().size())
			strncat(buffer, gameSettings[FT_Mod].as<ModInfo>()->name.get().c_str(), bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Map file
		strncat(buffer, "sMapFile = ", bufsize);
		if (gameSettings[FT_Map].as<LevelInfo>()->path.get().size())
			strncat(buffer, gameSettings[FT_Map].as<LevelInfo>()->path.get().c_str(), bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Map name
		strncat(buffer, "sMapName = ", bufsize);
		if (gameSettings[FT_Map].as<LevelInfo>()->name.get().size())
			strncat(buffer, gameSettings[FT_Map].as<LevelInfo>()->name.get().c_str(), bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Mod dir
		strncat(buffer, "sModDir = ", bufsize);
		if (gameSettings[FT_Mod].as<ModInfo>()->path.get().size())
			strncat(buffer, gameSettings[FT_Mod].as<ModInfo>()->path.get().c_str(), bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Loading time
		itoa(gameSettings[FT_LoadingTime], tmp, 10);
		fix_markend(tmp);
		strncat(buffer, "iLoadingTimes = ", bufsize);
		strncat(buffer, tmp, bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Server name
		strncat(buffer, "sServerName = ", bufsize);
		if (tLXOptions->sServerName.size())
			strncat(buffer, tLXOptions->sServerName.c_str(), bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Welcome message
		strncat(buffer, "sWelcomeMessage = ", bufsize);
		if (tLXOptions->sWelcomeMessage.size())
			strncat(buffer, tLXOptions->sWelcomeMessage.c_str(), bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Lives
		itoa(gameSettings[FT_Lives], tmp, 10);
		fix_markend(tmp);
		strncat(buffer, "iLives = ", bufsize);
		strncat(buffer, tmp, bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Max kills
		itoa(gameSettings[FT_KillLimit], tmp, 10);
		fix_markend(tmp);
		strncat(buffer, "iKillLimit = ", bufsize);
		strncat(buffer, tmp, bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Time limit
		itoa((int)((float)gameSettings[FT_TimeLimit] * 10), tmp, 10);
		fix_markend(tmp);
		strncat(buffer, "fTimeLimit = ", bufsize);
		strncat(buffer, tmp, bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Bonuses on
		strncat(buffer, "bBonusesOn = ", bufsize);
		strncat(buffer, gameSettings[FT_Bonuses] ? "true" : "false", bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Bonus names
		strncat(buffer, "bShowBonusName = ", bufsize);
		strncat(buffer, gameSettings[FT_ShowBonusName] ? "true" : "false", bufsize);
		strncat(buffer, "\n", bufsize);
		
		// Number of players
		if (cServer)  {
			itoa(game.worms()->size(), tmp, 10);
			fix_markend(tmp);
			strncat(buffer, "iNumPlayers = ", bufsize);
			strncat(buffer, tmp, bufsize);
			strncat(buffer, "\n", bufsize);
		}
		
		buffer[bufsize - 1] = '\0';
	} catch (...) {
		return buffer;
	}
	return buffer;
}

// This function also used in CrashHandler.cpp
void OlxWriteCoreDump_Win32(const char* fileName, PEXCEPTION_POINTERS pExInfo )
{
	// MSVC-compatible core dump, GDB cannot read it :(
	// Set the exception info for the minidump
	MINIDUMP_EXCEPTION_INFORMATION eInfo;
	eInfo.ThreadId = GetCurrentThreadId();
	eInfo.ExceptionPointers = pExInfo;
	eInfo.ClientPointers = FALSE;
	
	// Set the minidump info
	MINIDUMP_CALLBACK_INFORMATION cbMiniDump;
	cbMiniDump.CallbackRoutine = NULL;
	cbMiniDump.CallbackParam = 0;
	
	// Additional data
	MINIDUMP_USER_STREAM pExtraInfo[3];
	
	// Version info
	char version[64];
	strcpy(version, GetFullGameName());
	pExtraInfo[0].Type = LastReservedStream + 1;
	pExtraInfo[0].BufferSize = sizeof(version);
	pExtraInfo[0].Buffer = (void *)&version[0];
	
	// Current game info
	char game_info[1024];
	pExtraInfo[1].Type = LastReservedStream + 2;
	pExtraInfo[1].BufferSize = sizeof(game_info);
	pExtraInfo[1].Buffer = ReadGameInfoForReport(game_info, sizeof(game_info));
	
	// Current game state
	char game_state[1024];
	pExtraInfo[2].Type = LastReservedStream + 3;
	pExtraInfo[2].BufferSize = sizeof(game_state);
	pExtraInfo[2].Buffer = ReadGameStateForReport(game_state, sizeof(game_state));
	
	MINIDUMP_USER_STREAM_INFORMATION iStreams;
	iStreams.UserStreamCount = sizeof(pExtraInfo)/sizeof(MINIDUMP_USER_STREAM);
	iStreams.UserStreamArray = pExtraInfo;
	
	// Open the file
	HANDLE hFile = CreateFile((LPCSTR)fileName,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
	
	// Write the minidump
	if (hFile)  {
		MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpScanMemory | MiniDumpWithIndirectlyReferencedMemory);
		MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, type, &eInfo,&iStreams,&cbMiniDump);
	}
	
	// Close the file
	CloseHandle(hFile);
}

void OlxWriteCoreDump(const char* fileName) 
{
	OlxWriteCoreDump_Win32(fileName, NULL);
}


// all rest cases, except mingw
#elif !defined(__MINGW32__)

#ifdef GCOREDUMPER
#include <google/coredumper.h>
#endif
#include <climits>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstring>
#include <cstdio>

#if !defined(GCOREDUMPER)
static void GdbWriteCoreDump(const char* fname) {
	// WARNING: this is terribly slow like this
	char gdbparam[PATH_MAX + 200];
	sprintf(gdbparam,
			"attach %i \n"
			"gcore %s \n"
			"detach \n"
			"quit \n",
			getpid(), fname);
	FILE* p = popen("gdb -q", "w");
	if(p) {
		fprintf(p, "%s", gdbparam);
		fflush(p);
		int status = 0; wait(&status);
		pclose(p);
	}
}
#endif

void OlxWriteCoreDump(const char* file_postfix) {
	char corefile[PATH_MAX + 100];
	if(getcwd(corefile, PATH_MAX) == NULL) strcpy(corefile, "");
	strcat(corefile, "/core.OpenLieroX");
	if(file_postfix) { strcat(corefile, "."); strcat(corefile, file_postfix); }
	printf("writing coredump to %s\n", corefile);
	
	printf("dumping core ... "); fflush(0);
#if defined(GCOREDUMPER)
	WriteCoreDump(corefile);
#else
	GdbWriteCoreDump(corefile);
#endif
	printf("ready\n");
}


// MingW case for coredump, maybe later also others
#else

void OlxWriteCoreDump(const char*) {
	errors << "Coredumping not supported in this version" << endl;
}

#endif
