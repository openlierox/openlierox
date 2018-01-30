/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////



#include "LieroX.h"
#include "Debug.h"
#include "DeprecatedGUI/CGuiSkinnedLayout.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Menu.h"
#include "StringUtils.h"
#include "DeprecatedGUI/CBox.h"
#include "DeprecatedGUI/CImage.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CSlider.h"
#include "DeprecatedGUI/CTextbox.h"


namespace DeprecatedGUI {

///////////////////
// Add a widget to the gui layout
void CGuiSkinnedLayout::Add(CWidget *widget, int id, int x, int y, int w, int h)
{
	widget->Setup(id, x + iOffsetX, y + iOffsetY, w, h);
	widget->setRedrawMenu(false);
	widget->Create();
	widget->setParent(this);

	// Link the widget in
	cWidgets.push_back(widget);
}

void CGuiSkinnedLayout::SetOffset( int x, int y )
{
	int diffX = x - iOffsetX;
	int diffY = y - iOffsetY;
	iOffsetX = x;
	iOffsetY = y; 

	for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++ ) 
	{
		(*w)->Setup( (*w)->getID(), (*w)->getX() + diffX, (*w)->getY() + diffY, (*w)->getWidth(), (*w)->getHeight() );
	}
}

///////////////////
// Remove a widget
void CGuiSkinnedLayout::removeWidget(int id)
{
	for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)  {
		if (id == (*w)->getID())  {

			// If this is the focused widget, set focused to null
			if(id == cFocused->getID())
				cFocused = NULL;

			// Free
			(*w)->Destroy();
			delete (*w);

			// Remove
			cWidgets.erase(w);

			break;
		}
	}
}


///////////////////
// Shutdown the gui layout
void CGuiSkinnedLayout::Shutdown()
{
	for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)  {
		(*w)->Destroy();
		delete (*w);
	}

	cWidgets.clear();

	cFocused = NULL;
	bFocusSticked = false;
	setParent(NULL);
	cChildLayout = NULL;
}


///////////////////
// Draw the widgets
void CGuiSkinnedLayout::Draw(SDL_Surface * bmpDest)
{
	if( ! cChildLayout || ( cChildLayout && ! bChildLayoutFullscreen ) )
		for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)
		{
			if( (*w)->getEnabled() )
				(*w)->Draw(bmpDest);
		}
	
	if( cChildLayout )
		cChildLayout->Draw(bmpDest);
}


///////////////////
// Process all the widgets
bool CGuiSkinnedLayout::Process()
{
	mouse_t *tMouse = GetMouse();
	keyboard_t *Keyboard = GetKeyboard();
	// int ev=-1; // We don't have any event structure here, all events are just passed down to children.

	//SetGameCursor(CURSOR_ARROW); // Reset the cursor here

	// Parse keyboard events
	for(int i = 0; i < Keyboard->queueLength; i++) {
		const KeyboardEvent& kbev = Keyboard->keyQueue[i];
		if(kbev.down)
			KeyDown(kbev.ch, kbev.sym, kbev.state);
		else
			KeyUp(kbev.ch, kbev.sym, kbev.state);
	}


	if( tMouse->Down )
		MouseDown(tMouse, tMouse->Down);
	if( tMouse->Up )
		MouseClicked(tMouse, tMouse->Down);
	if( tMouse->Up )
		MouseUp(tMouse, tMouse->Up);
	if( tMouse->WheelScrollDown )
		MouseWheelDown(tMouse);
	if( tMouse->WheelScrollUp )
		MouseWheelUp(tMouse);
	MouseOver(tMouse);
	
	return ! bExitCurrentDialog;
}


///////////////////
// Return a widget based on id
CWidget *CGuiSkinnedLayout::getWidget(int id)
{
	for( std::list<CWidget *>::iterator w = cWidgets.begin() ; w != cWidgets.end() ; w++)  {
		if((*w)->getID() == id)
			return (*w);
	}

	return NULL;
}

////////////////////
// Get the widget ID
int	CGuiSkinnedLayout::GetIdByName(const std::string & Name)
{
	int ID = -1;
	// Find the standard or previously added widget
	ID = LayoutWidgets.getID(Name);

	// Non-standard widget, add it to the list
	if (ID == -1)
		ID = LayoutWidgets.Add(Name);
	return ID;
}

////////////////////
// Notifies about the error that occured
void CGuiSkinnedLayout::Error(int ErrorCode, const std::string& desc)
{
	errors << "GUI skin error: " << ErrorCode << " " << desc << endl;
}

int CGuiSkinnedLayout::MouseOver(mouse_t *tMouse)
{
	if( cChildLayout )
	{
		cChildLayout->MouseOver(tMouse);
		return -1;
	}

	SetGameCursor(CURSOR_ARROW); // Set default mouse cursor - widget will change it
	if( cFocused && bFocusSticked )
	{
		int ev = cFocused->MouseOver(tMouse);
		if( ev >= 0 )
		{
			ProcessChildEvent( ev, cFocused );
		}
		return -1;
	}

	for( std::list<CWidget *>::reverse_iterator w = cWidgets.rbegin() ; w != cWidgets.rend() ; w++)
	{
		if(!(*w)->getEnabled())
			continue;
		if((*w)->InBox(tMouse->X,tMouse->Y))
		{
			int ev = (*w)->MouseOver(tMouse);
			if( ev >= 0 )
			{
				ProcessChildEvent( ev, (*w) );
			}
			return -1;
		}
	}
	return -1;
}

int CGuiSkinnedLayout::MouseUp(mouse_t *tMouse, int nDown)
{
	if( cChildLayout )
	{
		cChildLayout->MouseUp(tMouse, tMouse->Up);
		return -1;
	}

	bool bFocusStickedOld = bFocusSticked;
	if( tMouse->Down == 0 )
		bFocusSticked = false;

	if( cFocused && bFocusStickedOld )
	{
		int ev = cFocused->MouseUp(tMouse, nDown);
		if( ev >= 0 )
		{
			ProcessChildEvent( ev, cFocused );
		}
		return -1;
	}
	return -1;
}

int CGuiSkinnedLayout::MouseDown(mouse_t *tMouse, int nDown)
{
	if( cChildLayout )
	{
		cChildLayout->MouseDown(tMouse, nDown);
		return -1;
	}

	if( cFocused && bFocusSticked )
	{
		int ev = cFocused->MouseDown(tMouse, nDown);
		if( ev >= 0 )
		{
			ProcessChildEvent( ev, cFocused );
		}
		return -1;
	}

	for( std::list<CWidget *>::reverse_iterator w = cWidgets.rbegin() ; w != cWidgets.rend() ; w++)
	{
		if(!(*w)->getEnabled())
			continue;
		if((*w)->InBox(tMouse->X,tMouse->Y))
		{
			FocusOnMouseClick( *w );
			int ev = (*w)->MouseDown(tMouse, nDown);
			if( ev >= 0 )
			{
				ProcessChildEvent( ev, (*w) );
			}
			if( cFocused )
				bFocusSticked = true;
			return -1;
		}
	}

	// Click on empty space
	FocusOnMouseClick( NULL );
	return -1;
}

	
int CGuiSkinnedLayout::MouseClicked(mouse_t *tMouse, int nDown)
{
	if( cChildLayout )
	{
		cChildLayout->MouseClicked(tMouse, nDown);
		return -1;
	}
	
	if( cFocused && bFocusSticked && cFocused->InBox(tMouse->X, tMouse->Y) )
	{
		int ev = cFocused->MouseClicked(tMouse, nDown);
		if( ev >= 0 )
		{
			ProcessChildEvent( ev, cFocused );
			return -1;
		}
	}
	
	for( std::list<CWidget *>::reverse_iterator w = cWidgets.rbegin() ; w != cWidgets.rend() ; w++)
	{
		if(!(*w)->getEnabled())
			continue;
		if((*w)->InBox(tMouse->X, tMouse->Y))
		{
			FocusOnMouseClick( *w );
			int ev = (*w)->MouseClicked(tMouse, nDown);
			if( ev >= 0 )
			{
				ProcessChildEvent( ev, (*w) );
			}
			if( cFocused )
				bFocusSticked = true;
			return -1;
		}
	}
	
	// Click on empty space
	FocusOnMouseClick( NULL );
	return -1;
}
	
int CGuiSkinnedLayout::MouseWheelDown(mouse_t *tMouse)
{
	if( cChildLayout )
	{
		cChildLayout->MouseWheelDown(tMouse);
		return -1;
	}

	if( cFocused && bFocusSticked )
	{
		int ev = cFocused->MouseWheelDown(tMouse);
		if( ev >= 0 )
		{
			ProcessChildEvent( ev, cFocused );
		}
		return -1;
	}

	for( std::list<CWidget *>::reverse_iterator w = cWidgets.rbegin() ; w != cWidgets.rend() ; w++)
	{
		if(!(*w)->getEnabled())
			continue;
		if((*w)->InBox(tMouse->X,tMouse->Y))
		{
			int ev = (*w)->MouseWheelDown(tMouse);
			if( ev >= 0 )
			{
				ProcessChildEvent( ev, (*w) );
			}
			return -1;
		}
	}
	return -1;
}

int CGuiSkinnedLayout::MouseWheelUp(mouse_t *tMouse)
{
	if( cChildLayout )
	{
		cChildLayout->MouseWheelUp(tMouse);
		return -1;
	}

	if( cFocused && bFocusSticked )
	{
		int ev = cFocused->MouseWheelUp(tMouse);
		if( ev >= 0 )
		{
			ProcessChildEvent( ev, cFocused );
		}
		return -1;
	}

	for( std::list<CWidget *>::reverse_iterator w = cWidgets.rbegin() ; w != cWidgets.rend() ; w++)
	{
		if(!(*w)->getEnabled())
			continue;
		if((*w)->InBox(tMouse->X,tMouse->Y))
		{
			int ev = (*w)->MouseWheelUp(tMouse);
			if( ev >= 0 )
			{
				ProcessChildEvent( ev, (*w) );
			}
			return -1;
		}
	}
	return -1;
}

int CGuiSkinnedLayout::KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	if( cChildLayout )
	{
		cChildLayout->KeyDown(c, keysym, modstate);
		return -1;
	}

	FocusOnKeyPress(c, keysym, false);
	if ( cFocused )
	{
		if(!cFocused->getEnabled())
			return -1;
		int ev = cFocused->KeyDown(c, keysym, modstate);
		if( ev >= 0 )
		{
			ProcessChildEvent( ev, cFocused );
		}
	}

	return -1;
}

int CGuiSkinnedLayout::KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	if( cChildLayout )
	{
		cChildLayout->KeyUp(c, keysym, modstate);
		return -1;
	}

	FocusOnKeyPress(c, keysym, true);
	if ( cFocused )
	{
		if(!cFocused->getEnabled())
			return -1;
		int ev = cFocused->KeyUp(c, keysym, modstate);
		if( ev >= 0 )
		{
			ProcessChildEvent( ev, cFocused );
		}
	}
	return -1;
}

void CGuiSkinnedLayout::FocusOnMouseClick( CWidget * w )
{
		if( cFocused == w )
			return;
		if( cFocused )
		{
			cFocused->setFocused(false);
			cFocused = NULL;
		}
		if( w && cFocused == NULL )
		{
			w->setFocused(true);
			cFocused = w;
		}
}

void CGuiSkinnedLayout::FocusOnKeyPress(UnicodeChar c, int keysym, bool keyup)
{
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
}

void CGuiSkinnedLayout::FocusWidget(int id)
{
	FocusOnMouseClick( getWidget(id) );
};

CWidget * CGuiSkinnedLayout::getWidgetAtPoint(int x, int y)
{
	for( std::list<CWidget *>::reverse_iterator w = cWidgets.rbegin() ; w != cWidgets.rend() ; w++)
		if ((*w)->getEnabled() && (*w)->InBox(x, y))
			return (*w);

	return this;
}

void CGuiSkinnedLayout::ExitDialog( const std::string & param, CWidget * source )
{
	CGuiSkinnedLayout * lp = (CGuiSkinnedLayout *) source->getParent();
	lp->bExitCurrentDialog = true;
	CGuiSkinnedLayout * lpp = (CGuiSkinnedLayout *) lp->getParent();
	if( lpp != NULL )
		lpp->cChildLayout = NULL;
	lp->setParent( NULL );
}

void CGuiSkinnedLayout::ChildDialog( const std::string & param, CWidget * source )
{
	CGuiSkinnedLayout * lp = (CGuiSkinnedLayout *) source->getParent();
	if( lp->cChildLayout != NULL )
		return;
	// Simple parsing of params
	std::vector<std::string> params = explode(param, ",");
	for( unsigned i=0; i<params.size(); i++ )
		TrimSpaces(params[i]);
	int x = 0, y = 0;
	bool fullscreen = false;
	if( params.size() > 1 )
		if( params[1] == "fullscreen" )
			fullscreen = true;
	if( params.size() > 2 )
	{
		x = atoi( params[1] );
		y = atoi( params[2] );
	}
	std::string file = params[0];
}

void CGuiSkinnedLayout::SetTab( const std::string & param, CWidget * source )
{
}

}; // namespace DeprecatedGUI
