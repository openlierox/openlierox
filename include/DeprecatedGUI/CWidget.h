/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Widget class
// Created 30/5/02
// Jason Boettcher


#ifndef __CWIDGET_H__DEPRECATED_GUI__
#define __CWIDGET_H__DEPRECATED_GUI__

#include "InputEvents.h"
#include "olx-types.h"
#include "SmartPointer.h"

namespace DeprecatedGUI {

// Widget messages
enum { WDM_SETENABLE = -1 };

// Generic event IDs
enum {
	OnMouseOver=0,
	OnMouseOut,
	OnMouseDown,
	OnClick,
	NumEvents
};

// Generic events
class generic_events_t { public:
	char Events[NumEvents][128];
};


// Widget types
enum WidgetType_t {
	wid_None=-1,
	wid_Button=0,
	wid_Label,
	wid_Listview,
	wid_Scrollbar,
	wid_Slider,
	wid_Textbox,
	wid_Titlebutton,
	wid_Textbutton,
	wid_Checkbox,
	wid_Inputbox,
	wid_Combobox,
	wid_Image,
	wid_Frame,
	wid_Animation,
	wid_GuiLayout
};

class CGuiLayoutBase;

class CWidget {
public:
	// Constructor
	CWidget() {
		iID = -1;

		iType = wid_None;
		iX = iY = 0;
		iWidth = iHeight = 1;
		bFocused = false;
		bEnabled = true;
		bRedrawMenu = true;
		bCanLoseFocus = true;
		iKeyboardNavigationOrder = 0;
	}

	CWidget(const CWidget&) { assert(false); }
	virtual ~CWidget(); 

protected:
	// Attributes

	int		iX, iY;
	int		iWidth, iHeight;
	bool	bFocused;
	WidgetType_t iType;
	bool	bRedrawMenu;
	bool	bCanLoseFocus;


private:
	int					iID;
	bool				bEnabled;
	int					iKeyboardNavigationOrder;

	generic_events_t	tEvents;
	CGuiLayoutBase		*cParent;


public:
	// Methods


	// Widget functions
	void			Setup(int id, int x, int y, int w, int h);
	bool			InBox(int x, int y);

    void            redrawBuffer();

	int				getID()						{ return iID; }
	void			setID(int _i)					{ iID = _i; }
	WidgetType_t	getType()					{ return iType; }

	void			setFocused(bool _f)				{ bFocused = _f; }
	bool			getFocused()				{ return bFocused; }

	bool			getEnabled()				{ return bEnabled; }
	void			setEnabled(bool _e)				{ bEnabled = _e; }

	bool			getRedrawMenu()				{ return bRedrawMenu; }
	void			setRedrawMenu(bool _r)			{ bRedrawMenu = _r; }

	int				getX()							{ return iX; }
	int				getY()							{ return iY; }
	void			move(int dx, int dy)			{ iX += dx; iY += dy; }
	int				getWidth()						{ return iWidth; }
	int				getHeight()						{ return iHeight; }

	CGuiLayoutBase	*getParent()				{ return cParent; }
	void			setParent(CGuiLayoutBase *l)	{ cParent = l; }

	int				getKeyboardNavigationOrder() const	{ return iKeyboardNavigationOrder; }
	// Default order is 0, positive value - widget will be the last, negative value - widget will be the first to be selected, range is between -120 and 120
	void			setKeyboardNavigationOrder(int i)	{ iKeyboardNavigationOrder = i; }

	bool			CanLoseFocus()				{ return bCanLoseFocus; }
	void			setLoseFocus(bool _f)			{ bCanLoseFocus = _f; }

	void			SetupEvents(generic_events_t *Events);	// Not used anywhere, should be removed
	void			ProcessEvent(int Event);	// Not used anywhere, should be removed


	// Virtual functions
	virtual	void	Create() = 0;
	virtual void	Destroy() = 0;

	//These events return an event id, otherwise they return -1
	virtual	int		MouseOver(mouse_t *tMouse) = 0;
	virtual	int		MouseClicked(mouse_t *tMouse, int nDown ) { return -1; }
	virtual	int		MouseUp(mouse_t *tMouse, int nDown ) = 0;
	virtual	int		MouseDown(mouse_t *tMouse, int nDown) = 0;
	virtual	int		MouseWheelUp(mouse_t *tMouse ) = 0;
	virtual	int		MouseWheelDown(mouse_t *tMouse) = 0;
	virtual	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate) = 0;
	virtual	int		KeyUp(UnicodeChar c, int keysym,  const ModifiersState& modstate) = 0;

	virtual	void	Draw(SDL_Surface * bmpDest) = 0;

	virtual uintptr_t	SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2) = 0;
	virtual uintptr_t	SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) = 0;
	virtual uintptr_t	SendMessage(int iMsg, std::string *sStr, uintptr_t Param) = 0;
};

// Base class for CGuiLayout and CGuiSkinnedLayout
class CGuiLayoutBase
{
	public:
	virtual ~CGuiLayoutBase() { };
	virtual void		Add(CWidget *widget, int id, int x, int y, int w, int h) = 0;
	// Add more functions here if needed
};

} // namespace DeprecatedGUI

#endif  //  __CWIDGET_H__DEPRECATED_GUI__
