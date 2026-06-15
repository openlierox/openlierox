/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// gui list view class
// Created 30/5/02
// Jason Boettcher


#ifndef __CLISTVIEW_H__DEPRECATED_GUI__
#define __CLISTVIEW_H__DEPRECATED_GUI__

#include <string>
#include "DeprecatedGUI/CWidget.h"
#include "DeprecatedGUI/CScrollbar.h"
#include "DynDraw.h"
#include "Utils.h"
#include "CodeAttributes.h"

namespace DeprecatedGUI {

// Event msg's for this control
enum {
	LV_NONE=-1,
	LV_CHANGED=0,
	LV_RIGHTCLK,
	LV_DOUBLECLK,
	LV_DELETE,
	LV_ENTER,
	LV_WIDGETEVENT,
	LV_MOUSEOVER
};


// Listview messages
enum {
	LVS_ADDCOLUMN=0,
	LVS_ADDITEM,
	LVM_REMOVEITEM,
	LVM_GETCURINDEX,
	LVS_GETCURSINDEX,
	LVM_GETITEMCOUNT,
	LVM_CLEAR,
	LVM_GETITEMINDEX,
	LVM_GETINDEX,
    LVM_GETCURITEM,
	LVM_SETOLDSTYLE,
	LVM_GETCOLUMNWIDTH
};


// Sub-item types
enum {
	LVS_IMAGE=0,
	LVS_TEXT,
	LVS_WIDGET
};

// Subitem vertical aligns
enum  {
	VALIGN_TOP=0,
	VALIGN_MIDDLE,
	VALIGN_BOTTOM
};



// Column structure
struct lv_column_t : DontCopyTag {
	std::string	sText;
	int			iWidth;
	bool		bDown;
	int			iSorted; // -1 = unsorted, 0 = descending, 1 = ascending
	Color		iColour;

	lv_column_t *tNext;

};


// Sub item structure
struct lv_subitem_t : DontCopyTag {
	lv_subitem_t() : iType(0), tWidget(NULL), fMouseOverTime(0), tNext(NULL) {} // safety
	~lv_subitem_t()  { if (tWidget) delete tWidget; }

	int			iType;
	std::string	sText;
	std::string sTooltip;
	SmartPointer<DynDrawIntf> bmpImage;
	CWidget		*tWidget;
	bool		bVisible;
	int			iExtra;
	int			iValign;
	Color		iColour;
	Color		iBgColour;

	TimeDiff	fMouseOverTime;
	lv_subitem_t *tNext;

};


// Item structure
struct lv_item_t : DontCopyTag {
	lv_item_t() : iIndex(0), tSubitems(NULL), tNext(NULL) {}
	
	std::string	sIndex;
	int			iIndex;
    int         _iID;
	bool		bSelected;
	int			iHeight;
	Color		iColour;
	Color		iBgColour;

	lv_subitem_t *tSubitems;

	lv_item_t	*tNext;

};

// Listview control class
class CListview: public CWidget, DontCopyTag {
public:
	// Constructors
	CListview() {
		tColumns = NULL;
		iNumColumns = 0;
		tItems = NULL;
		tLastItem = NULL;
		tSelected = NULL;
		tPreviousMouseSelection = NULL;
		iItemCount=0;
		bGotScrollbar = false;
		iType = wid_Listview;
		fLastMouseUp = AbsTime();
		iContentHeight = 0;
        iItemID = 0;
        bShowSelect = true;
		iLastMouseX = 0;
		iGrabbed = 0;
		bOldStyle = false;
		iSavedScrollbarPos = 0;
		iLastChar = 0;
		bDrawBorder = true;
		bNeedsRepaint = true;
		bCustomScrollbarSetup = false;
		bAlwaysVisibleScrollbar = false;
		tFocusedSubWidget = NULL;
		tMouseOverSubWidget = NULL;
		bSubItemsAreAligned = false;
		bMouseOverEventEnabled = false;
		bTooltipVisible = false;
		tMouseOver = NULL;
		holdedWidget = NULL;
	}

	~CListview() {
		Destroy();
	}


private:
	// Attributes
	bool			bOldStyle;
	bool			bDrawBorder;
	bool            bShowSelect;
	bool			bCustomScrollbarSetup;
	bool			bAlwaysVisibleScrollbar;

	// Columns
	int				iNumColumns;
	lv_column_t		*tColumns;
	int				iGrabbed;
	int				iLastMouseX;

	// Items
	lv_item_t		*tItems;
	lv_item_t		*tLastItem;
	lv_item_t		*tSelected;
	int				iItemCount;
    int             iItemID;
	int				iContentHeight;
	bool			bSubItemsAreAligned; // if the left item is too long, subitems are shifted right
	
	AbsTime			fLastMouseUp;
	int				iClickedSub;
	lv_item_t		*tPreviousMouseSelection;

	// Scrollbar
	CScrollbar		cScrollbar;
	bool			bGotScrollbar;
	int				iSavedScrollbarPos;

	// Tooltips
	bool			bTooltipVisible;
	std::string		sTooltipText;
	int				iTooltipX;
	int				iTooltipY;
	
	// Other
	bool			bNeedsRepaint;
	UnicodeChar		iLastChar;
	gui_event_t		tLastWidgetEvent;
	CWidget			*holdedWidget; // moused pressed and not released
	CWidget			*tFocusedSubWidget;
	CWidget			*tMouseOverSubWidget;

	bool			bMouseOverEventEnabled;
	lv_item_t		*tMouseOver;

private:
	void	ShowTooltip(const std::string& text, int ms_x, int ms_y);
	void	UpdateItemIDs();
	void	MoveMouseToCurrentItem();

public:
	// Methods

	void	Create();
	void	Destroy();

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse);
	int		MouseUp(mouse_t *tMouse, int nDown);
	int		MouseDown(mouse_t *tMouse, int nDown);
	int		MouseClicked(mouse_t *tMouse, int nDown);
	int		MouseWheelDown(mouse_t *tMouse);
	int		MouseWheelUp(mouse_t *tMouse);
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate);
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate);

	void	Draw(SDL_Surface * bmpDest);

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2);
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param);
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param);

	void	ReadjustScrollbar();
	void	SetupScrollbar(int x, int y, int h, bool always_visible);

	void	Clear();

	void	SortBy(int column, bool ascending); // One-time sort
	void	ReSort();
	void	SetSortColumn(int column, bool ascending); // Permanent sort
	int		GetSortColumn();

	void	AddColumn(const std::string& sText, int iWidth);
	void	AddColumn(const std::string& sText, int iWidth, Color iColour);
	lv_item_t* AddItem(const std::string& sIndex, int iIndex, Color iColour);
	void	AddSubitem(int iType, const std::string& sText, const SmartPointer<DynDrawIntf> & img, CWidget *wid, int iVAlign = VALIGN_MIDDLE, const std::string& tooltip = "");
	void	AddSubitem(int iType, const std::string& sText, const SmartPointer<DynDrawIntf> & img, CWidget *wid, int iVAlign, Color iColour, const std::string& tooltip = "");
	void	AddSubitem(int iType, const std::string& sText, const SmartPointer<SDL_Surface> & img, CWidget *wid, int iVAlign = VALIGN_MIDDLE, const std::string& tooltip = "") {
		AddSubitem(iType, sText, DynDrawFromSurface(img), wid, iVAlign, tooltip);
	}
	void	AddSubitem(int iType, const std::string& sText, const SmartPointer<SDL_Surface> & img, CWidget *wid, int iVAlign, Color iColour, const std::string& tooltip = "") {
		AddSubitem(iType, sText, DynDrawFromSurface(img), wid, iVAlign, iColour, tooltip);		
	}
	void	AddSubitem(const std::string& sText, int iVAlign = VALIGN_MIDDLE, const std::string& tooltip = "") {
		AddSubitem(LVS_TEXT, sText, (DynDrawIntf*)NULL, NULL, iVAlign, tooltip);
	}
	void	AddSubitem(const std::string& sText, int iVAlign, Color iColour, const std::string& tooltip = "") {
		AddSubitem(LVS_TEXT, sText, (DynDrawIntf*)NULL, NULL, iVAlign, iColour, tooltip);
	}

	void	RemoveItem(int iIndex);
	int		getIndex(int count);

	int		GetColumnWidth(int id);

    int     getSelectedID();
    void    setSelectedID(int id);

	void	setOldStyle(bool _s)	{ bOldStyle = _s; }
	bool	getOldStyle()		{ return bOldStyle; }

	bool&	subItemsAreAligned()	{ return bSubItemsAreAligned; }
	
    int     getNumItems()	{ return iItemCount; }

    void    scrollLast();

	int		getCurIndex()		{ if(tSelected) return tSelected->iIndex; else return -1; }
	std::string getCurSIndex()		{ if(tSelected) return tSelected->sIndex; else return ""; }
	lv_subitem_t	*getCurSubitem(int index);


	lv_subitem_t	*getCurSub();

	int			getItemCount()		{ return iItemCount; }
	lv_item_t	*getItems()			{ return tItems; }
	lv_item_t	*getLastItem()	{ return tLastItem; }
	lv_item_t* getItem(int index);
	lv_item_t* getItem(const std::string& name);
	lv_subitem_t *getSubItem(int item_index, int subitem_index);
	lv_subitem_t *getSubItem(lv_item_t *it, int subitem_index);

	int		getClickedSub()		{ return iClickedSub; }
	gui_event_t *getWidgetEvent()	{ return &tLastWidgetEvent; }

    void    setShowSelect(bool s)   { bShowSelect = s; }
	void	setDrawBorder(bool _d)	{ bDrawBorder = _d; }

	void	SaveScrollbarPos()    { iSavedScrollbarPos = cScrollbar.getValue(); }
	void	RestoreScrollbarPos() { cScrollbar.setValue(iSavedScrollbarPos); iSavedScrollbarPos = 0; }

	INLINE bool	NeedsRepaint()  {return bNeedsRepaint; }
	INLINE void	SetRepaint(bool _r)  { bNeedsRepaint = _r; }  // Explicitly set this listview needs to be repainted
	
	void	setMouseOverEventEnabled(bool b)	{ bMouseOverEventEnabled = b; }
	int		getMouseOverIndex()		 { if(tMouseOver) return tMouseOver->iIndex; else return -1; }
	std::string getMouseOverSIndex() { if(tMouseOver) return tMouseOver->sIndex; else return ""; }
};

} // namespace DeprecatedGUI

#endif  //  __CLISTVIEW_H__DEPRECATED_GUI__
