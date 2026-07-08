#include "console.h"
#include "variables.h"
#include "command.h"
#include "special_command.h"
#include "alias.h"
#include "util/text.h"
#include "util/macros.h"
#include "consoleitem.h"

#include "console-grammar.h"
#include "gusanos/allegro.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <stack>
#include <cctype>

#include <sstream>
#include <iostream>
#include <algorithm>

using namespace std;

/////////////////////////////// Console //////////////////////////////////////

//============================= LIFECYCLE ====================================


Console::~Console()
{
	// Delete the registered variables
	foreach(i, items)
	{
		delete i->second;
	}
	
	items.clear();
}

//============================= INTERFACE ====================================

void Console::registerItem(std::string const& name, ConsoleItem* item)
{
	item->m_owner = this;
	items[name] = item;
}

void Console::registerVariable(Variable* var)
{
	string const& name = var->getName();
	if (!name.empty())
	{
		std::map<std::string, ConsoleItem*, IStrCompare>::iterator i = items.find(name);
		if(i != items.end())
		{
			// Replace old variable
			delete i->second;
		}
		
		registerItem(name, var);

		// register also at OLX
		const static std::string dummyDefault("");
		CScriptableVars::RegisterVars("Gusanos") ((DynamicVar<std::string>*)var, name, dummyDefault);
	
	}
	else
	{
		delete var; // Empty name
	}
}

void Console::registerCommand(std::string const& name, GusCommand* command)
{
	if (!name.empty() && items.find(name) == items.end())
	{
		registerItem(name, command);
	}
	else
		delete command;
}

void Console::registerSpecialCommand(const std::string &name, int index, std::string (*func)(int,const std::list<std::string>&))
{
	if (!name.empty())
	{
		ItemMap::iterator tempItem = items.find(name);
		if (tempItem == items.end())
		{
			registerItem(name, new SpecialCommand(index, func));
		}
	}
}

void Console::registerAlias(const std::string &name, const std::string &action)
{
	if (!name.empty())
	{
		ItemMap::iterator tempItem = items.find(name);
		if (tempItem == items.end()
		|| !tempItem->second->isLocked())
		{
			if(tempItem != items.end())
				delete tempItem->second;
			registerItem(name, new Alias(name, action));
		}
	}
}

void Console::clearTemporaries()
{
	foreach_delete(i, items)
	{	
		if(i->second->temp)
		{
			delete i->second;
			items.erase(i);
		}
	}
}

struct TestHandler : public ConsoleGrammarBase
{
	TestHandler(std::istream& str_, Console& console_, bool parseRelease_ = false)
	: str(str_), console(console_), parseRelease(parseRelease_)
	{
		next();
	}
	
	int cur()
	{
		return c;
	}
	
	void next()
	{
		c = str.get();
		if(c == std::istream::traits_type::eof())
			c = -1;
	}
	
	std::string invoke(std::string const& name, std::list<std::string> const& args)
	{
		return console.invoke(name, args, parseRelease);
	}
	
	int c;
	std::istream& str;
	Console& console;
	bool parseRelease;
};

void Console::parseLine(const string &text, bool parseRelease)
{
	if(text == "") return;
	if(text[0] == '#') return; // comment
	
	std::istringstream ss(text);
	ConsoleGrammar<TestHandler> handler((TestHandler(ss, *this, parseRelease)));

	try
	{
		addLogMsg(handler.block());
	}
	catch(const SyntaxError& error)
	{
		addLogMsg(text);
		std::streamoff pos = handler.str.tellg();
		if(pos > 0)
		{
			addLogMsg(std::string((unsigned int)pos - 1, '-') + '^');
			addLogMsg(error.what());
		}
		else
		{
			addLogMsg(error.what() + string(" at end of input"));
		}
	}
}

std::string Console::invoke(string const& name, list<string> const& args, bool parseRelease)
{
	if(!parseRelease || name[0] == '+')
	{
		std::string nameCopy(name);
		if(parseRelease)
			nameCopy[0] = '-';

		ItemMap::iterator tempItem = items.find(nameCopy);
		if (tempItem != items.end())
		{
			return tempItem->second->invoke(args);
		} else
			return "UNKNOWN COMMAND: " + name;
	}
	
	return "";
}

void Console::addLogMsg(const string &msg)
{
	if (!msg.empty())
	{
		notes << "Gus con: " << msg << endl;
	}
}

int Console::executeConfig(const string &filename)
{
	ifstream file;
	OpenGameFileR(file, filename.c_str());

	if (file.is_open() && file.good())
	{
		string text2Parse;
		//...parse the file
		while (portable_getline(file, text2Parse))
		{
			parseLine(text2Parse);
		}

		return 1;
	}
	
	return 0;
}

