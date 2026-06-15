/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Text box
// Created 30/6/02
// Jason Boettcher


#ifndef __CTEXTBOX_H__DEPRECATED_GUI__
#define __CTEXTBOX_H__DEPRECATED_GUI__

#include "DeprecatedGUI/CWidget.h"
#include "InputEvents.h"
#include "Cursor.h"
#include "Timer.h"
#include "Event.h"


namespace DeprecatedGUI {

// Event types
enum {
	TXT_NONE=-1,
	TXT_CHANGE=0,
	TXT_MOUSEOVER,
	TXT_ENTER,
	TXT_TAB
};


// Messages
enum {
	TXS_GETTEXT=0,
	TXS_SETTEXT,
	TXM_SETFLAGS,
	TXM_SETMAX,
	TXM_GETTEXTLENGTH
};


// Flags
enum { 
	TXF_PASSWORD = 0x0001, 
	TXF_NOUNICODE = 0x0002 
};


class CTextbox : public CWidget {
public:
	// Constructor
	CTextbox() {
		iType = wid_Textbox;
		iFlags = 0;
		bDrawCursor = true;
		iScrollPos = 0;
		iSelLength = 0;
		iSelStart = 0;
		sSelectedText = "";
		bHolding = false;
		bHoldingMouse = false;
		iLastCurpos = 0;
		fTimeHolding = TimeDiff();
		iLastMouseX = 0;
		fLastRepeat = AbsTime();
		fLastClick = AbsTime();
		fScrollTime = TimeDiff();  // We can scroll
		bVar = NULL;
		iVar = NULL;
		fVar = NULL;
		sVar = NULL;
	}

	~CTextbox()  {
		Destroy();
	}


private:
	// Attributes

	std::string	sText;
	std::string sAltKey;  // if user is pressing Alt + Numbers, we remember the numbers and insert unicode character

	// these are related to the size of the string (sText.size()), NOT the displayed size
	size_t	iScrollPos;
	size_t	iCurpos;
	int		iSelLength; // if < 0, selection to the left, otherwise to the right
	size_t	iSelStart;
	
	Uint32	iFlags;
	std::string	sSelectedText;

	size_t	iMax;

	bool	bHolding;
	AbsTime	fTimePushed;
	UnicodeChar		iLastchar;
	int		iLastKeysym;

	bool	bHoldingMouse;
	TimeDiff	fTimeHolding;
	size_t	iLastCurpos;
	int		iLastMouseX;
	TimeDiff	fScrollTime;
	AbsTime	fLastRepeat;
	AbsTime	fLastClick;

	bool	bDrawCursor;

	bool		*bVar;
	int			*iVar;
	float		*fVar;
	std::string	*sVar;


public:
	// Methods

	void	Create();
	void	Destroy();

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse);
	int		MouseUp(mouse_t *tMouse, int nDown);
	int		MouseDown(mouse_t *tMouse, int nDown);
	int		MouseWheelDown(mouse_t *tMouse)		{ return TXT_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return TXT_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate);
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate);

	void	Draw(SDL_Surface * bmpDest);

	uintptr_t	SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2);
	uintptr_t	SendMessage(int iMsg, const std::string& sStr, uintptr_t Param);
	uintptr_t	SendMessage(int iMsg, std::string *sStr, uintptr_t Param);

	void	Backspace();
	void	Delete();
	void	SelectWord();
	void	Insert(UnicodeChar c);

	std::string	getText()						{ return sText; }
	void	setText(const std::string& buf);
	void	setFlag(Uint32 flag) { iFlags |= flag; }
	void	unsetFlag(Uint32 flag) { iFlags &= ~flag; }
	void	setCurPos(size_t pos)  { iCurpos = (pos > sText.size() ? sText.size() : pos); }

    void    PasteText();
	void	CopyText();

	void OnTimerEvent(Timer::EventData ev);
};

}; // namespace DeprecatedGUI

#endif  //  __CTEXTBOX_H__DEPRECATED_GUI__
