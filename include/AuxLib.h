/////////////////////////////////////////
//
//   OpenLieroX
//
//   Auxiliary Software class library
//
//   work by JasonB
//   code under LGPL
//   enhanced by Dark Charlie and Albert Zeyer
//
/////////////////////////////////////////


// Auxiliary Library
// Created 12/11/01
// By Jason Boettcher


#ifndef __AUXLIB_H__
#define __AUXLIB_H__

#include <SDL.h>
#include <string>


#include "SmartPointer.h"


// Routines
bool		InitializeAuxLib();
void		ShutdownAuxLib();
bool		SetVideoMode(); // only call from main thread; use doSetVideoModeInMainThread elsewhere

#ifdef WIN32
void		*GetWindowHandle();
#endif

void        FlipScreen();
void		CapFPS();

char*		GetAppPath();


void		PushScreenshot(const std::string& dir, const std::string& data);
void		ProcessScreenshots();

void		SetCrashHandlerReturnPoint(const char* name);
void		OpenLinkInExternBrowser(const std::string& url);
void		setCurThreadName(const std::string& name);
void		setCurThreadPriority(float p); // p in [-1,1], whereby 0 is standard

size_t		GetFreeSysMemory(); // returnes available physical memory in bytes
std::string	GetDateTimeText();	// Returns human-readable time
std::string	GetDateTimeFilename(); // Returns time for use in filename, so newer files will get alpha-sorted last

#ifdef DEBUG
bool		HandleDebugCommand(const std::string& cmd);
#else
#define		HandleDebugCommand(cmd) (false)
#endif


void doVideoFrameInMainThread();
void doSetVideoModeInMainThread();
void doActionInMainThread(Action* act);
void doVppOperation(Action* act);


// Asynchronously enable/disable mouse cursor in window manager, may be called from any thread
// Use this function instead of SDL_ShowCursor()
void EnableSystemMouseCursor(bool enable = true);

class VideoPostProcessor {
protected:
	SmartPointer<SDL_Window> m_window;
	SmartPointer<SDL_Renderer> m_renderer;
	SmartPointer<SDL_Texture> m_videoTexture;
	SmartPointer<SDL_Surface> m_videoSurface;
	SmartPointer<SDL_Surface> m_videoBufferSurface;
	static VideoPostProcessor instance;
	
public:
	// IMPORTANT: Don't call this while anyone else calls/accesses anything else here.
	static void flipBuffers();

	// IMPORTANT: only call these from the main thread
	static void process();
	static void render();
	static void cloneBuffer();

public:
	static VideoPostProcessor* get() { return &instance; }
	static void uninit();

	SDL_Window* sdl_window() const { return m_window.get(); }

	bool initWindow();
	bool resetVideo(); // this is called from SetVideoMode
	
	int screenWidth() { return 640; }
	int screenHeight() { return 480; }

	static const SmartPointer<SDL_Surface>& videoSurface() { return get()->m_videoSurface; }
	static const SmartPointer<SDL_Surface>& videoBufferSurface() { return get()->m_videoBufferSurface; }
	
};


// Subclass
#ifdef WIN32
void		SubclassWindow();
void		UnSubclassWindow();
#endif

#ifdef WIN32
int unsetenv(const char *name);
#endif


#endif  //  __AUXLIB_H__
