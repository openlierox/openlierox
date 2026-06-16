/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Graphics header file
// Created 30/6/02
// Jason Boettcher


#ifndef __GRAPHICS_H__DEPRECATED_GUI__
#define __GRAPHICS_H__DEPRECATED_GUI__

#include "Consts.h"

namespace DeprecatedGUI {

// Gui graphics
class gfxgui_t { public:
	SmartPointer<SDL_Surface> bmpScrollbar;
	SmartPointer<SDL_Surface> bmpSliderBut;
	SmartPointer<SDL_Surface> bmpCommandBtn;
};


// Game graphics
class gfxgame_t { public:
	SmartPointer<SDL_Surface> bmpCrosshair;
	SmartPointer<SDL_Surface> bmpMuzzle;
	SmartPointer<SDL_Surface> bmpExplosion;
	SmartPointer<SDL_Surface> bmpSmoke;
	SmartPointer<SDL_Surface> bmpChemSmoke;
	SmartPointer<SDL_Surface> bmpSpawn;
	SmartPointer<SDL_Surface> bmpHook;
	SmartPointer<SDL_Surface> bmpGameover;
	SmartPointer<SDL_Surface> bmpInGame;
	SmartPointer<SDL_Surface> bmpScoreboard;
    SmartPointer<SDL_Surface> bmpViewportMgr;
	SmartPointer<SDL_Surface> bmpSparkle;
	SmartPointer<SDL_Surface> bmpInfinite;
	SmartPointer<SDL_Surface> bmpLag;
	SmartPointer<SDL_Surface> bmpGameNetBackground;
	SmartPointer<SDL_Surface> bmpGameLocalBackground;
	SmartPointer<SDL_Surface> bmpGameLocalTopBar;
	SmartPointer<SDL_Surface> bmpGameNetTopBar;
	SmartPointer<SDL_Surface> bmpTeamColours[MAX_TEAMS];
	SmartPointer<SDL_Surface> bmpFlagSpawnpoint[MAX_TEAMS];
	SmartPointer<SDL_Surface> bmpFlagSpawnpointDefault;
	SmartPointer<SDL_Surface> bmpAI;
	SmartPointer<SDL_Surface> bmpClock;

	SmartPointer<SDL_Surface> bmpBonus;
	SmartPointer<SDL_Surface> bmpHealth;
};


extern	gfxgui_t	gfxGUI;
extern	gfxgame_t	gfxGame;


// Routines
void	InitializeColors();
bool	LoadFonts();
bool	LoadGraphics();
void	ShutdownGraphics();

}; // namespace DeprecatedGUI

#endif  //  __GRAPHICS_H__DEPRECATED_GUI__
