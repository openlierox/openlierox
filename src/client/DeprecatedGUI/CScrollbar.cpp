/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// gui scrollbar class
// Created 30/6/02
// Jason Boettcher


#include "LieroX.h"

#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"


namespace DeprecatedGUI {

///////////////////
// Create the scrollbar
void CScrollbar::Create()
{
	iMin=0;
	iMax=0;
	iValue=0;
	iItemsperbox = 1;
	iScrollPos = 0;
	bSliderGrabbed = false;
	iSliderGrabPos = 0;
	bTopButton=false;
	bBotButton=false;

	nButtonsDown = 0;
}


///////////////////
// Draw the scrollbar
void CScrollbar::Draw(SDL_Surface * bmpDest)
{
	int x=0;
	int length;
    
	// Top arrow
	if(bTopButton)
		x=15;
	DrawImageAdv(bmpDest, gfxGUI.bmpScrollbar, x,0, iX,iY, 15,14);

	// Bottom arrow
	x=0;
	if(bBotButton)
		x=15;
	DrawImageAdv(bmpDest, gfxGUI.bmpScrollbar, x,14, iX,iY+iHeight-14, 15,14);

	// Main bit
	DrawRectFill(bmpDest, iX,iY+14, iX+iWidth, iY+iHeight-14, tLX->clScrollbarBack);
	DrawVLine(bmpDest, iY+14, iY+iHeight-14, iX,tLX->clScrollbarBackLight);
	DrawVLine(bmpDest, iY+14, iY+iHeight-14, iX+iWidth,tLX->clScrollbarBackLight);

	// Slider
	if(iMax > iItemsperbox && iMax > 0) {
		length = (int)((float)iItemsperbox/(float)iMax * iHeight-30);
        length = MAX(length, 12);
        
		int pos = iScrollPos;
		if(pos+length > iHeight-30)
			pos=iHeight-30-length;
		if (pos < 0)  
			pos = 0;

		DrawRectFill(bmpDest, iX+2, iY+15+pos, iX+iWidth-1, iY+15+pos+length, tLX->clScrollbarFront);

        // Shine
        DrawVLine(bmpDest, iY+15+pos, iY+15+pos+length, iX+1, tLX->clScrollbarHighlight);
        DrawHLine(bmpDest, iX+1, iX+iWidth-1, iY+15+pos, tLX->clScrollbarHighlight);
        // Dark
        DrawVLine(bmpDest, iY+15+pos, iY+15+pos+length, iX+iWidth-1, tLX->clScrollbarShadow);
        DrawHLine(bmpDest, iX+1, iX+iWidth-1, iY+15+pos+length, tLX->clScrollbarShadow);
	}

	// Slight hack
	if( !GetMouse()->Button ) {
		bTopButton=false;
		bBotButton=false;
	}
		
}


///////////////////
// Mouse up
int CScrollbar::MouseUp(mouse_t *tMouse, int nDown)
{
	bSliderGrabbed = false;
	bTopButton=false;
	bBotButton=false;
	nButtonsDown = 0;
	return SCR_NONE;
}


///////////////////
// Mouse over
int CScrollbar::MouseOver(mouse_t *tMouse)
{
	if(!tMouse->Down) {
		bSliderGrabbed = false;
		bTopButton=false;
		bBotButton=false;
		nButtonsDown = 0;
	}
	
	return SCR_NONE;
}


///////////////////
// Mouse down
int CScrollbar::MouseDown(mouse_t *tMouse, int nDown)
{
	if((tMouse->X < iX || tMouse->X > iX+iWidth) && !bSliderGrabbed)
		return SCR_NONE;

	int length = (int)((float)iItemsperbox/(float)iMax * iHeight-30);
	length = MAX(length, 12);
    // Stabalise
    //int length = (int)((float)iItemsperbox/(float)iMax * iHeight-30-length);
	//length = MAX(length, 12);
	
	// Grab the slider
	if(tMouse->Y > iY+15+iScrollPos && tMouse->Y < iY+15+iScrollPos+length) {		
		if(!bSliderGrabbed) {
			bSliderGrabbed = true;
			iSliderGrabPos = tMouse->Y - (iY+15+iScrollPos);
			nButtonsDown = 0;
			return SCR_NONE;
		}
	}

	// Move the slider
	if(bSliderGrabbed) {
		float dist = (float)tMouse->Y - (iY+15+iScrollPos + iSliderGrabPos);
		float increment = iMax ? (float)iHeight/(float)iMax : 0;
		if(increment)
			iValue += (int)(dist / increment);
		UpdatePos();
		nButtonsDown = 0;
		return SCR_CHANGE;
	}

	
	// Do a key repeat
	bool Down = false;

	for(int i=0; i<3; i++) {
		if(tMouse->Down) {

			if( nButtonsDown & SDL_BUTTON(i+1) ) {
				if( fMouseNext[i] < tLX->currentTime ) {
					Down = true;
					fMouseNext[i] = tLX->currentTime + 0.05f;
				}
			} else {
				Down = true;
				nButtonsDown |= SDL_BUTTON(i+1);
				fMouseNext[i] = tLX->currentTime + 0.25f;
			}
		}
	}


	if( !Down )
		return SCR_NONE;

	// Top arrow
	if(tMouse->Y > iY && tMouse->Y < iY+14) {
		bTopButton = true;

		// Move up
		iValue--;
		UpdatePos();
		return SCR_CHANGE;
	}

	// Bottom arrow
	if(tMouse->Y > iY+iHeight-14 && tMouse->Y < iY+iHeight) {
		bBotButton = true;

		// Move down
		iValue++;
		UpdatePos();
		return SCR_CHANGE;
	}


	// Background click

	length = (int)((float)iItemsperbox/(float)iMax * iHeight-30);
    length = MAX(length, 12);
        
	// Background click
	int pos = iScrollPos;
	if(pos+length > iHeight-30)
		pos=iHeight-30-length;

	if (tMouse->Y > iY+15+pos+length)
	  iValue+=2;
	else
	  iValue-=2;

	UpdatePos();


	return SCR_CHANGE;
}

///////////////////
// Mouse wheel down
int CScrollbar::MouseWheelDown(mouse_t *tMouse)
{
	iValue++;
	UpdatePos();
	
	return SCR_CHANGE;
}

///////////////////
// Mouse wheel up
int CScrollbar::MouseWheelUp(mouse_t *tMouse)
{
	iValue--;
	UpdatePos();
	
	return SCR_CHANGE;
}


///////////////////
// Update the slider pos
void CScrollbar::UpdatePos()
{
    iMax = MAX(iMax,0);
    iMin = MAX(iMin,0);

    if(iMax < iItemsperbox) {
        iValue = 0;
        iScrollPos = 0;
        return;
    }

    int mx = iMax-iItemsperbox;

	iValue = CLAMP(iValue, 0, mx);

    // Prevent div by zero errors
    if(mx == 0)
        return;

    int length = (int)((float)iItemsperbox/(float)iMax * iHeight-30);
    length = MAX(length, 12);

	iScrollPos = (int)((float)(iHeight-30-length)/(float)mx * iValue);
}


///////////////////
// Process a sent message
uintptr_t CScrollbar::SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)
{
    switch( iMsg ) {

        // Get the value
        case SCM_GETVALUE:
            return iValue;
            break;

        // Set the value
        case SCM_SETVALUE:
            iValue = Param1;
            break;

        // Set the number of items per box
        case SCM_SETITEMSPERBOX:
            iItemsperbox = Param1;
            break;

        // Set the min
        case SCM_SETMIN:
            iMin = Param1;
            break;

        // Set the max
        case SCM_SETMAX:
            iMax = Param1;
            break;
    }

    UpdatePos();

    return 0;
}

}; // namespace DeprecatedGUI
