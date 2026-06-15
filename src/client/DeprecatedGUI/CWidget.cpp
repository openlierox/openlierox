/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Widget class
// Created 5/6/02
// Jason Boettcher


#include "LieroX.h"

#include "DeprecatedGUI/CWidget.h"
#include "DeprecatedGUI/Menu.h"
#include "StringUtils.h"


namespace DeprecatedGUI {

CWidget::~CWidget() 
{
}

///////////////////
// Setup the widget
void CWidget::Setup(int id, int x, int y, int w, int h)
{
	iID = id;
	iX = x;
	iY = y;
	iWidth = w;
	iHeight = h;
}


///////////////////
// Returns true if a point is inside this widget
bool CWidget::InBox(int x, int y)
{
	return (x > iX && x < iX+iWidth)  && (y > iY && y < iY+iHeight);
}


///////////////////
// Redraw the buffer on the widget
void CWidget::redrawBuffer()
{
    Menu_redrawBufferRect(iX, iY, iWidth, iHeight);
}

}; // namespace DeprecatedGUI

