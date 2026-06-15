// OpenLieroX


// Image
// Created 29/10/06
// Dark Charlie

// code under LGPL


#ifndef __CANIMATION_H__DEPRECATED_GUI__
#define __CANIMATION_H__DEPRECATED_GUI__

#include "InputEvents.h"
#include "GfxPrimitives.h"

namespace DeprecatedGUI {

// Events
enum {
	ANI_NONE = -1
};

class CAnimation : public CWidget {
public:
	CAnimation () {}

	// Constructor
	CAnimation(const std::string& Path, float frametime) {
		iType = wid_Animation;
		sPath = Path;
		tAnimation = NULL;
		if (Path != "")  {
			tAnimation = LoadGameImage(Path, true);

			if (tAnimation.get())  {
				iWidth = tAnimation.get()->w;
				iHeight = tAnimation.get()->h;
			} else {
				iWidth = iHeight = 0;
			}
		}
		fLastFrameChange = AbsTime();
		iCurFrame = 0;
		fFrameTime = frametime;

		Parse();
	}

private:
    // Attributes
	SmartPointer<SDL_Surface> tAnimation;
	std::string	sPath;
	std::vector<int> tFrameOffsets;
	std::vector<int> tFrameWidths;
	int			iNumFrames;
	int			iCurFrame;
	AbsTime		fLastFrameChange;
	float		fFrameTime;


public:
    // Methods

    void			Create();
	void			Destroy()		{  }
	void			Parse();

	std::string		getPath()		{ return sPath; }
	SmartPointer<SDL_Surface> getSurface()	{ return tAnimation; }
	float			getFrameTime()		{ return fFrameTime; }
	void			setFrameTime(float t) { fFrameTime = t; }
	void			Change(const std::string& Path, float frametime);

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)				{ return ANI_NONE; }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ return ANI_NONE; }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ return ANI_NONE; }
	int		MouseWheelDown(mouse_t *tMouse)			{ return ANI_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)			{ return ANI_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)		{ return ANI_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)		{ return ANI_NONE; }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2) { return 0; }
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }

	void	Draw(SDL_Surface * bmpDest);

};

}; // namespace DeprecatedGUI

#endif  //  __CANIMATION_H__DEPRECATED_GUI__


