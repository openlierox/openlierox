/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Title button
// Created 30/3/03
// Jason Boettcher


#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CTitleButton.h"



namespace DeprecatedGUI {

///////////////////
// Draw the title button
void CTitleButton::Draw(SDL_Surface * bmpDest)
{
	if (bRedrawMenu)
		Menu_redrawBufferRect(iX,iY, bmpImage.get()->w,39);

	int y = 10+iImageID*40;
	if(bMouseOver)
		y += 200;
	DrawImageAdv(bmpDest,bmpImage, 10,y, iX,iY, bmpImage.get()->w, 39);
	bMouseOver = false;
}

int CTitleButton::KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	if (keysym == SDLK_RETURN ||
		keysym == SDLK_KP_ENTER ||
		keysym == SDLK_LALT ||
		keysym == SDLK_LCTRL ||
		keysym == SDLK_LSHIFT ||
		keysym == SDLK_x ||
		keysym == SDLK_z)
		return TBT_CLICKED;
	return TBT_NONE;
}

}; // namespace DeprecatedGUI
