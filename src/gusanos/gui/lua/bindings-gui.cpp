#include "bindings-gui.h"

#include "../../luaapi/types.h"
#include "../../luaapi/macros.h"
#include "../../luaapi/classes.h"

#ifndef DEDICATED_ONLY
#include "../omfggui.h"
#include "../omfggui_windows.h"
#endif
#include "util/macros.h"

#include <cmath>
#include <string>
#include <list>
#include <iostream>
#include <vector>
#include "gusanos/allegro.h"
using std::cerr;
using std::endl;
#include <boost/lexical_cast.hpp>
#include <boost/bind/bind.hpp>
using boost::lexical_cast;

/*
Note:

All of the GUI / Menu widget stuff of Gusanos is here only
for compatibility of old Gusanos mods.
None of the GUI widgets will get drawn or can be interacted with
at any point.
However, we completely emulate them virtually as if they were
there. It's almost impossible from Lua to see that difference.

The XML loading must also be there so that everything works
as expected.
The GSS loading was removed as it's mostly for style sheets and
Lua will most likely never depend on any of that.
*/

namespace LuaBindings
{
	
LuaReference listIterator;

#ifndef DEDICATED_ONLY

template<class T>
int l_gui_wnd(lua_State* L)
{
	LuaContext context(L);
	std::map<std::string, std::string> attribs;
	
	if(lua_istable(context, 1))
	{
		lua_pushnil(context);
		while(lua_next(context, 1) != 0)
		{
			if(lua_isstring(context, -2))
			{
				char const* k = lua_tostring(context, -2);
				if(char const* v = lua_tostring(context, -1))
				{
					attribs[k] = v;
				}
			}
			context.pop(1);
		}
	}
	
	OmfgGUI::Wnd* n = lua_new_keep(T, (0, attribs), luaIngame);
	
	if(lua_istable(context, 2))
	{
		// Children
		for(int i = 1; ; ++i)
		{
			lua_rawgeti(context, 2, i);
			if(lua_isnil(context, -1))
			{
				context.pop(1);
				break;
			}
			OmfgGUI::Wnd* w = ASSERT_LOBJECT(OmfgGUI::Wnd, -1);
			n->addChild(w);
			context.pop(1);
		}
	}
	
	return 1;
}


/*! gui_load_xml(name)

	Loads an XML GUI hierarchy by name.
	
	**Note that the .xml extension is not included in the name**
*/
int l_gui_loadxml(lua_State* L)
{	
	// Note: Gusanos menu not used.
	// However, load and return something, because it would confuse+break old Gusanos mods like Promode otherwise.

	LuaContext context(L);
	
	OmfgGUI::Context& gui = *static_cast<OmfgGUI::Context *>(lua_touserdata(context, lua_upvalueindex(1)));

	int params = lua_gettop(context);
	                                                     
	if(params > 0)
	{
		char const* name = lua_tostring(context, 1);
		
		if(!name)
			return 1;
		
		OmfgGUI::Wnd* loadTo = gui.getRoot();
		
		if(params > 1)
		{
			//loadTo = static_cast<OmfgGUI::Wnd *>(lua_touserdata(context, 2));
			loadTo = ASSERT_LOBJECT(OmfgGUI::Wnd, 2); //(lua_touserdata(context, 2));
		}
		assert(loadTo);
				
		OmfgGUI::Wnd* w = gui.loadXMLFile(name, loadTo);
		if(w)
		{
			context.push(w->luaReference);
			return 1;
		}
	}
	
	return 0;
}

/*! gui_load_gss(name[, passive = false])

	Loads a GSS GUI sheet by name.
	If the parameter passive is supplied and is set to true, the styling of existing GUI
	elements won't be updated.
	
	**Note that the .gss extension is not included in the name**
*/

int l_gui_loadgss(lua_State* L)
{
	// we ignore that for now
	// Gusanos menu not used.
	return 0;
}

/*! gui_find(name)

	Finds a named GUI element and returns it as
	a Wnd object.
*/

int l_gui_find(lua_State* L)
{
	OmfgGUI::Context& gui = *static_cast<OmfgGUI::Context *>(lua_touserdata(L, lua_upvalueindex(1)));
	
	LuaContext context(L);
	
	char const* s = lua_tostring(context, 1);
	OmfgGUI::Wnd* w = gui.findNamedWindow(s);
	if(!w)
		return 0;

	context.push(w->luaReference);

	return 1;
}

int l_gui_root(lua_State* L)
{
	OmfgGUI::Context& gui = *static_cast<OmfgGUI::Context *>(lua_touserdata(L, lua_upvalueindex(1)));
	
	LuaContext context(L);
	
	OmfgGUI::Wnd* w = gui.getRoot();
	if(!w)
		return 0;
	context.push(w->luaReference);
	
	return 1;
}

int l_gui_windows_index(lua_State* L)
{
	OmfgGUI::Context& gui = *static_cast<OmfgGUI::Context *>(lua_touserdata(L, lua_upvalueindex(1)));
	
	LuaContext context(L);
	
	char const* s = lua_tostring(context, 2);
	if(!s)
		return 0;
	OmfgGUI::Wnd* w = gui.findNamedWindow(s);
	if(!w)
		return 0;

	context.push(w->luaReference);

	return 1;
}

/*! Wnd:add(windows)

	Adds child windows to this window.
	//windows// can be either a table of Wnd objects or a single Wnd object.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_add,  {
	int top = lua_gettop(context);
	for(int s = 2; s <= top; ++s)
	{
		if(OmfgGUI::Wnd* w = getLObject<OmfgGUI::Wnd>(context, s))
		{
			p->addChild(w);
		}
		else if(lua_istable(context, s))
		{
			for(int i = 1; ; ++i)
			{
				lua_rawgeti(context, s, i);
				if(lua_isnil(context, -1))
				{
					context.pop(1);
					break;
				}
				OmfgGUI::Wnd* w = ASSERT_LOBJECT(OmfgGUI::Wnd, -1);
				p->addChild(w);
				context.pop(1);
			}
		}
		else
		{
			// TODO: Error
		}
	}
	
	return 0;
})

/*! Wnd:attribute(name)

	Returns the value of the attribute //name// of the window.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_attribute,  {
	char const* name = lua_tostring(context, 2);
	std::string res;
	if(p->getAttrib(name, res))
		lua_pushstring(context, res.c_str());
	else
		return 0;
	
	return 1;
})

/*! Wnd:set_visibility(value)

	If //value// is false, hides the window.
	Otherwise makes it visible.
*/

LMETHODC(OmfgGUI::Wnd, gui_wnd_set_visibility,  {
	p->setVisibility(lua_toboolean(context, 2) != 0);
	return 0;
})

/*! Wnd:switch_to()

	Hides all windows, except this one, in the parent window.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_switch_to,  {
	context.push(p->switchTo());
	return 1;
})

/*! Wnd:is_visible()

	Returns true if the window is visible.
	Otherwise false.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_is_visible,  {
	lua_pushboolean(context, p->isVisibile());
	return 1;
})

LMETHODC(OmfgGUI::Wnd, gui_wnd_is_active,  {
	lua_pushboolean(context, p->isActive());
	return 1;
})

LMETHODC(OmfgGUI::Wnd, gui_wnd_deactivate,  {
	p->doSetActivation(false);
	return 0;
})

/*! Wnd:text()

	(Known as get_text before 0.9c)
	
	Returns the text of the window.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_get_text,  {
	if(p)
		lua_pushstring(context, p->getText().c_str());
	else
		return 0;
	
	return 1;
})

#ifndef NO_DEPRECATED
int l_gui_wnd_get_text_depr(lua_State* L)
{
	LuaContext context(L);
	LUA_WLOG_ONCE("get_text is deprecated, use the text method instead");
	return l_gui_wnd_get_text(L);
}
#endif

/*! Wnd:set_text(text)

	Sets the text of the window to //text//.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_set_text,  {
	char const* s = lua_tostring(context, 2);
	if(s)
		p->setText(s);
	
	return 0;
})

/*! Wnd:focus()

	Focuses the window.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_focus,  {
	p->focus();
	return 0;
})

/*! Wnd:set_sub_focus()

	Sets a window that will be focused when
	this window is focused.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_set_sub_focus,  {
	OmfgGUI::Wnd* sub = ASSERT_LOBJECT(OmfgGUI::Wnd, 2); //(lua_touserdata(context, 2));
	
	// Make sure that 'sub' is a child of 'p'
	OmfgGUI::Wnd* parent = sub->getParent();
	while(parent && parent != p)
		parent = parent->getParent();
	
	if(!parent)
		return 0; // 'sub' isn't a child
	
	parent = sub->getParent();
	while(parent)
	{
		parent->setSubFocus(sub);
		if(parent == p)
			return 0;
		sub = parent;
		parent = parent->getParent();
	}

	return 0;
})

/*! Wnd:activate()

	Makes the window recieve input from now on until it's deactivated.
*/
LMETHODC(OmfgGUI::Wnd, gui_wnd_activate,  {
	p->doSetActivation(true);
	return 0;
})

LMETHODC(OmfgGUI::Wnd, gui_wnd_bind,  {
	char const* cb = lua_tostring(context, 2);
	if(!cb) return 0;
	lua_pushvalue(context, 3);
	LuaReference ref;
	ref.create(context);
	context.push(p->registerCallback(cb, ref));
	return 1;
})

LMETHODC(OmfgGUI::Wnd, gui_wnd_child,  {
	char const* name = lua_tostring(context, 2);
	if(!name) return 0;
	OmfgGUI::Wnd* ch = p->getChildByName(name);
	if(!ch) return 0;
	context.push(ch->luaReference);
	return 1;
})

//! Check inherits Wnd

LMETHODC(OmfgGUI::Check, gui_check_state,  {
	context.push(p->getState());
	return 1;
})

LMETHODC(OmfgGUI::Check, gui_check_set_state,  {
	if((lua_toboolean(context, 2) != 0) != p->getState())
		p->toggleState();
	return 0;
})

//! Edit inherits Wnd

LMETHODC(OmfgGUI::Edit, gui_edit_set_lock,  {
	p->setLock(lua_toboolean(context, 2) != 0);
	return 0;
})

//! List inherits Wnd

/*! List:insert(columns...)

	Inserts an item into the list with the column values passed.
*/
LMETHODC(OmfgGUI::List, gui_list_insert,  {

	int c = lua_gettop(context);
	OmfgGUI::ListNode* n = lua_new_keep(OmfgGUI::ListNode, (""), context);
	p->push_back(n);
	for(int i = 2; i <= c; ++i)
		n->setText(i - 2, lua_tostring(context, i));
	
	return 1;
})

LMETHODC(OmfgGUI::List, gui_list_each,  {

	context.push(listIterator);
	context.push(p->getFirstNode()->luaReference);
	lua_pushnil(context);

	return 3;
})

int l_gui_listIterator(lua_State* L)
{
	LuaContext context(L);
	
	if(lua_isnil(context, 2))
		lua_pushvalue(context, 1);
	else
	{
		OmfgGUI::ListNode* i = ASSERT_LOBJECT(OmfgGUI::ListNode, 2);
	
		i = OmfgGUI::ListNode::getNextVisible(i);
		
		if(!i)
			return 0;
	
		context.push(i->luaReference);
	}
	
	return 1;
}

/*! List:subinsert(node, columns...)

	Inserts an item into the list, under another node, with the column values passed.
*/
LMETHODC(OmfgGUI::List, gui_list_subinsert,  {

	OmfgGUI::ListNode* parent = ASSERT_LOBJECT(OmfgGUI::ListNode, 2);
			
	int c = lua_gettop(context);
	OmfgGUI::ListNode* n = lua_new_keep(OmfgGUI::ListNode, (""), context);
	p->push_back(n, parent);
	for(int i = 3; i <= c; ++i)
		n->setText(i - 3, lua_tostring(context, i));
	
	return 1;
})

/*! List:clear()

	Removes all items in the list.
*/
LMETHODC(OmfgGUI::List, gui_list_clear,  {
	p->clear();
	return 0;
})

/*! List:sort(criteria)

	Sorts the list by the column passed or by a comparer function.
	
	If //criteria// is an integer, the list is sorted numerically by the column with that zero-based index.
	
	If //criteria// is a function, it's used as a comparer function that gets passed
	two parameters. It should return true if parameter 1 is supposed to appear before
	parameter 2.
*/
LMETHODC(OmfgGUI::List, gui_list_sort,  {
	if(lua_isnumber(context, 2))
	{
		unsigned int column = static_cast<unsigned int>(lua_tointeger(context, 2));
		
		p->sortNumerically(column);
	}
	else if(lua_isfunction(context, 2))
	{
		context.pushvalue(2);
		LuaReference ref;
		ref.create(context);
		p->sortLua(ref);
		ref.destroy();
	}

	return 0;
})

LMETHODC(OmfgGUI::List, gui_list_selection,  {
	// Nothing can be selected in our dummy virtual list.
	return 0;
})

LMETHODC(OmfgGUI::List, gui_list_main_selection,  {
	// Note: There is no main selection, but just return the first node.
	if(OmfgGUI::ListNode* n = p->getFirstNode())
	{
		context.push(n->luaReference);
		return 1;
	}
	return 0;
})

LMETHODC(OmfgGUI::List, gui_list_scroll_bottom,  {
	// Ignore.
	return 0;
})

/*! List:add_column(title, width)

	Adds a column to a list with the title //title//.
	//width// is a value in (0, 1) that specifies the width of this column proportional to the total width of the list.
*/
LMETHODC(OmfgGUI::List, gui_list_add_column,  {
	char const* name = lua_tostring(context, 2);
	lua_Number widthFactor = lua_tonumber(context, 3);
	
	p->addColumn(OmfgGUI::List::ColumnHeader(name, widthFactor));

	return 0;
})

/*! ListNode:is_selected()

	Returns true if the node is selected, otherwise false.
*/
LMETHODC(OmfgGUI::ListNode, gui_list_node_is_selected,  {
	context.push(p->selected);
	return 1;
})

/*! ListNode:data()

	Returns a table associated with this node.
*/
LMETHODC(OmfgGUI::ListNode, gui_list_node_data,  {
	if(p->luaData.isSet(context))
	{
		context.push(p->luaData);
	}
	else
	{
		lua_newtable(context);
		lua_pushvalue(context, -1);
		p->luaData.create(context);
	}
	
	return 1;
})

#endif


#ifndef DEDICATED_ONLY
void addGUIWndFunctions(LuaContext& context)
{
	context.tableFunctions()
		("attribute", l_gui_wnd_attribute)
		("set_visibility", l_gui_wnd_set_visibility)
		("is_visible", l_gui_wnd_is_visible)
		("is_active", l_gui_wnd_is_active)
#ifndef NO_DEPRECATED
		("get_text", l_gui_wnd_get_text_depr)
#endif
		("text", l_gui_wnd_get_text)
		("set_text", l_gui_wnd_set_text)
		("focus", l_gui_wnd_focus)
		("set_sub_focus", l_gui_wnd_set_sub_focus)
		("activate", l_gui_wnd_activate)
		("deactivate", l_gui_wnd_deactivate)
		("child", l_gui_wnd_child)
		("switch_to", l_gui_wnd_switch_to)
		("add", l_gui_wnd_add)
		//("bind", l_gui_wnd_bind)
	;
}

void addGUIListFunctions(LuaContext& context)
{
	context.tableFunctions()
		("insert", l_gui_list_insert)
		("subinsert", l_gui_list_subinsert)
		("clear", l_gui_list_clear)
		("add_column", l_gui_list_add_column)
		("sort", l_gui_list_sort)
		("selection", l_gui_list_selection)
		("main_selection", l_gui_list_main_selection)
		("each", l_gui_list_each)
		("scroll_bottom", l_gui_list_scroll_bottom)
	;
}

void addGUIEditFunctions(LuaContext& context)
{
	context.tableFunctions()
		("set_lock", l_gui_edit_set_lock)
	;
}

void addGUICheckFunctions(LuaContext& context)
{
	context.tableFunctions()
		("state", l_gui_check_state)
		("set_state", l_gui_check_set_state)
	;
}

void GUIWndMetatable(LuaContext& context)
{
	lua_newtable(context);
	lua_pushcfunction(context, l_gui_wnd_bind);
	lua_setfield(context, -2, "__newindex");                          
}

#endif

void initGUI(OmfgGUI::Context& gui, LuaContext& context)
{
	// Gusanos menu not used.
	// However, register dummy commands so that old Gusanos mods keep working.
	
#ifndef DEDICATED_ONLY

	// GUI Wnd method and metatable

	lua_pushlightuserdata(context, static_cast<void *>(&gui));
	context.function("gui_load_gss", l_gui_loadgss, 1);
	lua_pushlightuserdata(context, static_cast<void *>(&gui));
	context.function("gui_load_xml", l_gui_loadxml, 1);
	lua_pushlightuserdata(context, static_cast<void *>(&gui));
	context.function("gui_find", l_gui_find, 1);
	lua_pushlightuserdata(context, static_cast<void *>(&gui));
	context.function("gui_root", l_gui_root, 1);
	context.functions()
		("gui_button", l_gui_wnd<OmfgGUI::Button>)
		("gui_edit", l_gui_wnd<OmfgGUI::Edit>)
		("gui_list", l_gui_wnd<OmfgGUI::List>)
		("gui_label", l_gui_wnd<OmfgGUI::Label>)
		("gui_group", l_gui_wnd<OmfgGUI::Group>)
		("gui_check", l_gui_wnd<OmfgGUI::Check>)
		("gui_window", l_gui_wnd<OmfgGUI::Wnd>)
	;
	

	GUIWndMetatable(context);
	lua_pushstring(context, "__index");

	lua_newtable(context);
	
	addGUIWndFunctions(context);

	lua_rawset(context, -3);
	context.tableSetField(LuaID<OmfgGUI::Wnd>::value);
	//LuaReference ref = context.createReference();
	context.pushvalue(-1);
	context.pushvalue(-1);
	context.pushvalue(-1);
	OmfgGUI::Wnd::metaTable.create(context);
	OmfgGUI::Button::metaTable.create(context);
	OmfgGUI::Group::metaTable.create(context);
	OmfgGUI::Label::metaTable.create(context);
	
	// GUI Check method and metatable
	
	GUIWndMetatable(context);
	lua_pushstring(context, "__index");
	
	lua_newtable(context);
	
	addGUIWndFunctions(context);
	addGUICheckFunctions(context);
	
	lua_rawset(context, -3);
	context.tableSetField(LuaID<OmfgGUI::Wnd>::value);
	context.tableSetField(LuaID<OmfgGUI::Check>::value);
	OmfgGUI::Check::metaTable.create(context);
	
	// GUI Edit method and metatable
	
	GUIWndMetatable(context);
	lua_pushstring(context, "__index");
	
	lua_newtable(context);
	
	addGUIWndFunctions(context);
	addGUIEditFunctions(context);
	
	lua_rawset(context, -3);
	context.tableSetField(LuaID<OmfgGUI::Wnd>::value);
	context.tableSetField(LuaID<OmfgGUI::Edit>::value);
	OmfgGUI::Edit::metaTable.create(context);
	
	// GUI List method and metatable
	
	GUIWndMetatable(context);
	lua_pushstring(context, "__index");
	
	lua_newtable(context);
	
	addGUIWndFunctions(context);
	addGUIListFunctions(context);

	lua_rawset(context, -3);
	context.tableSetField(LuaID<OmfgGUI::Wnd>::value);
	context.tableSetField(LuaID<OmfgGUI::List>::value);
	OmfgGUI::List::metaTable.create(context);
	
	// GUI List node method and metatable
	
	lua_newtable(context);
	lua_pushstring(context, "__index");
	
	lua_newtable(context);
	
	context.tableFunctions()
		("is_selected", l_gui_list_node_is_selected)
		("data", l_gui_list_node_data)
	;
	
	lua_rawset(context, -3);
	context.tableSetField(LuaID<OmfgGUI::ListNode>::value);
	OmfgGUI::ListNode::metaTable.create(context);
	
	context.push(l_gui_listIterator);
	//context.regObject("gui_listIterator");
	listIterator.create(context);
#endif
}

}
