/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Title button
// Created 30/6/02
// Jason Boettcher


#ifndef __CTITLEBUTTON_H__DEPRECATED_GUI__
#define __CTITLEBUTTON_H__DEPRECATED_GUI__

#include "InputEvents.h"
#include "Cursor.h"
#include "DeprecatedGUI/CWidget.h"
#include "olx-types.h"
#include "DeprecatedGUI/CGuiLayout.h"

namespace DeprecatedGUI {

// Event types
enum {
	TBT_NONE=-1,
	TBT_MOUSEUP=0,
	TBT_MOUSEOVER,
	TBT_CLICKED
};


class CTitleButton : public CWidget {
public:
	// Constructor
	CTitleButton(int imgid, SmartPointer<SDL_Surface> image) {
		iImageID = imgid;
		bmpImage = image;
		bMouseOver = false;
		iType = wid_Titlebutton;
	}

private:
	// Attributes

	bool		bMouseOver;
	int			iImageID;
	SmartPointer<SDL_Surface> bmpImage;
	CGuiSkin::CallbackHandler cClick;

public:
	// Methods

	void	Create() { }
	void	Destroy() { }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)			{ bMouseOver=true; SetGameCursor(CURSOR_HAND); return TBT_MOUSEOVER; }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ return TBT_MOUSEUP; }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ bMouseOver=true; return TBT_NONE; }
	int		MouseClicked(mouse_t *tMouse, int nDown)	{ return TBT_CLICKED; }
	int		MouseWheelDown(mouse_t *tMouse)		{ return TBT_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return TBT_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return TBT_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return TBT_NONE; }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)	{ return 0; }
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }

	// Draw the title button
	void	Draw(SDL_Surface * bmpDest);

	void	LoadStyle() {}

	static CWidget * WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy )
	{
		CTitleButton * w = new CTitleButton( p[0].toInt(), tMenu->bmpMainTitles );
		w->cClick.Init( p[1].toString(), w );
		layout->Add( w, id, x, y, dx, dy );
		return w;
	};

	void	ProcessGuiSkinEvent(int iEvent) 
	{
		if( iEvent == TBT_CLICKED )
			cClick.Call();
	};
};

}; // namespace DeprecatedGUI

#endif  //  __CTITLEBUTTON_H__DEPRECATED_GUI__
