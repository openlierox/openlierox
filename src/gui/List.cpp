/*
 *  List.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 23.02.10.
 *  code under LGPL
 *
 */

#include <boost/bind/bind.hpp>
#include "gui/List.h"
#include "Debug.h"

void GuiListItem::setImage(const SmartPointer<DynDrawIntf>&) {
	warnings << "GuiListItem::setImage not implemented" << endl;
}

typedef boost::function< Iterator<GuiListItem::Pt>::Ref() > DynamicListFct;
static GuiList::Pt dynamicGuiList(DynamicListFct f) {
	struct DynamicList : GuiList {
		DynamicListFct f;
		DynamicList(DynamicListFct _f) : f(_f) {}
		virtual Iterator<GuiListItem::Pt>::Ref iterator() { return f(); }
	};
	return new DynamicList(f);
}

static Iterator<GuiListItem::Pt>::Ref iteratorForGuiList(const GuiItemList& l) {
	return GetIterator(l);
}

GuiList::Pt dynamicGuiList(boost::function< GuiItemList() > f) {
	return dynamicGuiList((DynamicListFct) boost::bind(&iteratorForGuiList, boost::bind(f)));
}
