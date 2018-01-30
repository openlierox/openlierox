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
	//bEnabled = true;	// For CGuiSkinnedLayout

	// Reset the events
	int i;
	for (i=0;i<NumEvents;i++)
		fix_strncpy(tEvents.Events[i],"");
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

//////////////////
// Setup the events
void CWidget::SetupEvents(generic_events_t *Events)
{
	int i;
	for (i=0; i<NumEvents; i++)
		fix_strncpy(tEvents.Events[i],Events->Events[i]);
}

/////////////////
// Process the specified event
void CWidget::ProcessEvent(int Event)
{
/*	return;
	// TODO: Use LUA
	char *Code;
	CGuiLayout *Parent = (CGuiLayout *)cParent;

	// Get the event source code
	Code = &tEvents.Events[Event][0];
	TrimSpaces(Code);

	// Nothing to process
	if (!strcmp("",Code))
		return;

	// Read the function name
	static char Function[32];
	ReadUntil(Code,'(',Function,sizeof(Function));
	Code = Code+fix_strnlen(Function)+1;
	TrimSpaces(Function);

	if(!stricmp(Function,"change"))  {

		// Get the widget name
		static char WidgetName[32];
		ReadUntil(Code,',',WidgetName,sizeof(WidgetName));
		Code = Code + fix_strnlen(WidgetName)+1;
		TrimSpaces(WidgetName);

		// Get the widget ID
		int ID = LayoutWidgets[Parent->getID()].getID(WidgetName);
		if (ID == -1)
			return;

		// Read the new image path
		static char ImagePath[32];
		ReadUntil(Code,')',ImagePath,sizeof(ImagePath));
		TrimSpaces(ImagePath);

		CImage *img;
		CWidget *w = Parent->getWidget(ID);
		if(w->getType() == wid_Image)
			img = (CImage *)Parent->getWidget(ID);
		else
			return;

		img->Change(ImagePath);
	}

	else if(!stricmp(Function,"PlaySound"))  {
		static char SoundPath[32];
		ReadUntil(Code,')',SoundPath,sizeof(SoundPath));
		Code = Code + strlen(SoundPath) + 1;
		TrimSpaces(SoundPath);

		SoundSamplePlay(SoundPath);
	}

	else if(!stricmp(Function,"Show"))  {
		// Get the widget name
		static char WidgetName[32];
		ReadUntil(Code,',',WidgetName,sizeof(WidgetName));
		Code = Code + strlen(WidgetName) +1;
		TrimSpaces(WidgetName);

		// Get the widget visibility parameter
		static char Visible[8];
		ReadUntil(Code,')',Visible,sizeof(Visible));
		Code = Code + strlen(Code) +1;
		TrimSpaces(Visible);

		// Convert to boolean
		bool bVisible = false;
		if (!stricmp("true",Visible))
			bVisible = true;
		if (!stricmp("yes",Visible))
			bVisible = true;
		if (!strcmp("1",Visible))
			bVisible = true;


		// Get the widget ID
		int ID = LayoutWidgets[Parent->getID()].getID(WidgetName);
		if (ID == -1)
			return;

		// Get the widget
		CWidget *w = Parent->getWidget(ID);
		if(!w)
			return;

		// Hide the widget
		w->setEnabled(bVisible);
	}*/

}

}; // namespace DeprecatedGUI

