/*
 *  Unit tests for CGuiLayout keyboard navigation (CGuiLayout.h).
 *
 *  Regression test for #815
 *  ("Using arrow keys to move text cursor messes up mouse"):
 *  while a text box is focused the arrow keys move its text cursor,
 *  so menu arrow-key navigation must not also fire
 *  and warp the mouse to another widget.
 */

#include "unittest.h"
#include "DeprecatedGUI/CGuiLayout.h"
#include "DeprecatedGUI/CWidget.h"
#include "DeprecatedGUI/CTextbox.h"

#include <SDL.h>

using namespace DeprecatedGUI;

namespace {

// A minimal navigable widget, so the layout test needs no fonts or graphics.
// handlesArrowKeys() is settable to model both a plain button (false)
// and a text-editing widget that owns the arrow keys (true).
class TestWidget : public CWidget {
public:
	TestWidget(WidgetType_t type, bool ownsArrows)
		: bOwnsArrows(ownsArrows) { iType = type; }

	bool	handlesArrowKeys() const override { return bOwnsArrows; }

	void	Create() override {}
	void	Destroy() override {}
	int		MouseOver(mouse_t*) override { return -1; }
	int		MouseUp(mouse_t*, int) override { return -1; }
	int		MouseDown(mouse_t*, int) override { return -1; }
	int		MouseWheelUp(mouse_t*) override { return -1; }
	int		MouseWheelDown(mouse_t*) override { return -1; }
	int		KeyDown(UnicodeChar, int, const ModifiersState&) override { return -1; }
	int		KeyUp(UnicodeChar, int, const ModifiersState&) override { return -1; }
	void	Draw(SDL_Surface*) override {}
	uintptr_t	SendMessage(int, uintptr_t, uintptr_t) override { return 0; }
	uintptr_t	SendMessage(int, const std::string&, uintptr_t) override { return 0; }
	uintptr_t	SendMessage(int, std::string*, uintptr_t) override { return 0; }

private:
	bool	bOwnsArrows;
};

}

void test_GuiLayoutArrowKeysStayInTextbox() {
	// A text box reports it uses the arrow keys itself; a plain button does not.
	CTextbox textbox;
	CHECK(textbox.handlesArrowKeys());
	TestWidget plainButton(wid_Button, false);
	CHECK(!plainButton.handlesArrowKeys());

	// A layout with a text field on the left and a button on its right.
	CGuiLayout layout;
	TestWidget* field = new TestWidget(wid_Textbox, true);
	TestWidget* button = new TestWidget(wid_Button, false);
	layout.Add(field, 1, 10, 10, 50, 20);
	layout.Add(button, 2, 100, 10, 50, 20);

	// From the button, Left arrow navigates to the field: navigation works.
	layout.FocusWidget(2);
	CHECK(layout.getFocusedWidget() == button);
	CHECK(layout.keyboardNavigationTarget(SDLK_LEFT) == field);

	// From the field, the arrow keys belong to the text cursor,
	// so navigation must not happen and the mouse must not be warped (#815).
	layout.FocusWidget(1);
	CHECK(layout.getFocusedWidget() == field);
	CHECK(layout.keyboardNavigationTarget(SDLK_LEFT) == NULL);
	CHECK(layout.keyboardNavigationTarget(SDLK_RIGHT) == NULL);

	layout.Shutdown();
}
