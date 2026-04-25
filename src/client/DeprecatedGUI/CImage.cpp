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

CWidget * CImage::WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy )
{
	CImage * w = new CImage( p[0].toString() );
	w->cClick.Init( p[1].toString(), w );
	if( dx == 0 )
		dx = w->iWidth;
	if( dy == 0 )
		dy = w->iHeight;
	layout->Add( w, id, x, y, dx, dy );
	return w;
};

static bool CImage_WidgetRegistered = 
	CGuiSkin::RegisterWidget( "image", & CImage::WidgetCreator )
							( "file", SVT_STRING )
							( "click", SVT_STRING );

}; // namespace DeprecatedGUI
