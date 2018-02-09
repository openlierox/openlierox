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


#include <assert.h>
#include "LieroX.h"

#include "GfxPrimitives.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Menu.h"
#include "DeprecatedGUI/CMenu.h"


namespace DeprecatedGUI {

///////////////////
// CMenu constructor
CMenu::CMenu(int nPosX, int nPosY)
{
    m_nPosX = nPosX;
    m_nPosY = nPosY;
	m_nSelectedIndex = -1;
	m_bContainsCheckableItems = false;

    m_nHeight = 0;
    m_nWidth = 0;
}


///////////////////
// Handle a menu message
uintptr_t CMenu::SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)
{
    switch(iMsg) {

        // Redraw the area to the buffer
        case MNM_REDRAWBUFFER:
            Menu_redrawBufferRect(m_nPosX, m_nPosY, m_nWidth+1,m_nHeight+1);
            return 0;
    }

    return 0;
}

uintptr_t CMenu::SendMessage(int iMsg, const std::string& sStr, uintptr_t Param)
{
	switch (iMsg)  {

	// Add an item
	case MNS_ADDITEM:
		addItem(Param,sStr);
		return 0;

	}

	return 0;
}


///////////////////
// Create the menu
void CMenu::Create()
{
    m_nHeight = 0;
    m_nWidth = 0;
	m_nSelectedIndex = -1;
	m_bContainsCheckableItems = false;

	m_psItemList.clear();
}


///////////////////
// Destroy the menu
void CMenu::Destroy()
{
	m_psItemList.clear();
}


///////////////////
// Add an item to the menu
void CMenu::addItem(int nID, const std::string& szName, bool checkable, bool checked)
{
    mnu_item_t i;

    i.nID = nID;
    i.szName = szName;
	i.bChecked = checked;
	i.bCheckable = checkable || checked;

    m_nHeight += (tLX->cFont.GetHeight() * 3 / 2);
    m_nHeight = MAX(m_nHeight, (tLX->cFont.GetHeight() * 3 / 2)+4);
	if (!m_bContainsCheckableItems && i.bCheckable)
		m_nWidth = MAX(m_nWidth + tLX->cFont.GetHeight() + 5, tLX->cFont.GetWidth(szName) + tLX->cFont.GetHeight() + 15 );
	else
		m_nWidth = MAX(m_nWidth, tLX->cFont.GetWidth(szName)+10 + (m_bContainsCheckableItems ? tLX->cFont.GetHeight() + 5 : 0) );
	m_bContainsCheckableItems = m_bContainsCheckableItems || i.bCheckable;

    // Link it in at the end
	m_psItemList.push_back(i);
}

///////////////////
// Get an item based on its index
mnu_item_t *CMenu::getItem(int nID)
{
	for (std::list<mnu_item_t>::iterator i = m_psItemList.begin(); i != m_psItemList.end(); i++)  {
		if (i->nID == nID)
			return &(*i);
	}

	return NULL;
}


///////////////////
// Draw the menu
void CMenu::Draw(SDL_Surface * bmpDest)
{
	int X = m_nPosX;
	int Y = m_nPosY;
	int W = m_nWidth;
	int H = m_nHeight;
	// Flip the menu if it doesnt fit the surface (screen)
	if (Y+H > bmpDest->h)
		Y = Y-H;
	if (X+W > bmpDest->w)
		X = X-W;

    Menu_redrawBufferRect(X+1, Y+1, W-1,H-1);

    DrawRectFill(bmpDest, X+1, Y+1, X+W-1, Y+H-1, tLX->clMenuBackground);
	if (bRedrawMenu)
		Menu_DrawBox(bmpDest, X, Y, X+W, Y+H);

	std::list<mnu_item_t>::iterator it = m_psItemList.begin();
    int y = Y+2;
	int x = m_bContainsCheckableItems ? X + 10 + tLX->cFont.GetHeight() : X + 5;
	for(int i=0; it != m_psItemList.end(); it++, i++) {

        if( m_nSelectedIndex == i )
            DrawRectFill(bmpDest, X+2,y,  X+W-1, y+(tLX->cFont.GetHeight() * 3 / 2), tLX->clMenuSelected);            
        tLX->cFont.Draw(bmpDest, x, y + tLX->cFont.GetHeight() / 6, tLX->clPopupMenu, it->szName);

		// Draw the check
		if (it->bCheckable && it->bChecked)  {
			int fh = tLX->cFont.GetHeight() / 2 + tLX->cFont.GetHeight() / 6;
			AntiAliasedLine(bmpDest, X + 5, y + fh + 2, X + 2 + fh, y + fh*2 - 3, tLX->clPopupMenu, PutPixelA);
			AntiAliasedLine(bmpDest, X + 2 + fh, y + fh*2 - 3, X + 2 + fh*2, y + 3, tLX->clPopupMenu, PutPixelA);
			AntiAliasedLine(bmpDest, X + 5, y + fh + 1, X + 2 + fh, y + fh*2 - 4, tLX->clPopupMenu, PutPixelA);
			AntiAliasedLine(bmpDest, X + 2 + fh, y + fh*2 - 4, X + 2 + fh*2, y + 2, tLX->clPopupMenu, PutPixelA);
			PutPixel(bmpDest, X + 5, y + fh + 2, tLX->clMenuBackground.get(bmpDest->format));
		}

        y += (tLX->cFont.GetHeight() * 3 / 2);
    }

	m_nPosX = X;
	m_nPosY = Y;
}


///////////////////
// Move over event
int CMenu::MouseOver(mouse_t *tMouse)
{
	if( !MouseInRect(m_nPosX, m_nPosY, m_nWidth, m_nHeight) )  {
		m_nSelectedIndex = -1;
        return MNU_NONE;
	}

    int y = m_nPosY + 2;
    std::list<mnu_item_t>::iterator it = m_psItemList.begin();
	for(int i = 0; it != m_psItemList.end(); it++, i++) {

        if( tMouse->Y > y && tMouse->Y < y + (tLX->cFont.GetHeight() * 3 / 2) ) {
			m_nSelectedIndex = i;
            break;
        }

        y += (tLX->cFont.GetHeight() * 3 / 2);
    }

    return MNU_NONE;
}


///////////////////
// Mouse Up event
int CMenu::MouseUp(mouse_t *tMouse, int nDown)
{
    // Lose focus?
    if( !MouseInRect(m_nPosX, m_nPosY, m_nWidth, m_nHeight) )
        return MNU_LOSTFOCUS;

    int y = m_nPosY + 2;
    std::list<mnu_item_t>::iterator it = m_psItemList.begin();
	for(; it != m_psItemList.end(); it++) {

		if( tMouse->Y >= y && tMouse->Y < y + (tLX->cFont.GetHeight() * 3 / 2) )  {
			if (it->bCheckable)
				it->bChecked = !it->bChecked;
            return MNU_USER + it->nID;
		}

        y += (tLX->cFont.GetHeight() * 3 / 2);
    }

    return MNU_NONE;
}


///////////////////
// Mouse down event
int CMenu::MouseDown(mouse_t *tMouse, int nDown)
{
    // Lose focus?
	if( !MouseInRect(m_nPosX, m_nPosY, m_nWidth, m_nHeight) )  {
		m_nSelectedIndex = -1;
        return MNU_LOSTFOCUS;
	}

    return MNU_NONE;
}

////////////////////
// Key down event
int CMenu::KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	switch (keysym)  {
		case SDLK_DOWN:
			if (m_nSelectedIndex == -1)  {
				if (!m_psItemList.empty()) {
					m_nSelectedIndex = 0;
				}
			} else {
				if (m_psItemList.empty())
					m_nSelectedIndex = -1;
				else if (m_nSelectedIndex + 1 < (int)m_psItemList.size()) {
					m_nSelectedIndex++;
				}
			}
			MoveMouseToCurrentItem();
			return MNU_SELECTION_CHANGED;
		break;

		case SDLK_UP:
			if (m_nSelectedIndex == -1 && !m_psItemList.empty()) {
				m_nSelectedIndex = m_psItemList.size() - 1;
			} else {
				if (m_psItemList.empty())
					m_nSelectedIndex = -1;
				else if (m_nSelectedIndex > 0) {
					m_nSelectedIndex--;
				}
			}
			MoveMouseToCurrentItem();
			return MNU_SELECTION_CHANGED;
		break;

		case SDLK_RETURN:
		case SDLK_KP_ENTER:
		case SDLK_LALT:
		case SDLK_LCTRL:
		case SDLK_LSHIFT:
		case SDLK_x:
		case SDLK_z:
			if (m_nSelectedIndex >= 0 && m_nSelectedIndex < (int)m_psItemList.size()) {
				std::list<mnu_item_t>::iterator it = m_psItemList.begin();
				for(int i = 0; i < m_nSelectedIndex && it != m_psItemList.end(); i++)
					it++;
				if (it->bCheckable)
					it->bChecked = !it->bChecked;
				return MNU_USER + it->nID;
			}
		break;
	}

	return MNU_NONE;
}

void CMenu::MoveMouseToCurrentItem()
{
	if (!Menu_IsKeyboardNavigationUsed() || !getFocused() || m_nSelectedIndex == -1)
		return;

	int y = m_nPosY + 2 + (m_nSelectedIndex + 1) * (tLX->cFont.GetHeight() * 3 / 2);

	struct RepositionMouse: public Action
	{
		int x, y;
		RepositionMouse(int _x, int _y): x(_x), y(_y)
		{
		}
		int handle()
		{
			SDL_WarpMouse(x, y);
			return true;
		}
	};
	doActionInMainThread( new RepositionMouse(m_nPosY + 2, y - 1) );
}

}; // namespace DeprecatedGUI
