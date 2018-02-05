/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Input box
// Created 31/7/02
// Jason Boettcher


#ifndef __CINPUTBOX_H__DEPRECATED_GUI__
#define __CINPUTBOX_H__DEPRECATED_GUI__

#include "InputEvents.h"
#include "DeprecatedGUI/CGuiSkinnedLayout.h"
#include "CodeAttributes.h"

namespace DeprecatedGUI {

// Inputbox events
enum {
	INB_NONE=-1,
	INB_MOUSEUP
};


// inputbox messages
enum {
	INM_GETVALUE=0,
	INS_GETTEXT
};


class CInputbox : public CWidget {
public:
	// Constructor.
	// slot selects which binding of a comma-separated control value this box
	// edits (0-based). -1 (the default) means the box represents the whole
	// value, as before.
	CInputbox(int val, const std::string& _text, SmartPointer<SDL_Surface> img, const std::string& name, int slot = -1) {
		iKeyvalue = val;
		sText = _text;
		sName = name;
		iSlot = slot;

		bmpImage = img;
		iType = wid_Inputbox;
		bMouseOver = false;
		sVar = NULL;
	}


private:
	// Attributes

	int			iKeyvalue;
	int			iSlot;
	std::string	sText;
	SmartPointer<SDL_Surface> bmpImage;
	bool		bMouseOver;
	std::string	sName;

	std::string		*sVar;

public:
	// Methods

	void	Create() { }
	void	Destroy() { }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)			{ bMouseOver = true; return INB_NONE; }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ return INB_MOUSEUP; }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ return INB_NONE; }
	int		MouseWheelDown(mouse_t *tMouse)		{ return INB_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return INB_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return INB_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate); // return event on key up so we won't process that same event inside key reader


	// Process a message sent
	INLINE uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2) {

				if(iMsg == INM_GETVALUE) {
						return iKeyvalue;
				}

				return 0;
			}

	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  {
		if (iMsg == INS_GETTEXT)  {
			*sStr = sText;
		}
		return 0;
	}

	// Draw the title button
	void	Draw(SDL_Surface * bmpDest);


	INLINE int		getValue()						{ return iKeyvalue; }
	INLINE void	setValue(int _v)					{ iKeyvalue = _v; }
	INLINE int		getSlot()						{ return iSlot; }
	INLINE std::string	getText()				{ return sText; }
	INLINE void	setText(const std::string& _t)		{ sText = _t; }
	INLINE std::string	getName()				{ return sName; }

	static CInputbox * InputBoxSelected;
	static std::string InputBoxLabel;	// "GUI.InputBoxLabel" skin string
};

} // namespace DeprecatedGUI

#endif  //  __CINPUTBOX_H__DEPRECATED_GUI__
