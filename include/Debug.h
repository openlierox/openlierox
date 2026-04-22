/*
 *  Debug.h
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 01.01.09.
 *  code under LGPL
 *
 */

#ifndef __OLXDEBUG_H__
#define __OLXDEBUG_H__

#include <string>
#include <cassert>
#include "CodeAttributes.h"
#include "util/StringConv.h"
#include "util/IPrintOutFct.h"
#include "ThreadPool.h" // ThreadId

// { these function should be safe to be called from everywhere, also from signalhandlers
bool AmIBeingDebugged();
void RaiseDebugger(); // if run in a debugger, it should raise it; if not, it should do nothing
void OlxWriteCoreDump(const char* file_postfix = NULL);
// }

// Returns current sourcefile pos as string
#define FILELINE1 (std::string(__FILE__) + ":" + itoa(__LINE__))
#define FILELINE FILELINE1

// If threadId is 0, assumes the current thread.
// Returns the amount of items written in the buffer-array.
int GetCallstack(ThreadId threadId, void **buffer, int size);

void DumpCallstack(const PrintOutFct& printer, void*const* buffer, int size);
void DumpCallstack(const PrintOutFct& printer);
void DumpCallstackPrintf(void* callpnt = NULL);

void DumpAllThreadsCallstack(const PrintOutFct& printer);

struct SDL_mutex;

struct Logger {
	int minCoutVerb;
	int minIngameConVerb;
	int minCallstackVerb;
	std::string prefix;
	std::string buffer;
	bool lastWasNewline;
	bool useStderr;
	SDL_mutex* mutex;

	Logger(int o, int ingame, int callst, const std::string& p, bool stderrOut = false);
	~Logger();
	void lock(); void unlock();

	struct LockedStreamWrapper {
		Logger* logger;
		LockedStreamWrapper(Logger* l) : logger(l) {}
		LockedStreamWrapper(const LockedStreamWrapper& l) : logger(l.logger) { ((LockedStreamWrapper&)l).logger = NULL; }
		~LockedStreamWrapper() { if(logger) logger->unlock(); }
		Logger* push_back() { Logger* tmp = logger; logger = NULL; return tmp; }
		Logger* push_back_and_unlock() { Logger* tmp = push_back(); tmp->unlock(); return tmp; }
		
		LockedStreamWrapper& operator<<(const std::string& msg) { logger->buffer += msg; return *this; }
		template<typename _T> LockedStreamWrapper& operator<<(_T v) { return operator<<(to_string(v)); }
		Logger& operator<<(Logger& (*__pf)(Logger&)) { return (*__pf)(*push_back_and_unlock()); }
		Logger& flush() { return push_back_and_unlock()->flush(); }
	};
	
	LockedStreamWrapper operator<<(const std::string& msg) { lock(); buffer += msg; return LockedStreamWrapper(this); }
	Logger& operator<<(Logger& (*__pf)(Logger&)) { return (*__pf)(*this); }
	template<typename _T> LockedStreamWrapper operator<<(_T v) { return operator<<(to_string(v)); }
	Logger& flush();
	
	// deprecated, only for easier find/replace with printf
	void operator()(const std::string& str) { (*this) << str; flush(); }
};

INLINE Logger& endl(Logger& __os) { return (__os << "\n").flush(); }
INLINE Logger& flush(Logger& __os) { return __os.flush(); }

struct PrintOnLogger : PrintOutFct {
	Logger& l;
	PrintOnLogger(Logger& _l) : l(_l) {}
	void print(const std::string& str) const { l << str; l.flush(); }
};

extern Logger notes;
extern Logger hints;
extern Logger warnings;
extern Logger errors;

#endif
