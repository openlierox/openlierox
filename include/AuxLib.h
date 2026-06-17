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
	int m_screenWidth = 640;
	// The width the current frame's content is laid out for. Either the full
	// screenWidth() (local games) or the base menuWidth / 640 (menus and
	// network games, so a wider screen gives no gameplay advantage). When it is
	// narrower than screenWidth() the content is drawn into the left columns
	// and presented horizontally centered, with black bars on the sides.
	// Set per-frame by the menu / gameplay draw. See render() and the mouse
	// handling, and displayScreenWidth()/displayScreenOffsetX() below.
	int m_displayScreenWidth = 640;
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
	
	int screenWidth() const { return m_screenWidth; }
	int screenHeight() const { return 480; }

	// The base view width: what menus are authored for, and the width network
	// games are constrained to. The actual screen may be wider (computed from
	// the desktop aspect ratio); such content stays this wide and is centered.
	// Height always matches screenHeight().
	static const int menuWidth = 640;

	// The effective layout width for the current frame (see m_displayScreenWidth).
	// All screen-width-dependent logic (viewport setup, centering, mouse) should
	// use this instead of screenWidth() so local vs network games behave right.
	void setDisplayScreenWidth(int w) { m_displayScreenWidth = w; }
	int displayScreenWidth() const { return m_displayScreenWidth; }

	// Horizontal offset that centers a displayScreenWidth()-wide view on the
	// screen. Zero when the view already fills the screen (local game, or 4:3).
	int displayScreenOffsetX() const { int o = (m_screenWidth - m_displayScreenWidth) / 2; return o > 0 ? o : 0; }

	// Horizontal offset to center a menuWidth-wide popup (the in-game Esc menu,
	// in-game options, ...) within the current view, so it overlays the
	// unchanged game centered. Zero for a network game (the whole view is
	// already presented centered); (screenWidth-menuWidth)/2 for a full-width
	// local game.
	int popupCenterOffsetX() const { int o = (m_displayScreenWidth - menuWidth) / 2; return o > 0 ? o : 0; }

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
