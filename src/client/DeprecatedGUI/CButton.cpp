/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Button
// Created 30/3/03
// Jason Boettcher


#include "LieroX.h"

#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CButton.h"
#include "GuiPrimitives.h"


namespace DeprecatedGUI {

///////////////////
// Draw the button
void CButton::Draw(SDL_Surface * bmpDest)
{
	if(!bVisible) return;
	
	// Don't draw empty image
	if (!bmpImage.get())
		return;

	if (iButtonType == BUT_MENU)  {
		if (bRedrawMenu)
			Menu_redrawBufferRect(iX,iY, iGoodWidth, 18);

		int y2 = 5+iImageID*40;
		if(bMouseOver)
			y2+=20;

		DrawImageAdv(bmpDest,bmpImage, 5,y2, iX,iY, iGoodWidth, 18);
		if (y2 >= bmpImage->h || bUseFallback)  {
			DrawSimpleButton(bmpDest, iX, iY, iWidth, iHeight, tLX->clWinBtnBody, tLX->clWinBtnLight, tLX->clWinBtnDark, bMouseDown && bMouseOver && bFocused);
			int trans = (bMouseDown && bMouseOver && bFocused) ? 2 : 0;
			tLX->cFont.DrawCentre(bmpDest, iX + iWidth / 2 + trans, iY + trans + (iHeight - tLX->cFont.GetHeight())/2, tLX->clNormalLabel, sButtonNames[iImageID]);
		}

		bMouseOver = false;	
	} else { // Two state and three state buttons
		if (bRedrawMenu)
			Menu_redrawBufferRect(iX,iY, iGoodWidth, bmpImage.get()->h);

		int numstates = iButtonType == BUT_TWOSTATES ? 2: 3;

		int x2 = 0;
		if (bMouseOver)
			x2 = (numstates - 2) * bmpImage.get()->w / numstates;
		if (bMouseDown)
			x2 = (numstates - 1) * bmpImage.get()->w / numstates;

		DrawImageAdv(bmpDest, bmpImage, x2, 0, iX, iY, bmpImage.get()->w / numstates, bmpImage.get()->h);

		bMouseOver = false;
	}
}


///////////////////
// Draw the button, without the buffer update
void CButton::Draw2(SDL_Surface * bmpDest)
{  
	// Temporarily disable buffer update and draw
	bool old_redraw = bRedrawMenu;
	bRedrawMenu = false;
	Draw(bmpDest);
	bRedrawMenu = old_redraw;
}


///////////////////
// Create
void CButton::Create()
{
    int y2 = 5+iImageID*40;
    Uint8 r,g,b,a;

    // Find the smallest width
	LOCK_OR_QUIT(bmpImage);
    iGoodWidth = 0;
    for( int y=y2; y<=y2+38 && y<bmpImage.get()->h; y++) {
        for( int x=0; x<bmpImage.get()->w; x++ ) {

            Uint32 pixel = GetPixel(bmpImage.get(), x,y);
            GetColour4(pixel, bmpImage.get()->format, &r,&g,&b,&a);
            if( a != 0 )
                iGoodWidth = MAX(iGoodWidth,x);
        }
    }
	UnlockSurface(bmpImage);

	if (y2 >= bmpImage->h || iGoodWidth < 2)  {
		bUseFallback = true;
		iGoodWidth = tLX->cFont.GetWidth(sButtonNames[iImageID]) + 20;
	} else
		bUseFallback = false;

    iGoodWidth = MIN(iGoodWidth+1,bmpImage.get()->w);
    
    initWidthHeight();
}

}; // namespace DeprecatedGUI
