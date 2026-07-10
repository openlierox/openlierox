/////////////////////////////////////////
//
//   OpenLieroX
//
//   Auxiliary Software class library
//
//   based on the work of JasonB
//   enhanced by Dark Charlie and Albert Zeyer
//
//   code under LGPL
//
/////////////////////////////////////////


// Auxiliary library
// Created 12/11/01
// By Jason Boettcher

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif


#include <atomic>
#include <iomanip>
#include <time.h>
#include <SDL.h>
#define Font Font_Xlib // Hack to prevent name clash in precompiled header and system libs
#include <SDL_syswm.h>
#undef Font
#include <cstdlib>
#include <sstream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
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
#endif

#include "LieroX.h"
#include "Cache.h"
#include "Debug.h"
#include "AuxLib.h"
#include "Error.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "FindFile.h"
#include "InputEvents.h"
#include "Cursor.h"
#include "StringUtils.h"
#include "sound/SoundsBase.h"
#include "Version.h"
#include "Timer.h"
#include "olx-types.h"
#include "CClient.h"
#include "CServer.h"
#include "Geometry.h"
#include "MainLoop.h"
#include "gusanos/allegro.h"


Null null;	// Used in timer class

static void applySystemMouseCursor();  // apply the wanted OS cursor visibility, main thread only



// TODO: is this the best format? why? comment that.
// Maybe SDL_PIXELFORMAT_ARGB8888 is better? My OpenGL renderer lists that as native format.
SDL_PixelFormat mainPixelFormat =
	{
		SDL_PIXELFORMAT_RGBA8888, // format
		NULL, //SDL_Palette *palette;
		32, //Uint8  BitsPerPixel;
		4, //Uint8  BytesPerPixel;
		{0, 0}, // padding
		0xff000000, 0xff0000, 0xff00, 0xff, //Uint32 Rmask, Gmask, Bmask, Amask;
		0, 0, 0, 0, //Uint8  Rloss, Gloss, Bloss, Aloss;
		24, 16, 8, 0, //Uint8  Rshift, Gshift, Bshift, Ashift;
		0, // refcount
		NULL // next ref
	};



///////////////////
// Initialize the standard Auxiliary Library
bool InitializeAuxLib()
{
	// We have already loaded all options from the config file at this time.

#ifdef linux
	//XInitThreads();	// We should call this before any SDL video stuff and window creation
#endif


	if(getenv("SDL_VIDEODRIVER"))
		notes << "SDL_VIDEODRIVER=" << getenv("SDL_VIDEODRIVER") << endl;

	// Initialize SDL. Let SDL2 pick its default video driver — the old
	// SDL1 "directx"/"windib" names are no longer valid in SDL2.
	int SDLflags = SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE;
	if(!bDedicated) {
		SDLflags |= SDL_INIT_VIDEO;
	} else {
		hints << "DEDICATED MODE" << endl;
		bDisableSound = true;
		bJoystickSupport = false;
	}

	if(SDL_Init(SDLflags) == -1) {
		errors << "Failed to initialize the SDL system!\nErrorMsg: " << std::string(SDL_GetError()) << endl;
		return false;
	}

	if(bJoystickSupport) {
		if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
			warnings << "WARNING: couldn't init gamecontroller/joystick subystem: " << SDL_GetError() << endl;
			bJoystickSupport = false;
		} else {
			// Subsystem is up - load the bundled SDL game controller mapping
			// database. It is downloaded from
			// https://github.com/mdqinc/SDL_GameControllerDB at build time and
			// shipped in the gamedir, augmenting SDL's built-in mappings so
			// that many more gamepads are recognised out of the box.
			const std::string gcdbPath = GetFullFileName("gamecontrollerdb.txt");
			if(gcdbPath.empty()) {
				notes << "no bundled gamecontrollerdb.txt found - using SDL's built-in mappings only" << endl;
			} else {
				// Log the resolved path up front so it is visible (e.g. over ADB
				// logcat on Android) even if the load below fails or hangs.
				notes << "gamecontrollerdb: loading mappings from " << gcdbPath << endl;
				const int added = SDL_GameControllerAddMappingsFromFile(gcdbPath.c_str());
				if(added < 0)
					warnings << "WARNING: couldn't load gamecontroller mappings from " << gcdbPath << ": " << SDL_GetError() << endl;
				else
					notes << "loaded " << added << " gamecontroller mappings from " << gcdbPath << endl;
			}
		}
	}

	if(!bDedicated && !SetVideoMode())
		return false;

    // Enable the system events
    SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	//SDL_EventState(SDL_VIDEOEXPOSE, SDL_ENABLE); // TODO: SDL2? it's SDL_WINDOWEVENT now

	// Enable unicode and key repeat
	//SDL_EnableUNICODE(1); // TODO: SDL2?
	//SDL_EnableKeyRepeat(200,20); // TODO: SDL2?

	
	/*
	Note about the different sound vars:
	  bDisableSound - if the sound system+driver is disabled permanentely
	  tLXOptions->bSoundOn - if the sound is enabled temporarely (false -> volume=0, nothing else)

	I.e., even with tLXOptions->bSoundOn=false (Audio.Enabled=false in config), the sound system
	will be loaded. To start OLX without the sound system, use the -nosound parameter.

	The console variable Audio.Enabled links to tLXOptions->bSoundOn.
	The console command 'sound' also wraps around tLXOptions->bSoundOn.

	tLXOptions->iSoundVolume will never be touched by OLX itself, only the user can modify it.
	tLXOptions->bSoundOn will also not be touched by OLX itself, only user actions can modify it.
	(Both points were somewhat broken earlier and kind of annoying.)
	*/
	
    if( !bDisableSound ) {
	    // Initialize sound
		//if(!InitSoundSystem(22050, 1, 512)) {
		if(!InitSoundSystem(44100, 1, 512)) {
		    warnings << "Failed the initialize the sound system" << endl;
			bDisableSound = true;
		}
    }
	if(bDisableSound) {
		notes << "soundsystem completly disabled" << endl;
		// NOTE: Don't change tLXOptions->bSoundOn here!
	}

	if( tLXOptions->bSoundOn ) {
		StartSoundSystem();
	}
	else
		StopSoundSystem();


	// Give a seed to the random number generator
	srand((unsigned int)time(NULL));

	if(!bDedicated) {
		//SmartPointer<SDL_Surface> bmpIcon = LoadGameImage("data/icon.png", true);
		//if(bmpIcon.get())
		// TODO use SDL_SetWindowIcon
		//	SDL_SetWindowIcon(bmpIcon.get(), NULL);
	}

	InitEventQueue();
	
	// Initialize the keyboard & mouse
	InitEventSystem();

	// Initialize timers
	InitializeTimers();

#ifdef DEBUG
	// Cache
	InitCacheDebug();
#endif


	return true;
}



///////////////////
// Set the video mode
bool SetVideoMode()
{
	if(bDedicated) {
		notes << "SetVideoMode: dedicated mode, ignoring" << endl;
		return true; // ignore this case
	}

	if (!tLXOptions)  {
		warnings << "SetVideoMode: Don't know what video mode to set, ignoring" << endl;
		return false;
	}

	return VideoPostProcessor::get()->initWindow();
}

#ifdef WIN32
//////////////////////
// Get the window handle
void *GetWindowHandle()
{
	SDL_Window* win = VideoPostProcessor::get()->sdl_window();
	if(!win)
		return 0;

	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	if(!SDL_GetWindowWMInfo(win, &info))
		return 0;

	return (void *)info.info.win.window;
}
#endif


void CapFPS() {
#ifdef __EMSCRIPTEN__
	// Don't sleep on wasm. The browser's compositor already paces
	// presentation at vsync (~60 Hz), so the game-thread sleeping here
	// only adds keyboard-input latency without saving any visible work:
	// any SDL_KEYDOWN that arrives in the OLX mainQueue while the thread
	// is mid-SDL_Delay has to wait for the sleep to end before
	// ProcessEvents picks it up next frame. Gamepad input doesn't suffer
	// because it's polled directly during simulation.
	return;
#else
	const TimeDiff fMaxFrameTime = TimeDiff( (tLXOptions->nMaxFPS > 0) ? (1.0f / (float)tLXOptions->nMaxFPS) : 0.0f );
	const AbsTime currentTime = GetTime();
	// tLX->currentTime is old time

	// Cap the FPS
	if(currentTime - tLX->currentTime < fMaxFrameTime)
		SDL_Delay((Uint32)(fMaxFrameTime - (currentTime - tLX->currentTime)).milliseconds());
	else
		// do at least one small break, else it's possible that we never receive signals from our OS
		SDL_Delay(1);
#endif
}


// Screenshot structure
struct screenshot_t {
	std::string sDir;
	std::string	sData;
};

struct ScreenshotQueue {
	std::list<screenshot_t> queue;
	SDL_mutex* mutex;
	ScreenshotQueue() : mutex(NULL) { mutex = SDL_CreateMutex(); }
	~ScreenshotQueue() { SDL_DestroyMutex(mutex); mutex = NULL; }
};

static ScreenshotQueue screenshotQueue;

void PushScreenshot(const std::string& dir, const std::string& data) {
	screenshot_t scr; scr.sDir = dir; scr.sData = data;
	ScopedLock lock(screenshotQueue.mutex);
	screenshotQueue.queue.push_back(scr);
}

static void TakeScreenshot(const std::string& scr_path, const std::string& additional_data);

////////////////
// Process any screenshots
void ProcessScreenshots()
{
	std::list<screenshot_t> scrs;
	{
		ScopedLock lock(screenshotQueue.mutex);
		scrs.swap(screenshotQueue.queue);
	}
	
	// Process all the screenhots in the queue
	for (std::list<screenshot_t>::iterator it = scrs.begin(); it != scrs.end(); it++)  {
		TakeScreenshot(it->sDir, it->sData);
	}
}







// ---------------- VideoPostProcessor ---------------------------------------------------------

VideoPostProcessor VideoPostProcessor::instance;
VideoPostProcessor::ScreenOverlayFn VideoPostProcessor::screenOverlay = NULL;


bool VideoPostProcessor::initWindow() {
	assert(isMainThread());

	bool resetting = false;

	// Check if already running
	if (m_videoSurface.get())  {
		resetting = true;
		notes << "resetting video mode" << endl;
	} else {
		notes << "setting video mode" << endl;
	}

#if defined(__EMSCRIPTEN__)
	// SDL2 + Emscripten doesn't support tearing down and recreating
	// the window / renderer cleanly: doing so destroys the canvas
	// element while the browser's main thread may still be dispatching
	// mouse events into the old context, which trips a heap-corruption
	// trap inside the malloc-on-mouse-event path. The 640x480 surface
	// never changes between menu and gameplay, so just keep the
	// existing window+renderer the first time around and short-circuit
	// any later "reset" calls.
	if (resetting) {
		notes << "  (Emscripten: reusing existing window+renderer)" << endl;
		return true;
	}
#endif

	// uninit first to ensure that the video thread is not running
	VideoPostProcessor::uninit();
	
	int vidflags = 0;
	
	// Check that the bpp is valid
	switch (tLXOptions->iColourDepth) {
		case 0:
		case 16:
		case 24:
		case 32:
			break;
		default: tLXOptions->iColourDepth = 16;
	}
	notes << "ColorDepth: " << tLXOptions->iColourDepth << endl;
	
	// BlueBeret's addition (2007): OpenGL support
	bool opengl = tLXOptions->bOpenGL;
	
#if defined(__EMSCRIPTEN__)
	// === Wasm fullscreen master switch ============================
	// Browser fullscreen + OpenLieroX is currently buggy: the canvas
	// resizes mid-game, mouse coordinates drift, and exiting fullscreen
	// leaves SDL in a bad state. Flip this to `true` if you want to
	// re-enable it after fixing those issues; for now we force it off
	// regardless of what the user picked in Options. See CLAUDE.md.
	static const bool kWasmAllowFullscreen = false;
	if(!kWasmAllowFullscreen && tLXOptions->bFullscreen) {
		notes << "Wasm: ignoring requested fullscreen "
		         "(disabled via kWasmAllowFullscreen)" << endl;
		tLXOptions->bFullscreen = false;
	}
#endif

	// Initialize the video
	if(tLXOptions->bFullscreen)  {
		vidflags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	}
	
	if (opengl) {
		vidflags |= SDL_WINDOW_OPENGL;
		//#ifndef MACOSX
		/*
		 short colorbitsize = (tLXOptions->iColourDepth==16) ? 5 : 8;
		 SDL_GL_SetAttribute (SDL_GL_RED_SIZE,   colorbitsize);
		 SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, colorbitsize);
		 SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE,  colorbitsize);
		 SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, colorbitsize);
		 //SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, tLXOptions->iColourDepth);
		 */
		//#endif
		//SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE,  8);
		//SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 24);
		//SDL_GL_SetAttribute (SDL_GL_BUFFER_SIZE, 32);		
		//needed?
		//SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1); // always use double buffering in OGL mode
	}
	
#ifdef WIN32
	UnSubclassWindow();  // Unsubclass before doing anything with the window
#endif
	
	
#ifdef WIN32
	// Reset the video subsystem under WIN32, else we get a "Could not reset OpenGL context" error when switching mode
	if (opengl && tLX)  {  // Don't reset when we're setting up the mode for first time (OpenLieroX not yet initialized)
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_InitSubSystem(SDL_INIT_VIDEO);
	}
#endif
	
	// Derive the screen width from the desktop's aspect ratio while keeping
	// the height fixed at screenHeight() (480). 4:3 -> 640, 16:10 -> 768,
	// 16:9 -> 854, etc. Rounded to an even number to keep buffers/textures happy.
	{
		SDL_DisplayMode dm;
		int displayIdx = 0;
		if(SDL_GetDesktopDisplayMode(displayIdx, &dm) == 0 && dm.w > 0 && dm.h > 0) {
			double w = (double)screenHeight() * (double)dm.w / (double)dm.h;
			int rounded = ((int)(w + 0.5) + 1) & ~1;
			if(rounded < 320) rounded = 320;
			m_screenWidth = rounded;
			notes << "desktop mode: " << dm.w << "x" << dm.h
				<< " -> using " << m_screenWidth << "x" << screenHeight() << endl;
		} else {
			m_screenWidth = 640;
			warnings << "could not query desktop display mode (" << SDL_GetError()
				<< "), falling back to " << m_screenWidth << "x" << screenHeight() << endl;
		}
	}

setvideomode:
	// Window title: the full version string incl. the "+git.HASH" suffix,
	// so a dev build is identifiable at a glance.
	// This is the exact build string (see GetGameVersionStringFull),
	// shown verbatim rather than reconstructed via Version.
	m_window = SDL_CreateWindow((std::string(GetGameName()) + " " + GetGameVersionStringFull()).c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth(), screenHeight(), vidflags);
	
	if(m_window.get() == NULL) {
		if (resetting)  {
			errors << "Failed to reset video mode"
			<< " (ErrorMsg: " << SDL_GetError() << "),"
			<< " let's wait a bit and retry" << endl;
			SDL_Delay(500);
			resetting = false;
			goto setvideomode;
		}
		
		if(tLXOptions->iColourDepth != 0) {
			errors << "Failed to use " << tLXOptions->iColourDepth << " bpp"
			<< " (ErrorMsg: " << SDL_GetError() << "),"
			<< " trying automatic bpp detection ..." << endl;
			tLXOptions->iColourDepth = 0;
			goto setvideomode;
		}
		
		if(vidflags & SDL_WINDOW_OPENGL) {
			errors << "Failed to use OpenGL"
			<< " (ErrorMsg: " << SDL_GetError() << "),"
			<< " trying without ..." << endl;
			vidflags &= ~SDL_WINDOW_OPENGL;
			goto setvideomode;
		}
		
		if(vidflags & SDL_WINDOW_FULLSCREEN_DESKTOP) {
			errors << "Failed to set full screen video mode "
			<< screenWidth() << "x" << screenHeight() << "x" << tLXOptions->iColourDepth
			<< " (ErrorMsg: " << SDL_GetError() << "),"
			<< " trying window mode ..." << endl;
			vidflags &= SDL_WINDOW_FULLSCREEN_DESKTOP;
			goto setvideomode;
		}
		
		SystemError("Failed to set the video mode " + itoa(screenWidth()) + "x" + itoa(screenHeight()) + "x" + itoa(tLXOptions->iColourDepth) + "\nErrorMsg: " + std::string(SDL_GetError()));
		return false;
	}
	
	// Hide the operating-system / browser cursor.
	// OpenLieroX draws its own software cursor where one is wanted (menus),
	// and that should be the only cursor the player ever sees.
	// We are on the main thread here, so apply it directly.
	applySystemMouseCursor();
	EnforceSystemMouseCursor();

#ifdef WIN32
	// Hint: Reset the mouse state - this should avoid the mouse staying pressed
	GetMouse()->Button = 0;
	GetMouse()->Down = 0;
	GetMouse()->FirstDown = 0;
	GetMouse()->Up = 0;
	
	if (!tLXOptions->bFullscreen)  {
		SubclassWindow();
	}
#endif
	
	// Set the change mode flag
	if (tLX)
		tLX->bVideoModeChanged = true;

	if(!VideoPostProcessor::get()->resetVideo())
		return false;
		
	// Clear screen to blank
	SDL_SetRenderDrawColor(m_renderer.get(), 0, 0, 0, 255);
	SDL_RenderClear(m_renderer.get());
	SDL_RenderPresent(m_renderer.get());
	
	notes << "video mode was set successfully" << endl;
	
	// SDL 2.0.3 and earlier seems to apply fullscreen + rescaling only
	// once we handled a few out-standing events.
	// This is fixed in SDL 2.0.4, so we wont do any workarounds here.
	// https://forums.libsdl.org/viewtopic.php?t=10688
	
	return true;
}

static void dumpRenderInfo(const SDL_RendererInfo& info) {
	notes << "Renderer '" << info.name << "':" << endl;
	notes << "  software fallback: " << bool(info.flags & SDL_RENDERER_SOFTWARE) << endl;
	notes << "  hardware accelerated: " << bool(info.flags & SDL_RENDERER_ACCELERATED) << endl;
	notes << "  vsync: " << bool(info.flags & SDL_RENDERER_PRESENTVSYNC) << endl;
	notes << "  rendering to texture: " << bool(info.flags & SDL_RENDERER_TARGETTEXTURE) << endl;
	notes << "  max texture size (WxH): " <<
		info.max_texture_width << " x " << info.max_texture_height << endl;
	notes << "  formats (" << info.num_texture_formats << "):" << endl;
	for(uint32_t i = 0;
		i < info.num_texture_formats &&
		i < sizeof(info.texture_formats)/sizeof(info.texture_formats[0]);
		++i) {
		notes << "    " << i << ": " << SDL_GetPixelFormatName(info.texture_formats[i]) << endl;
	}
}

static void dumpRenderInfo(SDL_Renderer* renderer) {
	SDL_RendererInfo info;
	if(SDL_GetRendererInfo(renderer, &info) != 0)
		warnings << "Error getting renderer info: " << SDL_GetError() << endl;
	else
		dumpRenderInfo(info);
}

bool VideoPostProcessor::resetVideo() {
#if defined(__EMSCRIPTEN__)
	// On Emscripten + pthreads, SDL's GLES2 backend posts every GL
	// call (glTexImage2D etc.) to the main thread via a proxy queue
	// whose atomics-on-mailbox path trips wasm's alignment trap. The
	// software renderer doesn't touch WebGL at all and is plenty
	// fast for 640x480 — pin to it.
	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
	m_renderer = SDL_CreateRenderer(m_window.get(), -1,
	                                SDL_RENDERER_SOFTWARE);
#else
	m_renderer = SDL_CreateRenderer(m_window.get(), -1, 0);
#endif
	if(!m_renderer.get()) {
		errors << "failed to init renderer: " << SDL_GetError() << endl;
		return false;
	}
	
	dumpRenderInfo(m_renderer.get());
	
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "best");  // make the scaled rendering look smoother.
	SDL_RenderSetLogicalSize(m_renderer.get(), screenWidth(), screenHeight());
	
	// IMPORTANT: Don't reallocate if we already have the buffers.
	// If we would do, the old surfaces would get deleted. This is bad
	// because other threads could use it right now.
	// XXX: Explain. I hope that no other threads are currently accessing it...
	if(!m_videoSurface.get()) {
		// Note: Allegro format (main Gusanos format) does not have alpha.
		// Some Gusanos functions, when they directly draw to the videoSurface,
		// need this.
		// XXX: This could be fixed in a cleaner way in Gusanos,
		// however, it also doesn't really matter that much.
		// TODO: Fix Gusanos gfx functions, so that they support an alpha channel.
		// Example code is e.g. SimpleParticle::draw().
		m_videoSurface = create_32bpp_sdlsurface__allegroformat(screenWidth(), screenHeight());
		if(!m_videoSurface.get()) {
			errors << "failed to init video surface: " << SDL_GetError() << endl;
			return false;
		}
	}
	DumpSurfaceInfo(m_videoSurface.get(), "main video surface");

	// Must be of same format as videoSurface, because we copy the pixels over.
	m_videoTexture = SDL_CreateTexture
	(
		m_renderer.get(),
		m_videoSurface->format->format,
		SDL_TEXTUREACCESS_STREAMING,
		screenWidth(), screenHeight()
	);
	if(!m_videoTexture.get()) {
		errors << "failed to init video texture: " << SDL_GetError() << endl;
		return false;
	}
	
	// No need to reinit this.
	if(!m_videoBufferSurface.get()) {
		// Should be same format as videoSurface.
		m_videoBufferSurface = GetCopiedImage(m_videoSurface);
		if(!m_videoBufferSurface.get()) {
			errors << "failed to init video backbuffer surface: " << SDL_GetError() << endl;
			return false;
		}
	}

	return true;
}


void VideoPostProcessor::flipBuffers() {
	std::swap(get()->m_videoBufferSurface, get()->m_videoSurface);
	// Capture the layout width the just-committed frame was drawn for,
	// so render() presents it with the matching centering
	// even after the game thread changes m_displayScreenWidth for the next frame.
	get()->m_committedDisplayScreenWidth = get()->m_displayScreenWidth;
}


// IMPORTANT: this has to be called from main thread!

void VideoPostProcessor::process() {
	ProcessScreenshots();

	// Hand the committed frame's layout width to the main-thread-only render().
	// This runs under the video mutex; render() does not.
	get()->m_renderDisplayScreenWidth = get()->m_committedDisplayScreenWidth;

	// Upload the drawn band; render() assembles the final frame on the GPU.
	void* pixels = get()->m_videoBufferSurface->pixels;
	SDL_UpdateTexture(get()->m_videoTexture.get(), NULL, pixels, get()->screenWidth() * sizeof (uint32_t));

	get()->updateSideGapTextures();
}

// Upload the precomputed gap strips to textures when buildSideGaps produced new ones.
void VideoPostProcessor::updateSideGapTextures() {
	if(m_sideGapKey == m_sideGapTexKey && m_leftGapTex.get() && m_rightGapTex.get())
		return;
	m_sideGapTexKey = m_sideGapKey;
	if(m_leftGap.get() && m_rightGap.get() && m_renderer.get()) {
		m_leftGapTex = SDL_CreateTextureFromSurface(m_renderer.get(), m_leftGap.get());
		m_rightGapTex = SDL_CreateTextureFromSurface(m_renderer.get(), m_rightGap.get());
	} else {
		m_leftGapTex = NULL;
		m_rightGapTex = NULL;
	}
}

// Fill a gap strip by extending a 1px edge column, cross-fading from sharp
// (nearX, next to the band) to vertically blurred (farX, the screen edge).
static void blurGapStrip(SDL_Surface* gap, SDL_Surface* col, int nearX, int farX) {
	const int h = gap->h;
	if(h > 512 || farX == nearX) return;

	const int colPitch = col->pitch, gapPitch = gap->pitch;
	const Uint8* colPix = (const Uint8*)col->pixels;
	Uint8* gapPix = (Uint8*)gap->pixels;

	// The edge column, and a vertically box-blurred copy of it.
	Uint32 sharp[512], blurred[512];
	for(int y = 0; y < h; y++)
		sharp[y] = *(const Uint32*)(colPix + y*colPitch);
	const int R = 40; // blur radius at the screen edge
	for(int y = 0; y < h; y++) {
		int sum[4] = {0,0,0,0}, cnt = 0;
		for(int k = -R; k <= R; k++) {
			int yy = y + k;
			if(yy < 0 || yy >= h) continue;
			const Uint8* p = (const Uint8*)&sharp[yy];
			for(int c = 0; c < 4; c++) sum[c] += p[c];
			cnt++;
		}
		Uint8* o = (Uint8*)&blurred[y];
		for(int c = 0; c < 4; c++) o[c] = (Uint8)(sum[c] / cnt);
	}

	for(int x = 0; x < gap->w; x++) {
		float t = (float)(x - nearX) / (float)(farX - nearX);
		if(t < 0) t = 0; else if(t > 1) t = 1;
		t *= t; // stay sharp near the band, blur harder toward the edge
		const int t256 = (int)(t * 256.0f + 0.5f);
		for(int y = 0; y < h; y++) {
			const Uint8* a = (const Uint8*)&sharp[y];
			const Uint8* b = (const Uint8*)&blurred[y];
			Uint8* o = gapPix + y*gapPitch + x*4;
			for(int c = 0; c < 4; c++)
				o[c] = (Uint8)((a[c]*(256 - t256) + b[c]*t256) >> 8);
		}
	}
}

// Precompute the side-gap fills from a menuWidth-wide menu background.
// Cheap to call every frame: rebuilds only when the edge columns or offset change.
void VideoPostProcessor::buildSideGaps(const SmartPointer<SDL_Surface>& bg) {
	const int offset = (m_screenWidth - menuWidth) / 2;
	const int h = screenHeight();
	if(!bg.get() || offset <= 0 || bg->w < menuWidth || bg->h < h
	   || bg->format->BytesPerPixel != 4) {
		m_leftGap = NULL; m_rightGap = NULL; m_sideGapKey = 0;
		return;
	}

	// Change key: hash of the edge columns + offset; skip the rebuild if unchanged.
	Uint32 key = 2166136261u ^ (Uint32)offset;
	const Uint8* bgPix = (const Uint8*)bg->pixels;
	const int lastX = menuWidth - 1;
	for(int y = 0; y < h; y++) {
		const Uint8* row = bgPix + y*bg->pitch;
		key = (key ^ *(const Uint32*)(row)) * 16777619u;
		key = (key ^ *(const Uint32*)(row + lastX*4)) * 16777619u;
	}
	if(key == m_sideGapKey && m_leftGap.get() && m_rightGap.get()) return;
	m_sideGapKey = key;

	// Pull the two edge columns into 1px surfaces of our format (converts once).
	SmartPointer<SDL_Surface> leftCol = create_32bpp_sdlsurface__allegroformat(1, h);
	SmartPointer<SDL_Surface> rightCol = create_32bpp_sdlsurface__allegroformat(1, h);
	m_leftGap = create_32bpp_sdlsurface__allegroformat(offset, h);
	m_rightGap = create_32bpp_sdlsurface__allegroformat(offset, h);
	if(!leftCol.get() || !rightCol.get() || !m_leftGap.get() || !m_rightGap.get()) {
		m_leftGap = NULL; m_rightGap = NULL;
		return;
	}
	SDL_Rect ls = { 0, 0, 1, h }, cd = { 0, 0, 1, h };
	SDL_BlitSurface(bg.get(), &ls, leftCol.get(), &cd);
	SDL_Rect rs = { menuWidth - 1, 0, 1, h };
	SDL_BlitSurface(bg.get(), &rs, rightCol.get(), &cd);

	// Left gap: sharp at its inner edge (adjacent to the band), blurred at x=0.
	blurGapStrip(m_leftGap.get(), leftCol.get(), offset - 1, 0);
	// Right gap: sharp at x=0 (adjacent to the band), blurred at its outer edge.
	blurGapStrip(m_rightGap.get(), rightCol.get(), 0, offset - 1);
}

void VideoPostProcessor::render() {
	//TestCircleDrawing(psScreen);
	//TestPolygonDrawing(psScreen);
	//DrawLoadingAni(psScreen, 320, 260, 50, 50, Color(128,128,128), Color(128,128,128,128), LAT_CIRCLES);
	//DrawLoadingAni(psScreen, 320, 260, 10, 10, Color(255,0,0), Color(0,255,0), LAT_CAKE);

	if(!get()->m_renderer.get()) return;
	SDL_Renderer* r = get()->m_renderer.get();
	SDL_Texture* band = get()->m_videoTexture.get();

	// Clear to black; the overlay leaves the draw color set, so pin it each frame.
	SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
	SDL_RenderClear(r);

	// Assemble the frame on the GPU: side gaps, then the (centered) band, then the cursor.
	const int w = get()->screenWidth();
	const int h = get()->screenHeight();
	int dw = get()->m_renderDisplayScreenWidth;
	if(dw <= 0 || dw > w) dw = w;
	int offset = (w - dw) / 2;
	if(offset < 0) offset = 0;

	if(offset > 0) {
		// Side gaps: precomputed blurred strips.
		// Until the first menu builds them the gaps stay black from the clear above;
		// if they were built but somehow not uploaded, that is a bug -- stay black, note once.
		if(get()->m_leftGapTex.get() && get()->m_rightGapTex.get()) {
			SDL_Rect leftDst  = { 0, 0, offset, h };
			SDL_Rect rightDst = { offset + dw, 0, w - (offset + dw), h };
			SDL_RenderCopy(r, get()->m_leftGapTex.get(),  NULL, &leftDst);
			SDL_RenderCopy(r, get()->m_rightGapTex.get(), NULL, &rightDst);
		} else if(get()->m_sideGapKey != 0) {
			static bool warned = false;
			if(!warned) { warned = true; warnings << "side-gap textures missing; presenting black bars" << endl; }
		}
		// The band is dw wide in the left columns; present it centered.
		// The mouse is shifted by the same offset (HandleMouseState), so clicks line up.
		SDL_Rect bandSrc = { 0, 0, dw, h };
		SDL_Rect bandDst = { offset, 0, dw, h };
		SDL_RenderCopy(r, band, &bandSrc, &bandDst);

		// Cursor on top at its true position, so it roams the gaps.
		// (Centered frames skip the in-band bake; see Cursor.cpp.)
		mouse_t* m = GetMouse();
		DrawCursorOnRenderer(r, m->X + offset, m->Y); // menu-local X -> screen X
	} else {
		// Content already fills the width (local game / 4:3); cursor is baked in.
		SDL_RenderCopy(r, band, NULL, NULL);
	}

	// Screen-space overlays (task bar, FPS), full width, on top.
	if(screenOverlay) screenOverlay(r);

	SDL_RenderPresent(r);

#ifdef __EMSCRIPTEN__
	// The very first presented frame means the wasm app is actually up on
	// screen — tell the JS shell so it drops the "Starting OpenLieroX…" overlay.
	// (Doing this here rather than when the main menu renders hides the overlay
	// the instant the engine shows anything, not a moment later.)
	// PROXY_TO_PTHREAD runs this on a worker; route to the browser main thread.
	{
		static bool reportedReady = false;
		if(!reportedReady) {
			reportedReady = true;
			MAIN_THREAD_ASYNC_EM_ASM({
				if (typeof window !== 'undefined' &&
				    typeof window.olxOnEngineReady === 'function') {
					window.olxOnEngineReady();
				}
			});
		}
	}
#endif
}

void VideoPostProcessor::cloneBuffer() {
	DrawImageAdv(get()->m_videoBufferSurface.get(), get()->m_videoSurface.get(), 0, 0, 0, 0, get()->m_videoSurface->w, get()->m_videoSurface->h);
}

void VideoPostProcessor::uninit() {
	instance.m_videoSurface = NULL; // should never be used before resetVideo() is called
	instance.m_videoBufferSurface = NULL; // else a restart keeps the old frame
	// Keep the gap surfaces + key across a video reset (they are renderer-independent):
	// buildSideGaps only re-runs in menus,
	// so an in-game reset (Alt+Enter) would otherwise lose the bars for good.
	// Only the textures below are renderer-owned; force them to re-upload.
	instance.m_sideGapTexKey = 0;

	// GPU textures below belong to m_renderer; drop them all before it dies.
	instance.m_videoTexture = NULL;
	instance.m_leftGapTex = NULL;
	instance.m_rightGapTex = NULL;
	InvalidateCursorTextures();
	instance.m_renderer = NULL;
	instance.m_window = NULL;
}




// ---------------------------------------------------------------------------------------------





///////////////////
// Shutdown the standard Auxiliary Library
void ShutdownAuxLib()
{
#ifdef WIN32
	UnSubclassWindow();
#endif

	// free all cached stuff like surfaces and sounds
	// HINT: we have to do it before we uninit the specific engines
	cCache.Clear();

	VideoPostProcessor::uninit();
	// quit video at this point to not get stuck in a fullscreen not responding game in case that it crashes in further quitting
	// in the case it wasn't inited at this point, this also doesn't hurt
	SDL_QuitSubSystem( SDL_INIT_VIDEO );

	QuitSoundSystem();

	// Shutdown the error system
	EndError();

#ifdef WIN32
	UnSubclassWindow();
#endif

	SDL_Quit();
}






//////////////////
// Helper funtion for screenshot taking
static std::string GetPicName(const std::string& prefix, size_t i, const std::string& ext)
{
	return prefix + (i == 0 ? std::string("") : itoa(i)) + ext;
}


////////////////////
// Helper function for TakeScreenshot
static std::string GetScreenshotFileName(const std::string& scr_path, const std::string& extension)
{
	std::string path = scr_path;

	// Append a slash if not present
	if (path[path.size() - 1] != '/' && path[path.size() - 1] != '\\')  {
		path += '/';
	}
	
	
	std::string filePrefix = GetDateTimeFilename();
	filePrefix += "-";
	if( tLX )
	{
		if( game.isLocalGame() )
			filePrefix += "local";
		else if( game.isServer() )
			filePrefix += tLXOptions->sServerName;
		else if( cClient )
			filePrefix += cClient->getServerName();
	}
	
	// Make filename more fileststem-friendly
	if( filePrefix.size() > 64 )
		filePrefix.resize(64);

#define S_LETTER_UPPER "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define S_LETTER_LOWER "abcdefghijklmnopqrstuvwxyz"
#define S_LETTER S_LETTER_UPPER S_LETTER_LOWER
#define S_NUMBER "0123456789"
#define S_SYMBOL ". -_&+"	// No "\\" symbol, no tab.
#define S_VALID_FILENAME S_LETTER_UPPER S_LETTER_LOWER S_NUMBER S_SYMBOL
	while( filePrefix.find_first_not_of(S_VALID_FILENAME) != std::string::npos )
		filePrefix[ filePrefix.find_first_not_of(S_VALID_FILENAME) ] = '-';

	static const size_t step = 256; // Step; after how many files we check if the filename still exists

	// We start at range from 1 to step
	size_t lower_bound = 0;
	size_t upper_bound = step;

	std::string fullname(path + GetPicName(filePrefix, upper_bound, extension));

	// Find a raw range of where the screenshot filename could be
	// For example: between lierox1000.png and lierox1256.png
	while (IsFileAvailable(fullname, false))  {
		lower_bound = upper_bound;
		upper_bound += step;

		fullname = path + GetPicName(filePrefix, upper_bound, extension);
	}

	// First file?
	if (!IsFileAvailable(path + GetPicName(filePrefix, lower_bound, extension)))
		return path + GetPicName(filePrefix, lower_bound, extension);

	// Use binary search on the given range to find the exact file name
	size_t i = (lower_bound + upper_bound) / 2;
	while (true)  {
		if (IsFileAvailable(path + GetPicName(filePrefix, i, extension), false))  {
			// If the current (i) filename exists, but the i+1 does not, we're done
			if (!IsFileAvailable(path + GetPicName(filePrefix, i + 1, extension)))
				return path + GetPicName(filePrefix, i + 1, extension);
			else  {
				// The filename is somewhere in the interval (i, upper_bound)
				lower_bound = i;
				i = (lower_bound + upper_bound) / 2;
			}
		} else {
			// The filename is somewhere in the interval (lower_bound, i)
			upper_bound = i;
			i = (lower_bound + upper_bound) / 2;
		}
	}

	return ""; // Should not happen
}

///////////////////
// Take a screenshot
// This should run on the main thread.
static void TakeScreenshot(const std::string& scr_path, const std::string& additional_data)
{
	if (scr_path.empty()) // Check
		return;

	notes << "Save screenshot to " << scr_path << endl;

	std::string	extension;

	// Set the extension
	switch (tLXOptions->iScreenshotFormat)  {
	case FMT_BMP: extension = ".bmp"; break;
	case FMT_PNG: extension = ".png"; break;
	case FMT_JPG: extension = ".jpg"; break;
	case FMT_GIF: extension = ".gif"; break;
	default: extension = ".png";
	}

	// Save the surface
	SaveSurface(VideoPostProcessor::videoBufferSurface(), GetScreenshotFileName(scr_path, extension),
		tLXOptions->iScreenshotFormat, additional_data);
}

#ifdef WIN32
LONG_PTR wpOriginal;
bool Subclassed = false;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

////////////////////
// Subclass the window (control the incoming Windows messages)
void SubclassWindow()
{
	if (Subclassed)
		return;

#pragma warning(disable:4311)  // Temporarily disable, the typecast is OK here
	wpOriginal = SetWindowLongPtr((HWND)GetWindowHandle(),GWLP_WNDPROC,(LONG_PTR)(&WindowProc));
#pragma warning(default:4311) // Enable the warning
	Subclassed = true;
}

////////////////////
// Remove the subclassing
void UnSubclassWindow()
{
	if (!Subclassed)
		return;

	SetWindowLongPtr((HWND)GetWindowHandle(),GWLP_WNDPROC, wpOriginal);

	Subclassed = false;
}

/////////////////////
// Subclass callback
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	// Ignore the unwanted messages
	switch (uMsg)  {
	case WM_ENTERMENULOOP:
		return 0;
	case WM_INITMENU:
		return 0;
	case WM_MENUSELECT:
		return 0;
	case WM_SYSKEYUP:
		return 0;
	}

#pragma warning(disable:4312)
	return CallWindowProc((WNDPROC)wpOriginal,hwnd,uMsg,wParam,lParam);
#pragma warning(default:4312)
}

//////////////////////
// unsetenv for WIN32, taken from libc source
static int _unsetenv(const char *name)
{
  size_t len;
  char **ep;

  if (name == NULL || *name == '\0' || strchr (name, '=') != NULL)
    {
      return -1;
    }

  len = strlen (name);

  ep = environ;
  while (*ep != NULL)
    if (!strncmp (*ep, name, len) && (*ep)[len] == '=')
      {
	/* Found it.  Remove this pointer by moving later ones back.  */
	char **dp = ep;

	do
	  dp[0] = dp[1];
	while (*dp++);
	/* Continue the loop in case NAME appears again.  */
      }
    else
      ++ep;

  return 0;
}

#if 0
static int _unsetenv(const wchar_t *name)
{
  size_t len;
  wchar_t **ep;

  if (name == NULL || *name == '\0' || wcschr (name, '=') != NULL)
    {
      return -1;
    }

  len = wcslen (name);

 // ep = _wenviron;
  while (*ep != NULL)
    if (!wcsncmp (*ep, name, len) && (*ep)[len] == '=')
      {
	/* Found it.  Remove this pointer by moving later ones back.  */
	wchar_t **dp = ep;

	do
	  dp[0] = dp[1];
	while (*dp++);
	// Continue the loop in case NAME appears again.  */
      }
    else
      ++ep;

  return 0;
}
#endif

int unsetenv(const char *name) {
	return _unsetenv(name);
//	return _unsetenv(Utf8ToUtf16(name).c_str());
}

#endif




#ifdef DEBUG
#include "CClient.h"

// TODO: move this to console (it was only nec. via chat because we didn't had the console globally before)
// HINT: This is called atm from CClientNetEngine::SendText().
// HINT: This is just a hack to do some testing in lobby or whatever.
// WARNING: These stuff is not intended to be stable, it's only for testing!
// HINT: Don't rely on this. If we allow the console later somehow in the lobby,
// this debug stuff will probably move there.
bool HandleDebugCommand(const std::string& text) {
	if(text.size() >= 3 && text.substr(0,3) == "///") {
		cClient->getChatbox()->AddText("DEBUG COMMAND", tLX->clNotice, TXT_NOTICE, tLX->currentTime);

		std::string cmd = text.substr(3);
		stringlwr(cmd);

		if(cmd == "reconnect") {
			notes << "DEBUG CMD: reconnect local client to " << cClient->getServerAddress() << endl;
			cClient->Connect(cClient->getServerAddress());
		} else if(cmd == "msgbox") {
			Menu_MessageBox("Test",
							"This is a very long text, a very long text, a very long text, a very long text, "
							"a very long text, a very long text, a very long text, a very long text, a very long text, "
							"a very long text, a very long text, a very long text, a very long text, a very long text, "
							"a very long text, a very long text, a very long text, a very long text, a very long text.\n"
							"Yes really, this text is very long, very long, very long, very long, very long, "
							"very very long, very very long, very very long, very very long, very very long.",
							DeprecatedGUI::LMB_OK);
		} else if(cmd == "register") {
			cServer->RegisterServer();
		} else
			notes << "DEBUG CMD unknown" << endl;

		return true;
	}
	return false;
}
#endif




void lierox_t::setupInputs() {
	if(!tLXOptions) {
		errors << "lierox_t::setupInputs: tLXOptions not set" << endl;
		return;
	}
	
	// Setup global keys
	cTakeScreenshot.Setup(tLXOptions->sGeneralControls[SIN_SCREENSHOTS]);
	cSwitchMode.Setup(tLXOptions->sGeneralControls[SIN_SWITCHMODE]);
	cIrcChat.Setup(tLXOptions->sGeneralControls[SIN_IRCCHAT]);
	cConsoleToggle.Setup(tLXOptions->sGeneralControls[SIN_CONSOLETOGGLE]);
	
	if(cClient)
		cClient->SetupGameInputs();
	else
		warnings << "lierox_t::setupInputs: cClient not set" << endl;
}

bool lierox_t::isAnyControlKeyDown() const {
	return cTakeScreenshot.isDown() || cSwitchMode.isDown() || cIrcChat.isDown() || cConsoleToggle.isDown();
}


// Whether the player should currently see the OS cursor.
// The menus and game hide it (OpenLieroX draws its own software cursor);
// the console and error dialogs show it.
static std::atomic<bool> systemMouseCursorWanted(false);

static void applySystemMouseCursor()
{
	// Should be called from the main thread, or you'll get a race condition with libX11.
	SDL_ShowCursor(systemMouseCursorWanted ? SDL_ENABLE : SDL_DISABLE);
}

#ifdef __APPLE__
// macOS re-shows the OS cursor on its own (window re-entry, app activation),
// and SDL_ShowCursor(SDL_DISABLE) rides on cursor rects
// that the window server applies unreliably,
// so the arrow keeps flickering back over the window.
// Enforce the wanted state at the hardware level instead,
// gated on whether the mouse is over our window
// so the arrow still shows over the title bar and menu bar.
extern "C" void mac__EnforceSystemCursorHidden(int hidden);
#endif

void EnforceSystemMouseCursor()
{
#ifdef __APPLE__
	if( bDedicated )
		return;
	// Hold the cursor hidden only while OLX is active
	// and the mouse is over our window,
	// so the normal arrow still shows over the title bar, the menu bar and other apps.
	// Losing either balances the hide back out (see the helper).
	const bool wantHidden = !systemMouseCursorWanted;
	const bool active = SDL_GetKeyboardFocus() != NULL;
	const bool mouseOverWindow = SDL_GetMouseFocus() != NULL;
	mac__EnforceSystemCursorHidden(wantHidden && active && mouseOverWindow);
#endif
}

void EnableSystemMouseCursor(bool enable)
{
	if( bDedicated )
		return;
#ifdef __EMSCRIPTEN__
	// Web build: never show the operating-system / browser cursor.
	// OpenLieroX's own software cursor is the only cursor the player
	// should ever see, so ignore requests to show the OS one.
	enable = false;
#endif
	systemMouseCursorWanted = enable;
	struct EnableMouseCursor: public Action
	{
		Result handle()
		{
			applySystemMouseCursor();
			EnforceSystemMouseCursor();
			return true;
		}
	};
	doActionInMainThread( new EnableMouseCursor() );
};
