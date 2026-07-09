/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Cursor header file
// Created 18/6/07
// Dark Charlie

#ifndef __CURSOR_H_
#define __CURSOR_H_

#include <string>
#include <SDL.h>
#include "SmartPointer.h"
#include "CodeAttributes.h"


// Cursor indexes
enum {
	CURSOR_NONE = -1,
	CURSOR_ARROW = 0,
	CURSOR_HAND,
	CURSOR_TEXT,
	CURSOR_RESIZE,
	CURSOR_COUNT
};

// Cursor types
enum {
	CUR_ARROW=0,
	CUR_SPLITTER,
	CUR_TEXT,
	CUR_AIM  // for future use, cursors like cross
};

extern int iMaxCursorWidth;
extern int iMaxCursorHeight;

// Cursor class
class CCursor  {
public:
	CCursor(const std::string& filename, int type);
	CCursor(SmartPointer<SDL_Surface> image, int type);
	~CCursor();
private:
	SmartPointer<SDL_Surface>	bmpCursor;
	SmartPointer<SDL_Texture>	bmpCursorTex; // GPU texture, lazily from bmpCursor
	CCursor			*cDown;
	CCursor			*cUp;
	int				iFrame;
	int				iFrameWidth;
	AbsTime			fAnimationSwapTime;
	bool			bAnimated;
	int				iNumFrames;
	int				iType;

	void			Init(int type);
public:
	void			Draw(SDL_Surface *dst);
	void			DrawOnRenderer(SDL_Renderer *renderer, int mouseX, int mouseY); // GPU, at a screen position
	void			InvalidateTexture(); // drop the GPU texture before the renderer is destroyed
	INLINE bool		IsAnimated()  { return bAnimated; }
	INLINE int		GetType()  { return iType; }
	INLINE void		SetType(int _t)  { iType = _t; }
	INLINE int		GetHeight()  { return bmpCursor.get() ? bmpCursor->h : 0; }
	INLINE int		GetWidth()  { return bmpCursor.get() ? bmpCursor->w : 0; }
};

// Game cursors handling

bool InitializeCursors();
void ShutdownCursors();
void SetGameCursor(int c);
void SetGameCursor(CCursor *c);
void DrawCursor(SDL_Surface *dst);
void DrawCursorOnRenderer(SDL_Renderer *renderer, int x, int y); // GPU, at a screen position
void InvalidateCursorTextures(); // before the renderer is destroyed
int GetCursorHeight(int c);
int GetCursorWidth(int c);
INLINE int GetMaxCursorHeight()  { return iMaxCursorHeight; }
INLINE int GetMaxCursorWidth()  { return iMaxCursorWidth; }

#endif // __CURSOR_H_
