// OpenLieroX


// Input box
// Created 30/3/03
// Jason Boettcher

// code under LGPL


#include "LieroX.h"

#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CImage.h"


namespace DeprecatedGUI {

///////////////////
// Draw the image
void CImage::Draw(SDL_Surface * bmpDest)
{
	// Don't try to draw non-existing image
	if (!tImage.get())
		return;

	if (bRedrawMenu)
		Menu_redrawBufferRect(iX,iY, tImage.get()->w,tImage.get()->h);

	// Clipping
	if(iX+iWidth > bmpDest->w)
		iX = bmpDest->w-iWidth;
	if(iX < 0)
		iX = 0;
	if(iY+iHeight > bmpDest->h)
		iY = bmpDest->h-iHeight;
	if(iY < 0)
		iY=0;

	// Draw the image
	tImage->draw(bmpDest,iX,iY); // Hopefully faster cropping
}

///////////////////
// Changes the image
void CImage::Change(const std::string& Path)
{
	if(Path == "")
		return;

	// Copy the new path
	sPath = Path;

	// Load the new image
	tImage = DynDrawFromSurface(LoadGameImage(sPath));
	if(!tImage.get()) return;
	
	// Update the width and height
	iWidth = tImage.get()->w;
	iHeight = tImage.get()->h;
}

void CImage::Change(const SmartPointer<DynDrawIntf>& bmpImg)
{
	// Just re-setup the image-related variables
	sPath = "";
	tImage = bmpImg;
	iWidth = bmpImg.get()->w;
	iHeight = bmpImg.get()->h;
}

/////////////////////
// This widget is a sendmessage
uintptr_t CImage::SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)
{
	return 0;
}

int CImage::KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	if (keysym == SDLK_RETURN ||
		keysym == SDLK_KP_ENTER ||
		keysym == SDLK_LALT ||
		keysym == SDLK_LCTRL ||
		keysym == SDLK_LSHIFT ||
		keysym == SDLK_x ||
		keysym == SDLK_z)
		return IMG_CLICK;
	return IMG_NONE;
}

}; // namespace DeprecatedGUI
