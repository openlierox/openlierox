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
#include "ThreadPool.h" // for Action


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

// Re-assert the wanted OS cursor visibility against the window server.
// macOS keeps re-showing the OS cursor on window re-entry and app activation,
// so call this regularly from the main thread.
// It is a cheap no-op on other platforms.
void EnforceSystemMouseCursor();

class VideoPostProcessor {
protected:
	SmartPointer<SDL_Window> m_window;
	SmartPointer<SDL_Renderer> m_renderer;
	SmartPointer<SDL_Texture> m_videoTexture;         // GPU texture: the drawn band, uploaded each frame
	// Double-buffered across the thread boundary:
	// the gameloop draws one while the main thread presents the other (flipBuffers).
	SmartPointer<SDL_Surface> m_videoSurface;         // draw target (front)
	SmartPointer<SDL_Surface> m_videoBufferSurface;   // committed frame (back)
	SmartPointer<SDL_Surface> m_leftGap, m_rightGap;  // precomputed side-gap fills (buildSideGaps)
	SmartPointer<SDL_Texture> m_leftGapTex, m_rightGapTex; // the gaps as GPU textures
	Uint32 m_sideGapKey = 0;    // change key: rebuild the gap surfaces only when it changes
	Uint32 m_sideGapTexKey = 0; // last key uploaded to the gap textures
	int m_screenWidth = 640;
	// Width the current frame is laid out for: screenWidth() (local),
	// or menuWidth (menus/net, composed centered). Set per-frame.
	int m_displayScreenWidth = 640;
	// Snapshot of m_displayScreenWidth taken when a drawn frame is committed
	// (flipBuffers, under the video mutex) and handed to render() via process(),
	// so each frame is presented with the layout width it was actually drawn for.
	// render() runs on the main thread,
	// while the game thread keeps updating m_displayScreenWidth for the next frame,
	// so reading it live in render() races
	// and can center a frame with the wrong offset at menu/game transitions.
	int m_committedDisplayScreenWidth = 640; // flipBuffers writes, process reads (both under mutex)
	int m_renderDisplayScreenWidth = 640;    // main-thread only: process writes, render reads
	static VideoPostProcessor instance;

	// Upload the gap surfaces to the gap textures when they changed. Main thread.
	void updateSideGapTextures();

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

	// Precompute the side-gap fills from a menuWidth-wide theme background.
	void buildSideGaps(const SmartPointer<SDL_Surface>& bg);

	// Hook to draw screen-space overlays (task bar, FPS) on top of the frame.
	// Runs on the main thread from render() (see cOverlayFont).
	typedef void (*ScreenOverlayFn)(SDL_Renderer* renderer);
	static void setScreenOverlay(ScreenOverlayFn fn) { screenOverlay = fn; }
private:
	static ScreenOverlayFn screenOverlay;
public:
	
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
