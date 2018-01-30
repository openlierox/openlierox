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
#include "DeprecatedGUI/CInputBox.h"


namespace DeprecatedGUI {

///////////////////
// Draw the input box
void CInputbox::Draw(SDL_Surface * bmpDest)
{
	if (bRedrawMenu)
		Menu_redrawBufferRect(iX,iY, bmpImage.get()->w, MAX(bmpImage.get()->h, tLX->cFont.GetHeight()));

	int y = bMouseOver ? 17 : 0;
	DrawImageAdv(bmpDest,bmpImage, 0, y, iX, iY, bmpImage.get()->w, 17);
	bMouseOver = false;
    tLX->cFont.DrawCentre(bmpDest, iX+25, iY+1, tLX->clWhite, sText);
}

CInputbox * CInputbox::InputBoxSelected = NULL;
std::string CInputbox::InputBoxLabel;

CInputboxInput::CInputboxInput(): CInputbox( 0, "", tMenu->bmpInputbox, "" )
{
	CInput::InitJoysticksTemp(); // for supporting joystick in CInput::Wait
}

CInputboxInput::~CInputboxInput()
{
	CInput::UnInitJoysticksTemp();
}

}; // namespace DeprecatedGUI
