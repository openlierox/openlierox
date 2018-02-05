/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Input box
// Created 30/3/03
// Jason Boettcher


#include "LieroX.h"

#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "CInput.h"
#include "Sounds.h"
#include "DeprecatedGUI/CInputBox.h"


namespace DeprecatedGUI {

CInputbox * CInputbox::InputBoxSelected = NULL;
std::string CInputbox::InputBoxLabel;

///////////////////
// Draw the input box
void CInputbox::Draw(SDL_Surface * bmpDest)
{
	const int nativeW = bmpImage.get()->w;
	// Draw at the widget's width; if it's wider than the box image, widen it.
	const int bw = (iWidth > nativeW) ? iWidth : nativeW;

	if (bRedrawMenu)
		Menu_redrawBufferRect(iX,iY, bw, MAX(bmpImage.get()->h, tLX->cFont.GetHeight()));

	int y = bMouseOver ? 17 : 0;
	if(bw <= nativeW) {
		DrawImageAdv(bmpDest, bmpImage, 0, y, iX, iY, nativeW, 17);
	} else {
		// Widen via a horizontal 3-slice: keep the left/right caps (which hold
		// the rounded corners and borders) at native size and repeat a flat
		// middle column to fill the gap. We use DrawImageAdv throughout so the
		// box image's alpha is blended (a plain stretch did not blend and
		// looked broken).
		const int capW = nativeW / 2;       // left/right cap width
		const int midSrcX = capW - 1;       // a flat interior column to repeat
		DrawImageAdv(bmpDest, bmpImage, 0, y, iX, iY, capW, 17);                          // left cap
		for(int dx = iX + capW; dx < iX + bw - capW; dx++)
			DrawImageAdv(bmpDest, bmpImage, midSrcX, y, dx, iY, 1, 17);                   // repeated middle
		DrawImageAdv(bmpDest, bmpImage, capW, y, iX + bw - capW, iY, nativeW - capW, 17); // right cap
	}
	bMouseOver = false;
    tLX->cFont.DrawCentre(bmpDest, iX + bw/2, iY+1, tLX->clWhite, sText);
}

int CInputbox::KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	if (keysym == SDLK_RETURN ||
		keysym == SDLK_KP_ENTER ||
		keysym == SDLK_LALT ||
		keysym == SDLK_LCTRL ||
		keysym == SDLK_LSHIFT ||
		keysym == SDLK_x ||
		keysym == SDLK_z) {
		PlaySoundSample(sfxGeneral.smpClick);
		return INB_MOUSEUP;
	}
	return INB_NONE;
}

}; // namespace DeprecatedGUI
