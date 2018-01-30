// OpenLieroX


// Image
// Created 29/10/06
// Dark Charlie

// code under LGPL


#ifndef __CBOX_H__DEPRECATED_GUI__
#define __CBOX_H__DEPRECATED_GUI__

#include "InputEvents.h"

namespace DeprecatedGUI {

// Event types
enum {
	BOX_NOEVENT=-2,
	BOX_NONE=-1,
	BOX_MOUSEOVER=0
};


class CBox : public CWidget {
public:
	// Constructor
	CBox(int round, int border, Color lightcolour, Color darkcolour, Color bgcolour) {
		iType = wid_Frame;
		iRound = round;
		iBorder = border;
		iLightColour = lightcolour;
		iDarkColour = darkcolour;
		iBgColour = bgcolour;
		bmpBuffer = NULL;
	}

private:
    // Attributes
	int		iRound;
	int		iBorder;
	Color	iLightColour;
	Color	iDarkColour;
	Color	iBgColour;

	SmartPointer<SDL_Surface> bmpBuffer;


public:
    // Methods

    void			Create()		{ iType = wid_Frame; PreDraw(); }
	void			Destroy();

	void			setRound(int round)			{ iRound = round; }
	int				getRound()				{ return iRound; }
	void			setBorder(int border)		{ iBorder = border; }
	int				getBorder()				{ return iBorder; }
	void			setLightColour(Color col)	{ iLightColour = col; }
	Color			getLightColour()		{ return iLightColour; }
	void			setDarkColour(Color col)	{ iDarkColour = col;  }
	Color			getDarkColour()			{ return iDarkColour; }
	void			setBgColour(Color col)		{ iBgColour = col; }
	Color			getBgColour()			{ return iBgColour; }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)				{ return CheckEvent(); }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ return CheckEvent(); }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ return CheckEvent(); }
	int		MouseWheelDown(mouse_t *tMouse)			{ return CheckEvent(); }
	int		MouseWheelUp(mouse_t *tMouse)			{ return CheckEvent(); }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)		{ return BOX_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)		{ return BOX_NONE; }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)	{ return 0; }
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }

	int		CheckEvent();

	void	PreDraw();
	void	Draw(SDL_Surface * bmpDest);
};

}; // namespace DeprecatedGUI

#endif  //  __CBOX_H__DEPRECATED_GUI__
