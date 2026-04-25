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
// Created 31/7/02
// Jason Boettcher


#ifndef __CINPUTBOX_H__DEPRECATED_GUI__
#define __CINPUTBOX_H__DEPRECATED_GUI__

#include "InputEvents.h"
#include "DeprecatedGUI/CGuiSkinnedLayout.h"
#include "CodeAttributes.h"

namespace DeprecatedGUI {

// Inputbox events
enum {
	INB_NONE=-1,
	INB_MOUSEUP
};


// inputbox messages
enum {
	INM_GETVALUE=0,
	INS_GETTEXT
};


class CInputbox : public CWidget {
public:
	// Constructor
	CInputbox(int val, const std::string& _text, SmartPointer<SDL_Surface> img, const std::string& name) {
		iKeyvalue = val;
		sText = _text;
		sName = name;

		bmpImage = img;
		iType = wid_Inputbox;
		bMouseOver = false;
		sVar = NULL;
	}


private:
	// Attributes

	int			iKeyvalue;
	std::string	sText;
	SmartPointer<SDL_Surface> bmpImage;
	bool		bMouseOver;
	std::string	sName;

	std::string		*sVar;

public:
	// Methods

	void	Create() { }
	void	Destroy() { }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse)			{ bMouseOver = true; return INB_NONE; }
	int		MouseUp(mouse_t *tMouse, int nDown)		{ return INB_MOUSEUP; }
	int		MouseDown(mouse_t *tMouse, int nDown)	{ return INB_NONE; }
	int		MouseWheelDown(mouse_t *tMouse)		{ return INB_NONE; }
	int		MouseWheelUp(mouse_t *tMouse)		{ return INB_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return INB_NONE; }
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ return INB_NONE; }


	// Process a message sent
	INLINE uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2) {

				if(iMsg == INM_GETVALUE) {
						return iKeyvalue;
				}

				return 0;
			}

	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  {
		if (iMsg == INS_GETTEXT)  {
			*sStr = sText;
		}
		return 0;
	}

	// Draw the title button
	void	Draw(SDL_Surface * bmpDest);

	INLINE void	LoadStyle() {}


	INLINE int		getValue()						{ return iKeyvalue; }
	INLINE void	setValue(int _v)					{ iKeyvalue = _v; }
	INLINE std::string	getText()				{ return sText; }
	INLINE void	setText(const std::string& _t)		{ sText = _t; }
	INLINE std::string	getName()				{ return sName; }

	static CWidget * WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy );
	void	ProcessGuiSkinEvent(int iEvent);

	static CInputbox * InputBoxSelected;
	static std::string InputBoxLabel;	// "GUI.InputBoxLabel" skin string
	friend class CInputboxInput;
};

class CInputboxInput: public CInputbox	// InputBoxDialog.xml should contain exactly one such control at the end
{
	private:
	int		iSkipFirstFrame;

	public:
	CInputboxInput();
	~CInputboxInput();
	
	void	Draw(SDL_Surface * bmpDest) {};

	static CWidget * WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy );
	
	void	ProcessGuiSkinEvent(int iEvent);

	static void UpdateCallback( const std::string & param, CWidget * source );
};

} // namespace DeprecatedGUI

#endif  //  __CINPUTBOX_H__DEPRECATED_GUI__
