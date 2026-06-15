/*
 *  Debug_DumpCallstack.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 06.04.12.
 *  code under LGPL
 *
 */

#include "Debug.h"
#include "util/macros.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cstdlib>

#ifndef HAVE_EXECINFO
#	if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
		// Bionic libc / Emscripten musl have no <execinfo.h> / backtrace().
#		define HAVE_EXECINFO 0
#	elif defined(__linux__)
#		define HAVE_EXECINFO 1
#	elif defined(__DARWIN_VERS_1050)
#		define HAVE_EXECINFO 1
#	else
#		define HAVE_EXECINFO 0
#	endif
#endif

#if HAVE_EXECINFO

#include <execinfo.h>

#if HASBFD

std::vector<std::string> backtrace_symbols_str(void *const *buffer, int size);

void DumpCallstackPrintf(void* callpnt) {
	void *callstack[128];
	int framesC = GetCallstack(0, callstack, sizeof(callstack));
	printf("backtrace() returned %d addresses\n", framesC);
	if(callpnt != NULL && framesC > 3) callstack[3] = callpnt; // expected to be called from signal handler
	std::vector<std::string> strs = backtrace_symbols_str(callstack, framesC);
	foreach(s, strs)
		printf("%s\n", s->c_str());
}

void DumpCallstack(const PrintOutFct& printer, void*const* buffer, int size) {
	printer.print("DumpCallstack: " + itoa(size) + " addresses:");
	std::vector<std::string> strs = backtrace_symbols_str(buffer, size);
	foreach(s, strs)
		printer.print(" " + *s + "\n");
}

#else

void DumpCallstackPrintf(void* callpnt) {
	void *callstack[128];
	int framesC = GetCallstack(0, callstack, sizeof(callstack));
	printf("backtrace() returned %d addresses\n", framesC);
	if(callpnt != NULL && framesC > 3) callstack[3] = callpnt; // expected to be called from signal handler
	char** strs = backtrace_symbols(callstack, framesC);
	for(int i = 0; i < framesC; ++i) {
		if(strs[i])
			printf("%s\n", strs[i]);
		else
			break;
	}
	free(strs);
}

void DumpCallstack(const PrintOutFct& printer, void*const* buffer, int size) {
	printer.print("DumpCallstack: " + itoa(size) + " addresses:");
	char** strs = backtrace_symbols(buffer, size);
	for(int i = 0; i < size; ++i) {
		if(strs[i])
			printer.print(std::string(" ") + strs[i] + "\n");
		else
			break;
	}
	free(strs);
}

#endif // HASBFD

void DumpCallstack(const PrintOutFct& printer) {
	void *callstack[128];
	int framesC = GetCallstack(0, callstack, sizeof(callstack));
	DumpCallstack(printer, callstack, framesC);
}

#elif defined(WIN32)

#include "StackWalker.h"  // Call Luke Stackwalker for help

// Override the default stackwalker with our own print functions
class PrintStackWalker : public StackWalker  {
private:
	const PrintOutFct *m_print;
	
public:
	PrintStackWalker(const PrintOutFct *fct) : StackWalker(RetrieveVerbose) { m_print = fct; }

	void OnLoadModule(LPCSTR img, LPCSTR mod, DWORD64 baseAddr, DWORD size, DWORD result, LPCSTR symType, LPCSTR pdbName, ULONGLONG fileVersion)
	{}
	
	void OnOutput(LPCSTR szText)  
	{
		if (m_print)
			m_print->print(std::string(szText));
		else
			printf(szText);
		
		StackWalker::OnOutput(szText);
	}
};

void DumpCallstack(const PrintOutFct& printer, void*const* buffer, int size) {
	printf("DumpCallstack for custom callstack not implemented\n");
}

void DumpCallstackPrintf(void* callpnt) 
{
	PrintStackWalker sw(NULL);
	sw.ShowCallstack();
}

void DumpCallstack(const PrintOutFct& printer) {
	PrintStackWalker sw(&printer);
	sw.ShowCallstack();
}

#else

#warning No DumpCallstack implementation for this arch/sys

void DumpCallstack(const PrintOutFct& printer, void*const* buffer, int size) {
	printf("DumpCallstack not implemented\n");
}

void DumpCallstackPrintf(void* callpnt) {
	printf("DumpCallstackPrintf not implemented\n");
}

void DumpCallstack(const PrintOutFct& printer) {
	printf("DumpCallstack not implemented\n");
}

#endif

void DumpAllThreadsCallstack(const PrintOutFct& printer) {
	std::set<ThreadId> threads;
	getAllThreads(threads);

	foreach(t, threads) {
		printer.print("thread 0x" + hex(*t) + " \"" + getThreadName(*t) + "\":\n");
		void *callstack[128];
		int framesC = GetCallstack(*t, callstack, sizeof(callstack));
		DumpCallstack(printer, callstack, framesC);
		printer.print("\n");
	}
}
