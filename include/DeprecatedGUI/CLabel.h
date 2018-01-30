/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Label
// Created 30/6/02
// Jason Boettcher


#ifndef __CLABEL_H__DEPRECATED_GUI__
#define __CLABEL_H__DEPRECATED_GUI__



#include "DeprecatedGUI/CWidget.h"
#include "InputEvents.h"
#include "StringUtils.h"
#include "Color.h"
#include "CodeAttributes.h"

class ScriptVar_t;
class CGuiLayoutBase;


namespace DeprecatedGUI {

// Label events
enum {
	LBL_NONE=-1
};

// Label messages
enum {
	LBS_SETTEXT
};

class CLabel : public CWidget {
public:
	CLabel() {}

	// Constructor
	CLabel(const std::string& text, Color col, bool center = false) {
		sText = text;
		iColour = col;
		iType = wid_Label;
		bCenter = center;
		bVar = NULL;
		iVar = NULL;
		fVar = NULL;
		sVar = NULL;
	}


private:
	// Attributes

	std::string	sText;
	Color		iColour;
	bool		bCenter;

	bool		*bVar;
	int			*iVar;
	float		*fVar;
	std::string	*sVar;

public:
	// Methods

	void	Create(); 
	void	Destroy() { }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)			{ return LBL_NONE; }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ return LBL_NONE; }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ return LBL_NONE; }
	int		MouseWheelDown(mouse_t *tMouse)		{ return LBL_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return LBL_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return LBL_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return LBL_NONE; }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2);
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param);
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param);

	void	ChangeColour(Color col);
	void	setText(const std::string& sStr);
	std::string getText() const;
	
	// Draw the label
	void	Draw(SDL_Surface * bmpDest);
};

}; // namespace DeprecatedGUI

#endif  //  __CLABEL_H__DEPRECATED_GUI__
