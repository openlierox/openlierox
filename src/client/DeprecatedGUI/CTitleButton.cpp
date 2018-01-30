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

}; // namespace DeprecatedGUI
