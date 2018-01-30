/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Slider
// Created 30/6/02
// Jason Boettcher


#ifndef __CSLIDER_H__DEPRECATED_GUI__
#define __CSLIDER_H__DEPRECATED_GUI__

#include <string>
#include "InputEvents.h"
#include "Color.h"

namespace DeprecatedGUI {

// Slider events
enum {
	SLD_NONE=-1,
	SLD_CHANGE=0
};


// Slider messages
enum {
	SLM_GETVALUE=0,
	SLM_SETVALUE
};


class CSlider : public CWidget {
public:
	// Constructor
	CSlider(int max, int min = 0, int val = 0, bool showText = false, int textPosX = 0, int textPosY = 0, 
				Color textColor = Color(), float valueScale = 1.0f, const std::string & appendText = "" ) {
		Create();
		iType = wid_Slider;
		iMax = max;
		iMin = min;
		iValue = val;
		iVar = NULL;
		bShowText = showText;
		iTextPosX = textPosX;
		iTextPosY = textPosY;
		iTextColor = textColor;
		fValueScale = valueScale;
		sAppendText = appendText;
	}


private:
	// Attributes

	int		iValue;
	int		iMax;
	int		iMin;
	int		*iVar;
	bool	bShowText;
	int		iTextPosX;
	int		iTextPosY;
	Color	iTextColor;
	float	fValueScale;
	std::string sAppendText;

public:
	// Methods

	void	Create() { }
	void	Destroy() { }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)			{ return SLD_NONE; }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ return SLD_NONE; }
	int		MouseDown(mouse_t *tMouse, int nDown);
	int		MouseWheelDown(mouse_t *tMouse)		{ return SLD_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return SLD_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return SLD_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return SLD_NONE; }

	void	Draw(SDL_Surface * bmpDest);

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2);
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }

	int		getValue()						{ return iValue; }
	void	setValue(int v)						{ iValue = v; }

	void	setMax(int _m)						{ iMax = _m; }
	void	setMin(int _m)						{ iMin = _m; }
	int		getMax() const						{ return iMax; }
	int		getMin() const						{ return iMin; }

};

}; // namespace DeprecatedGUI

#endif  //  __CSLIDER_H__DEPRECATED_GUI__
