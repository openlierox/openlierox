/////////////////////////////////////////
//
//   OpenLieroX
//
//   Auxiliary Software class library
//
//   based on the work of JasonB
//   enhanced by Dark Charlie and Albert Zeyer
//
//   code under LGPL
//
/////////////////////////////////////////


// Command/variable parsing
// Created 9/4/02
// Jason Boettcher


#include <limits.h>
#include "LieroX.h"
#include "Debug.h"
#include "CServer.h"
#include "CClient.h"
#include "OLXConsole.h"
#include "StringUtils.h"
#include "sex.h"
#include "game/CWorm.h"
#include "AuxLib.h"
#include "Version.h"
#include "CClientNetEngine.h"
#include "DeprecatedGUI/Menu.h"
#include "IRC.h"
#include "ProfileSystem.h"
#include "FindFile.h"
#include "DedicatedControl.h"
#include "CGameMode.h"
#include "Cache.h"
#include "CServerConnection.h"
#include "CServerNetEngine.h"
#include "CChannel.h"
#include "IpToCountryDB.h"
#include "Unicode.h"
#include "Autocompletion.h"
#include "OLXCommand.h"
#include "TaskManager.h"
#include "game/Mod.h"
#include "StringUtils.h"
#include "game/Game.h"
#include "gusanos/gusgame.h"
#include "game/WormInputHandler.h"
#include "sound/SoundsBase.h"
#include "game/Level.h"
#include "game/ServerList.h"
#include "EventQueue.h"
#include "client/ClientConnectionRequestInfo.h"
#include "gusanos/luaapi/context.h"


CmdLineIntf& stdoutCLI() {
	struct StdoutCLI : CmdLineIntf {
		virtual void pushReturnArg(const std::string& str) {
			notes << "Ret: " << str << endl;
		}
		
		virtual void finalizeReturn() {
			notes << "Ret." << endl;
		}
		
		virtual void writeMsg(const std::string& msg, CmdLineMsgType type) {
			Logger* l = &notes;
			switch(type) {
				case CNC_NORMAL: break;
				case CNC_WARNING: l = &warnings; break;
				case CNC_ERROR: l = &errors; break;
				case CNC_NOTIFY: l = &hints; break;
				default: (*l) << CmdLineMsgTypeAsString(type).substr(0,1) << ": ";
			}
			(*l) << msg << endl;
		}
				
	};
	static StdoutCLI cli;
	return cli;
}

ParamSeps ParseParams_Seps(const std::string& params) {
	bool quote = false;
	size_t start = 0;
	ParamSeps res;
	ParamSeps::iterator lastentry = res.begin();
	
	const_string_iterator i (params);
	for(; i.pos < params.size(); IncUtf8StringIterator(i, const_string_iterator(params, params.size()))) {

		// Check delimeters
		if(*i == ' ' || *i == ',') {
			if(start < i.pos)
				lastentry = res.insert(lastentry, ParamSeps::value_type(start, i.pos - start));
			start = i.pos + 1;
			
			continue;
		}

		// TODO: do we really want this?
		// Check comments
		if (i.pos + 1 < params.size())  {
			if(*i == '/' && params[i.pos+1] == '/') {
				if(start < i.pos)
					lastentry = res.insert(lastentry, ParamSeps::value_type(start, i.pos - start));
				start = params.size() + 1;
				
				// Just end here
				break;
			}
		}
		
		// Check quotes
		if(*i == '"') {
			quote = true;
			i.pos++;
			start = i.pos;
			
			// Read until another quote
			for(; i.pos < params.size(); IncUtf8StringIterator(i, const_string_iterator(params, params.size()))) {
				if(*i == '"') {
					quote = false;
					break;
				}
			}
			if(quote) break; // if we are still in the quote, break (else we would make an addition i++ => crash)
			lastentry = res.insert(lastentry, ParamSeps::value_type(start, i.pos - start));
			start = i.pos + 1;
			continue;
		}
		
		// check brackets (handle it like quotes but include them)
		if(*i == '(') {
			quote = true;
			i.pos++;
			
			// Read until all brackets closed
			unsigned int bracketDepth = 1;
			for(; i.pos < params.size(); IncUtf8StringIterator(i, const_string_iterator(params, params.size()))) {
				if(*i == '(') bracketDepth++;
				else if(*i == ')') { bracketDepth--; if(bracketDepth == 0) break; }
			}
			if(bracketDepth > 0) break; // if we are still in the bracket, break (else we would make an addition i++ => crash)
			quote = false;
			continue;			
		}
	}
	
	// Add the last token
	if(start < params.size()) {
		lastentry = res.insert(lastentry, ParamSeps::value_type(start, params.size() - start));
	}
	
	return res;
}


std::vector<std::string> ParseParams(const std::string& params) {
	std::vector<std::string> res;
	ParamSeps seps = ParseParams_Seps(params);
	for(ParamSeps::iterator i = seps.begin(); i != seps.end(); ++i)
		res.push_back(params.substr(i->first, i->second));
	return res;
}




///////////////////
// Converts ID as string to an integer, returns -1 on fail
/*static int atoid(const std::string& str)
{
	// Ignore any non-numerical characters before the actual number, in most cases this is #
	std::string::const_iterator it = str.begin();
	while (it != str.end())  {
		if (*it >= 48 && *it <= 57)
			break;

		it++;
	}

	// Convert the string to a number
	std::string id = std::string(it, str.end());
	if (id.size() == 0)
		return -1;

	bool fail;
	int res = from_string<int>(id, fail);

	if (fail)
		return -1;
	else
		return res;
}
*/


/*
======================================

              Commands

======================================
*/





struct AutocompleteRequest {
	CmdLineIntf& cli;
	AutocompletionInfo& autocomplete;
	AutocompletionInfo::InputState old;
	
	std::string pretxt, posttxt;
	std::string token;
	size_t tokenpos;
	
	static bool emptyStart(char c) {
		return c == ' ' || c == '\"' || c == ',';
	}
	
	AutocompletionInfo::InputState completeSuggestion(const std::string& repl, bool isCompleteSuggestion) const {
		AutocompletionInfo::InputState ret;
		ret.text = pretxt;
		ret.pos = pretxt.size() + repl.size();
		bool needToAddPostQuotes = false;
		bool hadPreQuotes = pretxt.size() > 0 && pretxt[pretxt.size()-1] == '\"';
		if(repl.find_first_of(" ,\t") != std::string::npos || hadPreQuotes) { // we must have quotes
			if(!hadPreQuotes) { // no pre-quotes
				// we need to put pre-quotes
				ret.text += '\"';
				ret.pos++;
			}
			if(posttxt.size() == 0 || posttxt[0] != '\"') // no post-quotes
				needToAddPostQuotes = true;
		}
		ret.text += repl;
		if(needToAddPostQuotes) ret.text += "\"";
		if(posttxt.size() > 0 && !emptyStart(posttxt[0])) {
			ret.text += " ";
			if(isCompleteSuggestion) { ret.pos++; if(needToAddPostQuotes) ret.pos++; }
		}
		else if(posttxt.size() > 0 && emptyStart(posttxt[0])) {
			if(isCompleteSuggestion) { ret.pos++; if(needToAddPostQuotes) ret.pos++; }
		}
		else if(posttxt.empty() && isCompleteSuggestion) {
			ret.text += " ";
			ret.pos++;
			if(needToAddPostQuotes) ret.pos++;
		}
		ret.text += posttxt;
		return ret;
	}
	
};




// TODO: Move this
static CWorm* CheckWorm(CmdLineIntf* caller, int id, const std::string& request)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(request + " works only as server");
		return NULL;
	}
	
	if(id <0 || id >= MAX_WORMS) {
		caller->writeMsg(request + " : Faulty ID " + itoa(id));
		return NULL;
	}
	CWorm *w = game.wormById(id, false);
	if(!w) {
		caller->writeMsg(request + " : ID " + itoa(id) + " not in use");
		return NULL;
	}
	return w;
}



struct Command;
typedef bool (*AutoCompleteFct) (Command*, AutocompleteRequest&);


struct Command : CommandDesc {
	std::map<unsigned int, AutoCompleteFct> paramCompleters;
	bool hidden;

	std::string minMaxStr() const {
		if(minParams == maxParams) return itoa(minParams);
		std::string s = itoa(minParams) + "-";
		if(maxParams == UINT_MAX) s += "*";
		else s += itoa(maxParams);
		return s;
	}
	
	std::string usageStr() const {
		if(usage != "") return name + " " + usage;
		else return name;
	}
	
	void printUsage(CmdLineIntf* caller) {
		caller->writeMsg("usage: " + usageStr());
	}
	
	std::string fullDesc() const {
		return usageStr() + " - " + desc;
	}
	
	virtual void exec(CmdLineIntf* caller, const std::vector<std::string>& params) = 0;

	void exec(CmdLineIntf* caller, const std::string& params) {
		std::vector<std::string> ps = ParseParams(params);
		
		// if we want max 1 param but we give more, this is a small hack to dont be too strict
		if(maxParams == 1 && ps.size() > 1) {
			for(size_t i = 1; i < ps.size(); ++i)
				ps[0] += " " + ps[i];
			ps.resize(1);
		}
		
		if(ps.size() < minParams || ps.size() > maxParams) {
			caller->writeMsg(minMaxStr() + " param" + ((maxParams == 1) ? "" : "s") + " needed, usage: " + usageStr(), CNC_DEV);
			//caller->writeMsg("bad cmd: " + name + " " + params);
			return;
		}
		
		exec(caller, ps);
	}
	
	bool completeParam(unsigned int i, AutocompleteRequest& request) {
		if(i + 1 > maxParams) {
			if(request.token != "")
				request.cli.writeMsg(name + " takes " + minMaxStr() + " param" + ((maxParams == 1) ? "" : "s") + ", " + itoa(i + 1) + " given", CNC_DEV);
			else
				request.cli.writeMsg(name + " - " + desc, CNC_DEV);
			return false;
		}
		
		std::map<unsigned int, AutoCompleteFct>::iterator p = paramCompleters.find(i);
		if(p != paramCompleters.end()) return (*p->second)(this, request);

		// no custom completer, so just write usage
		request.cli.writeMsg(fullDesc(), CNC_DEV);
		return false;
	}
	
	CWorm* getWorm(CmdLineIntf* caller, const std::string& param) {
		bool fail = true;
		int id = from_string<int>(param, fail);
		if(fail) {
			printUsage(caller);
			return NULL;
		}
		return CheckWorm(caller, id, name);
	}
	
};

typedef std::map<std::string, Command*, stringcaseless> CommandMap;
static CommandMap commands;



template <typename _List>
static bool autoCompleteForList(AutocompleteRequest& request, const std::string& unitname, const _List& unitlist) {
	if(request.token == "") {
		request.cli.writeMsg("please enter a " + unitname, CNC_DEV);
		return false;
	}
	
	typedef typename _List::const_iterator iterator;
	iterator it = unitlist.lower_bound(request.token);
	if(it == unitlist.end()) {
		request.cli.writeMsg("no such " + unitname, CNC_WARNING);
		return false;
	}
	
	if(!subStrCaseEqual(it->first, request.token, request.token.size())) {
		request.cli.writeMsg("no such " + unitname, CNC_WARNING);
		return false;
	}
	
	std::list<std::string> possibilities;
	
	for(iterator j = it; j != unitlist.end(); ++j) {
		if(subStrCaseEqual(request.token, j->first, request.token.size()))
			possibilities.push_back(j->first);
		else
			break;
	}
	
	if(possibilities.size() == 0) {
		// strange though..
		request.cli.writeMsg("unknown " + unitname, CNC_WARNING);
		return false;
	}
	
	if(possibilities.size() == 1) {
		// we have exactly one solution
		request.autocomplete.setReplace(request.old, request.completeSuggestion(possibilities.front(), true));
		return true;
	}
	
	size_t l = maxStartingCaseEqualStr(possibilities);
	if(l > request.token.size()) {
		// we can complete to some longer sequence
		request.autocomplete.setReplace(request.old, request.completeSuggestion(possibilities.front().substr(0,l), false));
		return true;
	}
	
	// send list of all possibilities
	std::string possStr;
	for(std::list<std::string>::iterator j = possibilities.begin(); j != possibilities.end(); ++j) {
		if(possStr.size() > 0) possStr += " ";
		possStr += *j;
	}
	request.cli.writeMsg(possStr);
	return false;	
}

static bool autoCompleteCommand(Command*, AutocompleteRequest& request) {
	return autoCompleteForList(request, "command", commands);
}

static bool autoCompleteVar(Command*, AutocompleteRequest& request) {
	std::list<std::string> possibilities;
	
	for(CScriptableVars::const_iterator it = CScriptableVars::lower_bound(request.token); it != CScriptableVars::Vars().end(); ++it) {
		// ignore callbacks
		if(it->second.var.type == SVT_CALLBACK) continue;
		
		if( subStrCaseEqual(request.token, it->first, request.token.size()) ) {
			std::string nextComplete = it->first.substr(0, request.token.size());
			for(size_t f = request.token.size();; ++f) {
				if(f >= it->first.size()) break;
				nextComplete += it->first[f];
				if(it->first[f] == '.') break;
			}
			
			if(possibilities.size() == 0 || *possibilities.rbegin() != nextComplete) {
				possibilities.push_back(nextComplete);
			}
		}
		else
			break;
	}
	
	if(possibilities.size() == 0) {
		request.cli.writeMsg("unknown variable", CNC_WARNING);
		return false;
	}
	
	if(possibilities.size() == 1) {
		// we have exactly one solution
		bool isComplete = possibilities.front().size() > 0 && possibilities.front()[possibilities.front().size()-1] != '.';
		request.autocomplete.setReplace(request.old, request.completeSuggestion(possibilities.front(), isComplete));
		return true;
	}
	
	size_t l = maxStartingCaseEqualStr(possibilities);
	if(l > request.token.size()) {
		// we can complete to some longer sequence
		request.autocomplete.setReplace(request.old, request.completeSuggestion(possibilities.front().substr(0, l), false));
		return true;
	}
	
	// send list of all possibilities
	std::string possStr;
	size_t startSugPos = 0;
	for(size_t p = 0; p < request.token.size(); ++p)
		if(request.token[p] == '.') {
			startSugPos = p + 1;
		}
	for(std::list<std::string>::iterator j = possibilities.begin(); j != possibilities.end(); ++j) {
		if(possStr.size() > 0) possStr += " ";
		possStr += j->substr(startSugPos);
	}
	request.cli.writeMsg(possStr);
	return false;
}

bool FileListCacheIntf::autoComplete(AutocompleteRequest& request) {
	Mutex::ScopedLock lock(mutex);
	if(!isReady) {
		request.cli.writeMsg("file list cache not ready yet - please try again in a moment", CNC_ERROR);
		return false;
	}
	return autoCompleteForList(request, name, filelist);
}

template <FileListCacheIntf*& filelist>
bool autoCompleteForFileListCache(Command*, AutocompleteRequest& request) {
	return filelist->autoComplete(request);
}


///////////////////
// Find a command with the same name
static Command *Cmd_GetCommand(const std::string& strName)
{
	CommandMap::iterator it = commands.find(strName);
	if(it != commands.end()) return it->second;
	return NULL;
}

CommandDesc* GetCommandDesc(const std::string& cmdname) {
	return Cmd_GetCommand(cmdname);
}

static void registerCommand(const std::string& name, Command* cmd) {
#ifdef DEBUG
	Command* old = Cmd_GetCommand(name);
	if(old) {
		errors << "Command '" << cmd->fullDesc() << "' as " << name << " will overwrite command " << old->name << endl;
	}
#endif
	commands.insert( CommandMap::value_type(name, cmd) );
}

static void registerCommand(Command* cmd) {
	registerCommand(cmd->name, cmd);
}




#ifndef TOSTRING
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#endif

#define COMMAND_EXTRA(_name, _desc, _us, _minp, _maxp, extras) \
	struct Cmd_##_name : public Command { \
		Cmd_##_name() { \
			name = TOSTRING(_name); desc = _desc; usage = _us; minParams = _minp; maxParams = _maxp; \
			hidden = false; \
			{ extras; } \
			registerCommand(this); \
		} \
		virtual void exec(CmdLineIntf* caller, const std::vector<std::string>& params); \
	} \
	__cmd_##_name;

#define COMMAND(_name, _desc, _us, _minp, _maxp) \
	COMMAND_EXTRA(_name, _desc, _us, _minp, _maxp, {})

// ------------- original console commands starting here ----------



COMMAND_EXTRA(crash, "crash the game", "", 0, 0, hidden = true);
void Cmd_crash::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	caller->writeMsg("In a previous version, the game would crash now!", CNC_NORMAL);
	// HINT: please don't add any code, which could make the game unstable
	//		(I myself just tested this command without knowing and BANG,
	//		I got an access violation. Perhaps the hoster of an important
	//		clan war does it...)
	if(GetGameVersion().releasetype == Version::RT_BETA) {
		caller->writeMsg("This version will crash too, though.", CNC_WARNING);
		// HINT: the current simple CrashHandler does not have any problems with this, thus it can stay here for testing
		(*(int*)0x13) = 42;
		assert(false);
	}
}

COMMAND_EXTRA(coreDump, "generate a core dump", "", 0, 0, hidden = true);
void Cmd_coreDump::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->writeMsg("Dumping core ...", CNC_NORMAL);
	doVideoFrameInMainThread();
	struct Dumper : Action {
		Result handle() {
			OlxWriteCoreDump("OlxCoreDump.dmp");
			hints << "Dumping core finished" << endl; // don't use caller here because it's not sure that it still exists
			return true;
		}
	};
	doActionInMainThread(new Dumper());
}

struct CLIPrintOutFct : PrintOutFct {
	CmdLineIntf* caller;
	CLIPrintOutFct() : caller(NULL) {}
	virtual void print(const std::string& msg) const {
		if(!caller) return;
		if(msg.empty()) return;
		if(msg[msg.size()-1] == '\n')
			caller->writeMsg(msg.substr(0, msg.size()-1));
		else
			caller->writeMsg(msg);
	}
};

COMMAND_EXTRA(debugDumpCallstack, "dump callstack", "", 0, 0, hidden = true);
void Cmd_debugDumpCallstack::exec(CmdLineIntf *caller, const std::vector<std::string>& params) {
	CLIPrintOutFct p;
	p.caller = caller;
	DumpCallstack(p);
}

COMMAND_EXTRA(debugDumpAllThreadsCallstack, "dump callstack of all threads", "", 0, 0, hidden = true);
void Cmd_debugDumpAllThreadsCallstack::exec(CmdLineIntf *caller, const std::vector<std::string>& params) {
	CLIPrintOutFct p;
	p.caller = caller;
	DumpAllThreadsCallstack(p);
}


COMMAND(suicide, "suicide first local human worm", "[#kills]", 0, 1);
void Cmd_suicide::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if (!cClient)  {
		caller->writeMsg("client not initialised", CNC_ERROR);
		return;
	}
	
	if(bDedicated) {
		caller->writeMsg("Cannot suicide in dedicated mode!", CNC_WARNING);
		return;
	}
	
	if(game.state != Game::S_Playing)  {
		caller->writeMsg("Cannot suicide when not playing!", CNC_WARNING);
		return;
	}
	
	CWorm* w = game.firstLocalHumanWorm();
	if(!w) {
		caller->writeMsg("no local human worm found", CNC_WARNING);
		return;
	}
	
	// Without arguments, just commit one suicide
	if (params.size() == 0)  {
		cClient->getNetEngine()->SendDeath(w->getID(), w->getID());
	}
	// A number has been entered, suicide the specified number
	else  {
		// Get the number
		bool fail;
		int number = from_string<int>(params[0], fail);
		if (fail)
			number = 1;
		
		if (number > (int)cClient->getGameLobby()[FT_Lives]+1)  // Safety, not needed really (should be covered in next condition)
			number = (int)cClient->getGameLobby()[FT_Lives]+1;
		if (number > w->getLives()+1)
			number = w->getLives()+1;
		if (number < 1)
			number = 1;
		
		// Suicide
		for (int i = 0; i < number; i++)
			cClient->getNetEngine()->SendDeath(w->getID(), w->getID());
	}
}

COMMAND(unstuck, "unstuck first local human worm", "", 0, 0);
// Unstuck a stucked worm
void Cmd_unstuck::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if (!cClient)  {
		caller->writeMsg("client not initialised", CNC_ERROR);
		return;
	}
	
	if(bDedicated) {
		caller->writeMsg("Cannot unstuck in dedicated mode!", CNC_WARNING);
		return;
	}
	
	// Not playing
	if(game.state != Game::S_Playing)  {
		caller->writeMsg("Cannot unstuck when not playing!", CNC_WARNING);
		return;
	}
	
	// Unstuck
	CWorm* w = game.firstLocalHumanWorm();
	if(!w) {
		caller->writeMsg("no local human worm found", CNC_WARNING);
		return;
	}

	if(w->getAlive())
		w->setPos(game.gameMap()->FindNearestSpot(w));
}

COMMAND(wantsJoin, "enable/disable wants to join messages", "true/false", 1, 1);
// Enables or disables wants to join messages
void Cmd_wantsJoin::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	const std::string arg = params[0];
	
	if (!stringcasecmp(arg,"on") || !stringcasecmp(arg,"true") || !stringcasecmp(arg,"1") || !stringcasecmp(arg,"yes"))  {
		tLXOptions->bAllowWantsJoinMsg = true;
		caller->writeMsg("\"Wants to join\" messages have been enabled", CNC_NORMAL);
	}
	else  {
		tLXOptions->bAllowWantsJoinMsg = false;
		caller->writeMsg("\"Wants to join\" messages have been disabled", CNC_NORMAL);
	}
	
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg("Note that this has no effect right now; it's for the case when you are hosting next time.");
		return;
	}
}

COMMAND(serverName, "rename server", "new-name", 1, 1);
void Cmd_serverName::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	tLXOptions->sServerName = params[0];
}

COMMAND_EXTRA(help, "list available commands or shows desription/usage of specific command", "[command]", 0, 1, paramCompleters[0] = &autoCompleteCommand);
void Cmd_help::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(params.size() > 0) {
		CommandMap::iterator it = commands.lower_bound(params[0]);
		if(it != commands.end()) {
			if(!stringcaseequal(it->first, params[0]))
				caller->writeMsg("Help: Was that a typo? Did you mean '" + it->first + "'?", CNC_WARNING);
			if(!stringcaseequal(it->second->name, it->first))
				caller->pushReturnArg(it->first + " is an alias for " + it->second->name);
			caller->pushReturnArg(it->second->usageStr());
			caller->pushReturnArg(it->second->desc);
			return;
		}
		
		caller->writeMsg("Help: command " + params[0] + " is unknown", CNC_WARNING);
	}
	
	caller->writeMsg("Available commands:");
	std::string cmd_help_buf;
	unsigned short count = 0;
	
	for(CommandMap::iterator it = commands.begin(); it != commands.end(); ++it) {
		if(!it->second->hidden && it->first == it->second->name) {
			cmd_help_buf += it->first;
			cmd_help_buf += " ";
			count++;
			if(count >= 5) {
				count = 0;
				caller->writeMsg("  " + cmd_help_buf);
				cmd_help_buf = "";
			}
		}
	}
	if(count && cmd_help_buf != "") {
		caller->writeMsg("  " + cmd_help_buf);
	}
	caller->writeMsg("Type 'longhelp' to get a list together with small description.");
	caller->writeMsg("Type 'help <command>' to get some help about this commad.");
}

COMMAND(longhelp, "list available commands and description", "", 0, 0);
void Cmd_longhelp::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->writeMsg("Available commands:");
	for(CommandMap::iterator it = commands.begin(); it != commands.end(); ++it) {
		if(!it->second->hidden && it->first == it->second->name) {
			caller->pushReturnArg(it->first + " - " + it->second->desc);
		}
	}
}

COMMAND_EXTRA(version, "print game version info", "", 0, 0, registerCommand("about", this));
void Cmd_version::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->pushReturnArg(GetFullGameName());
}

COMMAND_EXTRA(sex, "say something messy", "", 0, 0, hidden = true; registerCommand("fuck", this));
void Cmd_sex::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->pushReturnArg(sex(50));
}

COMMAND(disconnect, "disconnect from server or exit server", "", 0, 0);
void Cmd_disconnect::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.state <= Game::S_Inactive) {
		caller->writeMsg("game is inactive, cannot disconnect anything", CNC_WARNING);
		return;
	}
	
	if(cClient && cClient->getStatus() != NET_DISCONNECTED)
		cClient->Disconnect();
	
	game.state = Game::S_Inactive;
		
	if(!bDedicated && DeprecatedGUI::tMenu) {
		if(game.isLocalGame())
			GotoLocalMenu();
		else
			GotoNetMenu();
	}
}

COMMAND(volume, "set sound volume", "0-100", 1, 1);
void Cmd_volume::exec(CmdLineIntf* caller, const std::vector<std::string>& params)  {
	if(bDedicated) {
		caller->writeMsg(name + " cannot be used in dedicated mode");
		return;
	}
	
	bool fail = false;
	int vol = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	vol = MIN(vol,100);
	vol = MAX(vol,0);
	SetSoundVolume(vol);
	caller->writeMsg("new sound volume: " + itoa(vol));
}

COMMAND(sound, "enable or disable sound", "true/false", 1, 1);
void Cmd_sound::exec(CmdLineIntf* caller, const std::vector<std::string>& params)  {
	if(bDedicated) {
		caller->writeMsg(name + " cannot be used in dedicated mode");
		return;
	}
	
	const std::string arg = params[0];
	
	if (!stringcasecmp(arg,"on") || !stringcasecmp(arg,"true") || !stringcasecmp(arg,"1") || !stringcasecmp(arg,"yes"))  {
		StartSoundSystem();
		tLXOptions->bSoundOn = true;
		caller->writeMsg("sound is now enabled, volume = " + itoa(GetSoundVolume()));
	}
	else  {
		StopSoundSystem();
		tLXOptions->bSoundOn = false;
		caller->writeMsg("sound is now disabled");
	}
}

COMMAND_EXTRA(serverSideHealth, "turn on/off server-side health", "true/false", 1, 1, registerCommand("ssh", this));
void Cmd_serverSideHealth::exec(CmdLineIntf* caller, const std::vector<std::string>& params)  {
	if(!bDedicated) {
		caller->writeMsg("Sorry, server side health has been removed for non-dedicated servers", CNC_WARNING);
		return;
	}
	
	const std::string arg = params[0];
	 
	// Set the ssh
	tLXOptions->bServerSideHealth = stringcaseequal(arg,"on") || stringcaseequal(arg,"true") || stringcaseequal(arg,"1") || stringcaseequal(arg,"yes");
	 
	caller->writeMsg(std::string("Server-side health is now ") + (tLXOptions->bServerSideHealth ? std::string("enabled.") : std::string("disabled.")));
	
}

COMMAND(irc, "send message to IRC chat", "text", 1, 1);
void Cmd_irc::exec(CmdLineIntf* caller, const std::vector<std::string>& params)  {
	if (GetGlobalIRC())
		GetGlobalIRC()->sendChat(params[0]);
	else
		caller->writeMsg("IRC system is not running");
}

COMMAND(connect, "join a server", "server[:port] [player]", 1, 2);
void Cmd_connect::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(cServer)
		cServer->Shutdown();
	
	if(cClient && cClient->getStatus() != NET_DISCONNECTED)
		cClient->Disconnect();
		
	std::string server = params[0];
	std::string player =
		(params.size() >= 2) ? params[1] :
		bDedicated ? FindFirstCPUProfileName() :
		tLXOptions->sLastSelectedPlayer;
	if(params.size() == 1 && player == "") player = MainHumanProfile()->sName;
	if(!JoinServer(server, server, player)) return;
}

COMMAND(wait, "Execute commands after wait", "seconds|lobby|game command [args] [ ; command2 args... ]", 2, INT_MAX);
void Cmd_wait::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	
	if(cClient && cClient->getStatus() != NET_DISCONNECTED)
		cClient->Disconnect();
	
	struct WaitThread: public Action
	{
		std::vector<std::string> params;
		WaitThread(const std::vector<std::string>& _params): params(_params) {};
		Result handle()
		{
			int seconds = atoi(params[0]);
			if(seconds == 0)
				seconds = -1;
			
			while( game.state != Game::S_Quit ) // TODO: put mutex here
			{
				if(params[0] == "game" && cClient && game.state == Game::S_Playing) // TODO: put mutex here
				{
					//stdoutCLI().writeMsg("wait: trigger: game started");
					break;
				}
				if(params[0] == "lobby" && cClient && game.state == Game::S_Lobby) // TODO: put mutex here
				{
					//stdoutCLI().writeMsg("wait: trigger: lobby started");
					break;
				}
				if(seconds > 0)
				{
					seconds--;
					if(seconds <= 0)
					{
						//stdoutCLI().writeMsg("wait: trigger: timeout");
						break;
					}
				}
				SDL_Delay(1000);
			}
			
			if( game.state == Game::S_Quit ) // TODO: put mutex here
				return "OLX exited";
				
			for( std::vector<std::string>::iterator it = (++params.begin()); it != params.end(); )
			{
				std::string cmdName = *it;
				Command * cmd = Cmd_GetCommand(cmdName);
				std::string params1;
				it++;
				for( ; it != params.end() && *it != ";"; it++ )
					params1 += *it + " ";
				if( it != params.end() )
					it++;
				if(!cmd)
					stdoutCLI().writeMsg("wait: cannot execute command " + cmdName + " " + params1);
				if(cmd)
				{
					struct ExecCmd: public Action
					{
						Command * cmd;
						std::string params;
						Result handle()
						{
							cmd->exec( &stdoutCLI(), params );
							return true;
						}
					};
					ExecCmd * execCmd = new ExecCmd();
					execCmd->cmd = cmd;
					execCmd->params = params1;
					//stdoutCLI().writeMsg("wait: executing cmd: " + cmdName + " " + params1);
					mainQueue->push( execCmd );
				}
			}
			return true;
		}
	};
	
	threadPool->start( new WaitThread(params), "Cmd_wait() command thread", true );
}

COMMAND(setViewport, "Set viewport mode", "mode=follow|cycle|freelook|actioncam [wormID] [mode2] [wormID2]", 1, 4);
void Cmd_setViewport::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	
	if(game.state != Game::S_Playing)
	{
		caller->writeMsg("Cannot set viewport while not playing");
		return;
	}
	
	int mode1 = VW_FOLLOW, mode2 = VW_FOLLOW;
	int * modePtr = &mode1;
	for( int idx = 0; (int)params.size() > idx; idx += 2, modePtr = &mode2 )
	{
		if( params[idx] == "follow" )
			*modePtr = VW_FOLLOW;
		else if( params[idx] == "cycle" )
			*modePtr = VW_CYCLE;
		else if( params[idx] == "freelook" )
			*modePtr = VW_FREELOOK;
		else if( params[idx] == "actioncam" )
			*modePtr = VW_ACTIONCAM;
		else
		{
			caller->writeMsg("Invalid mode");
			return;
		}
	}
	CWorm *w1 = NULL;
	CWorm *w2 = NULL;
	CWorm **w = &w1;
	for( int idx = 1; (int)params.size() > idx; idx += 2, w = &w2 )
	{
		int id = atoi(params[idx]);
		if(id < 0 || id >= MAX_WORMS) 
		{
			caller->writeMsg("Faulty worm ID " + itoa(id));
			return;
		}
		*w = game.wormById(id, false);
		if(*w == NULL)
		{
			caller->writeMsg("Worm ID " + itoa(id) + " not used");
			return;
		}
	}

	cClient->SetupViewports(w1, w2, mode1, mode2);
}






// ------------- original dedicated commands starting here --------


COMMAND_EXTRA(quit, "quit game", "", 0, 0, registerCommand("exit", this));
void Cmd_quit::exec(CmdLineIntf* caller, const std::vector<std::string>&) {
	game.state = Game::S_Quit; // quit main-main-loop
}

COMMAND(msg, "print message on stdout", "text", 1, 1);
void Cmd_msg::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	hints << "DedicatedControl: message: " << params[0] << endl;
}

COMMAND(script, "load extern dedicated script", "[script] [args]", 0, 2);
void Cmd_script::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	std::string script = (params.size() > 0) ? params[0] : "";
	std::string args = (params.size() > 1) ? params[1] : "";
	
	if(!DedicatedControl::Get()) {
		caller->writeMsg("Error: can only load extern script in dedicated mode");
		return;
	}
	
	if(IsAbsolutePath(script)) {
		caller->writeMsg("Error: absolute path names are not allowed for script command");
		return;
	}

	if(script.find("..") != std::string::npos) {
		caller->writeMsg("Error: invalid script filename: " + script);
		return;
	}

	DedicatedControl::Get()->ChangeScript(script, args);
}

COMMAND(reinitLua, "reinits the global Lua context. also reloads Lua startup scripts", "", 0, 0);
void Cmd_reinitLua::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	quitLuaGlobal();
	initLuaGlobal();
}

COMMAND(addHuman, "add human player to game", "[profile]", 0, 1);
void Cmd_addHuman::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(bDedicated) {
		caller->writeMsg("cannot add human player in dedicated mode", CNC_WARNING);
		return;
	}
	
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	if( game.localWorms()->size() + 1 >= MAX_WORMS ) {
		caller->writeMsg("Too many worms!");
		return;
	}
	
	// try to find the requested worm
	std::string player = (params.size() > 0) ? params[0] : tLXOptions->sLastSelectedPlayer;
	TrimSpaces(player);
	StripQuotes(player);
	if(player == "") player = MainHumanProfile()->sName;
	
	SmartPointer<profile_t> p = FindProfile(player);
	if(p.get()) {
		if(p->iType != PRF_HUMAN->toInt()) {
			caller->writeMsg("worm " + player + " is not a human player", CNC_WARNING);
			return;
		}
		cClient->AddWorm(p);
		return;
	}
	
	caller->writeMsg("cannot find a profile for worm '" + player + "'"); 
}


COMMAND(addBot, "add bot to game", "[botprofile] [ai-diff] [inGame:*true/false]", 0, 3);
// adds a worm to the game (By string - id is way to complicated)
void Cmd_addBot::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	// Note: this check only for non-dedicated; in ded, we allow it, although it is a bit experimental
	if(!bDedicated && (game.isClient() || !cServer || !cServer->isServerRunning())) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	if( game.localWorms()->size() + 1 >= MAX_WORMS ) {
		caller->writeMsg("Too many worms!");
		return;
	}

	bool outOfGame = false;
	if(params.size() > 2) {
		bool fail = false;
		outOfGame = !from_string<bool>(params[2], fail);
		if(fail) { printUsage(caller); return; }
	}

	int wormAiDiff = -1;
	if(params.size() > 1) {
		bool fail = false;
		wormAiDiff = from_string<int>(params[1], fail);
		if(fail) { printUsage(caller); return; }
		if(wormAiDiff < -1 || wormAiDiff >= 4) {
			caller->writeMsg("only values from 0-3 are allowed for ai-difficulty, -1 is to use profile-default", CNC_WARNING);
			return;
		}
	}
	
	// try to find the requested worm
	if(params.size() > 0 && params[0] != "") {
		std::string localWorm = params[0];
		TrimSpaces(localWorm);
		StripQuotes(localWorm);
		
		SmartPointer<profile_t> p = FindProfile(localWorm);
		if(p.get()) {
			if(p->iType != PRF_COMPUTER->toInt()) {
				caller->writeMsg("worm " + localWorm + " is not a bot", CNC_WARNING);
				return;
			}
			int w = cClient->AddWorm(p, outOfGame);
			if(w >= 0) {
				if(wormAiDiff >= 0) game.wormById(w)->setAiDiff(wormAiDiff);
				caller->pushReturnArg(itoa(w));
			}
			return;
		}
		
		caller->writeMsg("cannot find worm profile " + localWorm + ", using random instead");
	}
	
	std::list<int> worms = cClient->AddRandomBots(1, outOfGame);
	for(std::list<int>::iterator i = worms.begin(); i != worms.end(); ++i) {
		if(wormAiDiff >= 0) game.wormById(*i)->setAiDiff(wormAiDiff);
		caller->pushReturnArg(itoa(*i));
	}
}

COMMAND(addBots, "add bots to game", "number [ai-diff]", 1, 2);
// adds a worm to the game (By string - id is way to complicated)
void Cmd_addBots::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	if( game.localWorms()->size() + 1 >= MAX_WORMS ) {
		caller->writeMsg("Too many worms!");
		return;
	}
	
	bool fail = false;
	int num = from_string<int>(params[0], fail);
	if(fail || num <= 0) {
		printUsage(caller);
		return;
	}

	int wormAiDiff = -1;
	if(params.size() > 1) {
		wormAiDiff = from_string<int>(params[1], fail);
		if(fail) { printUsage(caller); return; }
		if(wormAiDiff < -1 || wormAiDiff >= 4) {
			caller->writeMsg("only values from 0-3 are allowed for ai-difficulty, -1 is to use profile-default", CNC_WARNING);
			return;
		}
	}
	
	std::list<int> worms = cClient->AddRandomBots(num);
	for(std::list<int>::iterator i = worms.begin(); i != worms.end(); ++i) {
		if(wormAiDiff >= 0) game.wormById(*i)->setAiDiff(wormAiDiff);
		caller->pushReturnArg(itoa(*i));
	}
}

COMMAND(kickBot, "kick bot from game", "[reason]", 0, 1);
void Cmd_kickBot::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	std::string reason = (params.size() > 0) ? params[0] : "Dedicated command";
	int worm = cServer->getLastBot();
	if(worm < 0) {
		caller->writeMsg("there is no bot on the server");
		return;
	}
	cServer->kickWorm(worm, reason);
}

COMMAND(kickBots, "kick all bots from game", "[reason]", 0, 1);
void Cmd_kickBots::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	std::string reason = (params.size() > 0) ? params[0] : "Dedicated command";
	std::list<int> worms;
	for_each_iterator(CWorm*, w, game.localWorms())
		if( w->get()->getType() == PRF_COMPUTER )
			worms.push_back(w->get()->getID());
	if(worms.size() == 0)
		caller->writeMsg("there is no bot on the server");
	for(std::list<int>::iterator i = worms.begin(); i != worms.end(); ++i)
		cServer->kickWorm(*i, reason);		
}


COMMAND(killBots, "kill all bots out of game", "", 0, 0);
void Cmd_killBots::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	for_each_iterator(CWorm*, w, game.localWorms())
		if( w->get()->getType() == PRF_COMPUTER )
	{
		w->get()->setLives(0);
		cClient->getNetEngine()->SendDeath(w->get()->getID(), w->get()->getID());
	}
}

COMMAND(kickWorm, "kick worm", "id [reason]", 1, 2);
// Kick and ban will both function using ID
// It's up to the control-program to supply the ID
// - if it sends a string atoi will fail at converting it to something sensible
void Cmd_kickWorm::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	std::string reason = "";
	if (params.size() >= 2)
		reason = params[1];

	if(!CheckWorm(caller, id, name)) return;

	cServer->kickWorm(id,reason);
}

COMMAND(banWorm, "ban worm", "id [reason]", 1, 2);
void Cmd_banWorm::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	std::string reason = "";
	if (params.size() >= 2)
		reason = params[1];
	
	if(!CheckWorm(caller, id, name)) return;

	cServer->banWorm(id,reason);
}

COMMAND(muteWorm, "mute worm", "id", 1, 1);
// TODO: Add name muting, if wanted.
void Cmd_muteWorm::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	if(!CheckWorm(caller, id, name)) return;

	cServer->muteWorm(id);
}

COMMAND(unmuteWorm, "unmute worm", "id", 1, 1);
// TODO: Add name muting, if wanted.
void Cmd_unmuteWorm::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	if(!CheckWorm(caller, id, name)) return;
	
	cServer->unmuteWorm(id);
}


COMMAND(spawnWorm, "spawn worm", "id [pos]", 1, 2);
void Cmd_spawnWorm::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	if(game.state != Game::S_Playing) {
		caller->writeMsg("can only spawn worm when playing");
		return;
	}
	
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) { printUsage(caller); return; }
	CWorm* w = CheckWorm(caller, id, name);
	if(!w) return;
	
	CVec pos;
	bool havePos = false;
	if(params.size() > 1) {
		pos = from_string< VectorD2<int> >(params[1], fail);
		if(fail) { printUsage(caller); return; }
		havePos = true;
	}
	
	cServer->SpawnWorm(w, "Cmd_spawnWorm", havePos ? &pos : NULL);
}

COMMAND(setWormName, "Rename a worm", "id name [notify:*true/false]", 2, 3);
// Note that duplicate names ARE allowed here as opposed to the chat command setmyname.
// Why? We could for example want to have many bots with the same name.
void Cmd_setWormName::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client. Use chat command /setmyname instead", CNC_WARNING);
		return;
	}
	
	// Get the worm id and verify it
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	CWorm* w = CheckWorm(caller, id, name);
	if(!w) return;
	
	// What's the new name?
	std::string name = params[1];
	name = RemoveSpecialChars(name); // Strip unicode characters
	if (name.size() > 32)  // Check if not too long
		name.erase(32, std::string::npos);
	
	// Should we notify?
	bool notify = true;
	if(params.size() == 3) {
		notify = from_string<bool>(params[2], fail);
		if(fail) {
			printUsage(caller);
			return;
		}
	}
	
	// Set the name
	std::string oldname = w->getName();
	w->setName(name);

	// Send the update
	cServer->UpdateWorm(w);

	// Send the notification (eventually)
	if(notify) cServer->SendGlobalText(oldname + " is now known as " + name, TXT_NORMAL);
	notes << "worm rename: " << w->getID() << " : " << oldname << " renamed to " << w->getName() << endl;
}

COMMAND(setWormSkin, "Change the skin of a worm", "id skin", 2, 2);
void Cmd_setWormSkin::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client. Use chat command /setmyskin instead", CNC_WARNING);
		return;
	}
	
	// Get the worm id and verify it
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	CWorm* w = CheckWorm(caller, id, name);
	if(!w) return;
	
	// Set the skin	
	w->setSkin(params[1]);
	
	// Send the update
	cServer->UpdateWorm(w);

	notes << "worm skin change: " << w->getID() << " : " << params[1] << endl;
}

COMMAND(setWormColor, "Change the color of a worm", "id r g b", 4, 4);
void Cmd_setWormColor::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client. Use chat command /setmycolor instead", CNC_WARNING);
		return;
	}
	
	// Get the worm id and verify it
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	CWorm* w = CheckWorm(caller, id, name);
	if(!w) return;
	
	// What's the new color?
	Uint8 r, g, b;
	r = (Uint8) atoi(params[1]);
	g = (Uint8) atoi(params[2]);
	b = (Uint8) atoi(params[3]);
	
	// Set the colour
	w->setColour(Color(r, g, b));
	
	// Send the update
	cServer->UpdateWorm(w);

	notes << "worm color change: " << w->getID() << " : " << params[1] << " " << params[2] << " " << params[3] << endl;
}

COMMAND(setWormLives, "set worm lives", "id (-2: unlimited, -1: outofgame, >=0: lives)", 2, 2);
void Cmd_setWormLives::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) { printUsage(caller); return; }
	CWorm* w = CheckWorm(caller, id, name);
	if(!w) return;
	
	Sint16 lives = from_string<Sint16>(params[1], fail);
	if(fail) { printUsage(caller); return; }
	if(lives < -2) { printUsage(caller); return; }
	
	w->setLives(lives);
	for(int ii = 0; ii < MAX_CLIENTS; ii++) {
		if(cServer->getClients()[ii].getStatus() != NET_CONNECTED) continue;
		if(cServer->getClients()[ii].getNetEngine() == NULL) continue;
		cServer->getClients()[ii].getNetEngine()->SendWormScore( w );
	}	
}


COMMAND(getWormLives, "get worm lives", "id", 1, 1);
void Cmd_getWormLives::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) { printUsage(caller); return; }
	CWorm* w = CheckWorm(caller, id, "getWormLives");
	if(!w) return;
	
	caller->pushReturnArg(itoa(w->getLives()));
}


COMMAND(setWormTeam, "set worm team", "id team", 2, 2);
void Cmd_setWormTeam::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	CWorm *w = CheckWorm(caller, id, name);
	if (!w) return;
	
	int team = from_string<int>(params[1], fail);
	if(fail) {
		printUsage(caller);
		return;
	}

	if( team < 0 || team > 3 ) {
		caller->writeMsg("setWormTeam: invalid team number");
		return;
	}

	w->setTeam(team);
	cServer->UpdateWorm(w);
	// TODO: SendWormLobbyUpdate: is this still needed? which information does it contain which are not in UpdateWorm?
	// Also, if we need it, we should at least set target=w->getClient(). Also, if it is only needed for old clients,
	// we also should send it only to them to save some bandwidth.
	//cServer->SendWormLobbyUpdate();
	cServer->RecheckGame();
}

COMMAND(setWormSpeedFactor, "set worm speedfactor", "id factor", 2, 2);
void Cmd_setWormSpeedFactor::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	if(!CheckWorm(caller, id, name)) return;
	
	float factor = from_string<float>(params[1], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	cServer->SetWormSpeedFactor(id, factor);
}

COMMAND(setWormDamageFactor, "set worm damagefactor", "id factor", 2, 2);
void Cmd_setWormDamageFactor::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	if(!CheckWorm(caller, id, name)) return;
	
	float factor = from_string<float>(params[1], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	cServer->SetWormDamageFactor(id, factor);
}

COMMAND(setWormShieldFactor, "set worm shieldfactor", "id factor", 2, 2);
void Cmd_setWormShieldFactor::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	if(!CheckWorm(caller, id, name)) return;
	
	float factor = from_string<float>(params[1], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	cServer->SetWormShieldFactor(id, factor);
}

COMMAND(setWormCanUseNinja, "(dis)allow worm to use ninja", "id true/false", 2, 2);
void Cmd_setWormCanUseNinja::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	if(!CheckWorm(caller, id, name)) return;
	
	bool canUse = from_string<bool>(params[1], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	cServer->SetWormCanUseNinja(id, canUse);
}

COMMAND(setWormCanAirJump, "enable/disable air jump for worm", "id true/false", 2, 2);
void Cmd_setWormCanAirJump::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}

	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	if(!CheckWorm(caller, id, name)) return;
	
	bool canUse = from_string<bool>(params[1], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	cServer->SetWormCanAirJump(id, canUse);
}

COMMAND(authorizeWorm, "authorize worm", "id", 1, 1);
void Cmd_authorizeWorm::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient()) {
		caller->writeMsg(name + ": cannot do that as client", CNC_WARNING);
		return;
	}
	
	bool fail = true;
	int id = from_string<int>(params[0], fail);
	if(fail) {
		printUsage(caller);
		return;
	}
	
	if(!CheckWorm(caller, id, name)) return;

	cServer->authorizeWorm(id);
}

COMMAND_EXTRA(setVar, "set variable", "variable value", 2, 2, paramCompleters[0] = &autoCompleteVar);
void Cmd_setVar::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	std::string var = params[0];
	std::string value = params[1];

	RegisteredVar* varptr = CScriptableVars::GetVar(var);
	if( varptr == NULL )
	{
		caller->writeMsg("SetVar: no var with name " + var);
		notes << "Available vars:\n" << CScriptableVars::DumpVars() << endl;
		notes << "\nFor Python ded control script:\n" << endl;
		for( CScriptableVars::const_iterator it = CScriptableVars::begin(); it != CScriptableVars::end(); it++ )
		{
			notes << "setvar( \"" << it->first << "\", ";
			notes << it->second.var.toString();
			notes << ")" << endl;
		}
		return;
	}

	ClientRights rights; rights.Everything();
	if(NegResult r = varptr->allowedToAccess(true, rights)) {
		caller->writeMsg("SetVar: " + r.res.humanErrorMsg);
		return;
	}

	CScriptableVars::SetVarByString(varptr->var, value);
	//notes << "DedicatedControl: SetVar " << var << " = " << value << endl;

	cServer->UpdateGameLobby();
}

COMMAND_EXTRA(getVar, "read variable", "variable", 1, 1, paramCompleters[0] = &autoCompleteVar);
void Cmd_getVar::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	std::string var = params[0];
	
	RegisteredVar* varptr = CScriptableVars::GetVar(var);
	if( varptr == NULL ) {
		caller->writeMsg("GetVar: no var with name " + var);
		return;
	}

	ClientRights rights; rights.Everything();
	if(NegResult r = varptr->allowedToAccess(false, rights)) {
		caller->writeMsg("GetVar: " + r.res.humanErrorMsg);
		return;
	}
	
	caller->pushReturnArg(varptr->var.toString());
}

COMMAND(getFullFileName, "get full filename", "relativefilename", 1, 1);
void Cmd_getFullFileName::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->pushReturnArg(Utf8ToSystemNative(GetAbsolutePath(GetFullFileName(params[0], NULL))));
}

COMMAND(getWriteFullFileName, "get writeable full filename", "relativefilename", 1, 1);
void Cmd_getWriteFullFileName::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->pushReturnArg(Utf8ToSystemNative(GetAbsolutePath(GetWriteFullFileName(params[0]))));
}

COMMAND(startLobby, "start server lobby", "[serverport]", 0, 1);
void Cmd_startLobby::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.state != Game::S_Inactive) {
		caller->writeMsg("we cannot start the lobby in current state", CNC_NOTIFY);
		caller->writeMsg("stop game if you want to restart it", CNC_NOTIFY);
		return; // just ignore it and stay in current state
	}
	
	if(cServer && cServer->isServerRunning()) {
		caller->writeMsg("server is already running", CNC_NOTIFY);
		return;
	}
	
	if(game.isClient() && cClient && cClient->getStatus() != NET_DISCONNECTED)  {
		caller->writeMsg("cannot start server lobby as client", CNC_WARNING);
		return;
	}
			
	if( params.size() > 0 ) {
		int port = atoi(params[0]);
		if( port ) tLXOptions->iNetworkPort = port;
	}

	tLXOptions->iMaxPlayers = CLAMP(tLXOptions->iMaxPlayers, 2, (int)MAX_PLAYERS);

	game.startServer(/* localGame */ false);

	cClient->Shutdown();
	cClient->Clear();

	// Start the server
	if(!cServer->StartServer()) {
		// Crappy
		caller->writeMsg("ERROR: Server wouldn't start", CNC_ERROR);
		return;
	}

	// Lets connect me to the server
	if(!cClient->Initialize()) {
		// Crappy
		caller->writeMsg("ERROR: Client wouldn't initialize", CNC_ERROR);
		return;
	}

	cClient->connectInfo = new ClientConnectionRequestInfo;
	if (!bDedicated) {
		cClient->connectInfo->worms.push_back(MainHumanProfile());
	}

	cClient->Connect("127.0.0.1:" + itoa(cServer->getPort()));

	if(gameSettings[FT_Mod].as<ModInfo>()->path == "")
		gameSettings.overwrite[FT_Mod].as<ModInfo>()->path = "MW 1.0";
	{
		ModInfo modInfo = infoForMod(gameSettings[FT_Mod].as<ModInfo>()->path);
		if(!modInfo.valid) {
			caller->writeMsg("no mod for dedicated, " + gameSettings[FT_Mod].as<ModInfo>()->path.get() + " not found");
			// TODO..			
		}
		else
			gameSettings.overwrite[FT_Mod].as<ModInfo>()->name = modInfo.name;
	}

	// Get the game type
	if(gameSettings[FT_GameMode].as<GameModeInfo>()->mode == NULL)
		gameSettings.overwrite[FT_GameMode].as<GameModeInfo>()->mode = GameMode(GM_DEATHMATCH);

	if(gameSettings[FT_Map].as<LevelInfo>()->path == "")
		gameSettings.overwrite[FT_Map].as<LevelInfo>()->path = "CastleStrike.lxl";
	gameSettings.overwrite[FT_Map].as<LevelInfo>()->name = CMap::GetLevelName(gameSettings[FT_Map].as<LevelInfo>()->path);

	if(DedicatedControl::Get())
		DedicatedControl::Get()->LobbyStarted_Signal();

	// load the menu
	if(!bDedicated) {
		DeprecatedGUI::Menu_Current_Shutdown();
		
		// goto the joining dialog
		DeprecatedGUI::Menu_SetSkipStart(true);
		DeprecatedGUI::Menu_NetInitialize(false);
		DeprecatedGUI::Menu_Net_HostGotoLobby();
	}
}

COMMAND(startGame, "start game", "", 0, 0);
void Cmd_startGame::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.state == Game::S_Inactive) {
		caller->pushReturnArg("game state is inactive, please start lobby first");
		return;
	}

	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->pushReturnArg("cannot start game as client");
		return;
	}
	
	if(game.worms()->size() <= 1 && !gameSettings[FT_AllowEmptyGames]) {
		caller->pushReturnArg("cannot start game, too few players");
		return;
	}
	// Start the game
	game.startGame();
}

COMMAND(restartGame, "restart game", "", 0, 0);
void Cmd_restartGame::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.state == Game::S_Inactive) {
		caller->pushReturnArg("game state is inactive, please start lobby first");
		return;
	}

	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->pushReturnArg("cannot start game as client");
		return;
	}

	if(game.worms()->size() <= 1 && !gameSettings[FT_AllowEmptyGames]) {
		caller->pushReturnArg("cannot start game, too few players");
		return;
	}

	game.restartGame();
}


COMMAND_EXTRA(map, "set map", "filename", 1, 1, paramCompleters[0] = &autoCompleteForFileListCache<mapList>);
void Cmd_map::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg("cannot set map as client");
		return;
	}
	
	if(game.state != Game::S_Lobby) {
		caller->writeMsg("can only set map in lobby");
		return;
	}
		
	std::string filename = params[0];
	if(filename == "") {
		caller->writeMsg("specify map filename");
		return;
	}
	
	if(!mapList->includes(filename)) {
		caller->writeMsg("map '" + filename + "' not found", CNC_WARNING);
		return;
	}
		
	gameSettings.overwrite[FT_Map].as<LevelInfo>()->path = filename;
	if(!bDedicated)
		DeprecatedGUI::Menu_Net_HostLobbySetLevel(filename);
	cServer->UpdateGameLobby();
}

COMMAND_EXTRA(mod, "set mod", "filename", 1, 1, paramCompleters[0] = &autoCompleteForFileListCache<modList>);
void Cmd_mod::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg("cannot set mod as client");
		return;
	}
	
	if(game.state != Game::S_Lobby) {
		caller->writeMsg("can only set mod in lobby");
		return;
	}
	
	std::string filename = params[0];
	if(filename == "") {
		caller->writeMsg("specify mod filename");
		return;
	}

	if(!modList->includes(filename)) {
		caller->writeMsg("mod '" + filename + "' not found", CNC_WARNING);
		return;
	}
	
	gameSettings.overwrite[FT_Mod].as<ModInfo>()->path = filename;
	if(!bDedicated)
		DeprecatedGUI::Menu_Net_HostLobbySetMod(filename);
	cServer->UpdateGameLobby();
}

COMMAND(listMaps, "list all available maps", "", 0, 0);
void Cmd_listMaps::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	for(FileListCacheIntf::Iterator::Ref i = mapList->begin(); i->isValid(); i->next())
		caller->pushReturnArg(i->get().first);
}

COMMAND(listMods, "list all available mods", "", 0, 0);
void Cmd_listMods::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	for(FileListCacheIntf::Iterator::Ref i = modList->begin(); i->isValid(); i->next())
		caller->pushReturnArg(i->get().first);
}

COMMAND(gotoLobby, "go to lobby", "", 0, 0);
void Cmd_gotoLobby::exec(CmdLineIntf* caller, const std::vector<std::string>&) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) {
		caller->writeMsg("cannot goto lobby as client");
		return;
	}

	if(game.isLocalGame()) {
		// for local games, we just stop the server and go to local menu
		hints << "Cmd_gotoLobby: we are quitting server and going to local menu" << endl;
		GotoLocalMenu();
		return;
	}
	
	if(game.state == Game::S_Lobby) {
		caller->writeMsg("we are already in lobby");
		return;
	}

	game.gotoLobby("Cmd_gotoLobby");
}

COMMAND(chatMsg, "give a global chat message", "text", 1, 1);
void Cmd_chatMsg::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning())
	{
		if( cClient && (cClient->getStatus() == NET_CONNECTED) )
			cClient->getNetEngine()->SendText( params[0], game.localWorms()->tryGet() ? game.localWorms()->tryGet()->getName() : "" );
		else
		{
			caller->writeMsg("We are not running server and not connected to a server, cannot send msg");
			//hints << "Cmd_chatMsg::exec(): cClient " << cClient << " cClient->getStatus() " << NetStateString((ClientNetState)cClient->getStatus()) << endl;
		}
		return;
	}
	
	std::string msg = params[0];
	int type = TXT_NOTICE; // TODO: make variable
	cServer->SendGlobalText(msg, type);
}

COMMAND(privateMsg, "give a private message to a worm", "id text", 2, 2);
void Cmd_privateMsg::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;

	if( !w->getClient() || !w->getClient()->getNetEngine() ) {
		caller->writeMsg("worm " + itoa(w->getID()) + " is somehow crippled");
		return;
	}

	int type = TXT_NOTICE; // TODO: make variable
	w->getClient()->getNetEngine()->SendText(params[1], type);
}

COMMAND(getWormList, "get worm list", "", 0, 0);
void Cmd_getWormList::exec(CmdLineIntf* caller, const std::vector<std::string>& params)
{
	if(game.isClient() || !cServer || !cServer->isServerRunning()) { caller->writeMsg(name + " works only as server"); return; }
	
	for_each_iterator(CWorm*, w, game.worms())
		caller->pushReturnArg(itoa(w->get()->getID()));
}

COMMAND(getComputerWormList, "get computer worm list", "", 0, 0);
void Cmd_getComputerWormList::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) { caller->writeMsg(name + " works only as server"); return; }
	
	for_each_iterator(CWorm*, w, game.worms())
		if(w->get()->getType() == PRF_COMPUTER)
			caller->pushReturnArg(itoa(w->get()->getID()));
}

COMMAND(getWormName, "get worm name", "id", 1, 1);
void Cmd_getWormName::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {	
	CWorm* w = getWorm(caller, params[0]); if(!w) return;
	caller->pushReturnArg(w->getName());
}

COMMAND(getWormTeam, "get worm team", "id", 1, 1);
void Cmd_getWormTeam::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;
	caller->pushReturnArg(itoa(w->getTeam()));
}

COMMAND(getWormScore, "get worm score", "id", 1, 1);
void Cmd_getWormScore::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;
	caller->pushReturnArg(itoa(w->getLives()));
	caller->pushReturnArg(itoa(w->getScore()));
	caller->pushReturnArg(ftoa(w->getDamage()));
	caller->pushReturnArg(w->getAlive() ? ftoa(w->getHealth()) : "-1");
}

COMMAND(getWormIp, "get worm IP", "id", 1, 1);
void Cmd_getWormIp::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;
	
	// TODO: Perhaps we can cut out the second argument for the signal- but that would lead to the signal being much larger. Is it worth it?
	std::string str_addr;
	if(w->getClient() && w->getClient()->getChannel())
		NetAddrToString(w->getClient()->getChannel()->getAddress(), str_addr);
	if (str_addr != "")
		caller->pushReturnArg(str_addr);
	else
		caller->writeMsg("GetWormIp: str_addr == \"\"");
}

COMMAND(getWormLocationInfo, "get worm location info", "id", 1, 1);
void Cmd_getWormLocationInfo::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;

	std::string str_addr;
	NetAddrToString(w->getClient()->getChannel()->getAddress(), str_addr);
	if (str_addr != "")
	{
		IpInfo info = tIpToCountryDB->GetInfoAboutIP(str_addr);
		caller->pushReturnArg(info.continent);
		caller->pushReturnArg(info.countryName);
		caller->pushReturnArg(info.countryCode);
	}
	else
	{
		caller->writeMsg("GetWormCountryInfo: str_addr == \"\"");
	}
}


COMMAND(getWormPing, "get worm ping", "id", 1, 1);
void Cmd_getWormPing::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;
	
	if(!w->getClient() || !w->getClient()->getChannel()) {
		caller->writeMsg("worm " + itoa(w->getID()) + " has a crippled connection");
		return;
	}

	caller->pushReturnArg(itoa(w->getClient()->getChannel()->getPing()));
}

COMMAND(getWormSkin, "get worm skin", "id", 1, 1);
void Cmd_getWormSkin::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;
	caller->pushReturnArg(itoa(w->getSkin().getDefaultColor().getDefault()));
	caller->pushReturnArg(w->getSkin().getFileName());
}

COMMAND(getWormPos, "get worm position", "id", 1, 1);
void Cmd_getWormPos::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;	
	caller->pushReturnArg(ftoa(w->getPos().x));
	caller->pushReturnArg(ftoa(w->getPos().y));
}

COMMAND(getWormVelocity, "get worm velocity", "id", 1, 1);
void Cmd_getWormVelocity::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;	
	caller->pushReturnArg(ftoa(w->velocity().get().x));
	caller->pushReturnArg(ftoa(w->velocity().get().y));
}

COMMAND(getWormHealth, "get worm health", "id", 1, 1);
void Cmd_getWormHealth::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;	
	caller->pushReturnArg(ftoa(w->getHealth()));
}

COMMAND(getWormProps, "get worm properties", "id", 1, 1);
void Cmd_getWormProps::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;
	caller->pushReturnArg("SpeedFactor: " + ftoa(w->speedFactor()));
	caller->pushReturnArg("DamageFactor: " + ftoa(w->damageFactor()));
	caller->pushReturnArg("ShieldFactor: " + ftoa(w->shieldFactor()));	
	caller->pushReturnArg("CanAirJump: " + to_string<bool>(w->canAirJump()));
	caller->pushReturnArg("CanUseNinja: " + to_string<bool>(w->canUseNinja()));
}

COMMAND(selectWeapons, "select weapons for worm", "id", 1, 1);
void Cmd_selectWeapons::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient()) {
		caller->writeMsg("selectWeapons only works as server", CNC_WARNING);
		return;
	}
	
	CWorm* w = getWorm(caller, params[0]); if(!w) return;
	
	// NOTE: random weapons for host worms (in case bSameWeaponsAsHostWorm) are handled in local client
	if(!gameSettings[FT_SameWeaponsAsHostWorm] && gameSettings[FT_ForceRandomWeapons]) {
		w->GetRandomWeapons();
		cServer->SendWeapons(NULL, w);
		return;
	}
	
	if(!w->isFirstLocalHostWorm() && cServer->serverChoosesWeapons()) {
		caller->writeMsg("unhandled case for server chooses weapons", CNC_ERROR);
		return;
	}
	
	if(w->getClient() == NULL) {
		caller->writeMsg("connection of worm unset", CNC_ERROR);
		return;
	}
	
	w->bWeaponsReady = false;
	w->getClient()->setGameReady(false);
	w->getClient()->getNetEngine()->SendSelectWeapons(w);
}

COMMAND(whoIs, "get some info about a connection", "worm-id", 1, 1);
void Cmd_whoIs::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CWorm* w = getWorm(caller, params[0]); if(!w) return;	
	caller->pushReturnArg("ID/Name: " + itoa(w->getID()) + ":" + w->getName());
	if(cClient && cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode && cClient->getGameLobby()[FT_GameMode].as<GameModeInfo>()->mode->GameTeams() > 1)
		caller->pushReturnArg("Team: " + itoa(w->getTeam()));
	if(w->getClient()) {
		caller->pushReturnArg("Address: " + w->getClient()->getAddrAsString() + " (" + w->getClient()->ipInfo().countryName + ")");
		caller->pushReturnArg("Version: " + w->getClient()->getClientVersion().asString());
		caller->pushReturnArg("ConnectTime: " + ftoa((tLX->currentTime - w->getClient()->getConnectTime()).seconds()) + " secs");
		caller->pushReturnArg("NetSpeed: " + NetworkSpeedString((NetworkSpeed)w->getClient()->getNetSpeed()));
		caller->pushReturnArg("Ping: " + itoa(w->getClient()->getPing()));
		caller->pushReturnArg("LastResponse: " + ftoa((tLX->currentTime - w->getClient()->getLastReceived()).seconds()) + " secs ago");
		if(game.state != Game::S_Lobby) {
			caller->pushReturnArg("IsReady: " + to_string(w->getClient()->getGameReady()));
		}
	} else
		caller->pushReturnArg("Client is INVALID");

	if(game.state != Game::S_Lobby) {
		caller->pushReturnArg("IsAlive: " + to_string(w->getAlive()));
	}
}


COMMAND(mapInfo, "get map info", "", 0, 0);
void Cmd_mapInfo::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	CMap* m = game.gameMap();
	if(!m)
		caller->writeMsg("map not loaded", CNC_ERROR);
	else {
		caller->pushReturnArg(m->getName());
		caller->pushReturnArg(m->getFilename());
		caller->pushReturnArg(itoa(m->GetWidth()));
		caller->pushReturnArg(itoa(m->GetHeight()));
	}
}

COMMAND(findSpot, "find randm free spot in map (close to pos)", "[(x,y)]", 0, 1);
void Cmd_findSpot::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.isClient() || !cServer || !cServer->isServerRunning()) { caller->writeMsg(name + " works only as server"); return; }
	if(game.gameMap() == NULL) {
		caller->writeMsg("map not loaded", CNC_ERROR);
		return;
	}
	
	VectorD2<int> v;
	if(params.size() == 0)
		v = game.gameMap()->FindSpot();
	else {
		bool fail = false;
		VectorD2<int> closev = from_string< VectorD2<int> >(params[0], fail);
		if(fail) {
			printUsage(caller);
			return;
		}
		v = game.gameMap()->FindSpotCloseToPos(closev);
	}
	
	caller->pushReturnArg(itoa(v.x));
	caller->pushReturnArg(itoa(v.y));
}


COMMAND(getGameState, "get game state", "", 0, 0);
void Cmd_getGameState::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->pushReturnArg(Game::StateAsStr(game.state));
	caller->pushReturnArg(to_string(game.gameOver));
	caller->pushReturnArg(to_string(game.isServer()));
	caller->pushReturnArg(to_string(game.isLocalGame()));
}


COMMAND(dumpGameState, "dump game state", "", 0, 0);
void Cmd_dumpGameState::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->writeMsg("GameState: " + Game::StateAsStr(game.state), CNC_DEV);
	if(game.state == Game::S_Inactive) return;
	if(game.isServer() && cServer) cServer->DumpGameState(caller);
	else if(cClient) cClient->DumpGameState(caller);
	else caller->writeMsg("server nor client correctly initialised", CNC_ERROR);
}

COMMAND(dumpSysState, "dump system state", "", 0, 0);
void Cmd_dumpSysState::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	hints << "Threads:" << endl;
	threadPool->dumpState(stdoutCLI());
	hints << "Tasks:" << endl;
	taskManager->dumpState(stdoutCLI());
	hints << "Free system memory: " << (GetFreeSysMemory() / 1024) << " KB" << endl;
	hints << "Cache size: " << (cCache.GetCacheSize() / 1024) << " KB" << endl;
	hints << "Current time: " << GetDateTimeText() << endl;
}

#ifdef DEBUG
COMMAND(createDummyTask, "create dummy task", "[global queue]", 0, 1);
void Cmd_createDummyTask::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	TaskManager::QueueType queue = TaskManager::QT_QueueToSameTypeAndBreakCurrent;
	if(params.size() > 0 && from_string<bool>(params[0]))
		queue = TaskManager::QT_GlobalQueue;
	
	struct DummyTask : Task {
		int counter;
		DummyTask() : counter(30) { name = "dummy task"; }
		Result handle() {
			while(counter > 0) {
				SDL_Delay(1000);
				if(breakSignal) return "break";
				Mutex::ScopedLock lock(*mutex);
				counter--;
			}
			return true;
		}
		std::string statusText() {
			Mutex::ScopedLock lock(*mutex);
			return "Dummy task, " + itoa(counter) + " ...";
		}
	};
	taskManager->start(new DummyTask(), queue);
}
#endif

COMMAND(dumpConnections, "dump connections of server", "", 0, 0);
void Cmd_dumpConnections::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(cServer) cServer->DumpConnections();
	else caller->writeMsg("server not initialised");
}

COMMAND(dumpGameSettings, "dump game settings (all layers)", "", 0, 0);
void Cmd_dumpGameSettings::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	gameSettings.dumpAllLayers();
}

#ifdef MEMSTATS
COMMAND(printMemStats, "print memory stats", "", 0, 0);
void Cmd_printMemStats::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	printMemStats();
}
#endif

COMMAND(updateServerList, "update server list", "", 0, 0);
void Cmd_updateServerList::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	ServerList::get()->updateList();
}

COMMAND(isUpdatingServerList, "is server list still updating", "", 0, 0);
void Cmd_isUpdatingServerList::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	caller->pushReturnArg(to_string(ServerList::get()->isProcessing()));
}

struct PushArg : public ServerList::Action  {
	CmdLineIntf *m_caller;

	PushArg(CmdLineIntf *c) : m_caller(c) {}

	bool handleConst(const server_t::Ptr& s)
	{
		std::string addr = Replace(s->szAddress, ",", "");
		m_caller->pushReturnArg(addr + "," + s->szName);
		return true;
	}
};

COMMAND(getServerList, "get server list", "", 0, 0);
void Cmd_getServerList::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	PushArg argPusher(caller);
	ServerList::get()->eachConst(argPusher);
}

COMMAND(debugFindProblems, "do some system checks and print problems - no output means everything seems ok", "", 0, 0);
void Cmd_debugFindProblems::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(game.state >= Game::S_Preparing) { // game is running
		if(cClient == NULL)
			warnings << "game is running but cClient == NULL" << endl;
		else {
			for_each_iterator(CWorm*, w_, game.worms()) {
				CWorm& w = *w_->get();
				std::string name = itoa(w.getID()) + ":" + w.getName();

				if(w.getLocal() && w.inputHandler() == NULL)
					warnings << "local worm " << name << " does not have input handler set" << endl;
				
				if(gusGame.isEngineNeeded()) {
					if(w.getOwner() == NULL)
						warnings << "worm " << name << " has no owner player set" << endl;
					
					if(!game.isLocalGame()) {
						if(w.getNode() == NULL)
							warnings << "worm " << name << " has no network node" << endl;
						else {
							if(w.getLocal() && !w.getNode()->areWeOwner())
								warnings << "worm " << name << " is local but network node is not owner" << endl;
							else if(!w.getLocal() && w.getNode()->areWeOwner())
								warnings << "worm " << name << " is not local but network node is owner" << endl;
						}
					}
				}
			}
		}
		
		foreach(pi, game.players) {
			if(*pi == NULL) {
				warnings << "entry in game.players with player NULL" << endl;
				continue;
			}
			CWormInputHandler& p = **pi;
			
			std::string name = p.name() + " of worm " + (p.worm() ? (itoa(p.worm()->getID()) + ":" + p.worm()->getName()) : "UNSET");
			if(p.worm() == NULL)
				warnings << name << " has no worm set" << endl;
			else {
				if(p.worm()->getOwner() != &p)
					warnings << p.name() << "'s worm " << p.worm()->getID() << ":" << p.worm()->getName() << " has different player set" << endl;

				if(!game.isLocalGame()) {
					if(p.getNode() == NULL)
						warnings << name << " has no network node" << endl;
					else {
						if(p.worm()->getLocal() && !p.getNode()->areWeOwner())
							warnings << name << " is local but network node is not owner" << endl;
						else if(!p.worm()->getLocal() && p.getNode()->areWeOwner())
							warnings << name << " is not local but network node is owner" << endl;
					}
				}
			}
		}
	}	
}

COMMAND(exportLevel, "export current level data. or from the specified level", "exportlevelname [level]", 1, 2);
void Cmd_exportLevel::exec(CmdLineIntf *caller, const std::vector<std::string>& params) {
#ifndef DEDICATED_ONLY
	CMap* m = NULL;
	SmartPointer<CMap> tmpMap;
	if(params.size() >= 2) {
		tmpMap = new CMap();
		if(!tmpMap->Load("levels/" + params[1]))
			return caller->writeMsg("failed to load level " + params[1]);
		m = tmpMap.get();
	}
	else
		m = game.gameMap();
	if(!m || !m->isLoaded()) {
		caller->writeMsg("map is not loaded");
		return;
	}

	std::string filename = "levels/" + params[0];
	if(IsFileAvailable(filename, false, false)) {
		caller->writeMsg("level does already exist, choose another levelname");
		return;
	}

	SaveSurface(m->material->surf, filename + "/material.png", FMT_PNG, "");
	SaveSurface(m->bmpDrawImage, filename + "/level.png", FMT_PNG, "");
	SaveSurface(m->bmpBackImageHiRes, filename + "/background.png", FMT_PNG, "");
	if(m->bmpParallax.get())
		SaveSurface(m->bmpParallax, filename + "/paralax.png", FMT_PNG, ""); // keep typo for historical reasons
	if(m->bmpForeground.get())
		SaveSurface(m->bmpForeground, filename + "/foreground.png", FMT_PNG, "");
	if(m->lightmap)
		SaveSurface(m->lightmap->surf, filename + "/lightmap.png", FMT_PNG, "");

	{
		std::ofstream f;
		if(!OpenGameFileW(f, filename + "/config.cfg")) {
			errors << "error opening " << filename << "/config.cfg" << endl;
			return;
		}

		f << "double_res = 1" << endl;
		if(m->config()->darkMode)
			f << "dark_mode = 1" << endl;

		f.close();
	}
#endif // #ifndef DEDICATED_ONLY
}


COMMAND(saveConfig, "save current config", "[filename]", 0, 1);
void Cmd_saveConfig::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(tLXOptions) {
		if(params.size() == 0)
			tLXOptions->SaveToDisc();
		else
			tLXOptions->SaveToDisc("cfg/" + params[0]);
	}
	else
		caller->writeMsg("options structure not initialised", CNC_ERROR);
}

COMMAND(saveConfigSection, "save a specific config section", "section filename", 2, 2);
void Cmd_saveConfigSection::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(tLXOptions) {
		std::string section = params[0];
		if(section == "") {
			printUsage(caller);
			return;
		}
		if(section[section.size()-1] == '.') {
			caller->writeMsg("section must not end with a '.'");
			return;
		}
		if(!CScriptableVars::haveSomethingWith(section + ".")) {
			caller->writeMsg("section '" + section + "' is unknown");
			return;
		}
		tLXOptions->SaveSectionToDisc(section, "cfg/" + params[1]);
	}
	else
		caller->writeMsg("options structure not initialised", CNC_ERROR);
}

COMMAND(loadConfig, "load config file", "[filename]", 0, 1);
void Cmd_loadConfig::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(tLXOptions) {
		if(params.size() == 0)
			tLXOptions->LoadFromDisc();
		else
			tLXOptions->LoadFromDisc("cfg/" + params[0]);
	}
	else
		caller->writeMsg("options structure not initialised", CNC_ERROR);	
}

// Note: It doesn't make sense to send custom signal without args, so force at least 1 arg.
COMMAND(signal, "send custom signal to dedicated script", "<args>", 1, UINT_MAX);
void Cmd_signal::exec(CmdLineIntf* caller, const std::vector<std::string>& params) {
	if(!DedicatedControl::Get()) {
		caller->writeMsg("dedicated control not available", CNC_ERROR);
		return;
	}
	
	DedicatedControl::Get()->Custom_Signal( std::list<std::string>(params.begin(), params.end()) );
}

COMMAND(lua, "execute Lua command", "<cmd>", 1, 1);
void Cmd_lua::exec(CmdLineIntf *caller, const std::vector<std::string>& params) {
	int r = luaGlobal.execCode(params[0], *caller);
	for(int i = r; i > 0; --i)
		caller->pushReturnArg(luaGlobal.convert_tostring(-i));
	luaGlobal.pop(r);
}


static void HandleCommand(const CmdLineIntf::Command& command) {
	assert(isGameloopThread());

	std::string cmdstr = command.cmd; TrimSpaces(cmdstr);
	std::string params;
	size_t f = cmdstr.find(' ');
	if(f != std::string::npos) {
		params = cmdstr.substr(f + 1);
		TrimSpaces(params);
		cmdstr = cmdstr.substr(0, f);
	}
	if(cmdstr == "") return;
	
	CommandMap::iterator cmd = commands.find(cmdstr);
	
	if(cmd != commands.end()) {
		cmd->second->exec(command.sender, params);
	}
	// we must handle this command seperate
	else if( stringcaseequal(cmdstr, "nextsignal") ) {
		if(DedicatedControl::Get()) {
			if(!DedicatedControl::Get()->GetNextSignal(command.sender)) return;
		}
		else
			command.sender->writeMsg("nextsignal is only available in dedicated mode");
	}
	else {
		command.sender->writeMsg("unknown command: " + cmdstr + " " + params);
	}
	
	command.sender->finalizeReturn();
}


struct CmdQueue {
	
	SDL_mutex* pendingCommandsMutex;
	std::list<CmdLineIntf::Command> pendingCommands;
	
	CmdQueue() {
		pendingCommandsMutex = SDL_CreateMutex();
	}
	
	~CmdQueue() {
		SDL_DestroyMutex(pendingCommandsMutex);
		pendingCommandsMutex = NULL;
	}

};

static CmdQueue cmdQueue;


void Execute(const CmdLineIntf::Command& command) {
	ScopedLock lock(cmdQueue.pendingCommandsMutex);
	cmdQueue.pendingCommands.push_back(command);
}

bool havePendingCommands() {
	ScopedLock lock(cmdQueue.pendingCommandsMutex);
	return !cmdQueue.pendingCommands.empty();
}

void HandlePendingCommands() {
	while(true) {
		CmdLineIntf::Command command;
		{
			ScopedLock lock(cmdQueue.pendingCommandsMutex);
			if(cmdQueue.pendingCommands.size() > 0) {
				command = cmdQueue.pendingCommands.front();
				cmdQueue.pendingCommands.pop_front();
			}
			else
				break;
		}

		if(command.execScope.get())
			command.execScope->execNow(command);
		HandleCommand(command);
		command.sender->finishedCommand(command.cmd);
		command.execScope = NULL;

		if(game.state.ext.updated)
			// Next frame, we will continue with other pending commands.
			// For now, skip the rest because we might have some other work to do.
			break;
	}
}

std::vector<std::string> Execute_Here(const std::string& cmd) {
	if(!isGameloopThread()) {
		errors << "cannot Execute_Here(" + cmd + "): we are not in the gameloop thread" << endl;
		return std::vector<std::string>();
	}

	struct DirectCliWrapper : CmdLineIntf {
		std::vector<std::string> returns;
		
		virtual void pushReturnArg(const std::string& str) {
			returns.push_back(str);
		}

		virtual void finalizeReturn() {}

		virtual void writeMsg(const std::string& msg, CmdLineMsgType type = CNC_NORMAL) {
			stdoutCLI().writeMsg("Execute_Here: " + msg, type);
		}
		
	}
	directCli;
	
	HandleCommand(CmdLineIntf::Command(&directCli, NULL, cmd));
	return directCli.returns;
}


///////////////////
// Auto complete a command
bool AutoComplete(const std::string& text, size_t pos, CmdLineIntf& cli, AutocompletionInfo& autocomplete)
{
	struct Finalizer {
		AutocompletionInfo& autocomplete;
		Finalizer(AutocompletionInfo& i) : autocomplete(i) {}
		~Finalizer() { autocomplete.finalize(); }
	}
	finalizer(autocomplete);
	
	ParamSeps seps = ParseParams_Seps(text);
	if(seps.size() == 0) {
		// no text at all
		return false;
	}
	
	ParamSeps::value_type firstSep = *seps.begin();
	if(pos < firstSep.first) {
		// before any txt
		return false;
	}
 
	ParamSeps::iterator it = seps.lower_bound(pos);
	if(it == seps.end() || pos != it->first) --it;

	Command* cmd = NULL;
	if(it != seps.begin() || pos > it->first + it->second) {
		// it means that the command is already complete, or at least should be
		std::string cmdStr = text.substr(firstSep.first, firstSep.second);
		cmd = Cmd_GetCommand(cmdStr);
		if(!cmd) {
			cli.writeMsg("command unknown", CNC_WARNING);
			return false;
		}
	}
		
	unsigned int paramIndex = 0;
	for(ParamSeps::iterator j = seps.begin(); j != it; ++j, ++paramIndex) {}

	if(pos > it->first + it->second) {
		// we are between two parameters or at the very end
		if(cmd) {
			// it is the correct corresponding param to pos
			AutocompleteRequest request = {
				cli, autocomplete, AutocompletionInfo::InputState(text, pos),
				text.substr(0, pos), text.substr(pos),
				"", 0
			};

			// Note: just use paramIndex as it is because we want to point to next param
			return cmd->completeParam(paramIndex, request);
		}
		else
			cli.writeMsg("command unknown", CNC_WARNING);
		
		return false;
	}

	// it is the correct corresponding param to pos
	AutocompleteRequest request = {
		cli, autocomplete, AutocompletionInfo::InputState(text, pos),
			text.substr(0, it->first), text.substr(it->first + it->second),
			text.substr(it->first, it->second), pos - it->first
	};
	
	if(it == seps.begin())
		return autoCompleteCommand(NULL, request);

	if(!cmd) {
		cli.writeMsg("command unknown", CNC_WARNING);
		return false;
	}

	if(paramIndex > 0) paramIndex--;
	return cmd->completeParam(paramIndex, request);
}

