/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////

#ifndef __CGUISKINNEDLAYOUT_H__DEPRECATED_GUI__
#define __CGUISKINNEDLAYOUT_H__DEPRECATED_GUI__

#include "DeprecatedGUI/CWidget.h"
#include "DeprecatedGUI/CGuiLayout.h"
#include "DeprecatedGUI/CWidgetList.h"
#include <string>
#include <map>
#include <vector>

class ScriptVar_t;

namespace DeprecatedGUI {

// Almost exact copy of CGuiLayout but without references to global "LayoutWidgets" var and without global ID
class CGuiSkinnedLayout: public CWidget, public CGuiLayoutBase
{
public:
	// Constructor
	CGuiSkinnedLayout( int x = 0, int y = 0 ) {
		cFocused = NULL;
		bFocusSticked = false;
		//cMouseOverWidget = NULL;
		//iCanFocus = true;
		iOffsetX = x;
		iOffsetY = y;
		iType = wid_GuiLayout;
		bExitCurrentDialog = false;
		setParent( NULL );
		cChildLayout = NULL;
	}

	// Destructor
	~CGuiSkinnedLayout() { Shutdown(); }

private:
	// Attributes

	std::list<CWidget *>	cWidgets;
	CWidget			*cFocused;
	bool			bFocusSticked;	// if user hold mouse button over some widget the focus sticks to that widget, analog to CGuiLayout::iCanFocus which is never used

	bool		bExitCurrentDialog;	// Used to exit to MainMenu - remove when only skinned GUI will exist
	CGuiSkinnedLayout	*cChildLayout;
	bool		bChildLayoutFullscreen;	// Do not redraw itself if child is fullscreen

	CWidgetList	LayoutWidgets;
	int			iOffsetX, iOffsetY;	// Top-left corner of layout (just offset, may be negative)

	void		FocusOnKeyPress(UnicodeChar c, int keysym, bool keyup);
	void 		FocusOnMouseClick( CWidget * w );

public:
	// Methods

	void		Add(CWidget *widget, int id, int x, int y, int w, int h);
	CWidget		*getWidget(int id);
	CWidget		*getWidgetAtPoint(int x, int y);
	CWidget		*getWidget(const std::string & name){ return getWidget(GetIdByName(name)); };
    void        removeWidget(int id);
	int			GetIdByName(const std::string & name);
	void		Error(int ErrorCode, const std::string& desc);
	void		SetOffset( int x, int y );
	void		FocusWidget(int id);
	CWidget		*getFocused() const { return cFocused; }

	bool		Process();	// Called only for main layout -dispatches messages to children, returns false on exit layout
	virtual void Draw(SDL_Surface * bmpDest);

	void		Shutdown();

	void	Create() { };
	void	Destroy() { Shutdown(); }

	// CWidget functions
	// These event handlers will route events to children event handlers
	int		MouseOver(mouse_t *tMouse);
	int		MouseUp(mouse_t *tMouse, int nDown);
	int		MouseDown(mouse_t *tMouse, int nDown);
	int		MouseClicked(mouse_t *tMouse, int nDown);
	int		MouseWheelDown(mouse_t *tMouse);
	int		MouseWheelUp(mouse_t *tMouse);
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate);
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate);

	// Other functions are empty, because GUI layout don't do any actions itself
	uintptr_t	SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2) { return 0; };
	uintptr_t	SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; };
	uintptr_t	SendMessage(int iMsg, std::string *sStr, uintptr_t Param) { return 0; };

	virtual void ProcessChildEvent(int iEvent, CWidget * child) { };

	static void ExitDialog( const std::string & param, CWidget * source );
	static void ChildDialog( const std::string & param, CWidget * source );
	static void SetTab( const std::string & param, CWidget * source );
};

} // namespace DeprecatedGUI

#endif
