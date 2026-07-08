/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Cursor source file
// Created 18/6/07
// Dark Charlie

#include "LieroX.h"

#include "FindFile.h"
#include "InputEvents.h"
#include "AuxLib.h"
#include "GfxPrimitives.h"
#include "ConfigHandler.h"
#include "Cursor.h"
#include "olx-types.h"
#include "Timer.h"


//
// Game cursor handling
//


CCursor *tCurrentCursor = NULL;
CCursor *tCursors[CURSOR_COUNT] = {NULL, NULL, NULL, NULL}; // TODO: any cursor_count independent way?
float fCursorFrameTime = 0.2f;
Timer *tAnimTimer = NULL;

int iMaxCursorWidth = 0;
int iMaxCursorHeight = 0;

////////////////////
// Timer event, currently does nothing, just makes the screen repaint
void OnTimerAnimation(Timer::EventData)
{

}

//////////////////
// Initialize cursors
bool InitializeCursors()
{
	tCurrentCursor = NULL;
	
	// Load the cursors
	tCursors[CURSOR_ARROW] = new CCursor("data/frontend/mouse.png",CUR_ARROW);
	tCursors[CURSOR_HAND] = new CCursor("data/frontend/mouse_hand.png",CUR_ARROW);
	tCursors[CURSOR_TEXT] = new CCursor("data/frontend/mouse_text.png",CUR_TEXT);
	tCursors[CURSOR_RESIZE] = new CCursor("data/frontend/mouse_resize.png",CUR_SPLITTER);

	// Load the frame time from external config
	ReadFloat("data/frontend/frontend.cfg","Cursors","FrameTime",&fCursorFrameTime,0.2f);

	// Check that all were loaded correctly and calculate maximal width/height
	bool result = true;
	for (byte i=0;i<CURSOR_COUNT;i++)
		if (tCursors[i])  {
			// Max width/height
			if (tCursors[i]->GetWidth() > iMaxCursorWidth)
				iMaxCursorWidth = tCursors[i]->GetWidth();

			if (tCursors[i]->GetHeight() > iMaxCursorHeight)
				iMaxCursorHeight = tCursors[i]->GetHeight();
		} else {
			result = false;
		}

	tAnimTimer = new Timer("animation", OnTimerAnimation, NULL, 10, false);


	return result;
}

//////////////////
// Shutdown the cursors
void ShutdownCursors()
{
	tCurrentCursor = NULL; // else it dangles past the free below (e.g. composePresentFrame)

	// Free all the cursor structures
	for (byte i=0; i<CURSOR_COUNT; i++)
		if (tCursors[i])  {
			delete tCursors[i];
			tCursors[i] = NULL;
		}

	if (tAnimTimer)  {
		tAnimTimer->stop();
		delete tAnimTimer;
		tAnimTimer = NULL;
	}
}

////////////////
// Set the current cursor
void SetGameCursor(int c)  {
	if (c < 0 || c >= CURSOR_COUNT)
		tCurrentCursor = NULL;
	else  {
		tCurrentCursor = tCursors[c];
		if (tCurrentCursor->IsAnimated() && !tAnimTimer->running())
			tAnimTimer->start();
		else
			tAnimTimer->stop();
	}
}

////////////////
// Set the current cursor
void SetGameCursor(CCursor *c)
{
	tCurrentCursor = c;
	if (c && c->IsAnimated() && !tAnimTimer->running())
		tAnimTimer->start();
	else
		tAnimTimer->stop();
}

/////////////////
// Draw game cursor
void DrawCursor(SDL_Surface * dst) {
	// On centered frames composePresentFrame() draws the cursor (see there),
	// so skip the in-band bake here to keep the gap-filling edge columns clean.
	if (VideoPostProcessor::get()->displayScreenOffsetX() > 0)
		return;
	if (tCurrentCursor)
		tCurrentCursor->Draw(dst);
}

/////////////////
// Draw game cursor at an explicit position
void DrawCursorAt(SDL_Surface * dst, int x, int y) {
	if (tCurrentCursor)
		tCurrentCursor->DrawAdv(dst, x, y);
}

///////////////////
// Get height of a cursor
int GetCursorHeight(int c)  {
	if (c < 0 || c >= CURSOR_COUNT)
		return 0;
	else
		return tCursors[c]->GetHeight();
}


///////////////////
// Get width of a cursor
int GetCursorWidth(int c)  {
	if (c < 0 || c >= CURSOR_COUNT)
		return 0;
	else
		return tCursors[c]->GetWidth();
}



//
// Cursor class
//

/////////////////
// Constructors
CCursor::CCursor(const std::string& filename, int type)
{
	bmpCursor = LoadGameImage(filename,true);

	Init(type);

	// Get the filename for the "down" cursor
	std::string down_filename = filename.substr(0,filename.rfind('.')) + "_down.png";

	// Get the filename for the "up" cursor
	std::string up_filename = filename.substr(0,filename.rfind('.')) + "_up.png";

	// Load up and down states if they're present
	if (IsFileAvailable(down_filename))
		cDown = new CCursor(down_filename,iType);
	if (IsFileAvailable(up_filename))
		cUp = new CCursor(up_filename,iType);
}

CCursor::CCursor(SmartPointer<SDL_Surface> image, int type)
{
	bmpCursor = image;

	Init(type);
}


/////////////////////
// Common code for both constructors
void CCursor::Init(int type)
{
	// Defaults
	iType = type;
	fAnimationSwapTime = tLX->currentTime;
	bAnimated = false;
	iFrameWidth = 0;
	iNumFrames = 0;
	iFrame = 0;
	cUp = NULL;
	cDown = NULL;

	// Load the cursor
	if (bmpCursor.get())  {
		if (bmpCursor.get()->w >= 2*bmpCursor.get()->h && (bmpCursor.get()->w % bmpCursor.get()->h) == 0)  {  // The file contains more frames
			bAnimated = true;
			iFrameWidth = bmpCursor.get()->h;
			iNumFrames = bmpCursor.get()->w / iFrameWidth;
			iFrame = 0;
			
		} else {  // Only one frame
			bAnimated = false;
			iFrameWidth = bmpCursor.get()->w;
			iNumFrames = 1;
			iFrame = 0;
		}

		// Set the color key
		SetColorKey(bmpCursor.get());
	}
}

////////////////
// Destructor
CCursor::~CCursor()
{
	// Freed by the cache
	bmpCursor = NULL;

	// Free the special cases
	if (cDown)
		delete cDown;
	if (cUp)
		delete cUp;
}

//////////////////
// Draw the cursor
void CCursor::Draw(SDL_Surface * dst)
{
	mouse_t *Mouse = GetMouse();
	DrawAdv(dst, Mouse->X, Mouse->Y);
}

//////////////////
// Draw the cursor at an explicit position
void CCursor::DrawAdv(SDL_Surface * dst, int mouseX, int mouseY)
{
	// Check
	if (!dst || !bmpCursor.get())
		return;

	mouse_t *Mouse = GetMouse();

	// Special states first
	if (Mouse->FirstDown)  // Mouse down
	{
		if (cDown)  {
			cDown->DrawAdv(dst, mouseX, mouseY);
			return;
		}
	}
	else if (Mouse->Up) // Mouse up
	{
		if (cUp)  {
			cUp->DrawAdv(dst, mouseX, mouseY);
			return;
		}
	}

	// Image position
	int X = mouseX;
	int Y = mouseY;

	// Change the position depending on the type
	switch (iType)  {
	case CUR_ARROW:
	case CUR_TEXT:
		// No position change needed
		break;
	case CUR_SPLITTER:
		X -= iFrameWidth / 2;
		break;
	case CUR_AIM:
		X -= iFrameWidth / 2;
		Y -= bmpCursor.get()->h / 2;
		break;
	default:
		warnings << "CCursor::Draw - unknown type" << endl;
		break;
	};


	// Process animating
	if ((tLX->currentTime - fAnimationSwapTime).seconds() >= fCursorFrameTime)  {
		iFrame++;
		iFrame %= iNumFrames;
		fAnimationSwapTime = tLX->currentTime;
	}

	// Draw the cursor
	DrawImageAdv(dst,bmpCursor,iFrame*iFrameWidth,0,X,Y,iFrameWidth,bmpCursor.get()->h);
}
