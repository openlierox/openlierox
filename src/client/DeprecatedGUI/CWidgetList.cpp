/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Custom browser class
// Created 7/6/02
// Jason Boettcher


#include "LieroX.h"

#include "DeprecatedGUI/Menu.h"

/*					 */
/*	Widget IDs list  */
/*					 */

namespace DeprecatedGUI {

//////////////////
// Adds a new item to the widget ID list
// Returns the id of added item
int CWidgetList::Add(const std::string& Name)
{
	if (Name == "")
		return -1;

	// Find the ID of the new item
	int id = iCount+1;

	// Create new item
	widget_item_t *new_item = new widget_item_t;
	if (!new_item)
		return -1;

	// Fill in the item details
	new_item->iID = id;
	new_item->sName = Name;
	new_item->tNext = NULL;  // It will be the last item

	// Link it in
	widget_item_t *last_item = tItems;

	// Add the new item at the end of the list
	if(last_item)  {
		while(last_item->tNext)  {
			last_item = last_item->tNext;
		}
		last_item->tNext = new_item;
	}

	// The list is emtpy
	else  {
		tItems = new_item;
	}

	// Successfully added
	iCount++;

	return id;
}

////////////////
// Get the name of widget by it's ID
std::string CWidgetList::getName(int ID)
{
	// The list is empty
	if (!tItems)
		return "";

	// Go through the items
	widget_item_t *item = tItems;
	while(item)  {
		if (item->iID == ID)
			return item->sName;
		item = item->tNext;
	}

	// Not found
	return "";
}

////////////////
// Get the ID of widget by it's name
int	CWidgetList::getID(const std::string& Name)
{
	// The list is empty
	if (!tItems)
		return -1;

	// No name specified
	if (Name == "")
		return -1;

	// Go through the items
	widget_item_t *item = tItems;
	while(item)  {
		if (Name == item->sName)
			return item->iID;
		item = item->tNext;
	}

	// Not found
	return -1;
}

///////////////
// Shutdown the widget IDs list
void CWidgetList::Shutdown()
{
	// The list is already empty
	if (!tItems)
		return;

	// Go through the list an delete each item
	widget_item_t *item = tItems;
	widget_item_t *next = NULL;
	for(;item;item=next)  {
		// Delete the item
		next = item->tNext;
		delete item;
	}
}

}; // namespace DeprecatedGUI
