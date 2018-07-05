/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// gui Listview class
// Created 16/6/02
// Jason Boettcher


#include "LieroX.h"

#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "StringUtils.h"
#include "Cursor.h"
#include "Timer.h"
#include "sound/SoundsBase.h"
#include "AuxLib.h"


namespace DeprecatedGUI {

#define TOOLTIP_VSPACE 2
#define TOOLTIP_HSPACE 5

///////////////////
// Draw the list view
void CListview::Draw(SDL_Surface * bmpDest)
{
	bNeedsRepaint = false; // We're repainting :)

	lv_subitem_t *sub = NULL;

	if (bRedrawMenu)
		Menu_redrawBufferRect(iX,iY, iWidth,iHeight);


	// Draw the columns
	lv_column_t *col = tColumns;
	int x=iX+4;

	if (bOldStyle)  {
		for(short i=1;col;col = col->tNext,i++)   {
			tLX->cFont.Draw(bmpDest, x, iY, col->iColour, col->sText);
			x += col->iWidth;
		}
	} else {
		short col_w;
		for(short i=1;col;col = col->tNext,i++)   {
			col_w = col->iWidth;
			// Last column has to be thiner
			if (i == iNumColumns)  {
				if (x-2+col_w-1 != iX+iWidth-4)  { // Column wider than the space for it
					col_w = iWidth-x+iX+4;
					col->iWidth = col_w;  // Update it when we've counted it...
				}
				col_w--;
			}
			Menu_DrawWinButton(bmpDest,x-2,iY+2,col_w-3,tLX->cFont.GetHeight(),col->bDown);
			switch (col->iSorted)  {
			case 0:	DrawImage(bmpDest,tMenu->bmpTriangleUp,x+col_w-tMenu->bmpTriangleUp.get()->w-9,iY+7); break;
			case 1:	DrawImage(bmpDest,tMenu->bmpTriangleDown,x+col_w-tMenu->bmpTriangleDown.get()->w-9,iY+7); break;
			}

			tLX->cFont.DrawCentreAdv(bmpDest, x+(col_w/2)-3, iY+2, x+2, MIN(col_w-2,iX+iWidth-x), tLX->clNormalLabel, col->sText);

			x += col->iWidth-2;
		}
	}


	// Get the top
	int y=iY;
	if (tColumns)  {
		if (bOldStyle)
			y = iY+tLX->cFont.GetHeight();
		else
			y = iY+tLX->cFont.GetHeight()+2;
	}

	if (bDrawBorder)
		y += 2;

	// Re-setup the scrollbar
	if (!bCustomScrollbarSetup)  {
		cScrollbar.Setup(0, iX+iWidth-16, y, 14, iHeight - y + iY - 1);
	}

	x = iX+4;
	lv_item_t *item = tItems;
	int count=0;

	// Right bound
	int right_bound = iX+iWidth-2;
	if(bGotScrollbar || bAlwaysVisibleScrollbar)
		right_bound = MIN(cScrollbar.getX() - 2, iX + iWidth - 3);

	int texty = 0;

	{
		SDL_Rect clipRect = {(SDLRect::Type) (iX+4), (SDLRect::Type) y, (SDLRect::TypeS) (iWidth-8), (SDLRect::TypeS) (iHeight - (y-iY))};
		ScopedSurfaceClip clip(bmpDest, clipRect);
		
		// Draw the items
		for(;item;item = item->tNext) {
			if(count++ < cScrollbar.getValue())
				continue;

			x = iX+4;

			col = tColumns;

			// Find the max height
			int h = MAX(tLX->cFont.GetHeight(), item->iHeight);

			if(y + h >= 480) break; // TODO: seems that it crashs without this (test by commenting out the next line)
			if(y >= iY + iHeight) break;

			// Background colour?
			DrawRectFill(bmpDest, x - 2, y, right_bound, y + h - 2, item->iBgColour);

			// Selected?
			if(item->bSelected && bShowSelect) {
				if(bFocused)
					DrawRectFill(bmpDest,x-2,y,right_bound,y+h-2,tLX->clListviewSelected);
				else
					DrawRect(bmpDest,x-2,y,right_bound,y+h-2,tLX->clListviewSelected);
			}

			// Draw the sub items
			if(item->tSubitems) {

				sub = item->tSubitems;
				col = tColumns;


				// Text vertical position
				switch (sub->iValign)  {
				case VALIGN_TOP:
					texty = y;
					break;
				case VALIGN_BOTTOM:
					texty = y + h - tLX->cFont.GetHeight();
					break;
				default: // Middle
					texty = y + (h-tLX->cFont.GetHeight())/2;
					break;
				}

				for(;sub;sub = sub->tNext) {

					int itemWidth = 0;
					if(sub->bVisible) {
						// Background colour
						DrawRectFill(bmpDest, x - 2, y, col ? MIN(col->iWidth - 8, right_bound) : right_bound, y + h - 2, item->iBgColour);

						switch(sub->iType)  {
						case LVS_TEXT:  {
							// Get the colour
							Color colour = item->iColour;
							if (sub->iColour != tLX->clPink)
								colour = sub->iColour;

							if (col && !bOldStyle && !bSubItemsAreAligned)
								tLX->cFont.DrawAdv(bmpDest, x, texty, MAX(0, MIN( ( col->iWidth > 8 ? col->iWidth-8 : 0 ), right_bound-x)), colour, sub->sText);
							else
								tLX->cFont.DrawAdv(bmpDest, x, texty, right_bound-x-2, colour, sub->sText);
							
							if(bSubItemsAreAligned)
								itemWidth = tLX->cFont.GetWidth(sub->sText);
						}
						break;

						case LVS_IMAGE:
							{
								// Set special clipping
								SDL_Rect new_rect;
								new_rect.x = x;
								new_rect.y = y;
								new_rect.w = col ? col->iWidth - 4 : (iX + iWidth - x - 2);
								new_rect.h = item->iHeight;
								// Clipping
								if (!ClipRefRectWith((SDLRect&)new_rect, (SDLRect&)clipRect))
									break;
								ScopedSurfaceClip imgClip(bmpDest, new_rect);

								// Draw according to valign
								switch (sub->iValign)  {
								case VALIGN_TOP:
									sub->bmpImage->draw(bmpDest,x,y);
									break;

								case VALIGN_BOTTOM:
									sub->bmpImage->draw(bmpDest, x, y + item->iHeight - sub->bmpImage.get()->h);
									break;

								// Middle
								default:
									sub->bmpImage->draw(bmpDest, x, y + item->iHeight/2 - sub->bmpImage.get()->h/2);
									break;
								}
								
								break;
							}
						case LVS_WIDGET:
							CWidget *w = sub->tWidget;

							// Draw according to valign
							switch (sub->iValign)  {
							case VALIGN_TOP:
								w->Setup(w->getID(), x, y, w->getWidth(), w->getHeight());
								break;

							case VALIGN_BOTTOM:
								w->Setup(w->getID(), x, y + item->iHeight - w->getHeight(), w->getWidth(), w->getHeight());
								break;

							// Middle
							default:
								w->Setup(w->getID(), x, y + item->iHeight/2 - w->getHeight()/2, w->getWidth(), w->getHeight());
								break;
							}
							if (w->getEnabled())
								w->Draw(bmpDest);
						break;
						}
					}

					if(col) {
						if(bSubItemsAreAligned && itemWidth + 3 > col->iWidth) {
							x += itemWidth + 3;
						} else
							x += col->iWidth - 2;
						col = col->tNext;
					}
				}
			}

			y += h;
		}
	}

	// Draw the scrollbar
	if(bGotScrollbar || bAlwaysVisibleScrollbar)
		cScrollbar.Draw(bmpDest);

    // Draw the rectangle
	if (bDrawBorder)
		Menu_DrawBoxInset(bmpDest, iX, iY+(tLX->cFont.GetHeight()-1)*bOldStyle, iX+iWidth, iY+iHeight);

	// Draw tooltips
	if (bTooltipVisible)  {
		int w = tLX->cFont.GetWidth(sTooltipText) + 2 * TOOLTIP_HSPACE;
		int h = tLX->cFont.GetHeight(sTooltipText) + 2 * TOOLTIP_VSPACE;
		Color color = tLX->clComboboxShowAllMain;
		color.a = 220;
		DrawRectFill(bmpDest, iTooltipX, iTooltipY, iTooltipX + w, iTooltipY + h, color);
		tLX->cFont.Draw(bmpDest, iTooltipX + TOOLTIP_HSPACE, iTooltipY + TOOLTIP_VSPACE, tLX->clNormalText, sTooltipText);
		DrawRect(bmpDest, iTooltipX, iTooltipY, iTooltipX + w, iTooltipY + h, tLX->clComboboxShowAllBorder);
	}
	//bTooltipVisible = false;  // It might get re-enabled next frame
}

void CListview::AddColumn(const std::string& sText, int iWidth) {
	AddColumn(sText, iWidth, tLX->clNormalLabel);
}

///////////////////
// Add a column to the list view
void CListview::AddColumn(const std::string& sText, int iWidth, Color iColour)
{
	lv_column_t *col;

	col = new lv_column_t;
	if(col == NULL) {
		// Out of memory
		return;
	}

	// Set the info
	col->sText = sText;
	col->iWidth = iWidth;
	col->tNext = NULL;
	col->bDown = false;
	col->iSorted = -1;
	col->iColour = iColour;


	// Add it to the list
	int curwidth = 0;
	lv_column_t *widest = NULL;
	if(tColumns) {
		lv_column_t *c = tColumns;
		widest = c;
		for(;c;c = c->tNext) {
			curwidth += c->iWidth;
			if(c->tNext == NULL) {
				c->tNext = col;
				break;
			}
			if (c->iWidth > widest->iWidth)
				widest = c;
		}
	}
	else
		tColumns = col;

	int maxwidth = this->iWidth-4-curwidth;

	// If the specified width is weird (negative, zero or too wide), we'll count a better one
	if (iWidth <= 0 || iWidth > maxwidth)  {
		int defwidth = tLX->cFont.GetWidth(sText)+4;
		// The current columns take whole space, so we need to make some for the new column
		if (maxwidth <= 1)  {
			// Clamp the widest column to make space for this one
			if (widest)  {
				maxwidth = widest->iWidth/2;
				defwidth = maxwidth;
				widest->iWidth /= 2;
			} else {
				defwidth = maxwidth;
			}
		// There's still some space for the current column
		} else {
			// The column has weird (really short) name, so we use maximal size
			if (defwidth <= 5)
				defwidth = maxwidth;
			// Insert the column, make sure it's width is not greater than the maximal allowed width
			else
				defwidth = MIN(defwidth,maxwidth);
		}
		col->iWidth = defwidth;
	}

	iNumColumns++;
	bNeedsRepaint = true; // We need a repaint
}


///////////////////
// Add an item to the list view
lv_item_t* CListview::AddItem(const std::string& sIndex, int iIndex, Color iColour)
{
	lv_item_t *item = new lv_item_t;

	if(item == NULL) {
		// Out of memory
		return NULL;
	}

	// Set the info
	item->sIndex = sIndex;
	item->iIndex = iIndex;
	item->tNext = NULL;
	item->bSelected = false;
	item->tSubitems = NULL;
	item->iHeight = tLX->cFont.GetHeight();			// Text height
	item->iColour = iColour;
	item->iBgColour = tLX->clBlack;
	item->iBgColour.a = SDL_ALPHA_TRANSPARENT;
    item->_iID = iItemID++;

	// Add it to the list
	if(tItems) {
		lv_item_t *i = tItems;
		for(;i;i = i->tNext) {
			if(i->tNext == NULL) {
				i->tNext = item;
				break;
			}
		}
	}
	else {
		tItems = item;
		tSelected = item;
		tSelected->bSelected = true;
		//writeLog("Index: %d\n",tSelected->iIndex);
	}

	tLastItem = item;

	// Adjust the scrollbar
	iItemCount++;

	// Do we use a scrollbar?
	//if(cScrollbar.getMax()*20 >= iHeight)
	//	bGotScrollbar = true;

	// Readjust the scrollbar
	ReadjustScrollbar();

	// We need a repaint
	bNeedsRepaint = true;
	
	return item;
}

void CListview::AddSubitem(int iType, const std::string& sText, const SmartPointer<DynDrawIntf> & img, CWidget *wid, int iVAlign, const std::string& tooltip) {
	AddSubitem(iType, sText, img, wid, iVAlign, tLX->clPink, tooltip);
}

///////////////////
// Add a sub item to the last item
void CListview::AddSubitem(int iType, const std::string& sText, const SmartPointer<DynDrawIntf> & img, CWidget *wid, int iVAlign, Color iColour, const std::string& tooltip)
{
	// No last item
	if (!tLastItem)  {
		return;
	}

	// Allocate
	lv_subitem_t *sub = new lv_subitem_t;

	if(sub == NULL) {
		// Out of memory
		return;
	}

	// Set the info
	sub->iType = iType;
	sub->sText = sText;
	sub->bmpImage = NULL;
	sub->tWidget = NULL;
	sub->tNext = NULL;
	sub->bVisible = true;
	sub->iExtra = 0;
	sub->iValign = iVAlign;
	sub->sTooltip = tooltip;
	sub->fMouseOverTime = 0;
	if (iColour == tLX->clPink)
		sub->iColour = tLastItem->iColour;
	else
		sub->iColour = iColour;
	sub->iBgColour = tLX->clBlack;
	sub->iBgColour.a = SDL_ALPHA_TRANSPARENT;

	// Set special info
	switch (iType)  {
	case LVS_IMAGE:
		if (!img.get())  {
			delete sub;
			return;
		}
		sub->bmpImage = img;
		tLastItem->iHeight = MAX(tLastItem->iHeight, img.get()->h);
		break;
	case LVS_WIDGET:
		if (!wid)  {
			delete sub;
			return;
		}
		sub->tWidget = wid;
		tLastItem->iHeight = MAX(tLastItem->iHeight, wid->getHeight());
		break;
	}

	// Add this sub item to the current item
	if(tLastItem->tSubitems) {
		lv_subitem_t *s = tLastItem->tSubitems;
		for(;s;s = s->tNext) {
			if(s->tNext == NULL) {
				s->tNext = sub;
				break;
			}
		}
	} else
		tLastItem->tSubitems = sub;



	// Find the max height of this item
	sub = tLastItem->tSubitems;
	for(;sub;sub = sub->tNext) {
		if(sub->iType == LVS_IMAGE) {
			tLastItem->iHeight = MAX(tLastItem->iHeight,(sub->bmpImage.get()->h+4));
		}
	}

	// Readjust the scrollbar
	ReadjustScrollbar();

	bNeedsRepaint = true; // Repaint required
}

///////////////////
// Shows a tooltip
void CListview::ShowTooltip(const std::string &text, int ms_x, int ms_y)
{
	if (!text.size() || (bTooltipVisible && sTooltipText == text))
		return;

	bTooltipVisible = true;
	iTooltipY = ms_y - tLX->cFont.GetHeight(text) - 2 * TOOLTIP_VSPACE;
	iTooltipX = ms_x - TOOLTIP_HSPACE - tLX->cFont.GetWidth(text) / 2;
	sTooltipText = text;
}


///////////////////
// Re-adjust the scrollbar
void CListview::ReadjustScrollbar()
{
	// Go through each item and find the average height
	lv_item_t *item = tItems;
	int count = 0;
	int size = 0;

	for(;item;item=item->tNext) {
		size += item->iHeight;
		count++;
	}

	// Buffer size on top & bottom
	int display_height = iHeight;  // Size of box with items
	if (tColumns)
		display_height -= tLX->cFont.GetHeight() + 4;
	if (bDrawBorder)
		display_height -= 4;

	if(count == 0) {
		cScrollbar.setItemsperbox(0);
		cScrollbar.setValue(0);
		bGotScrollbar = false;

		// Return to prevent a divide-by-zero
		return;
	}

	// Find the average
	size /= count;

	int h = display_height / size;

	cScrollbar.setItemsperbox( h );
    cScrollbar.setMax(count);
    cScrollbar.setValue(0);

	bGotScrollbar = count > h;
}


///////////////////
// Remove an item from the list
void CListview::RemoveItem(int iIndex)
{
	lv_item_t *prev = NULL;
	lv_item_t *i = tItems;
	lv_item_t *next = NULL;
	lv_subitem_t *s,*sub;
	int first = true;

	// Find the item and it's previous item
	prev = i;
	for(;i;i=i->tNext) {

		if(i->iIndex == iIndex) {
			next = i->tNext;

			// If it's the first item, we do it differently
			if(first) {

				// Free the sub items
				for(s=i->tSubitems;s;s=sub) {
					sub = s->tNext;
					if (s->tWidget == tFocusedSubWidget)
						tFocusedSubWidget = NULL;
					delete s;
				}
				delete i;

				// Set the first one to point to the next in the list
				tItems = next;
			} else {
				// Free the sub items
				for(s=i->tSubitems;s;s=sub) {
					sub = s->tNext;
					if (s->tWidget == tFocusedSubWidget)
						tFocusedSubWidget = NULL;
					delete s;
				}
				delete i;

				// The previous item now points to the next one
				prev->tNext = next;
			}
			i = next;
			iItemCount--;
			//break;
		}

		if(!i)
			break;

		prev = i;
		first=false;
	}

	// Find the last item
	i=tItems;
	tLastItem = NULL;
	for(;i;i=i->tNext) {
		if(i->tNext == NULL)
			tLastItem = i;
	}

	tSelected = NULL;

	// Adjust the scrollbar
	//cScrollbar.setMax( cScrollbar.getMax()-1 );

	/*// Do we use a scrollbar?
	if(cScrollbar.getMax()*20 >= iHeight)
		bGotScrollbar = true;
	else
		bGotScrollbar = false;*/

	UpdateItemIDs();

	// Readjust the scrollbar
	ReadjustScrollbar();

	bNeedsRepaint = true; // Repaint required
}


///////////////////
// Get the first sub item from the currently selected item
lv_subitem_t *CListview::getCurSub()
{
	if(tSelected)
		return tSelected->tSubitems;

	return NULL;
}

/////////////////////
// Gets the width of a specified column
int CListview::GetColumnWidth(int id)
{
	if (id > iNumColumns)
		return 5;

	// Go through the columns
	lv_column_t *col = tColumns;
	for (int i=0;i<id && col;col=col->tNext,i++)  {}

	if (col)
		return col->iWidth;
	else
		return 5;

}

///////////////
// Sorts the list by the current sorting column
// Useful, when you're re-filling the list or adding new items
void CListview::ReSort()
{
	lv_column_t *col = tColumns;

	// Find the column
	int i=0;
	for (;col;col=col->tNext,i++) {
		if (col->iSorted != -1)
			break;
	}

	// Not found
	if (!col)
		return;

	// Sort
	SortBy(i,col->iSorted==1);
}

///////////////
// Sorts the listview by specified column, ascending or descending
void CListview::SortBy(int column, bool ascending)
{
	// Check
	if (column < 0 || column >= iNumColumns)
		return;

	// Get the item
	lv_item_t *item = tItems;
	if (!item)
		return;

	lv_item_t *prev_item = NULL;
	lv_subitem_t *subitem1 = NULL;
	lv_subitem_t *subitem2 = NULL;

	bool bSwapped = true;

	// Bubble sort the list
	while (bSwapped)  {
		bSwapped = false;
		prev_item = NULL;
		for(item=tItems;item->tNext;item=item->tNext) {
			subitem1=item->tSubitems;
			subitem2=item->tNext->tSubitems;

			// Get subitem 1
			int i;
			for(i=0;i != column && subitem1;subitem1=subitem1->tNext,i++) {	}

			// Get subitem 2
			for(i=0;i != column && subitem2;subitem2=subitem2->tNext,i++) { }

			bool swap = false;
			if (subitem2 && subitem1)  {
				// Swap the two items?
				bool failed1,failed2;
				int nat_cmp1 = from_string<int>(subitem1->sText,failed1);
				int nat_cmp2 = from_string<int>(subitem2->sText,failed2);
				// First try, if we compare numbers
				if (!failed1 && !failed2)  {
					if (ascending)
						swap = nat_cmp1 > nat_cmp2;
					else
						swap = nat_cmp2 > nat_cmp1;
				// String comparison
				} else {
					int tmp = stringcasecmp(subitem1->sText,subitem2->sText);
					if (ascending)
						swap = tmp > 0;
					else
						swap = tmp < 0;
				}
			} else  {
				if (ascending)
					swap = subitem1 != NULL;
				else
					swap = subitem2 != NULL;
			}

			// Swap if they're not in the right order
			if (swap)  {
				lv_item_t *it3 = item->tNext;
				lv_item_t *it4 = item->tNext->tNext;
				if (prev_item)
					prev_item->tNext = it3;
				else
					tItems = it3;
				it3->tNext = item;
				item->tNext = it4;
				if (!it4)  {
					tLastItem = it3;
					break;
				}
				bSwapped = true;
				prev_item = item;
				continue;
			}

			prev_item = item;
		}
	}

	// Update the ID of the selected item
	int i=0;
	for (item=tItems;item;item=item->tNext,i++)
		if (item == tSelected)  {
			iItemID = i;
			break;
		}

	UpdateItemIDs();

	bNeedsRepaint = true; // Repaint required

}

///////////////
// Sorts the listview by specified column, ascending or descending, the sorting is stored in column
void CListview::SetSortColumn(int column, bool ascending)
{
	lv_column_t *col = tColumns;

	// Find the column
	int i=0;
	for (;col;col=col->tNext,i++)
		if( column == i )
			col->iSorted = ( ascending ? 1 : 0 );
		else
			col->iSorted = -1;

	ReSort();
}

/////////////////
// Get the sorted column index
int CListview::GetSortColumn()
{
	int i = 0;
	for (lv_column_t *col = tColumns; col; col=col->tNext,i++)
		if (col->iSorted != -1)
			return i;
	return -1;
}


///////////////////
// Clear the items
void CListview::Clear()
{
	// Free the items
	lv_item_t *i,*item;
	lv_subitem_t *s,*sub;
	for(i=tItems;i;i=item) {
		item = i->tNext;

		// Free the sub items
		for(s=i->tSubitems;s;s=sub) {
			sub = s->tNext;

			delete s;
		}

		delete i;
	}

	tItems = NULL;
	tLastItem = NULL;
	tSelected = NULL;
	tPreviousMouseSelection = NULL;
	tFocusedSubWidget = NULL;
	tMouseOverSubWidget = NULL;
	holdedWidget = NULL;
	tMouseOver = NULL;

	cScrollbar.setMin(0);
	cScrollbar.setMax(1);
	cScrollbar.setValue(0);
	iItemCount=0;
    iItemID = 0;
	bGotScrollbar=false;
	bNeedsRepaint = true; // Repaint required
}


///////////////////
// Create event
void CListview::Create()
{
	// Destroy any previous settings
	Destroy();
	iItemCount=0;
    iItemID = 0;
	bGotScrollbar=false;
    bShowSelect = true;

	cScrollbar.Create();
	cScrollbar.Setup(0, iX+iWidth-16, iY+17, 14, iHeight-18);
	cScrollbar.setItemsperbox( iHeight / 18 );
	cScrollbar.setMin(0);
	cScrollbar.setMax(1);
	cScrollbar.setValue(0);

	bNeedsRepaint = true; // Repaint required
}


///////////////////
// Destroy event
void CListview::Destroy()
{
	// Free the columns
	lv_column_t *c,*col;
	for(c=tColumns ; c ; c=col) {
		col = c->tNext;

		delete c;
	}
	tColumns = NULL;


	// Free the items
	lv_item_t *i,*item;
	lv_subitem_t *s,*sub;
	for(i=tItems;i;i=item) {
		item = i->tNext;

		// Free the sub items
		for(s=i->tSubitems;s;s=sub) {
			sub = s->tNext;

			delete s;
		}

		delete i;
	}

	tFocusedSubWidget = NULL;
	tMouseOverSubWidget = NULL;
	tItems = NULL;
}


////////////////////
// Updates _iID field of the items
void CListview::UpdateItemIDs()
{
	lv_item_t *it = tItems;
	if (!it)
		return;

	int i = 0;
	while (it)  {
		it->_iID = i;
		it = it->tNext;
		++i;
	}
}

///////////////////
// Get an index based on item count
int CListview::getIndex(int count)
{
	lv_item_t *item = tItems;
	int i;

	for(i=0; item; item=item->tNext, i++) {

		if(i==count)
			return item->iIndex;
	}

	return -1;
}




///////////////////
// Mouse over event
int	CListview::MouseOver(mouse_t *tMouse)
{
	if(tMouse->X > iX+iWidth-20 && tMouse->Y >= iY+20 && bGotScrollbar) {
		cScrollbar.MouseOver(tMouse);
		return LV_NONE;
	}

	// Reset the cursor
	SetGameCursor(CURSOR_ARROW);

	// Go through the columns and check, if the mouse is in the space between two columns
	if (!bOldStyle)  {
		if( tMouse->Y >= iY+2 && tMouse->Y <= iY+2+tLX->cFont.GetHeight()+1)  {
			lv_column_t *col = tColumns;
			if (col)  {
				int x = iX+col->iWidth-2;
				col = col->tNext;
				for (;col;col = col->tNext)  {
					if (tMouse->X >= x && tMouse->X <= x+4)  {
						SetGameCursor(CURSOR_RESIZE);
						return LV_NONE;
					}
					x += col->iWidth-2;
				}
			}
		}
	}

	// If the mouse is not down, make sure the focused subwidget can lose the focus
	if (tFocusedSubWidget && !tMouse->Down && !tFocusedSubWidget->CanLoseFocus())  {
		tFocusedSubWidget->setLoseFocus(true);
	}

	// Go through items and subitems, processing the widgets
	tMouseOverSubWidget = NULL; // Reset it here
	lv_item_t *item = tItems;
	lv_subitem_t *subitem = NULL;
	int scroll = (bGotScrollbar ? cScrollbar.getValue() : 0);
	int y = iY + 2 + (tColumns ? tLX->cFont.GetHeight() + 2 : 0);
	for(int i = 0;item;item = item->tNext, i++) {
		if (i < scroll)
			continue;
		subitem = item->tSubitems;
		int x = iX + 2;
		lv_column_t *col = tColumns;
		for (; subitem; subitem = subitem->tNext)  {
			if (subitem->iType == LVS_WIDGET)  {
				if (subitem->tWidget->getEnabled() && subitem->tWidget->InBox(tMouse->X, tMouse->Y))  {
					tLastWidgetEvent.cWidget = subitem->tWidget;
					tLastWidgetEvent.iControlID = subitem->tWidget->getID();
					tLastWidgetEvent.iEventMsg = subitem->tWidget->MouseOver(tMouse);
					tMouseOverSubWidget = subitem->tWidget;
				}
			} else if (col) {
				// Check if the mouse is over the subitem
				if (tMouse->X >= x && tMouse->X < col->iWidth + x)  {
					if (tMouse->Y >= y && tMouse->Y < item->iHeight + y)  {
						// Make sure we get called again when the time for showing the tooltip comes
						if (subitem->fMouseOverTime == TimeDiff())  {
							Timer t("CListview tooltip waiter", null, NULL, 600, true);
							t.startHeadless();
						}

						subitem->fMouseOverTime += tLX->fDeltaTime;

						// If the mouse has been longer time over the subitem, show the tooltip
						if (subitem->fMouseOverTime >= 0.6f)
							ShowTooltip(subitem->sTooltip, tMouse->X, tMouse->Y);
						else
							bTooltipVisible = false; // Hide the tooltip when the time is not yet up
					} else
						subitem->fMouseOverTime = 0;
				} else
					subitem->fMouseOverTime = 0;
				x += col->iWidth - 2;
				col = col->tNext;
			}
		}
		y += item->iHeight;
	}

	bNeedsRepaint = true; // Repaint required

	if (Menu_IsKeyboardNavigationUsed() && getFocused()) {
		MoveMouseToCurrentItem();
	}

	if( !bMouseOverEventEnabled )
		return LV_NONE;

	if(tMouse->X < iX || tMouse->X > iX+iWidth-18)
		return LV_NONE;

	// Go through the items
	y = iY+tLX->cFont.GetHeight()+2;
	if (!tColumns)
		y = iY+2;
	item = tItems;
	int count=0;

	for(;item;item = item->tNext) 
	{
		if(count++ < cScrollbar.getValue())
			continue;

		// Find the max height
		int h = item->iHeight;

		if(tMouse->Y >= y && tMouse->Y < y+h) 
		{
			tMouseOver = item;
			break;
		}

		y+=h;
		if(y > iY+iHeight)
			break;
	}

	return LV_MOUSEOVER;
}


///////////////////
// Mouse down event
int	CListview::MouseDown(mouse_t *tMouse, int nDown)
{
	if(holdedWidget) {
		tLastWidgetEvent.cWidget = holdedWidget;
		tLastWidgetEvent.iControlID = holdedWidget->getID();
		tLastWidgetEvent.iEventMsg = holdedWidget->MouseDown(tMouse, nDown);
		if(tLastWidgetEvent.iEventMsg >= 0) return LV_WIDGETEVENT;
		else return LV_NONE;
	}
	
	if(((tMouse->X > iX+iWidth-20) && bGotScrollbar) || cScrollbar.getGrabbed()) {
		holdedWidget = NULL;
		cScrollbar.MouseDown(tMouse, nDown);
		return LV_NONE;
	}

	if(tMouse->X < iX || tMouse->X > iX+iWidth-18)
		return LV_NONE;

	//
	// Column headers
	//
	if (!bOldStyle)  {
		lv_column_t *col = tColumns;
		lv_column_t *prev = NULL;
		int i=0;

		// First of all, reset the click state of all headers
		for (;col;col=col->tNext)
			col->bDown = false;

		col = tColumns;

		// Is any of the columns grabbed? Move it
		if (iGrabbed > 0)  {
			int x = iX+4;

			// Get the column
			for (i=0;i != iGrabbed && col;i++) {
				prev = col;
				x+=col->iWidth-2;
				col = col->tNext;
			}

			// Resize the two columns
			int w1,w2;
			w1=w2=0;
			if (prev)  {
				w1 = prev->iWidth + tMouse->X - iLastMouseX;
			}
			w2 = col->iWidth - tMouse->X + iLastMouseX;

			// Resize only if they both will have at least minimal width
			if (w1 > 4 && w2 > 4)  {
				prev->iWidth = w1;
				col->iWidth = w2;
			}


			iLastMouseX = tMouse->X;
			SetGameCursor(CURSOR_RESIZE);

			return LV_NONE;
		}

		// Not grabbed
		if( tMouse->Y >= iY+2 && tMouse->Y <= iY+2+tLX->cFont.GetHeight()+1)  {
			int x = iX+4;
			col = tColumns;
			prev = NULL;
			for (i=0;col;col = col->tNext,i++)  {
				col->bDown = false;
				// If in the area between two columns, grab
				if (tMouse->X >= x-8 && tMouse->X <= x && col != tColumns)  {
					iGrabbed = i;
					// Hack
					if (prev)
						prev->bDown = false;
				}
				// Click
				else if (tMouse->X >= x && tMouse->X <= x+col->iWidth-3 && iGrabbed <= 0)  {
					SetGameCursor(CURSOR_ARROW);
					col->bDown = true;
				}
				x += col->iWidth-2;
				prev = col;
			}
			iLastMouseX = tMouse->X;
			return LV_NONE;
		}
	}

    /*if( !(tMouse->FirstDown & SDL_BUTTON(1)) )
        return LV_NONE;*/


	iClickedSub = -1;

	// Go through the items
	int y = iY+tLX->cFont.GetHeight()+2;
	if (!tColumns)
		y = iY+2;
	lv_item_t *item = tItems;
	int count=0;

	// Remove focus from the active widget, the following loop will maybe recover it
	if (tFocusedSubWidget) {
		if (tFocusedSubWidget->CanLoseFocus())
			tFocusedSubWidget->setFocused(false);
		else  {
			tFocusedSubWidget->MouseDown(tMouse, nDown);
			return LV_NONE;  // The currently selected widget cannot lose the focus, no work left for us
		}
	}

	for(;item;item = item->tNext) {
		if(count++ < cScrollbar.getValue())
			continue;

		// Find the max height
		int h = item->iHeight;

		if(tMouse->Y >= y && tMouse->Y < y+h) {

			int event = LV_CHANGED;

			// Go through the sub items, check which one was clicked on
			lv_subitem_t *sub = item->tSubitems;
			lv_column_t *col = tColumns;
			int x = iX;
			int i=0;
			for(;sub;sub=sub->tNext,i++) {

				if(sub->bVisible) {
					if(col) {
						if(tMouse->X > x && tMouse->X < x+col->iWidth)
							iClickedSub = i;
					}
				}

				// Process any widget
				if (sub->tWidget && sub->iType == LVS_WIDGET)  {
					if (sub->tWidget->getEnabled() && sub->tWidget->InBox(tMouse->X, tMouse->Y))  {
						bool go_event = false; // True if we should process the event
						if (tFocusedSubWidget)
							go_event = tFocusedSubWidget->CanLoseFocus() || tFocusedSubWidget == sub->tWidget;
						else
							go_event = true;

						if (go_event)  {
							tLastWidgetEvent.cWidget = sub->tWidget;
							tLastWidgetEvent.iControlID = sub->tWidget->getID();
							tLastWidgetEvent.iEventMsg = sub->tWidget->MouseDown(tMouse, nDown);
							sub->tWidget->setFocused(true);
							tFocusedSubWidget = sub->tWidget;
							if (tLastWidgetEvent.iEventMsg != -1)
								event = LV_WIDGETEVENT;
							holdedWidget = tFocusedSubWidget;
						}
					}
				}

				if(col) {
					x += col->iWidth;
					col = col->tNext;
				}
			}

            if(tSelected) {
				tSelected->bSelected = false;

                // If we changed the selection, reset the mouseup var to avoid double clicks
                // when changing items
                if( tSelected->_iID != item->_iID )
                    fLastMouseUp = AbsTime();
            } else
                fLastMouseUp = AbsTime();


			tSelected = item;
			tSelected->bSelected = true;

			return event;
		}

		y+=h;
		if(y > iY+iHeight)
			break;
	}

	// If we get here, the focus wasn't restored and we clear the focused widget
	tFocusedSubWidget = NULL;

	bNeedsRepaint = true; // Repaint required

	return LV_NONE;
}


///////////////////
// Mouse up event
int	CListview::MouseUp(mouse_t *tMouse, int nDown)
{
	if(holdedWidget) {
		tLastWidgetEvent.cWidget = holdedWidget;
		tLastWidgetEvent.iControlID = holdedWidget->getID();
		tLastWidgetEvent.iEventMsg = holdedWidget->MouseUp(tMouse, nDown);
		holdedWidget = NULL;
		if(tLastWidgetEvent.iEventMsg >= 0) return LV_WIDGETEVENT;
		else return LV_NONE;
	}
	iLastMouseX = 0;

	if((tMouse->X > iX+iWidth-20 || cScrollbar.getGrabbed()) && bGotScrollbar) {
		cScrollbar.MouseUp(tMouse, nDown);
		return LV_NONE;
	}
	
	if(tMouse->X < iX || tMouse->X > iX+iWidth-18) {
		fLastMouseUp = AbsTime();
		return LV_NONE;
	}

	// Column headers
	if (!bOldStyle && tColumns)  {
		if( tMouse->Y >= iY+2 && tMouse->Y <= iY+2+tLX->cFont.GetHeight()+1 && tLX->currentTime-fLastMouseUp >= 0.15f && iGrabbed <= 0)  {
			fLastMouseUp = tLX->currentTime;
			int x = iX+4;
			lv_column_t *col = tColumns;
			for (int i=0;col;col = col->tNext,i++)  {
				col->bDown = false;
				if (tMouse->X >= x && tMouse->X <= x+col->iWidth-3)  {
					bool asc = col->iSorted == -1 || col->iSorted == 0;
					SortBy(i,asc);
					if (asc)
						col->iSorted = 1;
					else
						col->iSorted = 0;
				} else
					col->iSorted = -1;
				x += col->iWidth-2;
			}
			return LV_NONE;
		}

		iGrabbed = 0;
	}

	iClickedSub = -1;

	// Go through the items
	int y = iY+tLX->cFont.GetHeight()+2;
	if (!tColumns)
		y = iY+2;
	lv_item_t *item = tItems;
	int count=0;

	// Remove focus from the active widget, the following loop will maybe recover it
	if (tFocusedSubWidget)  {
		if (tFocusedSubWidget->CanLoseFocus())
			tFocusedSubWidget->setFocused(false);
		else  {  // If there is a focused subwidget that couldn't lose focus until now, send the event and allow to take off the focus
			tFocusedSubWidget->setLoseFocus(true);
			tFocusedSubWidget->MouseUp(tMouse, nDown);
			return LV_NONE;
		}
	}

	for(;item;item = item->tNext) {
		if(count++ < cScrollbar.getValue())
			continue;

		// Find the max height
		int h = item->iHeight;

		if(tMouse->Y > y && tMouse->Y < y+h) {

			int event = LV_CHANGED;

            // Right click?
            if( tMouse->Up & SDL_BUTTON(3) )
                event = LV_RIGHTCLK;

			if(tSelected && (tMouse->Up & SDL_BUTTON(1))) {
				if(tSelected->_iID == item->_iID) {
					//notes << "tLX->currentTime " << tLX->currentTime.seconds() << " fLastMouseUp " << fLastMouseUp.seconds() << " tLX->currentTime - fLastMouseUp " << (tLX->currentTime - fLastMouseUp).seconds() << endl;
					if(tLX->currentTime - fLastMouseUp < 1.5f) {
						event = LV_DOUBLECLK;
						fLastMouseUp = AbsTime();
					}
				}
			}

			// Go through the sub items, check which one was clicked on
			lv_subitem_t *sub = item->tSubitems;
			lv_column_t *col = tColumns;
			int x = iX;
			int i=0;
			for(;sub;sub=sub->tNext,i++) {

				if(sub->bVisible) {
					if(col) {
						if(tMouse->X > x && tMouse->X < x+col->iWidth)
							iClickedSub = i;
					}
				}

				// Process any widget
				if (sub->tWidget && sub->iType == LVS_WIDGET)  {
					if (sub->tWidget->getEnabled() && sub->tWidget->InBox(tMouse->X, tMouse->Y))  {
						bool go_event = false;
						if (tFocusedSubWidget)
							go_event = tFocusedSubWidget->CanLoseFocus() || tFocusedSubWidget == sub->tWidget;
						else
							go_event = true;

						if (go_event) {
							tLastWidgetEvent.cWidget = sub->tWidget;
							tLastWidgetEvent.iControlID = sub->tWidget->getID();
							tLastWidgetEvent.iEventMsg = sub->tWidget->MouseUp(tMouse, nDown);
							sub->tWidget->setFocused(true);
							tFocusedSubWidget = sub->tWidget;
							if (tLastWidgetEvent.iEventMsg != -1)
								event = LV_WIDGETEVENT;
						}
					}
				}

				if(col) {
					x += col->iWidth;
					col = col->tNext;
				}
			}



			if(tSelected)
				tSelected->bSelected = false;

			tSelected = item;
			tSelected->bSelected = true;

			if(event != LV_DOUBLECLK)
				fLastMouseUp = tLX->currentTime;

			if (tSelected == tPreviousMouseSelection && tSelected != NULL && event == LV_CHANGED) {
				event = LV_ENTER; // Click selected row for text lists to perform an action
			}
			tPreviousMouseSelection = tSelected;

			return event;
		}

		y+=h;
		if(y>=iY+iHeight)
			break;
	}

	// If we get here, the focus wasn't restored and we clear the focused widget
	tFocusedSubWidget = NULL;

	bNeedsRepaint = true; // Repaint required

	return LV_NONE;
}

///////////////////
// Mouse wheel down event
int	CListview::MouseWheelDown(mouse_t *tMouse)
{
	// Any sub-widget is active?
	if (tMouseOverSubWidget)  {
		tLastWidgetEvent.cWidget = tMouseOverSubWidget;
		tLastWidgetEvent.iControlID = tMouseOverSubWidget->getID();
		tLastWidgetEvent.iEventMsg = tMouseOverSubWidget->MouseWheelDown(tMouse);
		if (tLastWidgetEvent.iEventMsg != -1)
			return LV_WIDGETEVENT;
	}

	if(bGotScrollbar)  {
		cScrollbar.MouseWheelDown(tMouse);
		bNeedsRepaint = true; // Repaint required
	}

	return LV_NONE;
}

int	CListview::MouseClicked(mouse_t *tMouse, int nDown)
{
	// Any sub-widget is active?
	if (tMouseOverSubWidget)  {
		tLastWidgetEvent.cWidget = tMouseOverSubWidget;
		tLastWidgetEvent.iControlID = tMouseOverSubWidget->getID();
		tLastWidgetEvent.iEventMsg = tMouseOverSubWidget->MouseClicked(tMouse, nDown);
		if (tLastWidgetEvent.iEventMsg != -1)
			return LV_WIDGETEVENT;
	}
		
	return LV_NONE;
}
	
	
///////////////////
// Mouse wheel up event
int	CListview::MouseWheelUp(mouse_t *tMouse)
{
	// Any sub-widget is active?
	if (tMouseOverSubWidget)  {
		tLastWidgetEvent.cWidget = tMouseOverSubWidget;
		tLastWidgetEvent.iControlID = tMouseOverSubWidget->getID();
		tLastWidgetEvent.iEventMsg = tMouseOverSubWidget->MouseWheelUp(tMouse);
		if (tLastWidgetEvent.iEventMsg != -1)
			return LV_WIDGETEVENT;
	}

	if(bGotScrollbar)  {
		cScrollbar.MouseWheelUp(tMouse);
		bNeedsRepaint = true; // Repaint required
	}

	return LV_NONE;
}

void CListview::MoveMouseToCurrentItem()
{
	if (!tSelected)
		return;

	// Go through the items
	int y = iY + tLX->cFont.GetHeight() + 2;
	if (!tColumns)
		y = iY + 2;
	lv_item_t *item = tItems;

	for (int count = 0; item; item = item->tNext) {
		if (count++ < cScrollbar.getValue())
			continue;

		y += item->iHeight;
		if(y >= iY + iHeight)
			return;

		if (item == tSelected) {
			Menu_WarpMouse(iX + 3, y - 1);
			return;
		}
	}
}

/////////////////
// Key down event
int CListview::KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	// Any sub-widget is active?
	if (tFocusedSubWidget)  {
		tLastWidgetEvent.cWidget = tFocusedSubWidget;
		tLastWidgetEvent.iControlID = tFocusedSubWidget->getID();
		tLastWidgetEvent.iEventMsg = tFocusedSubWidget->KeyDown(c, keysym, modstate);
		if (tLastWidgetEvent.iEventMsg != -1)
			return LV_WIDGETEVENT;
	}

	if (tSelected) {
		// Go through every witget in a current list row, return if one of them widget returns non-empty event
		for(lv_subitem_t *sub = tSelected->tSubitems; sub; sub=sub->tNext) {
			if (sub->bVisible && sub->tWidget && sub->iType == LVS_WIDGET && sub->tWidget->getEnabled()) {
				int eventMsg = sub->tWidget->KeyDown(c, keysym, modstate);
				if (eventMsg >= 0) {
					tLastWidgetEvent.cWidget = sub->tWidget;
					tLastWidgetEvent.iControlID = sub->tWidget->getID();
					tLastWidgetEvent.iEventMsg = eventMsg;
					sub->tWidget->setFocused(true);
					return LV_WIDGETEVENT;
				}
			}
		}
	}

	// TODO: why is this here?
//	if (c == iLastChar && c)
//		return LV_NONE;

	if (c >= 31)  {
		// TODO: handle normal characters
		// share some code with dropdownbox here
		//warnings << "Normal keys are currently ignored for listview" << endl;
		//return LV_NONE; // don't return here, else we would ignore SDLK_DOWN etc.
	}

	iLastChar = c;

	int moveRows = 0;
	if (keysym == SDLK_DOWN) {
		moveRows++;
	}
	if (keysym == SDLK_UP)  {
		moveRows--;
	}
	if (keysym == SDLK_PAGEDOWN) {
		moveRows += 10;
	}
	if (keysym == SDLK_PAGEUP)  {
		moveRows -= 10;
	}

	if (moveRows != 0) {
		bool processed = 0;

		// Down arrow
		for (; moveRows > 0; moveRows--) {
			if (tSelected) {
				if (tSelected->tNext)  {
					tSelected->bSelected = false;
					tSelected = tSelected->tNext;
					tSelected->bSelected = true;
					iLastChar = SDLK_DOWN;
					if (bGotScrollbar)
						if (tSelected->_iID >= (cScrollbar.getItemsperbox()-1 + cScrollbar.getValue()))
							cScrollbar.setValue( cScrollbar.getValue()+1 );
					processed = true;
				}
			} else {
				tSelected = tItems;
				if(tSelected) {
					tSelected->bSelected = true;
					if (bGotScrollbar)
						cScrollbar.setValue(0);
					processed = true;
				}
			}
		}

		// Up arrow
		for (; moveRows < 0; moveRows++) {
			lv_item_t *i = tItems;
			if (tItems)  {
				int idx = 0;
				for ( ; i && i->tNext; i=i->tNext, idx++ )  {
					if (i->tNext == tSelected) {
						if (tSelected)
							tSelected->bSelected = false;
						tSelected = i;
						tSelected->bSelected = true;
						iLastChar = SDLK_UP;
						if (bGotScrollbar)
							if (cScrollbar.getValue() > idx)
									cScrollbar.setValue( cScrollbar.getValue()-1 );
						PlaySoundSample(sfxGeneral.smpClick);
						processed = true;
					}
				}
			}
		}

		if (processed) {
			MoveMouseToCurrentItem();
			PlaySoundSample(sfxGeneral.smpClick);
			return LV_CHANGED;
		}
	}

	// Home
	if (keysym == SDLK_HOME)  {
		if (tItems)  {
			if (tSelected)
				tSelected->bSelected = false;
			tSelected = tItems;
			tSelected->bSelected = true;
			if (bGotScrollbar)
				cScrollbar.setValue(0);
			PlaySoundSample(sfxGeneral.smpClick);
			return LV_CHANGED;
		}
	}

	// End
	if (keysym == SDLK_END)  {
		if (tLastItem)  {
			if (tSelected)
				tSelected->bSelected = false;
			tSelected = tLastItem;
			tSelected->bSelected = true;
			if (bGotScrollbar)
				cScrollbar.setValue(tSelected->_iID);
			PlaySoundSample(sfxGeneral.smpClick);
			return LV_CHANGED;
		}
	}

	// Delete
	if (keysym == SDLK_DELETE)  {
		iLastChar = SDLK_DELETE;
		PlaySoundSample(sfxGeneral.smpClick);
		return LV_DELETE;
	}

	// Enter
	if (keysym == SDLK_RETURN ||
		keysym == SDLK_KP_ENTER ||
		keysym == SDLK_LALT ||
		keysym == SDLK_LCTRL ||
		keysym == SDLK_LSHIFT ||
		keysym == SDLK_x ||
		keysym == SDLK_z) {
		iLastChar = SDLK_RETURN;
		PlaySoundSample(sfxGeneral.smpClick);
		return LV_ENTER;
	}

	bNeedsRepaint = true; // Repaint required

	return LV_NONE;
}

////////////////
// Key up event
int CListview::KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	// Any sub-widget is active?
	if (tFocusedSubWidget)  {
		tLastWidgetEvent.cWidget = tFocusedSubWidget;
		tLastWidgetEvent.iControlID = tFocusedSubWidget->getID();
		tLastWidgetEvent.iEventMsg = tFocusedSubWidget->KeyUp(c, keysym, modstate);
		if (tLastWidgetEvent.iEventMsg != -1)
			return LV_WIDGETEVENT;
	}

	if (tSelected) {
		// Go through every witget in a current list row, return if one of them widget returns non-empty event
		for(lv_subitem_t *sub = tSelected->tSubitems; sub; sub=sub->tNext) {
			if (sub->bVisible && sub->tWidget && sub->iType == LVS_WIDGET && sub->tWidget->getEnabled()) {
				int eventMsg = sub->tWidget->KeyUp(c, keysym, modstate);
				if (eventMsg >= 0) {
					tLastWidgetEvent.cWidget = sub->tWidget;
					tLastWidgetEvent.iControlID = sub->tWidget->getID();
					tLastWidgetEvent.iEventMsg = eventMsg;
					sub->tWidget->setFocused(true);
					return LV_WIDGETEVENT;
				}
			}
		}
	}

	return LV_NONE;
}

///////////////////
// Get the ID of the currently selected item
int CListview::getSelectedID()
{
//	if (!this) // TODO: dirty hack to fix use-after-delete
//		return -1;
    if(tSelected)
        return tSelected->_iID;
    return -1;
}


///////////////////
// Set the cur item to the item with the matching ID
void CListview::setSelectedID(int id)
{
    lv_item_t *item = tItems;
	for(;item;item = item->tNext) {

        if(item->_iID == id) {
            if(tSelected)
                tSelected->bSelected = false;
            item->bSelected = true;
            tSelected = item;

			// Scroll to the item if needed
			if (bGotScrollbar)  {
				cScrollbar.setValue(tSelected->_iID);
			}

            return;
        }
    }
}


///////////////////
// Scroll to the last item
void CListview::scrollLast()
{
	if (bGotScrollbar)  {
		cScrollbar.setValue(cScrollbar.getMax());
		cScrollbar.UpdatePos();
		bNeedsRepaint = true; // Repaint required
	}
}

////////////////////
// Get the specified subitem of the selected item
lv_subitem_t *CListview::getCurSubitem(int index)
{
	if (!tSelected)
		return NULL;
	lv_subitem_t *sub = tSelected->tSubitems;
	for (int i=0;i<index && sub;i++,sub=sub->tNext) {}
	return sub;
}


///////////////////
// This widget is send a message
uintptr_t CListview::SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2)
{
	std::string s = "";

	switch(iMsg) {

		// Remove an item
		case LVM_REMOVEITEM:
			RemoveItem(Param1);
			break;

		// Get the current item's index
		case LVM_GETCURINDEX:
			return getCurIndex();

		// Get the item count
		case LVM_GETITEMCOUNT:
			return getItemCount();

		// Clear the items
		case LVM_CLEAR:
			Clear();
			break;

		// Get the index of an item based on count
		case LVM_GETITEMINDEX:
			return getIndex(Param1);

		// Get the index of an item based on index
		//case LVM_GETINDEX:

        // Return the current item
        case LVM_GETCURITEM:
            if(tSelected)
                return (uintptr_t)tSelected;
            return 0;

		// Set the old-style property
		case LVM_SETOLDSTYLE:
			bOldStyle = true;
			bNeedsRepaint = true; // Repaint required
			break;

		// Get the column width
		case LVM_GETCOLUMNWIDTH:
			return GetColumnWidth(Param1);
			break;

		default:
			errors << "Bad listview message " << iMsg << endl;
	}

	return 0;
}

uintptr_t CListview::SendMessage(int iMsg, const std::string& sStr, uintptr_t Param)
{
	switch (iMsg)  {

	// Add a column
	case LVS_ADDCOLUMN:
		AddColumn(sStr,Param);
		break;

	// Add an item
	case LVS_ADDITEM:
		AddItem(sStr,Param,tLX->clListView);
		break;

	default:
		errors << "Bad listview message" << iMsg << endl;
	}

	return 0;
}


uintptr_t CListview::SendMessage(int iMsg, std::string *sStr, uintptr_t Param)
{
	switch (iMsg)  {

	// Get the current item's text index
	case LVS_GETCURSINDEX:
		*sStr = getCurSIndex();
		return *sStr != "";
		break;
	}

	return 0;
}

///////////////////
// Get an item based on the index
lv_item_t* CListview::getItem(int index) {
	for(lv_item_t* i = tItems; i; i = i->tNext) {
		if(i->iIndex == index)
			return i;
	}
	return NULL;
}

/////////////////
// Get an item based on the name
lv_item_t* CListview::getItem(const std::string& name) {
	for(lv_item_t* i = tItems; i; i = i->tNext) {
		if(stringcasecmp(i->sIndex,name) == 0)
			return i;
	}
	return NULL;
}

////////////////
// Custom scrollbar setup
void CListview::SetupScrollbar(int x, int y, int h, bool always_visible)
{
	int full_auto_setup = 0;

	if (h <= 0) { // Height should be set automatically
		full_auto_setup++;
		h = iHeight;
		if (tColumns)  {
			h -= tLX->cFont.GetHeight();
			if (!bOldStyle)
				h -= 4;
		}

		if (bDrawBorder)
			h -= 4;

		h = MAX(h, 1); // Safety
	}

	if (y < 0)  { // Y position should be set automatically
		full_auto_setup++;
		y = iY;
		if (tColumns)  {
			iY += tLX->cFont.GetHeight();
			if (!bOldStyle)
				h += 4;
		}

		if (bDrawBorder)
			y += 2;
	}

	if (x < 0)  { // X position should be set automatically
		full_auto_setup++;
		x = iX + iWidth - 14;
	}


	// Setup the scrollbar
	cScrollbar.Setup(0, x, y, 14, h);
	bAlwaysVisibleScrollbar = always_visible;
	bCustomScrollbarSetup = (full_auto_setup != 3); // If full auto setup is 3, all the three params should
													// be set up automatically
}

/////////////////
// Get a specified subitem
lv_subitem_t *CListview::getSubItem(int item_index, int subitem_index)
{
	return getSubItem( getItem(item_index), subitem_index );
}

/////////////////
// Get a specified subitem
lv_subitem_t *CListview::getSubItem(lv_item_t *it, int subitem_index)
{
	if (!it)
		return NULL;

	lv_subitem_t *sub = it->tSubitems;
	for (int i=0; sub && i < subitem_index; sub = sub->tNext, ++i) {}

	return sub;
}

}; // namespace DeprecatedGUI
