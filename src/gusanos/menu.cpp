#ifndef DEDICATED_ONLY

#include "menu.h"
#include "gfx.h"
#include "blitters/blitters.h"
#include "font.h"
#include "sprite_set.h"
#include "sprite.h"
#include "script.h"
#include "luaapi/context.h"
#include "gui/omfggui_windows.h"
#include "gconsole.h"
#include <boost/bind/bind.hpp>
#include <iostream>
#include <list>
#include <string>
#include <fstream>
#include <sstream>
using std::cout;
using std::endl;

ResourceLocator<XMLFile, false, false> xmlLocator;

namespace OmfgGUI
{

GContext menu;

std::string cmdLoadXML(std::list<std::string> const& args)
{
	// Not really used. We don't draw anything of it.
	// However, we load to trick old Gusanos mods.
	
	if(args.size() > 0)
	{
		std::string ret;

		std::list<std::string>::const_iterator i = args.begin();
		std::string path;
		Wnd* loadTo = menu.getRoot();
		
		path = *i++;
		if(i != args.end())
			loadTo = menu.findNamedWindow(*i++);
			
		if(!loadTo)
			return "DESTINATION WINDOW NOT FOUND";
			
		XMLFile f;
		if(xmlLocator.load(&f, path))
		{
			menu.buildFromXML(f.f, loadTo);
			return "";
		}
		else
			return "ERROR LOADING \"" + path + '"';
	}
	
	return "GUI_LOADXML <FILE> [<DEST>] : LOADS AN XML FILE INTO A WINDOW (ROOT BY DEFAULT)";
}

std::string cmdLoadGSS(std::list<std::string> const& args)
{
	// Ignore without error.
	// This were old Gusanos style cheets for Gusanos menu,
	// which is not used (except dummy compatibility code).
	return "";
}

std::string cmdGSS(std::list<std::string> const& args)
{
	return ""; // ignore without error. see cmdLoadGSS()
}

std::string cmdFocus(std::list<std::string> const& args)
{
	return ""; // ignore without error
}

GContext::GContext()
: Context()
{}

void GContext::init()
{
	console.registerCommands()
		("GUI_LOADXML", cmdLoadXML)
		("GUI_LOADGSS", cmdLoadGSS)
		("GUI_GSS", cmdGSS)
		("GUI_FOCUS", cmdFocus)
	;
}

LuaContext& GContext::luaContext()
{
	return luaIngame;
}

void GContext::clear()
{
	delete m_rootWnd;
	m_rootWnd = 0;

	// Gusanos menu not used.
	// However, create dummy root wnd so that some old Gusanos mods work.
		
	std::map<std::string, std::string> attributes;
	attributes["id"] = "root";
	Wnd* root = lua_new(Wnd, (0, attributes), luaIngame);
	setRoot(root);
}

Wnd* GContext::loadXMLFile(std::string const& name, Wnd* loadTo)
{
	XMLFile f;
	if(xmlLocator.load(&f, name))
	{
		return buildFromXML(f.f, loadTo);
	}
	
	return 0;
}

}

#endif
