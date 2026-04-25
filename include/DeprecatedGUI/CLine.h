// OpenLieroX

// Line
// Created 5/11/06
// Dark Charlie

// code under LGPL


#ifndef __CLINE_H__DEPRECATED_GUI__
#define __CLINE_H__DEPRECATED_GUI__

#include "Color.h"


namespace DeprecatedGUI {

// Line events
enum {
	LIN_NONE=-1
};


class CLine : public CWidget {
public:
	// Constructor
	CLine(int x1, int y1, int dx, int dy, Color col) {
		iX = x1;
		iY = y1;
		iWidth = dx;
		iHeight = dy;
		iColour = col;
	}


private:
	// Attributes
	Color	iColour;

public:
	// Methods

	void	Create() { }
	void	Destroy() { }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)			{ return LIN_NONE; }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ return LIN_NONE; }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ return LIN_NONE; }
	int		MouseWheelDown(mouse_t *tMouse)		{ return LIN_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return LIN_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return LIN_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return LIN_NONE; }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)	{ 
							return 0;
						}
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }

	void	ChangeColour(Color col)			{ iColour = col; }

	// Draw the line
	void	Draw(SDL_Surface * bmpDest) {
		DrawLine(bmpDest, iX, iY, iX + iWidth, iY + iHeight, iColour); 
	}

	void	LoadStyle() {}

	static CWidget * WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy )
	{
		CWidget * w = new CLine( x, y, x+dx, y+dy, p[0].toColor() );
		layout->Add( w, id, x, y, dx, dy );
		return w;
	}
	
	void	ProcessGuiSkinEvent(int iEvent) {}
};

}; // namespace DeprecatedGUI

#endif  //  __CLINE_H__DEPRECATED_GUI__
