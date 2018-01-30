/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Slider
// Created 30/6/02
// Jason Boettcher


#include "LieroX.h"

#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CSlider.h"


namespace DeprecatedGUI {

///////////////////
// Draw the slider
void CSlider::Draw(SDL_Surface * bmpDest)
{
	if (bRedrawMenu)
		Menu_redrawBufferRect(iX,iY, iWidth,iHeight);

	DrawHLine( bmpDest, iX+5, iX+iWidth-5, iY+iHeight/2-1, tLX->clSliderDark);
	DrawHLine( bmpDest, iX+5, iX+iWidth-5, iY+iHeight/2,   tLX->clSliderLight);
	DrawHLine( bmpDest, iX+5, iX+iWidth-5, iY+iHeight/2+1, tLX->clSliderDark);

	// Draw the button
	int x = iX+5;
	int w = iWidth - 10;
	int val = (int)( ((float)w/(float)(iMax - iMin)) * (float)(iValue - iMin) ) + x;

	int y = (iY+iHeight/2) - gfxGUI.bmpSliderBut.get()->h/2;
	DrawImage(bmpDest,gfxGUI.bmpSliderBut,val-3,y);
	
	if( !bShowText )
		return;
	
	std::string sText = ftoa( iValue * fValueScale ) + sAppendText;
	tLX->cFont.Draw(bmpDest, iX + iTextPosX, iY + iTextPosY, iTextColor, sText);
}


///////////////////
// Mouse down on the slider
int CSlider::MouseDown(mouse_t *tMouse, int nDown)
{
	int x = iX - 5; // a bit more left to have more natural swapping
	int w = iWidth - 10;

	int val = (int)( (float)(iMax - iMin) / ( (float)w / (float)(tMouse->X-x)) );
	iValue = val + iMin;

	if(tMouse->X > x+w)
		iValue = iMax;
	if(tMouse->X < x)
		iValue = iMin;

	// Clamp the value
	iValue = MAX(iMin,iValue);
	iValue = MIN(iMax,iValue);

	return SLD_CHANGE;
}


///////////////////
// This widget is send a message
uintptr_t CSlider::SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)
{
	switch(iMsg) {

		// Get the value
		case SLM_GETVALUE:
			return iValue;

		// Set the value
		case SLM_SETVALUE:
			iValue = (int)Param1;
			break;

	}

	return 0;
}

}; // namespace DeprecatedGUI
