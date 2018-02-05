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
// Created 5/6/02
// Jason Boettcher


#include <assert.h>


#include "LieroX.h"
#include "Debug.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Menu.h"
#include "StringUtils.h"
#include "Sounds.h"
#include "DeprecatedGUI/CBox.h"
#include "DeprecatedGUI/CImage.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CSlider.h"
#include "DeprecatedGUI/CTextbox.h"
#include "OLXConsole.h"
#include "sound/SoundsBase.h"



namespace DeprecatedGUI {


bool CGuiLayout::bKeyboardNavigation = true;


///////////////////
// Initialize the layout
void CGuiLayout::Initialize(int LayoutID)
{
	Shutdown();

	iID = LayoutID;

	cWidgets.clear();
	cFocused = NULL;
	cMouseOverWidget = NULL;

	// Reset mouse repeats
	nMouseButtons = 0;
	for(int i=0; i<3; i++)
		fMouseNext[i] = AbsTime();

}


///////////////////
// Add a widget to the gui layout
void CGuiLayout::Add(CWidget *widget, int id, int x, int y, int w, int h)
{
	if(!widget) {
		errors << "CGuiLayout::Add: widget is unset" << endl;
		return;
	}
	
	widget->Setup(id, x, y, w, h);
	widget->Create();
	widget->setParent(this);

	// Link the widget in
	cWidgets.push_front(widget);
}

void CGuiLayout::AddBack(CWidget *widget, int id, int x, int y, int w, int h)
{
	if(!widget) {
		errors << "CGuiLayout::Add: widget is unset" << endl;
		return;
	}
	
	widget->Setup(id, x, y, w, h);
	widget->Create();
	widget->setParent(this);
	
	// Link the widget in
	cWidgets.push_back(widget);
}
	

///////////////////
// Remove a widget
void CGuiLayout::removeWidget(int id)
{
    for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)  {
		if (id == (*w)->getID())  {

			// If this is the focused widget, set focused to null
			if(cFocused) {
				if(id == cFocused->getID())  {
					cFocused = NULL;
					bCanFocus = true;
				}
			}

			// Free it
			(*w)->Destroy();
			delete *w;

			cWidgets.erase(w);

			break;
		}
	}
}

/////////////////////////
// Focus a widget
void CGuiLayout::FocusWidget(int id)
{
    for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)  {
		if (id == (*w)->getID())  {

			// Take off focus from any previously focused widget
			if(cFocused) {
				if(id == cFocused->getID() || !cFocused->CanLoseFocus())
					break;
				cFocused->setFocused(false);
				cFocused = NULL;
			}

			(*w)->setFocused(true);
			cFocused = (*w);

			break;
		}
	}
}


///////////////////
// Shutdown the gui layout
void CGuiLayout::Shutdown()
{
	for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)  {
		(*w)->Destroy();
		delete (*w);
	}
	cWidgets.clear();

	// tEvent is freed in destructor

	cFocused = NULL;
	cMouseOverWidget = NULL;
	
	tooltip = NULL;
}

}

struct TooltipIntern {
	std::string text;
	DeprecatedGUI::CBox box;
	SDL_Rect keepArea;
	TooltipIntern() : box(0, 2, tLX->clBoxLight, tLX->clBoxDark, tLX->clDialogBackground) {}

	void setPos(const VectorD2<int>& p) { box.Setup(0, p.x, p.y, box.getWidth(), box.getHeight()); }
	
	void setText(const std::string& t) {
		text = t;
		VectorD2<int> txtSize;
		txtSize.x = tLX->cFont.GetWidth(text);
		txtSize.y = tLX->cFont.GetHeight(text);
		if(txtSize.x + 2 != box.getWidth() || txtSize.y + 2 != box.getHeight()) {
			box.Setup(0, box.getX(), box.getY(), txtSize.x + 2, txtSize.y + 2);
			box.PreDraw();
		}
	}
	
	void setKeepArea(const SDL_Rect& a) { keepArea = a; }
	
	void draw(SDL_Surface* dst) {
		box.Draw(dst);
		tLX->cFont.Draw(dst, box.getX() + 1, box.getY() + 1, tLX->clNormalLabel, text);
	}
};

template <> void SmartPointer_ObjectDeinit<TooltipIntern> ( TooltipIntern * obj ) {
	delete obj;
}

namespace DeprecatedGUI {

///////////////////
// Draw the widgets
void CGuiLayout::Draw(SDL_Surface * bmpDest)
{
	if(bmpDest == NULL) {
		errors << "CGuiLayout::Draw: bmpDest == NULL" << endl;
		return;
	}

	if(bmpDest->format == NULL) {
		errors << "CGuiLayout::Draw: bmpDest->format == NULL" << endl;
		return;
	}
	
	// Draw the widgets in reverse order
	for( std::list<CWidget *>::reverse_iterator w = cWidgets.rbegin() ; w != cWidgets.rend() ; w++)  {
		if((*w)->getEnabled())
			(*w)->Draw(bmpDest);
	}
	
	if(tooltip.get())
		tooltip->draw(bmpDest);
}


//////////////////
// Build the layout according to code specified in skin file
bool CGuiLayout::Build()
{
	return true;
}


///////////////////
// Process all the widgets
gui_event_t *CGuiLayout::Process()
{
	mouse_t *tMouse = GetMouse();
	int ev=-1;
	int widget = false;

	SetGameCursor(CURSOR_ARROW); // Reset the cursor here

	if(tooltip.get() && !PointInRect(tMouse->X, tMouse->Y, tooltip->keepArea))
		tooltip = NULL;
	
	if (!tEvent)  {
		// TODO: when can this happen? is this an error? if so, why no error msg?
		// TODO: what is tEvent? why is it global and not local?
		return NULL;
	}

	// Put it here, so the mouse will never display
	
	EnableSystemMouseCursor(false);

	// If the application has lost the focus, remove set all CanLoseFocus to false
	// as they make no sense anymore and can make trouble
	if (cFocused && !ApplicationHasFocus())
		cFocused->setLoseFocus(true);

	bool widgetSelectedWithKeyboard = false;

	// Parse keyboard events to the focused widget
	// Make sure a key event happened
	keyboard_t *Keyboard = GetKeyboard();
	if(!Con_IsVisible() && Keyboard->queueLength > 0) {

		// Process Escape key in menus
		for(int i = 0; i < Keyboard->queueLength; i++) {
			const KeyboardEvent& kbev = Keyboard->keyQueue[i];
			if(!kbev.down && kbev.sym == SDLK_ESCAPE) {
				// Try to find 'Resume game' button first, hitting Escape during the match should go back to the match
				for(auto widget: cWidgets) {
					if(widget && widget->getType() == wid_Button &&
						((CButton *) widget)->getImageID() == BUT_RESUME) {
						ev = widget->MouseClicked(GetMouse(), 1);
						tEvent->iEventMsg = ev;
						tEvent->iControlID = widget->getID();
						tEvent->cWidget = widget;
						return tEvent;
					}
				}
				// Try to find 'Back' or 'Quit' button
				for(auto widget: cWidgets) {
					if(widget && widget->getType() == wid_Button && (
						((CButton *) widget)->getImageID() == BUT_BACK ||
						((CButton *) widget)->getImageID() == BUT_QUIT ||
						((CButton *) widget)->getImageID() == BUT_QUITGAME ||
						((CButton *) widget)->getImageID() == BUT_LEAVE ||
						((CButton *) widget)->getImageID() == BUT_CANCEL ||
						((CButton *) widget)->getImageID() == BUT_NO)) {
						ev = widget->MouseClicked(GetMouse(), 1);
						tEvent->iEventMsg = ev;
						tEvent->iControlID = widget->getID();
						tEvent->cWidget = widget;
						return tEvent;
					}
				}
				// Some dialogs have only 'OK' button, press it if nothing else works
				for(auto widget: cWidgets) {
					if(widget && widget->getType() == wid_Button &&
						((CButton *) widget)->getImageID() == BUT_OK) {
						ev = widget->MouseClicked(GetMouse(), 1);
						tEvent->iEventMsg = ev;
						tEvent->iControlID = widget->getID();
						tEvent->cWidget = widget;
						return tEvent;
					}
				}
			}
		}

		// If we don't have any focused widget, get the first textbox
		if (!cFocused)  {
			for( std::list<CWidget *>::iterator txt = cWidgets.begin() ; txt != cWidgets.end() ; txt++)  {
				if ((*txt)->getType() == wid_Textbox && (*txt)->getEnabled()) {
					cFocused = *txt;
					(*txt)->setFocused(true);
					break;
				}
			}
		}


		if (cFocused)  {

			// Check the characters
			for(int i = 0; i < Keyboard->queueLength; i++) {
				const KeyboardEvent& kbev = Keyboard->keyQueue[i];
				if(kbev.down)
					ev = cFocused->KeyDown(kbev.ch, kbev.sym, kbev.state);
				else
					ev = cFocused->KeyUp(kbev.ch, kbev.sym, kbev.state);
			}


			if(ev >= 0) {
				tEvent->iEventMsg = ev;
				tEvent->iControlID = cFocused->getID();
				tEvent->cWidget = cFocused;
				return tEvent;
			}
		}

		// Navigate menu with arrow keys
		for(int i = 0; i < Keyboard->queueLength; i++) {
			const KeyboardEvent& kbev = Keyboard->keyQueue[i];

			if (kbev.down && (kbev.sym == SDLK_UP || kbev.sym == SDLK_LEFT || kbev.sym == SDLK_DOWN || kbev.sym == SDLK_RIGHT)) {

				bKeyboardNavigation = true;
				CWidget * selected = NULL;

				if (!bCanFocus || cFocused == NULL || !cFocused->CanLoseFocus()) {
					// Do nothing
				} else if (kbev.sym == SDLK_UP || kbev.sym == SDLK_LEFT) {
					int posmin = 0;
					int posmax = cFocused->getX() + cFocused->getY() * 0x10000 + cFocused->getKeyboardNavigationOrder() * 0x10000000;
					for(auto widget: cWidgets) {
						if (!widget->getEnabled() || widget == cFocused || widget->getType() == wid_Label || widget->getType() == wid_None)
							continue;
						int pos = widget->getX() + widget->getY() * 0x10000 + widget->getKeyboardNavigationOrder() * 0x10000000;
						if (posmin < pos && posmax > pos) {
							posmin = pos;
							selected = widget;
						}
					}
				} else if (kbev.sym == SDLK_DOWN || kbev.sym == SDLK_RIGHT) {
					int posmin = cFocused->getX() + cFocused->getY() * 0x10000 + cFocused->getKeyboardNavigationOrder() * 0x10000000;
					int posmax = INT_MAX;
					for(auto widget: cWidgets) {
						if (!widget->getEnabled() || widget == cFocused || widget->getType() == wid_Label || widget->getType() == wid_None)
							continue;
						int pos = widget->getX() + widget->getY() * 0x10000 + widget->getKeyboardNavigationOrder() * 0x10000000;
						if (posmin < pos && posmax > pos) {
							posmax = pos;
							selected = widget;
						}
					}
				}

				if (cFocused && selected && cFocused != selected) {
					cFocused->setFocused(false);
					cFocused = selected;
					widgetSelectedWithKeyboard = true;
				}
			}
		}
	}

	if (!cFocused && bKeyboardNavigation) {
		int posmax = INT_MAX;
		for(auto widget: cWidgets) {
			if (!widget->getEnabled() || widget->getType() == wid_Label || widget->getType() == wid_None)
				continue;
			int pos = widget->getX() + widget->getY() * 0x10000 + widget->getKeyboardNavigationOrder() * 0x10000000;
			if (posmax > pos) {
				posmax = pos;
				cFocused = widget;
			}
		}
		widgetSelectedWithKeyboard = true;
	}

	if (cFocused && widgetSelectedWithKeyboard) {
		cFocused->setFocused(true);
		struct RepositionMouse: public Action
		{
			int x, y;
			RepositionMouse(int _x, int _y): x(_x), y(_y)
			{
			}
			Result handle()
			{
				SDL_WarpMouseInWindow(NULL, x, y);
				return true;
			}
		};
		doActionInMainThread( new RepositionMouse(cFocused->getX() + 1, cFocused->getY() + cFocused->getHeight() - 2) );
		PlaySoundSample(sfxGeneral.smpClick);
	}

	// Special mouse button event first (for focused widgets)
	// !bCanFocus -> we are currently holding an object (pressed moused but didn't released it)
	if(cFocused && !bCanFocus) {		
		widget = true;
		
		if(tMouse->Down) {
			// Process the skin-defined code
			cFocused->ProcessEvent(OnMouseDown);
			
			if( (ev = cFocused->MouseDown(tMouse, tMouse->Down)) >= 0) {
				tEvent->iEventMsg = ev;
				tEvent->iControlID = cFocused->getID();
				tEvent->cWidget = cFocused;
				return tEvent;
			}
		}
		else if(tMouse->Up) {
			bCanFocus = true;
			bKeyboardNavigation = false; // Mouse click disables key navigation mode
			// Process the skin defined code
			// OnClick is only fired if mouseup was done over the widget
			if(cFocused->InBox(tMouse->X,tMouse->Y))
				cFocused->ProcessEvent(OnClick);
			
			if(cFocused->InBox(tMouse->X,tMouse->Y))
				if( (ev = cFocused->MouseClicked(tMouse, tMouse->Up)) >= 0) {
					tEvent->iEventMsg = ev;
					tEvent->iControlID = cFocused->getID();
					tEvent->cWidget = cFocused;
					return tEvent;
				}

			if( (ev = cFocused->MouseUp(tMouse, tMouse->Up)) >= 0) {
				tEvent->iEventMsg = ev;
				tEvent->iControlID = cFocused->getID();
				tEvent->cWidget = cFocused;
				return tEvent;
			}
			
			return NULL;
		}
		else
			// TODO: in what cases can this happen?
			bCanFocus = true;
	}
	
	// Go through all the widgets
	for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)  {
		tEvent->cWidget = *w;
		tEvent->iControlID = (*w)->getID();

		// Don't process disabled widgets
		if(!(*w)->getEnabled())
			continue;

		if((*w)->InBox(tMouse->X,tMouse->Y)) {

			// Mouse wheel up
			if(tMouse->WheelScrollUp)  {
				widget = true;
				/*if(cFocused)
					cFocused->setFocused(false);
				(*w)->setFocused(true);
				cFocused = *w;*/

				if( (ev = (*w)->MouseWheelUp(tMouse)) >= 0) {
					tEvent->iEventMsg = ev;
					return tEvent;
				}
			}

			// Mouse wheel down
			if(tMouse->WheelScrollDown)  {
				widget = true;
				/*if(cFocused)
					cFocused->setFocused(false);
				(*w)->setFocused(true);
				cFocused = *w;*/

				if( (ev = (*w)->MouseWheelDown(tMouse)) >= 0) {
					tEvent->iEventMsg = ev;
					return tEvent;
				}
			}

			// Mouse button event first
			if(tMouse->Down) {

				widget = true;
				if (bCanFocus)  {
					if(cFocused)  {
						if (cFocused->CanLoseFocus())  {
							cFocused->setFocused(false);
							(*w)->setFocused(true);
							cFocused = *w;
							bCanFocus = false;
						}
					}
					else  {
						(*w)->setFocused(true);
						cFocused = *w;
						bCanFocus = false;
					}

				}

				// Process the skin defined code
				(*w)->ProcessEvent(OnMouseDown);

				if( (ev = (*w)->MouseDown(tMouse, tMouse->Down)) >= 0) {
					tEvent->iEventMsg = ev;
					return tEvent;
				}
			}

			// TODO: i don't understand that. mouseup should always only be sent to the widget where we have sent the mousedown
			/*
			// Mouse up event
			if(tMouse->Up) {
				bCanFocus = true;
				widget = true;
				if(cFocused)  {
					if(cFocused->CanLoseFocus())  {
						cFocused->setFocused(false);
						(*w)->setFocused(true);
						cFocused = *w;
					}
				}
				else  {
					(*w)->setFocused(true);
					cFocused = *w;
				}

				// Process the skin defined code
				(*w)->ProcessEvent(OnClick);

				if( (ev = (*w)->MouseUp(tMouse, tMouse->Up)) >= 0) {
					tEvent->iEventMsg = ev;
					return tEvent;
				}
			}
			 */

			// Mouse over
			if (*w != cMouseOverWidget)  {
				(*w)->ProcessEvent(OnMouseOver);

				// For the current Mouse over widget this means a mouse out event
				if(cMouseOverWidget)
					cMouseOverWidget->ProcessEvent(OnMouseOut);

				cMouseOverWidget = *w;
			}

			if( (ev = (*w)->MouseOver(tMouse)) >= 0) {
				widget = true;
				tEvent->iEventMsg = ev;
				return tEvent;
			}

			// -2 - the widget says, that no event happened (various reasons)
			if (ev != -2)
				return NULL;
		}
	}

	// If mouse is over empty space it means, it's not over any widget ;-)
	if (cMouseOverWidget)  {
		cMouseOverWidget->ProcessEvent(OnMouseOut);
		cMouseOverWidget = NULL;
	}

	// If the mouse is clicked on empty space, take the focus of off the widget (if we can)
	if(!widget && (tMouse->Up)) {
		bCanFocus = true;
		if(cFocused)  {
			// Can we take the focus off?
			if (cFocused->CanLoseFocus())  {
				cFocused->setFocused(false);
				cFocused = NULL;
			}
			else  {
				cFocused->MouseUp(tMouse, tMouse->Up);
				cFocused->setLoseFocus(true);
			}
		}
		else  {
			cFocused = NULL;
		}
	}

	// Non-widget wheel up
	if(tMouse->WheelScrollUp)  {
		tEvent->iControlID = gev_MouseWheel;
		tEvent->iEventMsg = gev_MouseWheelUp;
		return tEvent;
	}

	// Non-widget wheel down
	if(tMouse->WheelScrollDown)  {
		tEvent->iControlID = gev_MouseWheel;
		tEvent->iEventMsg = gev_MouseWheelDown;
		return tEvent;
	}


	return NULL;
}


///////////////////
// Return a widget based on id
CWidget *CGuiLayout::getWidget(int id)
{
	for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)  {
		if((*w)->getID() == id)
			return *w;
	}

	return NULL;
}

////////////////////
// Get the widget ID
int	CGuiLayout::GetIdByName(const std::string& Name)
{
	int ID = -1;
	// Find the standard or previously added widget
	ID = LayoutWidgets[iID].getID(Name);

	// Non-standard widget, add it to the list
	if (ID == -1)
		ID = LayoutWidgets[iID].Add(Name);
	return ID;
}

////////////////////
// Notifies about the error that occured
void CGuiLayout::Error(int ErrorCode, const std::string& Text)
{
	errors << "CGuiLayout: " << ErrorCode << " - " << Text << endl;
}

///////////////
// Set a property for all widgets
void CGuiLayout::SetGlobalProperty(int property, int value)
{
# define FOREACH for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)

	// Set the property
	switch (property)  {
	case PRP_REDRAWMENU:
		FOREACH (*w)->setRedrawMenu(value != 0);
		break;
	case PRP_ENABLED:
		FOREACH (*w)->setEnabled(value != 0);
		break;
	case PRP_ID:
		FOREACH (*w)->setID(value);
		break;
	default:
		errors  << "CGuiLayout::SetGlobalProperty: unknown property" << endl;
	}

#undef FOREACH

}


///////////////////
// Send a message to a widget
uintptr_t CGuiLayout::SendMessage(int iControl, int iMsg, uintptr_t Param1, uintptr_t Param2)
{
	CWidget *w = getWidget(iControl);

	// Couldn't find widget
	if(w == NULL)
		return 0;

	// Check if it's a widget message
	if(iMsg < 0) {
		switch( iMsg ) {

			// Set the enabled state of the widget
			case WDM_SETENABLE:
				w->setEnabled(Param1 != 0);
				break;
		}
		return 0;
	}

	return w->SendMessage(iMsg, Param1, Param2);
}

uintptr_t CGuiLayout::SendMessage(int iControl, int iMsg, const std::string& sStr, uintptr_t Param)
{
	CWidget *w = getWidget(iControl);

	// Couldn't find widget
	if(w == NULL)
		return 0;

	return w->SendMessage(iMsg, sStr, Param);

}

uintptr_t CGuiLayout::SendMessage(int iControl, int iMsg, std::string *sStr, uintptr_t Param)
{
	// Check the string
	if (!sStr)
		return 0;

	CWidget *w = getWidget(iControl);

	// Couldn't find widget
	if(w == NULL)
		return 0;

	return w->SendMessage(iMsg, sStr, Param);
}

void CGuiLayout::setTooltip(const SDL_Rect& keepArea, VectorD2<int> pos, const std::string& msg) {
	if(msg == "") {
		tooltip = NULL;
		return;
	}
	
	if(tooltip.get() == NULL) tooltip = new TooltipIntern();
	tooltip->setText(msg);
	tooltip->setPos(pos);
	tooltip->setKeepArea(keepArea);
}
	
	
}; // namespace DeprecatedGUI
