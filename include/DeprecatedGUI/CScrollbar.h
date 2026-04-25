/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// gui scrollbar class
// Created 30/6/02
// Jason Boettcher


#ifndef __CSCROLLBAR_H__DEPRECATED_GUI__
#define __CSCROLLBAR_H__DEPRECATED_GUI__


#include "InputEvents.h"
#include "DeprecatedGUI/CWidget.h"
#include "DeprecatedGUI/CGuiLayout.h"
#include "DeprecatedGUI/CGuiSkin.h"

namespace DeprecatedGUI {

// Scrollbar events
enum {
	SCR_NONE=-1,
	SCR_CHANGE=0
};

// Scrollbar messages
enum {
    SCM_GETVALUE,
    SCM_SETVALUE,
    SCM_SETITEMSPERBOX,
    SCM_SETMAX,
    SCM_SETMIN
};


class CScrollbar : public CWidget {
public:
	// Constructor
	CScrollbar() {
		iType = wid_Scrollbar;
		iMin = 0;
		iMax = 1;
		iValue = 0;
		iItemsperbox = 1;
		iVar = NULL;
		iWidth = 16;
	}


private:
	// Attributes
	int		iMin;
	int		iMax;
	int		iValue;

	int		iScrollPos;
	int		iItemsperbox;
	bool	bSliderGrabbed;
	int		iSliderGrabPos;

	bool	bTopButton;
	bool	bBotButton;

	int		nButtonsDown;
	AbsTime	fMouseNext[3];

	int		*iVar;
	CGuiSkin::CallbackHandler cClick;

public:
	// Methods

	void	Create();
	void	Destroy() { }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse);
	int		MouseUp(mouse_t *tMouse, int nDown);
	int		MouseDown(mouse_t *tMouse, int nDown);
	int		MouseWheelDown(mouse_t *tMouse);
	int		MouseWheelUp(mouse_t *tMouse);
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return SCR_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return SCR_NONE; }

	void	Draw(SDL_Surface * bmpDest);

	void	LoadStyle() {}

	void	UpdatePos();


	void	setMin(int _min)				{ iMin = _min; UpdatePos(); }
	void	setMax(int _max)				{ iMax = _max; UpdatePos(); } // TODO: that should the max possible value!
	void	setValue(int _value)			{ iValue = _value; UpdatePos(); }

	void	setItemsperbox(int _i)			{ iItemsperbox = _i; }
    int     getItemsperbox()            { return iItemsperbox; }

	int		getValue()					{ return iValue; }
	int		getMax()					{ return iMax; } // TODO: that should return the max possible value!!
	bool	getGrabbed()				{ return bSliderGrabbed; }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2);
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }

	static CWidget * WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy )
	{
		CScrollbar * w = new CScrollbar();
		layout->Add( w, id, x, y, dx, dy );
		// Should be set after scrollbar is added to layout
		w->cClick.Init( p[4].toString(), w );
		w->setMin( p[0].toInt() );
		w->setMax( p[1].toInt() );
		w->setItemsperbox( p[2].toInt() );
		w->iVar = CScriptableVars::GetVarP<int>( p[3].toString() );
		if( w->iVar )
			w->setValue( *w->iVar );
		return w;
	};
	
	void	ProcessGuiSkinEvent(int iEvent) 
	{
		if( iEvent == CGuiSkin::SHOW_WIDGET )
		{
			if( iVar )
				setValue( *iVar );
		}
		if( iEvent == SCR_CHANGE )
		{
			if( iVar )
				*iVar = iValue;
			cClick.Call();
		}
	}
};

} // namespace DeprecatedGUI

#endif  //  __CSCROLLBAR_H__DEPRECATED_GUI__
