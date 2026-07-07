/////////////////////////////////////////
//
//         OpenLieroX
//
// based on sources for Carnage Marines
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Console source code
// Created 7/4/02
// Jason Boettcher

#include <list>
#include "ThreadPool.h"
#include "LieroX.h"
#include "Debug.h"
#include "Clipboard.h"
#include "AuxLib.h"
#include "OLXConsole.h"
#include "GfxPrimitives.h"
#include "InputEvents.h"
#include "StringUtils.h"
#include "Options.h"
#include "MathLib.h"
#include "ReadWriteLock.h"
#include "Timer.h"
#include "Mutex.h"
#include "Condition.h"
#include "PreInitVar.h"
#include "Autocompletion.h"
#include "OLXCommand.h"
#include "FindFile.h"
#include "util/macros.h"




// Console states
#define		CON_HIDDEN		0
#define		CON_DROPPING	1
#define		CON_DOWN		2
#define		CON_HIDING		3

#define		MAX_CONLENGTH	256
#define		MAX_CONLINES	15
#define		MAX_CONHISTORY	100


struct conline_t {
	int			Colour;
	std::string	strText;
};



struct console_t {
		
	int			iState;
	float		fPosition;
	UnicodeChar	iLastchar;
	
	size_t		iCurpos;
	conline_t	Line[MAX_CONLINES];
	
	bool		bBlinkState; // true = displayed
	AbsTime		fBlinkTime;
	
	SmartPointer<SDL_Surface> bmpConPic;
	
};



static void saveHistory();

struct IngameConsole : CmdLineIntf {
	SmartPointer<ThreadPoolItem> thread;
	Mutex mutex;
	Condition changeSignal;
	PIVar(bool,false) quit;
	
	std::list<KeyboardEvent> keyQueue;
	AutocompletionInfo::InputState input;
	Mutex inputMutex;
	
	typedef std::list<std::string> History;
	History history;
	std::string backupInputBuffer;
	History::iterator historyPos;
	
	void pushKey(const KeyboardEvent& input) {
		Mutex::ScopedLock lock(mutex);
		keyQueue.push_back(input);
		changeSignal.broadcast();
	}

	void addHistoryEntry(const std::string& text) {
		// no need to lock because we only access it in handleKey()
		for(History::iterator i = history.begin(); i != history.end();) {
			std::string buf = *i; TrimSpaces(buf);
			if(*i == text || buf == "") i = history.erase(i);
			else ++i;
		}
		history.push_back(text);
		if(history.size() > MAX_CONHISTORY)
			history.pop_front();
		invalidateHistoryPos();
	}

	void invalidateHistoryPos() {
		historyPos = history.end();
	}
	
	void handleKey(const KeyboardEvent& queue);
	
	void handleKeys(std::list<KeyboardEvent>& queue) {
		while(queue.size() > 0) {
			KeyboardEvent ev = queue.front();
			queue.pop_front();
			handleKey(ev);
		}
	}
	
	Result handler() {
		Mutex::ScopedLock lock(mutex);
		while(!quit) {
			{
				std::list<KeyboardEvent> queue; queue.swap(keyQueue);
				mutex.unlock();
				handleKeys(queue);
				mutex.lock();
			}

			if(keyQueue.size() > 0) continue;
			changeSignal.wait(mutex);
		}
		return true;
	}

	// -------- CmdLineIntf ---------
	
	virtual void pushReturnArg(const std::string& str) {
		Con_AddText(CNC_NOTIFY, ":- " + str, false);
	}
	
	virtual void finalizeReturn() {
		// Place is too short for this.
		//Con_AddText(CNC_NOTIFY, ":.", false);
	}
	
	virtual void writeMsg(const std::string& msg, CmdLineMsgType type) {
		Con_AddText(int(type), msg, false);
	}

	// ------------------------------
	
	IngameConsole() : thread(NULL) {
		invalidateHistoryPos();
	}
	
	~IngameConsole() {
		stopThread();
	}

	void stopThread() {
		if(thread.get()) {
			{
				Mutex::ScopedLock lock(mutex);
				quit = true;
				changeSignal.broadcast();
			}
			threadPool->wait(thread);
			thread = NULL;
		}
	}
	
	void startThread() {
		stopThread();
		quit = false;
		input.text = "";
		input.pos = 0;
		keyQueue.clear();
		thread = StartMemberFuncInThread(IngameConsole, IngameConsole::handler, "IngameConsole handler");
	}
};

static IngameConsole ingameConsole;


void IngameConsole::handleKey(const KeyboardEvent& ev) {
	if(!ev.down) return;

	// Backspace
	if(ev.sym == SDLK_BACKSPACE) {
		if(input.pos > 0) {
			Mutex::ScopedLock lock(inputMutex);

			input.pos = CLAMP(input.pos, size_t(0), input.text.size()); // safty
			const_string_iterator newpos(input.text, input.pos);
			DecUtf8StringIterator(newpos, const_string_iterator(input.text, 0));

			input.text.erase(newpos.pos, input.pos - newpos.pos);
			input.pos = newpos.pos;
		}
		goto finalHandleKey;
	}
	
	// Delete
	if(ev.sym == SDLK_DELETE)  {
		if(input.text.size() > 0) {
			Mutex::ScopedLock lock(inputMutex);

			input.pos = CLAMP(input.pos, size_t(0), input.text.size()); // safty
			const_string_iterator nextpos(input.text, input.pos);
			IncUtf8StringIterator(nextpos, const_string_iterator(input.text, input.text.size()));

			input.text.erase(input.pos, nextpos.pos - input.pos);
		}
		goto finalHandleKey;
	}
	
	// Left arrow
	if(ev.sym == SDLK_LEFT)  {
		if(input.pos > 0) {
			Mutex::ScopedLock lock(inputMutex);

			input.pos = CLAMP(input.pos, size_t(0), input.text.size()); // safty
			const_string_iterator newpos(input.text, input.pos);
			DecUtf8StringIterator(newpos, const_string_iterator(input.text, 0));

			input.pos = newpos.pos;
		}
		goto finalHandleKey;
	}
	
	// Right arrow
	if(ev.sym == SDLK_RIGHT)  {
		if(input.pos < input.text.size()) {
			Mutex::ScopedLock lock(inputMutex);

			input.pos = CLAMP(input.pos, size_t(0), input.text.size()); // safty
			const_string_iterator nextpos(input.text, input.pos);
			IncUtf8StringIterator(nextpos, const_string_iterator(input.text, input.text.size()));

			input.pos = nextpos.pos;
		}
		goto finalHandleKey;
	}
	
	// Home
	if(ev.sym == SDLK_HOME)  {
		Mutex::ScopedLock lock(inputMutex);
		input.pos = 0;
		goto finalHandleKey;
	}
	
	// End
	if(ev.sym == SDLK_END)  {
		Mutex::ScopedLock lock(inputMutex);
		input.pos = input.text.size();
		goto finalHandleKey;
	}
	
	// Paste
	if(ev.ch == 22)  {
		// Get the text
		std::string buf = copy_from_clipboard();
		
		// Paste
		{
			Mutex::ScopedLock lock(inputMutex);
			input.pos = CLAMP(input.pos, size_t(0), input.text.size()); // safty
			input.text = input.text.substr(0, input.pos) + buf + input.text.substr(input.pos);
			input.pos += buf.size();
		}
		
		goto finalHandleKey;
	}
	
	
	// Enter key
	if(ev.ch == '\n' || ev.ch == '\r') {

		std::string cmd = input.text;
		
		{
			Mutex::ScopedLock lock(inputMutex);
			// Note: don't reset input.pos because it's nice if we can remember that			
			input.pos = TransformRawToUtf8ToRaw(input.text, input.pos, "");
			input.text = "";
		}

		// Parse the line
		TrimSpaces(cmd);
		if(cmd != "") {
			Con_AddText(CNC_NORMAL, "]" + cmd);
			Execute(this, cmd);
			addHistoryEntry(cmd);
			saveHistory();
		}
	
		invalidateHistoryPos();
		
		goto finalHandleKey;
	}
	
	// Tab
	if(ev.ch == '\t') {
		{
			Mutex::ScopedLock lock(inputMutex);
			input.pos = CLAMP(input.pos, size_t(0), input.text.size()); // safty
		}
		
		// Auto-complete
		AutocompletionInfo autoCompleteInfo;
		AutoComplete(input.text, input.pos, *this, autoCompleteInfo);
		bool fail = false;
		AutocompletionInfo::InputState newState;
		autoCompleteInfo.popWait(input, newState, fail);
		if(!fail) {
			Mutex::ScopedLock lock(inputMutex);
			input = newState;
		}
		
		goto finalHandleKey;
	}
	
		
	// Handle the history keys
	
	// Up arrow
	if(ev.sym == SDLK_UP) {
		if(historyPos != history.begin()) {
			std::string oldtext = input.text;
			historyPos--;
			
			Mutex::ScopedLock lock(inputMutex);
			input.text = *historyPos;
			input.pos = TransformRawToUtf8ToRaw(oldtext, input.pos, input.text);
		}
	
		goto finalHandleKey;
	}
	
	// Down arrow
	if(ev.sym == SDLK_DOWN) {
		if(historyPos != history.end()) {
			std::string oldtext = input.text;
			historyPos++;
			
			Mutex::ScopedLock lock(inputMutex);
			if(historyPos == history.end())
				input.text = backupInputBuffer;
			else
				input.text = *historyPos;
			input.pos = TransformRawToUtf8ToRaw(oldtext, input.pos, input.text);
		}
		
		goto finalHandleKey;
	}

	// Normal key
	if(ev.ch > 31) {
		if(tLX && tLX->cFont.CanDisplayCharacter(ev.ch)) {
			std::string buf = GetUtf8FromUnicode(ev.ch);
			{
				Mutex::ScopedLock lock(inputMutex);
				input.pos = CLAMP(input.pos, size_t(0), input.text.size()); // safty
				input.text = input.text.substr(0, input.pos) + buf + input.text.substr(input.pos);
				input.pos += buf.size();
			}
		}
		else
			notes << "Ingame console: char '" << GetUtf8FromUnicode(ev.ch) << "'(" << itoa(ev.ch) << ") cannot be displayed and is ignored" << endl;
		
		goto finalHandleKey;
	}
	
	
finalHandleKey:
	if(historyPos != history.end())
		*historyPos = input.text;
	else
		backupInputBuffer = input.text;
}




console_t	*Console = NULL;

bool Con_IsInited() { return Console != NULL; }

static const char* HistoryFilename = "game-cli-history.txt";

static void loadHistory() {
	FILE* fp = OpenGameFile(HistoryFilename, "r");
	if(!fp) return;

	while( !feof(fp) && !ferror(fp) ) {
		std::string l = ReadUntil(fp);
		TrimSpaces(l);
		if(l.empty()) continue;

		ingameConsole.addHistoryEntry(l);
	}

	fclose(fp);
	return;
}

static void saveHistory() {
	FILE* fp = OpenGameFile(HistoryFilename, "w");
	if(!fp) return;

	foreach(l, ingameConsole.history) {
		fwrite(l->c_str(), l->size(), 1, fp);
		fwrite("\n", 1, 1, fp);
	}

	fclose(fp);
	return;
}

// for Con_AddText
static SDL_mutex* con_mutex = NULL;

Timer *con_timer = NULL;

///////////////////
// Initialize the console
int Con_Initialize()
{
	con_mutex = SDL_CreateMutex();
	con_timer = new Timer("Console animation", null, NULL, 30);  // To make the animation smooth in the menu
	
	int n;
	Console = new console_t;;
	if(Console == NULL)
		return false;


	Console->fPosition = 1.0f;
	Console->iState = CON_HIDDEN;
	Console->iLastchar = 0;
	Console->iCurpos = 0;
	Console->fBlinkTime = 0;
	Console->bBlinkState = true;

	for(n=0;n<MAX_CONLINES;n++) {
		Console->Line[n].strText = "";
		Console->Line[n].Colour = CNC_NORMAL;
	}

	loadHistory();
	ingameConsole.startThread();
	
    Console->bmpConPic = LoadGameImage("data/gfx/console.png");
    if(!Console->bmpConPic.get())
        return false;
	
	return true;
}


///////////////////
// Toggle the console
void Con_Toggle()
{
	// Update the cursor blink state
	Console->fBlinkTime = 0;
	Console->bBlinkState = true;

	con_timer->start();

	if(Console->iState == CON_HIDDEN || Console->iState == CON_HIDING) {
		Console->iState = CON_DROPPING;
        if(!tLXOptions->bFullscreen)
        	EnableSystemMouseCursor(true);
	}

	else if(Console->iState == CON_DROPPING || Console->iState == CON_DOWN) {
		Console->iState = CON_HIDING;
        if(!tLXOptions->bFullscreen)
        	EnableSystemMouseCursor(false);
	}
}


///////////////////
// Hide the console
void Con_Hide()
{
	Console->iCurpos = 0;
	Console->iState = CON_HIDDEN;
	Console->fPosition = 1;
	Console->fBlinkTime = 0;
	Console->bBlinkState = true;
}


static void Con_ProcessCharacter(const KeyboardEvent& input);


///////////////////
// Process the console
void Con_Process(TimeDiff dt)
{
	if(tLX && tLX->cConsoleToggle.isDownOnce())
		Con_Toggle();

	keyboard_t *kb = GetKeyboard();
	
	if(!tLX->cConsoleToggle.isDown()) {
		// Process the input
		for(int i = 0; i < kb->queueLength; i++) {
			Con_ProcessCharacter(kb->keyQueue[i]);
		}
	}
	
	// Skip the first frame (because of high dt in menu)
	if (kb->queueLength)
		return;

	switch(Console->iState) {
	case CON_DROPPING:
		Console->fPosition -= 3.0f*dt.seconds();
		break;
	case CON_HIDING:
		Console->fPosition += 3.0f*dt.seconds();
		break;
	}

	if(Console->fPosition < 0.0f) {
		Console->iState = CON_DOWN;
		Console->fPosition = 0.0f;
		con_timer->stop();
	}
	if(Console->fPosition > 1) {
		Console->iState = CON_HIDDEN;
		Console->fPosition = 1;

		Console->Line[0].strText = "";
		con_timer->stop();
	}

}

///////////////////
// Handles the character typed in the console
static void Con_ProcessCharacter(const KeyboardEvent& input)
{
	if(!input.down) return;
	Console->fBlinkTime = 0;
	Console->bBlinkState = true;
	
	if( input.sym == SDLK_ESCAPE ) {
		if (Console->iState != CON_HIDING && Console->iState != CON_HIDDEN)
			Con_Toggle();
		return;
	}
	
	if(Console->iState != CON_DOWN && Console->iState != CON_DROPPING)
		return;

	ingameConsole.pushKey(input);
}


///////////////////
// Add a string of text to the console
// TODO: this function is ineffective because Console->Line is not a std::list
void Con_AddText(int colour, const std::string& text, bool alsoToLogger)
{
	if (text == "")
		return;

	std::vector<std::string> lines = explode(text,"\n");

	{
		ScopedLock lock(con_mutex);
		
		// Move all the text up, losing the last line
		int n;
		for(n = MAX_CONLINES - (int)lines.size() - 1; n >= 1; n--) {
			Console->Line[n + (int)lines.size()].strText = Console->Line[n].strText;
			Console->Line[n + (int)lines.size()].Colour = Console->Line[n].Colour;
		}

		// Add the lines
		n = (int)lines.size();
		for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++, n--)  {
			Console->Line[n].strText = *it;
			Console->Line[n].Colour = colour;
		}
	}
	
	if(alsoToLogger) {
		for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); it++)  {
			notes << "Ingame console: ";
			std::string typeStr = CmdLineMsgTypeAsString((CmdLineMsgType)colour);
			if(typeStr != "") notes << typeStr << ": ";
			notes << *it << endl;
		}
	}
}



///////////////////
// Draw the console
void Con_Draw(SDL_Surface * bmpDest)
{
	if(Console->iState == CON_HIDDEN)
		return;

	int y = (int)(-Console->fPosition * (float)Console->bmpConPic.get()->h);
	int texty = y+Console->bmpConPic.get()->h-28;

	const Color Colours[6] = {tLX->clConsoleNormal, tLX->clConsoleNotify, tLX->clConsoleError, tLX->clConsoleWarning,
		                 tLX->clConsoleDev, tLX->clConsoleChat };

	DrawImage(bmpDest,Console->bmpConPic,0,y);

	AutocompletionInfo::InputState input;
	{
		Mutex::ScopedLock lock(ingameConsole.inputMutex);
		input = ingameConsole.input;
	}

	Console->fBlinkTime += tLX->fDeltaTime;
	Console->bBlinkState = Console->fBlinkTime.milliseconds() % 1000 < 500;

	// Draw the lines of text
	for(int n = 0; n < MAX_CONLINES; n++, texty -= 15) {
		std::string buf = "";

		if(n==0)
			buf = "]" + input.text;
		else
			buf = Console->Line[n].strText;

		if(n==0 && Console->bBlinkState)  {
			DrawVLine(
				bmpDest,
				texty, texty + tLX->cFont.GetHeight(),
				16 + tLX->cFont.GetWidth(input.text.substr(0, input.pos)),
				tLX->clConsoleCursor);
		}

		if (n)
			stripdot(buf, Console->bmpConPic->w - 10);  // Don't make the text overlap

		tLX->cFont.Draw(bmpDest, 12, texty, Colours[Console->Line[n].Colour], buf);
	}
}


///////////////////
// Returns if the console is in use
bool Con_IsVisible()
{
	return Console->iState != CON_HIDDEN;
}


///////////////////
// Shutdown the console
void Con_Shutdown()
{
	ingameConsole.stopThread();
	
	if(Console)
		delete Console;
	Console = NULL;
	
	if(con_mutex)
		SDL_DestroyMutex(con_mutex);
	con_mutex = NULL;

	if (con_timer)
		delete con_timer;
	con_timer = NULL;
}



void Con_Execute(const std::string& cmd) {
	Execute(&ingameConsole, cmd);
}
