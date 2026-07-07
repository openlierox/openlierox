#ifndef OMFG_GUI_BUTTON_H
#define OMFG_GUI_BUTTON_H

#include "wnd.h"

namespace OmfgGUI
{

class Button : public Wnd
{
public:
	static LuaReference metaTable;
	
	Button(Wnd* parent_, std::map<std::string, std::string> const& properties)
	: Wnd(parent_, properties, "button")
	{}
};

}

#endif //OMFG_GUI_BUTTON_H

