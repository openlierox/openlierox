/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Combo box
// Created 3/9/02
// Jason Boettcher


#ifndef __CCOMBOBOX_H__DEPRECATED_GUI__
#define __CCOMBOBOX_H__DEPRECATED_GUI__

#include <list>
#include <boost/signals2.hpp>

#include "StringUtils.h"
#include "InputEvents.h"
#include "DeprecatedGUI/CScrollbar.h"
#include "DynDraw.h"
#include "gui/List.h"
#include "DeprecatedGUI/CGuiSkin.h"

class ScriptVar_t;
struct ScriptVarPtr_t;

namespace DeprecatedGUI {

// Combo box events
enum {
	CMB_NONE=-1,
	CMB_CHANGED
};

// Combobox messages
enum {
	CBS_ADDITEM,
	CBS_ADDSITEM,
	CBM_GETCOUNT,
	CBM_GETCURINDEX,
	CBS_GETCURSINDEX,
    CBS_GETCURNAME,
	CBM_CLEAR,
	CBM_SETCURSEL,
    CBS_SETCURSINDEX,
    CBM_SETCURINDEX,
	CBM_ISDROPPED,
	CBM_SETSORTED,
	CBM_SETUNIQUE
};

// Sorting directions
enum  {
	SORT_DESC = -1,
	SORT_NONE = 0,
	SORT_ASC = 1
};


// Item structure
struct cb_item_t : GuiListItem {
	cb_item_t() : iTag(0) {}
	cb_item_t(const std::string& name) : sName(name), iTag(0) {}
	
	std::string	sIndex;
	std::string	sName;
	SmartPointer<DynDrawIntf> tImage;
	int iTag;
	
	virtual std::string caption() { return sName; }
	virtual SmartPointer<DynDrawIntf> image() { return tImage; }
	
	virtual std::string index() { return sIndex; }
	virtual int tag() { return iTag; }
	
	virtual void setImage(const SmartPointer<DynDrawIntf>& img) { tImage = img; }
};



class CCombobox : public CWidget {
public:
	// Constructor
	CCombobox() {
		iSelected = 0;
		bGotScrollbar = false;
		bDropped = false;
		bArrowDown = false;
        bLastDropped = false;
		iDropTime = AbsTime();
		iNow = AbsTime();
		bCanSearch = true;
		iKeySelectedItem = -1;
		iSortDirection = SORT_NONE;
		bUnique = false;
		iType = wid_Combobox;
		var = NULL;
		varPtr = NULL;
	}


private:
	// Attributes

	// Items
	GuiList::Pt listBackend;
	std::list<GuiListItem::Pt> tItems;
	int 			iSelected;
	bool			bGotScrollbar;
	bool			bDropped;
	bool			bArrowDown;
    bool			bLastDropped;
	AbsTime			iDropTime;
	AbsTime			iNow;
	int				iKeySelectedItem;

	// Stuff
	bool			bCanSearch;
	int				iSortDirection;
	bool			bUnique;

	// Scrollbar
	CScrollbar		cScrollbar;

	ScriptVar_t		*var;
	ScriptVarPtr_t	*varPtr;
	CGuiSkin::CallbackHandler cClick;

private:
	GuiListItem::Pt getItemRW(int index);
	void	Sort(bool ascending);
	void	Unique();
	std::list<GuiListItem::Pt>::iterator lowerBound(const GuiListItem::Pt& item, int *index, bool *equal);
	std::list<GuiListItem::Pt>::iterator upperBound(const GuiListItem::Pt& item, int *index, bool *equal);

public:
	// Methods

	void	Create();
	void	Destroy();

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse);
	int		MouseUp(mouse_t *tMouse, int nDown);
	int		MouseDown(mouse_t *tMouse, int nDown);
	int		MouseWheelDown(mouse_t *tMouse);
	int		MouseWheelUp(mouse_t *tMouse);
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)	{ bCanSearch = true; return CMB_NONE; }
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate);

	void	Draw(SDL_Surface * bmpDest);
	void	LoadStyle() {}
	
	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2);
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param);
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param);

	void	setListBackend(const GuiList::Pt& l) { listBackend = l; }
	/*
	 In a perfect world, we would not need this function and it all should
	 be done automatically (the GuiList automatically pushes the update).
	 Though, this is easier for now - so now, when you use a list backend
	 and when it needs an update, you must call this. This will then
	 clear the current list and just copy everything over from the GuiList.
	 */
	void	updateFromListBackend();

    void    clear();
	int		addItem(const std::string& sindex, const std::string& name, const SmartPointer<DynDrawIntf>& img = NULL, int tag = 0);
	int		addItem(int index, const std::string& sindex, const std::string& name, const SmartPointer<DynDrawIntf>& img = NULL, int tag = 0);
	const std::list<GuiListItem::Pt>& getItems()	{ return tItems; }
	const GuiListItem::Pt getItem(int index) const;
	int getItemIndex(const GuiListItem::Pt& item);	
	int		getItemsCount();
	const GuiListItem::Pt getItem(const std::string& name) const;
	const GuiListItem::Pt getSIndexItem(const std::string& sIndex) const;
	int getIndexBySIndex(const std::string& szName);
	int getIndexByName(const std::string& szName);
	void	setCurItem(int index);
	void	setCurItem(const GuiListItem::Pt& item);
    void    setCurSIndexItem(const std::string& szString);
    void    setCurItemByName(const std::string& szString);
    bool	selectNext();
    bool	selectPrev();
    int		findItem(UnicodeChar startLetter);
	void	setImage(const SmartPointer<DynDrawIntf>& img, int ItemIndex);
	int		getSelectedIndex();
	const GuiListItem::Pt getSelectedItem();
	bool	getDropped() { return bDropped; }
	void	setSorted(int sort_direction);
	int		getSorted();
	void	setUnique(bool _u);
	bool	getUnique();
	int getItemHeight();
	
	const GuiListItem::Pt getLastItem();

	void	setAttachedVar(ScriptVar_t* v)	{ var = v; }
	void	setAttachedVar(ScriptVarPtr_t* v)	{ varPtr = v; }
	
	static CWidget * WidgetCreator( const std::vector< ScriptVar_t > & p, CGuiLayoutBase * layout, int id, int x, int y, int dx, int dy );
	void	ProcessGuiSkinEvent(int iEvent);
	
	boost::signals2::signal<void (const GuiListItem::Pt&)> OnChangeSelection;

};

} // namespace DeprecatedGUI

#endif  //  __CCOMBOBOX_H__DEPRECATED_GUI__
