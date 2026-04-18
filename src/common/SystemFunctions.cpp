/*
 *  SystemFunctions.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 01.12.09.
 *  code under LGPL
 *
 */



#include <iomanip>
#include <time.h>
#include <SDL.h>
#define Font Font_Xlib // Hack to prevent name clash in precompiled header
#include <SDL_syswm.h>
#undef Font
#include <cstdlib>
#include <sstream>
#include <cstring>

#if defined(__APPLE__)
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <sys/sysctl.h>
#include <mach/mach_traps.h>
#elif defined(WIN32) || defined(WIN64)
#include <windows.h>
#else
#include <cstdio>
#include <unistd.h>
#endif

#ifdef __FREEBSD__
#include <sys/sysctl.h>
#include <vm/vm_param.h>
#include <sys/vmmeter.h>
#endif

#ifndef WIN32
#include <sys/types.h>
#include <sys/resource.h>
#include <unistd.h>
#include <signal.h>
#endif

#include "StringUtils.h"
#include "ThreadPool.h"
#include "util/macros.h"

#if !defined(WIN32) || defined(HAVE_PTHREAD)
#include <pthread.h>
#ifndef HAVE_PTHREAD
#define HAVE_PTHREAD
#endif

#ifndef HAVE_PTHREAD_NAME
// pthread_setname_np only available since MacOSX SDK 10.6
#if defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_6
#define HAVE_PTHREAD_NAME
// glibc 2.12 introduced this
#elif defined(__GLIBC_PREREQ)
#	if __GLIBC_PREREQ(2,12)
#		define HAVE_PTHREAD_NAME
#	endif
#endif
#endif

#endif



#ifdef THREADNAME_VIA_SIGUSR1
static void setCurThreadName__signalraiser(const char* name) {
#ifndef _MSC_VER
	// this signal is only for debuggers, we should ignore it
	signal( SIGUSR1, SIG_IGN );

	// KDevelop will catch this, read the 'name' parameter and continue execution
	raise( SIGUSR1 );
#endif
}
#endif


ThreadId getCurrentThreadId() {
#ifdef HAVE_PTHREAD
	return (ThreadId) pthread_self();
#else
	// SDL_ThreadID returns a Uint32.
	// On 64bit systems, this might not be enough information about the current thread.
	// Win64 thread HANDLE is 64bit, aswell as pthread_t.
	return (ThreadId) SDL_ThreadID();
#endif
}

//////////////////
// Gives a name to the thread
// Code taken from http://www.codeproject.com/KB/threads/Name_threads_in_debugger.aspx
void setCurThreadName(const std::string& name)
{
#ifdef _MSC_VER // Currently only for MSVC, haven't found a Windows-general way (I doubt there is one)
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType; // Must be 0x1000.
		LPCSTR szName; // Pointer to name (in user addr space).
		DWORD dwThreadID; // Thread ID (-1=caller thread).
		DWORD dwFlags; // Reserved for future use, must be zero.
	} THREADNAME_INFO;
	
	THREADNAME_INFO info;
	{
		info.dwType = 0x1000;
		info.szName = name.c_str();
		info.dwThreadID = (DWORD)-1;
		info.dwFlags = 0;
	}
	
	__try
	{
		// RaiseException's argument array is ULONG_PTR-sized, not
		// DWORD-sized. Equivalent on x86; only x64 needs the cast.
		RaiseException( 0x406D1388 /* MSVC EXCEPTION */, 0, sizeof(info)/sizeof(ULONG_PTR), (const ULONG_PTR*)&info );
	}
	__except (EXCEPTION_CONTINUE_EXECUTION)
	{
	}
#endif

#ifdef THREADNAME_VIA_SIGUSR1
	setCurThreadName__signalraiser(name.c_str());
#endif

#ifdef HAVE_PTHREAD_NAME
// http://stackoverflow.com/questions/2369738/can-i-set-the-name-of-a-thread-in-pthreads-linux/7989973#7989973
#ifdef __APPLE__
	pthread_setname_np(name.c_str());
#else
	pthread_setname_np(pthread_self(), name.c_str());
#endif
#endif
}

std::string getThreadName(ThreadId tid) {
#ifdef HAVE_PTHREAD_NAME
	char buf[128] = "\0";
	if(pthread_getname_np((pthread_t) tid, buf, sizeof(buf)) == 0)
		if(strlen(buf) > 0)
			return buf;
#endif

	if(threadPool) {
		std::map<ThreadId, std::string> threads;
		threadPool->getAllWorkingThreads(threads);
		foreach(t, threads) {
			if(t->first == tid)
				return t->second;
		}
	}

	return "<unknown thread>";
}

std::string getCurThreadName() {
	return getThreadName(getCurrentThreadId());
}

void setCurThreadPriority(float p) {
#ifdef WIN32
	
#elif defined(__APPLE__)
	//int curp = getpriority(PRIO_DARWIN_THREAD, 0); 
	//int newp = p >= 0 ? 0 : 1;
	//notes << "curp:" << curp << ", newp:" << newp << endl;
	//setpriority(PRIO_DARWIN_THREAD, 0, newp);
#endif
}



size_t GetFreeSysMemory() {
#if defined(__APPLE__)
	vm_statistics_data_t page_info;
	vm_size_t pagesize;
	mach_msg_type_number_t count;
	kern_return_t kret;
	
	pagesize = 0;
	kret = host_page_size (mach_host_self(), &pagesize);
	count = HOST_VM_INFO_COUNT;
	
	kret = host_statistics (mach_host_self(), HOST_VM_INFO,(host_info_t)&page_info, &count);
	return page_info.free_count * pagesize;
#elif defined(__WIN64__)
	MEMORYSTATUSEX memStatex;
	memStatex.dwLength = sizeof (memStatex);
	::GlobalMemoryStatusEx (&memStatex);
	return memStatex.ullAvailPhys;
#elif defined(__WIN32__)
	MEMORYSTATUS memStatus;
	memStatus.dwLength = sizeof(MEMORYSTATUS);
	::GlobalMemoryStatus(&memStatus);
	return memStatus.dwAvailPhys;
#elif defined(__SUN__) && defined(_SC_AVPHYS_PAGES)
	return sysconf(_SC_AVPHYS_PAGES) * sysconf(_SC_PAGESIZE);
#elif defined(__FREEBSD__)
	int vm_vmtotal[] = { CTL_VM, VM_METER };
	struct vmtotal vmdata;
	
	size_t len = sizeof(vmdata);
	int result = sysctl(vm_vmtotal, sizeof(vm_vmtotal) / sizeof(*vm_vmtotal), &vmdata, &len, NULL, 0);
	if(result != 0) return 0;
	
	return vmdata.t_free * sysconf(_SC_PAGESIZE);
#elif defined(__linux__)
	
	// get it from /proc/meminfo
	FILE *fp = fopen("/proc/meminfo", "r");
	if ( fp )
	{
		unsigned long freeMem = 0;
		unsigned long buffersMem = 0;
		unsigned long cachedMem = 0;
		struct SearchTerm { const char* name; unsigned long* var; };
		SearchTerm terms[] = { {"MemFree:", &freeMem}, {"Buffers:", &buffersMem}, {"Cached:", &cachedMem} };
		
		char buf[1024];
		int n = fread(buf, sizeof(char), sizeof(buf) - 1, fp);
		buf[sizeof(buf)-1] = '\0';
		if(n > 0) {
			for(unsigned int i = 0; i < sizeof(terms) / sizeof(SearchTerm); ++i) {
				char* p = strstr(buf, terms[i].name);
				if(p) {
					p += strlen(terms[i].name);
					*terms[i].var = strtoul(p, NULL, 10);
				}
			}
		}
		
		fclose(fp);
		// it's written in KB in meminfo
		return ((size_t)freeMem + (size_t)buffersMem + (size_t)cachedMem) * 1024;
	}
	
	return 0;
#else
#warning No GetFreeSysMemory implementation for this arch/sys
	return 50 * 1024 * 1024; // return 50 MB (really randomly made up, but helps for cache system)
#endif
}




std::string GetDateTimeText()
{
	time_t t = time(NULL);
	
	if (t == (time_t)-1)
		return "TIME-ERROR1";
	
	char* timeCstr = ctime(&t);
	if(timeCstr == NULL)
		return "TIME-ERROR2";
	
	std::string timeStr(timeCstr);
	TrimSpaces(timeStr);
	return timeStr;
}

std::string GetDateTimeFilename()
{
	// Add date and server name to screenshot filename
	time_t curtime1 = time(NULL);
	if (curtime1 == (time_t)-1)
		return "TIME-ERROR1";
	struct tm *curtime = localtime(&curtime1);
	if (curtime == NULL)
		return "TIME-ERROR2";
	char filePrefixTime[200] = {0};
	strftime(filePrefixTime, sizeof(filePrefixTime), "%Y%m%d-%H%M", curtime);
	return filePrefixTime;
}


