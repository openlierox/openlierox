/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Checkbox
// Created 30/7/02
// Jason Boettcher


#ifndef __CCHECKBOX_H__DEPRECATED_GUI__
#define __CCHECKBOX_H__DEPRECATED_GUI__

#include "DeprecatedGUI/CWidget.h"
#include "DeprecatedGUI/CGuiSkin.h"
#include "InputEvents.h"

class ScriptVar_t;

namespace DeprecatedGUI {

// Checkbox events
enum {
	CHK_NONE=-1,
	CHK_CHANGED
};

// Checkbox messages
enum {
	CKM_SETCHECK=0,
	CKM_GETCHECK
};


class CCheckbox : public CWidget {
public:
	// Constructor
	CCheckbox(bool val) {
		bValue = val;
        bmpImage = NULL;
		iType = wid_Checkbox;
		bVar = NULL;
		iVar = NULL;
	}

	CCheckbox(bool* val) {
		bValue = *val;
        bmpImage = NULL;
		iType = wid_Checkbox;
		bVar = val;
		iVar = NULL;
	}

	CCheckbox(ScriptVar_t& var);
	
private:
	// Attributes
	bool		bValue;
	SmartPointer<SDL_Surface> bmpImage;
	bool		*bVar;
	int			*iVar;
	CGuiSkin::CallbackHandler cClick;

public:
	// Methods

	void	Create();
	void	Destroy() { }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)			{ return CHK_NONE; }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ bValue = !bValue;	updatePointers(); return CHK_CHANGED; }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ return CHK_NONE; }
	int		MouseWheelDown(mouse_t *tMouse)		{ return CHK_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return CHK_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return CHK_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return CHK_NONE; }


	// Process a message sent
	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2) {

				switch(iMsg) {
					case CKM_SETCHECK:
						bValue = Param1 != 0;
						updatePointers();
						return 0;
					case CKM_GETCHECK:
						return (int)bValue;
				}

				return 0;
			}
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }


	// Draw the title button
	void	Draw(SDL_Surface * bmpDest);

	void	LoadStyle() {}

	bool	getValue()						{ return bValue; }
	void	setValue(bool v)				{ bValue = v; updatePointers(); }

	void	updatePointers()	{
		if( bVar )
			*bVar = bValue;
		if( iVar )
			*iVar = (int)bValue;
	}

	static CWidget * WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy )
	{
		CCheckbox * w = new CCheckbox(false);
		w->bVar = CScriptableVars::GetVarP<bool>( p[0].toString() );
		w->iVar = CScriptableVars::GetVarP<int>( p[0].toString() );
		if( w->bVar )
			w->bValue = *w->bVar != 0;
		if( w->iVar )
			w->bValue = *w->iVar != 0;
		w->cClick.Init( p[1].toString(), w );
		layout->Add( w, id, x, y, dx, dy );
		return w;
	};

	void	ProcessGuiSkinEvent(int iEvent)
	{
		if( iEvent == CGuiSkin::SHOW_WIDGET )
		{
			if( bVar )
				bValue = *bVar;
			if( iVar )
				bValue = *iVar != 0;
		};
		if( iEvent == CHK_CHANGED )
		{
			// IMPORTANT TODO: this gets not called. why?
			updatePointers();
			cClick.Call();
		};
	};
};

}; // namespace DeprecatedGUI

#endif  //  __CCHECKBOX_H__DEPRECATED_GUI__
