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

#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CTextButton.h"
#include "Cursor.h"


namespace DeprecatedGUI {

int		CTextButton::MouseOver(mouse_t *tMouse)
{
	SetGameCursor(CURSOR_HAND);
	bMouseOver = true;
	return TXB_MOUSEOVER;
}

void	CTextButton::Draw(SDL_Surface * bmpDest) 
{
	CLabel::ChangeColour( bMouseOver ? iColGlow : iColNormal );
	bMouseOver = false;
	CLabel::Draw( bmpDest );
}

int CTextButton::KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	if (keysym == SDLK_RETURN ||
		keysym == SDLK_KP_ENTER ||
		keysym == SDLK_LALT ||
		keysym == SDLK_LCTRL ||
		keysym == SDLK_LSHIFT ||
		keysym == SDLK_x ||
		keysym == SDLK_z)
		return TXB_MOUSEUP;
	return CLabel::KeyDown( c, keysym, modstate );
}

}; // namespace DeprecatedGUI
