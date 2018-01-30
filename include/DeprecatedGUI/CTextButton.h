/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// The CTextButton (clickable label) - the current buttons are using images only, 
// this is not very handy when writing skins.
// TODO: Add large font to OLX graphics and add option to use large font for labels and CTextButton.
// TODO: Merge with CButton

#ifndef __CTEXTBUTTON_H__DEPRECATED_GUI__
#define __CTEXTBUTTON_H__DEPRECATED_GUI__

#include "DeprecatedGUI/CLabel.h"
#include "Color.h"

namespace DeprecatedGUI {

enum {
	TXB_MOUSEUP=0,
	TXB_MOUSEOVER
};

class CTextButton : public CLabel {
public:

	CTextButton(const std::string& text, Color colNormal, Color colGlow):
		CLabel ( text, colNormal )
	{
		iColNormal = colNormal;
		iColGlow = colGlow;
		bMouseOver = false;
		iType = wid_Textbutton;
	}

private:
	// Attributes

	Color	iColNormal;
	Color	iColGlow;
	bool	bMouseOver;

public:

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse);
	int		MouseUp(mouse_t *tMouse, int nDown)	{ return TXB_MOUSEUP; }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ return CLabel::MouseDown( tMouse, nDown ); }
	int		MouseWheelDown(mouse_t *tMouse)		{ return CLabel::MouseWheelDown( tMouse ); }
	int		MouseWheelUp(mouse_t *tMouse)		{ return CLabel::MouseWheelUp( tMouse ); }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return CLabel::KeyDown( c, keysym, modstate ); }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return CLabel::KeyUp( c, keysym, modstate ); }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)	{ return CLabel::SendMessage( iMsg, Param1, Param2 ); }
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return CLabel::SendMessage( iMsg, sStr, Param ); }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return CLabel::SendMessage( iMsg, sStr, Param ); }

	void	Draw(SDL_Surface * bmpDest);
};

}; // namespace DeprecatedGUI

#endif  //  __CTEXTBUTTON_H__DEPRECATED_GUI__
