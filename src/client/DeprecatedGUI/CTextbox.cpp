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

/*
	HINT: all string-positions used in this code are interpreted as the position if it were an Utf8 encoded string (and of course also the Draw-functions handle it like this)
*/

#include "LieroX.h"



#include <string>

#include "Clipboard.h"
#include "AuxLib.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "StringUtils.h"
#include "Mutex.h"
#include "DeprecatedGUI/CTextbox.h"




namespace DeprecatedGUI {

static Timer * CursorBlinkerTimer = NULL;
static CTextbox * CursorBlinkerTimerTarget = NULL;
static std::set< CTextbox * > TextboxesCount; // Create() and Destroy() may be called twice at random
static Mutex CursorBlinkerTimerMutex;


///////////////////
// Create the text box
void CTextbox::Create()
{
	iCurpos = 0;
	iLastCurpos = 0;
	sText = "";
	iMax = (size_t) -1; // highest possible size_t value
	bHolding = false;
	bHoldingMouse = false;
	fTimeHolding = 0;
	fTimePushed = AbsTime();
	fLastRepeat = AbsTime();
	iLastchar = 0;
	iLastKeysym = 0;
	Mutex::ScopedLock l(CursorBlinkerTimerMutex);
	TextboxesCount.insert(this);
	if (CursorBlinkerTimer == NULL)  {
		CursorBlinkerTimer = new Timer;
		CursorBlinkerTimer->name = "CTextbox cursor blinker";
		CursorBlinkerTimer->interval = 300;
		CursorBlinkerTimer->once = false;
		CursorBlinkerTimer->onTimer.handler() = getEventHandler(this, &CTextbox::OnTimerEvent);
		CursorBlinkerTimer->start();
	}
}

/////////////////
// Destroy the textbox
void CTextbox::Destroy()
{
	Mutex::ScopedLock l(CursorBlinkerTimerMutex);
	if( CursorBlinkerTimerTarget == this )
		CursorBlinkerTimerTarget = NULL;
	TextboxesCount.erase(this);
	if( TextboxesCount.empty() && CursorBlinkerTimer != NULL )
	{
		CursorBlinkerTimer->stop();
		delete CursorBlinkerTimer;
		CursorBlinkerTimer = NULL;
		CursorBlinkerTimerTarget = NULL;
	}
}

//////////////////
// Handles an event coming from the timer
void CTextbox::OnTimerEvent(Timer::EventData ev)
{
	Mutex::ScopedLock l(CursorBlinkerTimerMutex);
	if( CursorBlinkerTimerTarget )
		CursorBlinkerTimerTarget->bDrawCursor = !CursorBlinkerTimerTarget->bDrawCursor;
}


///////////////////
// Draw the text box
void CTextbox::Draw(SDL_Surface * bmpDest)
{
	std::string text = sText;

	if (bRedrawMenu)
		Menu_redrawBufferRect(iX,iY, iWidth,iHeight);
	Menu_DrawBoxInset(bmpDest, iX, iY-2, iX+iWidth, iY+iHeight+2);

	if(iFlags & TXF_PASSWORD) {

		// Draw astericks for password
		text = "";
		size_t len = Utf8StringSize(sText);
		for(size_t i = 0; i < len; i++)
			text += '*';
	}

	int cursorpos = (int)iCurpos;

	// User can scroll to left
	cursorpos -= (int)iScrollPos;
	if (cursorpos < 0)  {
		cursorpos = 5;  // Number of characters, that will be displayed on left if we scroll left and cursor is on the most left
		iScrollPos -= 5;
		if ((int)iScrollPos < 0)
			iScrollPos = 0;
	}

	// Destroy the selection when we lose focus
	/*if(!iFocused)  {
		iSelLength = 0;
		iSelStart = 0;
		sSelectedText = "";
		iCurpos = 0;
		iLastCurpos = 0;
		fTimeHolding = 0;
		iHoldingMouse = false;
		iLastMouseX = 0;
	}*/

	// Shift the text, if it overlapps
	Utf8Erase(text,0,iScrollPos);

	// Draw selection
	if (iSelLength)  {
		int x1=0,x2=0;  // Positions of the selection

		if (iSelLength > 0)  
			iSelStart = iCurpos;
		else
			iSelStart = iCurpos + iSelLength;


		if (iSelStart <= iScrollPos) {
			x1 = 3;
			x2 = MIN(
				x1 + tLX->cFont.GetWidth(
					Utf8SubStr(text, 0, abs(iSelLength) - iScrollPos + iSelStart)),
				iWidth - 3);
		} else {
			x1 = 3 + tLX->cFont.GetWidth(
				Utf8SubStr(text, 0, iSelStart - iScrollPos));
			x2 = MIN(
				x1 + tLX->cFont.GetWidth(
					Utf8SubStr(text, iSelStart - iScrollPos, abs(iSelLength))),
				iWidth - 3);
		}

		// Update the selected text
		sSelectedText = Utf8SubStr(sText, iSelStart, abs(iSelLength));

		if (x1 < x2)
			DrawRectFill(
				bmpDest,
				iX + x1, iY + 2, iX + x2, iY + iHeight,
				tLX->clSelection);
	}

	// Draw text
	tLX->cFont.DrawAdv(bmpDest, iX + 3, iY, iWidth-4, tLX->clTextBox, text);

	// Draw cursor (only when focused)
	if(bFocused) {

		if (bDrawCursor)  {
			// Determine the cursor position in pixels
			int x = tLX->cFont.GetWidth(Utf8SubStr(text, 0, (unsigned int)cursorpos));

			DrawVLine(
				bmpDest,
				iY + 3, iY + iHeight - 3, MIN(iX + x + 3, iX + iWidth),
				tLX->clTextboxCursor);
		}

		Mutex::ScopedLock l(CursorBlinkerTimerMutex);
		CursorBlinkerTimerTarget = this;
	}
}


///////////////////
// Keydown event
int CTextbox::KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	bDrawCursor = true;
	bCanLoseFocus = true;

	// Handle holding keys
	if(bHolding) {
		if(iLastchar != c || iLastKeysym != keysym)
			bHolding = false;
		else {
			if(tLX->currentTime - fTimePushed < 0.25f)
				return TXT_NONE;
			if (tLX->currentTime - fLastRepeat < 0.03f)
				return TXT_NONE;
			fLastRepeat = tLX->currentTime;
		}
	}

	if(!bHolding) {
		bHolding = true;
		fTimePushed = tLX->currentTime;
	}

	iLastchar = c;
	iLastKeysym = keysym;


	// Backspace
	if(keysym == SDLK_BACKSPACE) {
		Backspace();
		return TXT_CHANGE;
	}


	// Left arrow
	if (keysym == SDLK_LEFT) {
		if (modstate.bShift)  {
			if (iCurpos)
				iSelLength++;
		}
		else
			iSelLength = 0;

		if(iCurpos)
			iCurpos--;

		return TXT_CHANGE;
	}

	// Right arrow
	if(keysym == SDLK_RIGHT) {
		size_t len = Utf8StringSize(sText);
		if(iCurpos <= len)  {
			if (modstate.bShift)  {
				if (iCurpos != len)
					iSelLength--;
			}
			else
				iSelLength = 0;

			if (iCurpos != len)
				iCurpos++;
		}

		// Prevents weird behavior when we're at the end of text
		if(iScrollPos)  {
			if(tLX->cFont.GetWidth(Utf8SubStr(sText, iScrollPos, (size_t)-1)) < (iWidth - 4))
				return TXT_NONE;
		}

		// Set the scroll position
		if (iCurpos)
			if(tLX->cFont.GetWidth(Utf8SubStr(sText, 0, iCurpos)) > (iWidth - 7))
				iScrollPos++;

		return TXT_CHANGE;
	}

	// Home
	if(keysym == SDLK_HOME) {
		// If the shift key is down, select the text
		if(modstate.bShift)
			if(iSelLength > 0)
				iSelLength += (int)iCurpos;
			else
				iSelLength = (int)iCurpos;
		else
			iSelLength = 0;

		// Safety
		if((size_t)iSelLength > Utf8StringSize(sText))
			iSelLength = (int)Utf8StringSize(sText);

		iCurpos = 0;
		iScrollPos = 0;
		return TXT_CHANGE;
	}

	// End
	if(keysym == SDLK_END) {
		if (modstate.bShift)
			iSelLength = -(int)Utf8StringSize(sText) + (int)iCurpos + iSelLength;
		else
			iSelLength = 0;

		iCurpos = Utf8StringSize(sText);

		if (tLX->cFont.GetWidth(sText) > iWidth - 3)  {
			iScrollPos = 0;
			for(std::string::const_iterator it = sText.begin(); it != sText.end(); IncUtf8StringIterator(it, sText.end())) {
				iScrollPos++;
				if(tLX->cFont.GetWidth(std::string(it, (std::string::const_iterator)sText.end())) < iWidth)
					break;
			}
		}
		else
			iScrollPos = 0;

		return TXT_CHANGE;
	}

	// Select all
	if ((modstate.bCtrl || modstate.bGui) && keysym == SDLK_a) {
		iCurpos = Utf8StringSize(sText);
		iSelStart = 0;
		iSelLength = -((int)Utf8StringSize(sText));

		return TXT_CHANGE;
	}

	// Enter
	if(keysym == SDLK_RETURN || keysym == SDLK_KP_ENTER) {
		return TXT_ENTER;
	}

	if(c == SDLK_TAB) {
		return TXT_TAB;
	}

    // Ctrl-v or Super-v or Shift-Insert (paste)
    if(((modstate.bCtrl || modstate.bGui) && keysym == SDLK_v ) ||
		( modstate.bShift && keysym == SDLK_INSERT )) {
        PasteText();
        return TXT_CHANGE;
    }

    // Ctrl-c or Super-c or Ctrl-insert (copy)
    if(((modstate.bCtrl || modstate.bGui) && keysym == SDLK_c ) ||
		( (modstate.bCtrl || modstate.bGui) && keysym == SDLK_INSERT )) {
        CopyText();
        return TXT_CHANGE;
    }

    // Ctrl-x or Super-x or Shift-Delete (cut)
    if(((modstate.bCtrl || modstate.bGui) && keysym == SDLK_x ) ||
		( modstate.bShift && keysym == SDLK_DELETE )) {
        CopyText();
		Delete();
        return TXT_CHANGE;
    }

	// Delete
	if(keysym == SDLK_DELETE) {
		Delete();
		return TXT_CHANGE;
	}

	// Alt + numbers
	if(modstate.bAlt)  {
		if (c >= 0x30 && c <= 0x39)  {
			sAltKey += (char)c;
			return TXT_NONE;
		}
	} else {  // Alt not pressed
		sAltKey = "";
	}

	if(modstate.bCtrl && taunts->getTauntForKey( keysym ) != "" ) {
		sText += taunts->getTauntForKey( keysym );
		return TXT_ENTER;
	}

	// No visible character
	if (c == 0)
		return TXT_NONE;

	// Insert character
	Insert(c);

	return TXT_CHANGE;
}


///////////////////
// Keyup event
int CTextbox::KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	bHolding = false;
	bCanLoseFocus = true;

	// Check if alt has been released and insert any character code that has been typed
	if (keysym == SDLK_LALT || keysym == SDLK_RALT)
		if (sAltKey.size() != 0)  {  // If there's some code saved, insert it

			// Convert the code
			bool fail = false;
			UnicodeChar in = (UnicodeChar)from_string<int>(sAltKey, fail);
			
			// Insert
			if (!fail)
				Insert(in);

			// Clear
			sAltKey = "";
		}

	return TXT_NONE;
}

///////////////////
// Mouse down event
int	CTextbox::MouseDown(mouse_t *tMouse, int nDown)
{
	SetGameCursor(CURSOR_TEXT);

	int deltaX = tMouse->X - iX;
	bDrawCursor = true;

	if (sText == "")
	{
		// Paste with 2nd or 3rd mouse button
		if( tMouse->FirstDown & SDL_BUTTON(2) || tMouse->FirstDown & SDL_BUTTON(3) )
			PasteText();
		return TXT_MOUSEOVER;
	}

	// Scroll the textbox using mouse
	fScrollTime += tLX->fDeltaTime;

	// Save the current position
	iLastCurpos = iCurpos;

	bool scrolled = false;

	if(fScrollTime > 0.05f)  {
		// Scroll left
		if(iScrollPos && tMouse->X-iX <= -5)  {
			if(iScrollPos > 0)  {
				iScrollPos--;
				iCurpos--;
				scrolled = true;
			}
		}

		// Scroll right
		else if(tLX->cFont.GetWidth(Utf8SubStr(sText, iScrollPos)) > iWidth-5 && tMouse->X > iX+iWidth)  {
			if (iCurpos <= Utf8StringSize(sText))  {
				iScrollPos++;
				iCurpos++;
				scrolled = true;
			}
		}

		fScrollTime = 0;
	}

	// If we click somewhere behind the text, but in the text box, set the cursor to the end
	if(deltaX > tLX->cFont.GetWidth(Utf8SubStr(sText, iScrollPos)) && InBox(tMouse->X,tMouse->Y))  {
		iCurpos = Utf8StringSize(sText);
	}
	else  {
		// Set the new cursor pos
		if (deltaX < (tLX->cFont.GetWidth(Utf8SubStr(sText, 0, 1)) / 2))  // Area before the first character
			iCurpos = iScrollPos;
		else  {
			std::string buf = Utf8SubStr(sText, iScrollPos);
			size_t pos = Utf8StringSize(sText);
			int w, prev_w;
			w = prev_w = 0;
			while ((w = tLX->cFont.GetWidth(buf)) > deltaX && pos)  {
				pos--;
				Utf8Erase(buf, pos);
				prev_w = w;
			}

			if ((prev_w - w) / 3 < (deltaX - w))
				pos++;

			iCurpos = pos + iScrollPos;

		}  // else 2
	}  // else 1

	fTimeHolding += tLX->fDeltaTime;

	// Holding the mouse
	/*if (fTimeHolding > 0.25)
		bHoldingMouse = true;*/
	bHoldingMouse = !tMouse->FirstDown;

	// Safety (must be *after* mouse scrolling)
	if (iCurpos > Utf8StringSize(sText))  {
		iCurpos = Utf8StringSize(sText);
	}

	if(bHoldingMouse)  {
		if ((abs(tMouse->X - iLastMouseX) && (iCurpos - iLastCurpos)) || scrolled)  {
			iSelLength += -(int)(iCurpos - iLastCurpos);
			// We can't lose the focus
		}
		bCanLoseFocus = false;
	}

	if (tMouse->FirstDown)  {
		// Remove any previous selection
		iSelLength = 0;
		iSelStart = 0;
		sSelectedText = "";
		iLastCurpos = iCurpos;
		// We can lose focus now
		bCanLoseFocus = true;
	}

	// Paste with 2nd or 3rd mouse button
	if( ! bHoldingMouse && 
		( tMouse->FirstDown & SDL_BUTTON(2) || tMouse->FirstDown & SDL_BUTTON(3) ) )
		PasteText();

	// Set up the variables for selection
	//iLastCurpos = iCurpos;
	iLastMouseX = tMouse->X;
	if(!bHoldingMouse)
		bHoldingMouse = true;

	return TXT_MOUSEOVER;

}

///////////////////
// Mouse up event
int	CTextbox::MouseUp(mouse_t *tMouse, int nDown)
{
	SetGameCursor(CURSOR_TEXT);

	fTimeHolding = 0;

	bHoldingMouse = false;

	iLastMouseX = 0;

	fScrollTime = 0;  // We can scroll the textbox using mouse

	// Doubleclick
	if (tLX->currentTime - fLastClick <= 0.25)  {
		SelectWord();
	}

	fLastClick = tLX->currentTime;

	// We can lose focus now
	bCanLoseFocus = true;

	return TXT_NONE;
}

////////////////////
// Mouse over event
int CTextbox::MouseOver(mouse_t *tMouse)
{
	SetGameCursor(CURSOR_TEXT);
	bCanLoseFocus = !tMouse->Down;
	return TXT_MOUSEOVER;
}

////////////
// Select a word
void CTextbox::SelectWord()
{
	// Empty text
	if (sText.size() == 0)
		return;

	std::string::iterator curpos = Utf8PositionToIterator(sText, iCurpos);
	std::string::iterator left = curpos;
	std::string::iterator right = curpos;

	iSelStart = iCurpos;

	// Go from cursor to left, until a beginning of a word or beginning of the text
	do  {
		DecUtf8StringIterator(left, sText.begin());
		iSelStart--;
	} while (left != sText.begin() && (isalnum((uchar)*left) || (uchar)*left >= 128));  // >= 128 - UTF8 hack, if you have a better idea how to do it, please fix it

	if (!isalnum((uchar)*left) && (uchar)*left < 128)  {
		iSelStart++;
	}

	// Go from cursor to right, until an end of a word or end of the text
	do  {
		IncUtf8StringIterator(right, sText.end());
		iCurpos++;
	} while (right != sText.end() && (isalnum((uchar)*right) || (uchar)*right >= 128));

	// Set the selection
	iSelLength = (int)iSelStart - (int)iCurpos;
}


///////////////////
// Backspace
void CTextbox::Backspace()
{
	// Delete any selection
	if(iSelLength)  {
		Delete();
		return;
	}

	if(iCurpos<=0)
		return;

	Utf8Erase(sText, --iCurpos, 1);
	
	if (iScrollPos)
		iScrollPos--;
}


///////////////////
// Delete
void CTextbox::Delete()
{
	// Delete selection
	if(iSelLength)  {
		Utf8Erase(sText, iSelStart, abs(iSelLength));
		iCurpos = iSelStart;
		iSelLength = 0;
		if (iScrollPos > iCurpos)
			iScrollPos = iCurpos;
		if (tLX->cFont.GetWidth(sText) <= iWidth-5)
			iScrollPos = 0;
		return;
	}


	if(iCurpos >= Utf8StringSize(sText))
		return;

	Utf8Erase(sText, iCurpos, 1);
}


///////////////////
// Insert a character
void CTextbox::Insert(UnicodeChar c)
{
	// Delete any selection
	if (iSelLength)
		Delete();

	// If no unicode, try to convert to ascii
	if (iFlags & TXF_NOUNICODE)  {
		c = (unsigned char)UnicodeCharToAsciiChar(c);
		if (c == 0xFF)  // Cannot convert
			return;
	}

	// Check for the max
	if(Utf8StringSize(sText) >= iMax)
		return;

	// Check that the current font can display this character, if not, quit
	if (!tLX->cFont.CanDisplayCharacter(c))
		return;

	// Safety
	if(iCurpos > Utf8StringSize(sText))
		iCurpos = Utf8StringSize(sText);

	Utf8Insert(sText, iCurpos++, GetUtf8FromUnicode(c));

    // If the text size is greater than the textbox size, scroll the text
	if (iCurpos == Utf8StringSize(sText))
		while(tLX->cFont.GetWidth(Utf8SubStr(sText, iScrollPos)) > iWidth-5) {
			iScrollPos++;
		}
}

/////////////////////
// Replace the current text with the buf
void CTextbox::setText(const std::string& buf) {
	sText = buf;

	iCurpos = Utf8StringSize(sText);
	iScrollPos = 0;
	iSelStart = iCurpos;
	iSelLength = 0;
	sSelectedText = "";
	iLastCurpos = iCurpos;
	fScrollTime = 0;
}

///////////////////
// This widget is send a message
uintptr_t CTextbox::SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2) {

	switch(iMsg) {

		// Get the text length
		case TXM_GETTEXTLENGTH:
			return (uintptr_t)Utf8StringSize(sText);
			break;

		// Set some flags
		case TXM_SETFLAGS:
			iFlags = (int)Param1;
			break;

		// Set the max length
		case TXM_SETMAX:
			iMax = Param1;
			if (Utf8StringSize(sText) > iMax)
				Utf8Erase(sText, iMax);

			break;

	}

	return 0;
}

uintptr_t CTextbox::SendMessage(int iMsg, const std::string& sStr, uintptr_t Param)
{
	switch (iMsg)  {

	// Get the text
	case TXS_SETTEXT:
		setText(sStr);
		break;
	}

	return 0;
}


uintptr_t CTextbox::SendMessage(int iMsg, std::string *sStr, uintptr_t Param)
{
	switch (iMsg)  {

	// Get the text
	case TXS_GETTEXT:
		*sStr = sText;
		return sText != "";
		break;
	}

	return 0;
}


///////////////////
// Paste some text from the clipboard
void CTextbox::PasteText()
{
	std::string text;

	if(iSelLength)
		Delete();

    text = copy_from_clipboard();
	replace(text, "\r", " ");
	replace(text, "\n", "<br>");
	replace(text, "\t", " ");

	// When copying from clipboard huge string will lock the game for several seconds
	/*if(text.size() > 1000)
		text.erase(1000);
	
	// Insert the text
	for(std::string::const_iterator i = text.begin(); i != text.end(); )
		Insert( GetNextUnicodeFromUtf8(i, text.end()) );*/

	// Delete any selection
	if (iSelLength)
		Delete();

	// If no unicode, try to convert to ascii
	if (iFlags & TXF_NOUNICODE)  {
		text = UnicodeToAscii(text);
	}

	size_t txtlen = Utf8StringSize(sText);
	size_t newtxtlen = Utf8StringSize(text);

	// Check for the max
	if(txtlen + newtxtlen >= iMax)  {
		if (txtlen >= iMax)
			return;

		newtxtlen = iMax - txtlen;
		Utf8Erase(text, newtxtlen);
	}

	// Check that the current font can display this character, if not, quit
	/*if (!tLX->cFont.CanDisplayCharacter(c))
		return;*/

	// Safety
	if(iCurpos > txtlen)
		iCurpos = txtlen;

	Utf8Insert(sText, iCurpos + 1, text);
	iCurpos += newtxtlen;
	txtlen = txtlen + newtxtlen;

    // If the text size is greater than the textbox size, scroll the text
	while(tLX->cFont.GetWidth(Utf8SubStr(sText, iScrollPos, MAX<int>(0, (int)iCurpos - (int)iScrollPos))) > iWidth-5) {
		iScrollPos++;
	}
}

///////////////////
// Copy selection to the clipboard
void CTextbox::CopyText()
{
	if (!iSelLength)
		return;
	copy_to_clipboard(sSelectedText);
}

}; // namespace DeprecatedGUI
