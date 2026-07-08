#include <SDL_thread.h>
#include "StdinCLISupport.h"
#include "ThreadPool.h"
#include "Mutex.h"
#include "OLXCommand.h"
#include "Debug.h"
#include "InputEvents.h"
#include "Autocompletion.h"
#include "FindFile.h"

static const char* HistoryFilename = "stdin-cli-history.txt";

static bool hasStdinCLISupport = false;

bool stdinCLIActive() { return hasStdinCLISupport; }

#ifndef HAVE_LINENOISE
Result initStdinCLISupport() {
	return "not implemented";
}

void quitStdinCLISupport() {}
void activateStdinCLIHistory() {}
void stdinCLIRestoreTerminal() {}

StdinCLI_StdoutScope::StdinCLI_StdoutScope() {}
StdinCLI_StdoutScope::~StdinCLI_StdoutScope() {}
#else

#include <unistd.h>
#include <sys/select.h>
#include <linenoise.h>

struct StdinCmdLineIntf : CmdLineIntf {
	void pushReturnArg(const std::string& str) {
		notes << "CLI return: " << str << endl;
	}

	void finalizeReturn() {
		notes << "CLI return." << endl;
	}

	void writeMsg(const std::string& str, CmdLineMsgType type) {
		// TODO: handle type
		hints << "CLI: " << str << endl;
	}

};
StdinCmdLineIntf stdinCLI;

static Mutex stdoutMutex;
static bool stdoutInRawMode = false;
static bool quit = false;
static bool historySupport = false;

struct LinenoiseEnvCustom : LinenoiseEnv {
	LinenoiseEnvCustom() {
		prompt = "OLX> ";
	}
	virtual ssize_t read(void* d, size_t nbyte) {
		Mutex::ScopedUnlock unlock(stdoutMutex);

		while(true) {
			struct timeval time;
			time.tv_sec = 0;
			time.tv_usec = 10 * 1000; // 10ms
			fd_set set;
			FD_ZERO(&set);
			FD_SET(fd, &set);
			int r = select(fd+1, &set, NULL, NULL, &time);
			{
				Mutex::ScopedLock lock(stdoutMutex);
				if(r > 0)
					return LinenoiseEnv::read(d, nbyte);
				if(r < 0)
					return -1;
				if(quit) return 0;
			}
		}
		return -1;
	}
};

LinenoiseEnvCustom linenoiseEnv;

StdinCLI_StdoutScope::StdinCLI_StdoutScope() {
	stdoutMutex.lock();
	if(stdoutInRawMode) {
		linenoiseEnv.eraseLine();
		linenoiseDisableRawMode(linenoiseEnv.fd);
	}
}

StdinCLI_StdoutScope::~StdinCLI_StdoutScope() {
	if(stdoutInRawMode) {
		linenoiseEnableRawMode(linenoiseEnv.fd);
		linenoiseEnv.refreshLine();
	}
	stdoutMutex.unlock();
}

bool linenoiseCompletionCallbackFunc(const std::string& buf, LinenoiseCompletions* lc) {
	Mutex::ScopedUnlock unlock(stdoutMutex);

	// Auto-complete
	AutocompletionInfo autoCompleteInfo;
	AutoComplete(linenoiseEnv.buf, linenoiseEnv.pos, stdinCLI, autoCompleteInfo);
	bool fail = false;
	AutocompletionInfo::InputState oldState(linenoiseEnv.buf, linenoiseEnv.pos);
	AutocompletionInfo::InputState newState;
	autoCompleteInfo.popWait(oldState, newState, fail);
	if(!fail) {
		Mutex::ScopedLock lock(stdoutMutex);
		linenoiseEnv.buf = newState.text;
		linenoiseEnv.pos = newState.pos;
		linenoiseEnv.refreshLine();
		return true;
	}

	return false;
}

int handleStdin(void*) {
	while(!linenoiseEnv.hadReadError) {
		bool useHistory = false;
		std::string buf;
		{
			Mutex::ScopedLock lock(stdoutMutex);
			if(quit) break;
			stdoutInRawMode = true;
			buf = linenoiseEnv.getNextInput();
			stdoutInRawMode = false;
			useHistory = historySupport;
		}
		if(!buf.empty()) {
			Execute( &stdinCLI, buf );
			WakeupIfNeeded();
			if(useHistory)
				linenoiseHistorySave(GetWriteFullFileName(HistoryFilename));
		}
	}
	if(!quit)
		notes << "CLI read error. quit" << endl;
	return 0;
}

static SDL_Thread* stdinHandleThread = NULL;

Result initStdinCLISupport() {
	if(!isatty(STDIN_FILENO))
		return "stdin is not a terminal type device";

	if(linenoiseIsUnsupportedTerm())
		return "linenoise does not support this type of terminal";

	linenoiseSetCompletionCallback(linenoiseCompletionCallbackFunc);

	stdinHandleThread = SDL_CreateThread(handleStdin, "stdin", NULL);
	if(stdinHandleThread == NULL)
		return "couldn't create thread";

	hasStdinCLISupport = true;
	return true;
}

void quitStdinCLISupport() {
	if(!hasStdinCLISupport) return;
	if(!stdinHandleThread) return;

	notes << "wait for StdinCLI quit" << endl;
	{
		Mutex::ScopedLock lock(stdoutMutex);
		quit = true;
	}
	SDL_WaitThread(stdinHandleThread, NULL);
	stdinHandleThread = NULL;
	hasStdinCLISupport = false;

	// latch raw off now; teeStdoutQuit does the final restore once its thread is gone
	linenoiseRestoreTerminal(linenoiseEnv.fd);
}

void activateStdinCLIHistory() {
	if(!hasStdinCLISupport) return;

	Mutex::ScopedLock lock(stdoutMutex);
	linenoiseHistoryLoad(GetFullFileName(HistoryFilename));
	historySupport = true;
}

void stdinCLIRestoreTerminal() {
	linenoiseRestoreTerminal(linenoiseEnv.fd);
}

#endif // HAVE_LINENOISE
