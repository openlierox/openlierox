/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Menu
// Created 28/6/03
// Jason Boettcher


#ifndef __CMENU_H__DEPRECATED_GUI__
#define __CMENU_H__DEPRECATED_GUI__


#include "InputEvents.h"
#include <list>

namespace DeprecatedGUI {

// Event types
enum {
	MNU_NONE=-1,
	MNU_LOSTFOCUS=0,
	MNU_SELECTION_CHANGED,
	MNU_USER        // Must be last
};


// Messages
enum {
	MNS_ADDITEM,
    MNM_REDRAWBUFFER
};


// Menu item structure
class mnu_item_t { public:
    int     nID;
	std::string  szName;
	bool	bChecked;
	bool	bCheckable;
};


class CMenu : public CWidget {
private:
    // Attributes

    int     m_nPosX, m_nPosY;
    int     m_nWidth, m_nHeight;
	int		m_nSelectedIndex;
	bool	m_bContainsCheckableItems;

	std::list<mnu_item_t>  m_psItemList;

	void	MoveMouseToCurrentItem();


public:
    // Methods

    CMenu(int nPosX, int nPosY);

    void	Create();
	void	Destroy();

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse);
	int		MouseUp(mouse_t *tMouse, int nDown);
	int		MouseDown(mouse_t *tMouse, int nDown);
	int		MouseWheelDown(mouse_t *tMouse)		{ return MNU_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return MNU_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate);
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return MNU_NONE; }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2);
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param);
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }
    
	void	Draw(SDL_Surface * bmpDest);

    void    addItem(int nID, const std::string& szName, bool checkable = false, bool checked = false);
	mnu_item_t *getItem(int nID);
	
	int		getMenuWidth() const { return m_nWidth; }
	int		getMenuHeight() const { return m_nHeight; }
	int		getMenuX() const { return m_nPosX; }
	int		getMenuY() const { return m_nPosY; }

};

}; // namespace DeprecatedGUI

#endif  //  __CMENU_H__DEPRECATED_GUI__
