#include "gconsole.h"
#ifndef DEDICATED_ONLY
#include "font.h"
#include "sprite_set.h"
#include "sprite.h"
#endif
#include "script.h"
#include "gusgame.h"
#include "util/math_func.h"

#include "network.h" //TEMP
#include "Debug.h"
#include "FindFile.h"

#include "gusanos/allegro.h"
#include <boost/bind/bind.hpp>
#include <boost/lexical_cast.hpp>
using boost::lexical_cast;

using namespace std;

string echoCmd(list<string> const& args)
{
	if(args.size() > 0)
	{
		std::list<string>::const_iterator i = args.begin();
		std::string ret = *i++;
		
		for(; i != args.end(); ++i)
		{
			ret += ' ' + *i;
		}
		
		return ret;
	}
	
	return "ECHO <ARGS> ... : PRINTS OUT ARGS TO THE CONSOLE";
}

string execCmd(const list<string> &args)
{
	if (!args.empty())
	{
		if ( console.executeConfig(*args.begin()) )
		{
			return "DONE";
		}
		return ( "COULDN'T EXEC " + *args.begin() );
	}
	return "EXEC <FILENAME> : EXECUTE A SCRIPT FILE";
}

string aliasCmd(const list<string> &args)
{
	string name;
	string action;
	
	if (!args.empty())
	{
		list<string>::const_iterator argument;
		argument=args.begin();
		
		name = *argument;
		
		argument++;
		
		if ( argument != args.end() )
		{
			action = *argument;
			console.registerAlias(name,action);
		}

		return "";
	}
	return "ALIAS <NAME> <ACTION> : REGISTER ALIAS TO ACTION";
}


static std::string dummy_CON_SPEED(const std::list<std::string> &args) {
	warnings << "Gus CON_SPEED obsolete/unused" << endl;
	return "";
}

static std::string dummy_CON_HEIGHT(const std::list<std::string> &args) {
	warnings << "Gus CON_HEIGHT obsolete/unused" << endl;
	return "";
}

static std::string dummy_CON_FONT(const std::list<std::string> &args) {
	warnings << "Gus CON_FONT obsolete/unused" << endl;
	return "";
}

void GConsole::init()
{
	console.registerCommands()
		("CON_SPEED", dummy_CON_SPEED)
		("CON_HEIGHT", dummy_CON_HEIGHT)
		("CON_FONT", dummy_CON_FONT)
	;
	
	console.registerCommands()
		(string("EXEC"), execCmd)
		(string("ALIAS"), aliasCmd)
		(string("ECHO"), echoCmd)
	;	
}

int GConsole::executeConfig(const std::string& filename)
{
	std::string p(gusGame.getModPath() + "/" + filename);
	if ( gusExists(p) )
		return Console::executeConfig(p);
	else
		return Console::executeConfig(gusGame.getDefaultPath() + "/" + filename);
}


GConsole console;

