/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// GUI Layout class
// Created 30/5/02
// Jason Boettcher


#ifndef __CGUILAYOUT_H__DEPRECATED_GUI__
#define __CGUILAYOUT_H__DEPRECATED_GUI__

#include <SDL.h> // for SDL_Rect
#include "DeprecatedGUI/CWidget.h"
#include "CVec.h"
#include "SmartPointer.h"

struct TooltipIntern;
template <> void SmartPointer_ObjectDeinit<TooltipIntern> ( TooltipIntern * obj );

namespace DeprecatedGUI {


// layout event structure
struct gui_event_t {
	int		iControlID;
	int		iEventMsg;

	CWidget	*cWidget;
};

enum {
	gev_MouseWheel = -9999, // iControlID
	gev_MouseWheelUp = 1, // iEventMsg
	gev_MouseWheelDown = 2 // iEventMsg
};

// Errors
enum {
	ERR_OUTOFMEMORY=0,
	ERR_UNKNOWNPROPERTY,
	ERR_COULDNOTPARSE,
	ERR_EMPTYDOC,
	ERR_INVALIDROOT
};


// Global properties
enum {
	PRP_REDRAWMENU,
	PRP_ENABLED,
	PRP_ID
};



	
class CGuiLayout: public CGuiLayoutBase {
public:
	// Constructor
	CGuiLayout() {
		tEvent = new gui_event_t;
		cFocused = NULL;
		cMouseOverWidget = NULL;
		bCanFocus = true;
		iID = -1;
		//Initialize();
	}

	// Destructor
	~CGuiLayout() {
		if (tEvent)
			delete tEvent;
		tEvent = NULL;
	}


private:
	// Attributes

	std::list<CWidget *> cWidgets;
	gui_event_t		*tEvent;
	CWidget			*cFocused;
	CWidget			*cMouseOverWidget;

	int				iID;

	// Mouse button repeats
	int				nMouseButtons;
	AbsTime			fMouseNext[3];

	// Can we set focus to another widget?
	bool			bCanFocus;

	SmartPointer<TooltipIntern> tooltip;

	static bool		bKeyboardNavigation;

	
public:
	// Methods

	void		Initialize(int LayoutID = -1);

	bool		Build();

	void		Add(CWidget *widget, int id, int x, int y, int w, int h);
	void		AddBack(CWidget *widget, int id, int x, int y, int w, int h);
	void		FocusWidget(int id);
	CWidget		*getWidget(int id);
    void        removeWidget(int id);
	int			GetIdByName(const std::string& Name);
	void		Error(int ErrorCode, const std::string& text);

	gui_event_t	*Process();
	void		Draw(SDL_Surface * bmpDest);

	void		Shutdown();

	void		SetGlobalProperty(int property, int value);

	CWidget		*getFocusedWidget()		{ return cFocused; }

	// Messaging
	uintptr_t		SendMessage(int iControl, int iMsg, uintptr_t Param1, uintptr_t Param2);
	uintptr_t		SendMessage(int iControl, int iMsg, const std::string& sStr, uintptr_t Param);
	uintptr_t		SendMessage(int iControl, int iMsg, std::string *sStr, uintptr_t Param);

	// Variables
	int			getID()		{ return iID; }
	void		setID(int _id)	{ iID = _id; }

	void		setTooltip(const SDL_Rect& keepArea, VectorD2<int> pos, const std::string& msg);

	static bool isKeyboardNavigationUsed() { return bKeyboardNavigation; }
	static void setKeyboardNavigationUsed(bool used) { bKeyboardNavigation = used; }
};

} // namespace DeprecatedGUI

#endif  //  __CGUILAYOUT_H__DEPRECATED_GUI__
