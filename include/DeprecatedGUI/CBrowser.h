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


#ifndef __CBROWSER_H__DEPRECATED_GUI__
#define __CBROWSER_H__DEPRECATED_GUI__


#include "InputEvents.h"
#include <libxml/HTMLparser.h>
#include <stack>
#include "LieroX.h"
#include "Timer.h"
#include "Protocol.h"
#include "CScrollbar.h"
#include "HTTP.h"
#include "GfxPrimitives.h"


namespace DeprecatedGUI {

#define BORDER_SIZE 2

// Browser messages
enum {
	BRW_NONE = -1,
	BRW_KEY_PROCESSED = 0,
	BRW_KEY_NOT_PROCESSED
};

struct FontFormat  {
	FontFormat() : bold(false), underline(false), italics(false), preformatted(false), size(0) {}
	bool bold;
	bool underline;
	bool italics;
	bool preformatted;
	int size;
	Color color;
};


// Browser class
class CBrowser : public CWidget {
public:
	CBrowser() :
		bFinished(false),
		tHtmlDocument(NULL),
		tRootNode(NULL),
		fLastMouseScroll(0),
		iBorderSize(BORDER_SIZE),
		iDocumentHeight(0),
		tCurrentLine(NULL),
		curX(0),
		curY(0),
		iCurIndent(0),
		iCursorColumn(0),
		iCursorLine(0),
		iSelectionStartLine(0),
		iSelectionStartColumn(0),
		iSelectionEndLine(0),
		iSelectionEndColumn(0),
		bSelectionGrabbed(false),
		bDrawCursor(true),
		tTimer(NULL),
		bShowScrollbar(true),
		bmpBuffer(NULL),
		bNeedsRender(false)
	{
		tCurrentContext.tCurrentGroup = NULL;
		tBgColor = Color(0,0,0,0);
	}

public:

	// Everything in the browser is an object - a chunk of text, a link, ...
	// Objects are included in lines
	class CBrowserObject  {
	public:
		enum Type  {
			objNone = -1,
			objText = 0,
			objHr,
			objList
		};

		typedef void(CBrowser::*EventHandler)(CBrowserObject *);
	public:
		CBrowserObject(CBrowser *parent, const SDL_Rect& rect) :
		  tRect(rect),
		  tClickFunc(NULL),
		  tMouseMoveFunc(NULL),
		  tParent(parent),
		  iType(objNone),
		  bSelectable(false)
		  {}

		CBrowserObject() :
		  tClickFunc(NULL),
		  tMouseMoveFunc(NULL),
		  tParent(NULL),
		  iType(objNone),
		  bSelectable(false)
		  { tRect = MakeRect(0, 0, 0, 0); }

		CBrowserObject(const CBrowserObject& oth)  { operator=(oth); }

		virtual ~CBrowserObject() {}

	protected:
		friend class CBrowser;
		SDL_Rect	tRect; // HINT: position in the document, not on screen!
		EventHandler tClickFunc;
		EventHandler tMouseMoveFunc;
		CBrowser	*tParent;
		Type		iType;
		bool		bSelectable;

	public:
		bool	InBox(int x, int y) const  { return x >= tRect.x && x < (tRect.x + tRect.w) 
			&& y >= tRect.y && y < (tRect.y + tRect.h); }
		int		getWidth()		{ return tRect.w; }
		int		getHeight()		{ return tRect.h; }
		bool	isSelectable()	{ return bSelectable; }

	protected:
		virtual void	DoMouseMove(int x, int y)	{  }
		virtual void	DoMouseDown(int x, int y)	{  }
		virtual void	DoClick(int x, int y)		{  }
		void CopyPropertiesTo(CBrowserObject *obj) const  {
			obj->tRect = tRect;
			obj->tClickFunc = tClickFunc;
			obj->tMouseMoveFunc = tMouseMoveFunc;
			obj->tParent = tParent;
			obj->iType = iType;
			obj->bSelectable = bSelectable;
		}

	public:
		CBrowser *getParent() const			{ return tParent; }
		Type getType() const				{ return iType; }
		void setClickHandler(EventHandler h) { tClickFunc = h; }
		void setMouseMoveHandler(EventHandler h) { tMouseMoveFunc = h; }

		virtual void Clear()  { tRect = MakeRect(0, 0, 0, 0); 
				tClickFunc = NULL; tMouseMoveFunc = NULL; tClickFunc = NULL; tParent = NULL; }

		CBrowserObject& operator=(const CBrowserObject& oth)  {
			assert(false);
			return *this;
		}

		virtual void Render(SDL_Surface *dest, int x, int y) = 0;

		virtual CBrowserObject *Clone() const = 0;

	};

	// Group of objects
	// An example of a group is a link (<a></a>)
	class CObjectGroup  {
	private:
		std::list<CBrowserObject *> tObjects;

	protected:
		void CopyPropertiesTo(CObjectGroup *oth) const {
			oth->tObjects = tObjects;
		}
	public:
		CObjectGroup()  {}
		CObjectGroup(const CObjectGroup& oth)  { operator=(oth); }
		CObjectGroup& operator=(const CObjectGroup& oth)  {
			assert(false);
			return *this;
		}
		virtual ~CObjectGroup() {}

		virtual void DoMouseMove(int x, int y)	{}
		virtual void DoClick(int x, int y)		{}

		virtual CObjectGroup *Clone() const = 0;

		const std::list<CBrowserObject *>& getObjects()	{ return tObjects; }
		void addObject(CBrowserObject *obj)				{ tObjects.push_back(obj); }
		bool InBox(int x, int y);
	};

private:
	class CBrowserLine  {
	public:
		CBrowserLine() : iHeight(0), iLeftMargin(0), iMaxWidth(0), iDocumentY(0) {}
		CBrowserLine(int margin) : iHeight(0), iLeftMargin(margin), iMaxWidth(0), iDocumentY(0) {}

		CBrowserLine(const CBrowserLine& oth)  { operator=(oth); }
		CBrowserLine& operator=(const CBrowserLine& oth)  {
			if (&oth != this)  {
				iHeight = oth.iHeight;
				iLeftMargin = oth.iLeftMargin;
				iMaxWidth = oth.iMaxWidth;
				iDocumentY = oth.iDocumentY;
				tObjects = oth.tObjects;
				sPureText = oth.sPureText;
			}
			return *this;
		}

		~CBrowserLine();
	private:
		int			iHeight;
		int			iLeftMargin;
		int			iMaxWidth;
		int			iDocumentY;
		std::list<CBrowserObject *> tObjects;
		std::string	sPureText; // For better selection handling

	public:
		void setHeight(int h)		{ iHeight = h; }
		void setLeftMargin(int m)	{ iLeftMargin = m; }
		void setMaxWidth(int m)		{ iMaxWidth = m; }
		void setDocumentY(int y)	{ iDocumentY = y; }
		int	getHeight()				{ return iHeight; }
		int	getLeftMargin()			{ return iLeftMargin; }
		int getMaxWidth()			{ return iMaxWidth; }
		int getDocumentY()			{ return iDocumentY; }
		bool isEmpty()				{ return tObjects.empty(); }
		void ClearObjects()			{ tObjects.clear(); sPureText.clear(); }
		const std::string& getPureText()	{ return sPureText; }

		std::list<CBrowserObject *>& getObjects()	{ return tObjects; }
		void addObject(CBrowserObject *obj);
	};

	class ParseContext  {
	public:
		FontFormat		tFormat;
		CObjectGroup	*tCurrentGroup;
	};

private:
	// Attributes
	CHttp					cHttp;
	std::string				sURL;
	std::string				sHostName;
	bool					bFinished;
	htmlDocPtr				tHtmlDocument;
	htmlNodePtr				tRootNode;
	std::vector<CBrowserLine *>	tLines;
	std::list<CObjectGroup *>	tGroups;
	AbsTime					fLastMouseScroll;
	int						iBorderSize;
	size_t					iDocumentHeight;

	// Parsing temps
	ParseContext			tCurrentContext;
	std::stack<ParseContext> tContextStack;
	CBrowserLine			*tCurrentLine;
	Color					tBgColor;
	int						curX;
	int						curY;
	int						iCurIndent; // Where the new lines should start (left margin)

	// Selection & caret
	size_t					iCursorColumn;
	size_t					iCursorLine;
	size_t					iSelectionStartLine;
	size_t					iSelectionStartColumn;
	size_t					iSelectionEndLine;
	size_t					iSelectionEndColumn;

	size_t					iSelectionGrabLine;
	size_t					iSelectionGrabColumn;
	bool					bSelectionGrabbed;
	bool					bDrawCursor;
	Timer					*tTimer;  // For cursor blink

	// Window attributes
	CScrollbar				cScrollbar;
	bool					bUseScroll;
	bool					bShowScrollbar;

	// Other
	SmartPointer<SDL_Surface>	bmpBuffer;
	bool					bNeedsRender;

	// Methods
	htmlDocPtr				ParseHTML(const std::string& data);
	void					AppendDocument(htmlDocPtr doc);
	void					AppendNode(xmlNodePtr node);
	void					UpdateRenderObjects();
	void					ClearParser();
	void					InitNodeWalk();
	void					EndNodeWalk();
	void					RenderContent();
	bool					GetDrawSelectionBounds(size_t& top_line, size_t& bot_line);
	void					DrawSelectionForText(CBrowserObject *obj, size_t& line, size_t& column, int scroll_y);
	void					DrawSelection();
	void					AddObject(CBrowserObject *obj);
	void					AddTextObject(const std::string& text);
	void					ParseText(const std::string& text);
	void					WalkNodes(xmlNodePtr node);
	void					WalkChildren(xmlNodePtr node);
	bool					InSelection(size_t line, size_t column);
	std::string				GetSelectedText();
	bool					IsSelectionEmpty();
	void					ClearSelection();
	void					SwapSelectionEnds();
	void					EndLine();
	void					AdjustScrollbar(bool mouse = false);
	void					ReRender();
	void					DrawCursor(SDL_Surface *bmpDest);
	void					ResetScrollbar();
	void					AdjustBuffer();
	void					OnTimerEvent(Timer::EventData ev);
	void					PushContext()	{ tContextStack.push(tCurrentContext); }
	void					PopContext()	{ tCurrentContext = tContextStack.top(); tContextStack.pop(); }
	
	
	// Link helper functions
	void					StartLink(const std::string& url);
	void					EndLink();
	std::string				GetFullURL(const std::string& url);


public:
	// Methods


	void	Create();
	void	Destroy();

	void	ClearDocument()	{ ClearParser(); bNeedsRender = true; }

	//These events return an event id, otherwise they return -1
	int		MouseOver(mouse_t *tMouse);
	int		MouseUp(mouse_t *tMouse, int nDown);
	int		MouseWheelDown(mouse_t *tMouse);
	int		MouseWheelUp(mouse_t *tMouse);
	int		MouseDown(mouse_t *tMouse, int nDown);
	int		KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate);
	int		KeyUp(UnicodeChar c, int keysym, const ModifiersState& modstate) { return BRW_NONE; }

	uintptr_t SendMessage(int iMsg, uintptr_t Param1, uintptr_t Param2) { return 0; }
	uintptr_t SendMessage(int iMsg, const std::string& sStr, uintptr_t Param) { return 0; }
	uintptr_t SendMessage(int iMsg, std::string *sStr, uintptr_t Param)  { return 0; }

	void	Draw(SDL_Surface * bmpDest);

	void	MousePosToCursorPos(int ms_x, int ms_y, size_t& cur_x, size_t& cur_y);
	void	CursorPosToMousePos(size_t cur_x, size_t cur_y, int& ms_x, int& ms_y);

	void	LoadFromHTTP(const std::string& url);
	void	LoadFromFile(const std::string& file);
	void	LoadFromString(const std::string& data);
	void	AppendData(const std::string& data);
	void	ProcessHTTP();

	int		getBorderSize()		{ return iBorderSize; }
	void	setBorderSize(int size)	{ iBorderSize = size; }

	// TODO: create a descendant chatbox class and move the chatbox things there, they do not belong to browser
	void	InitializeChatBox( const std::string & initText = "" );
	void	AddChatBoxLine(const std::string & text, Color color, TXT_TYPE textType, bool bold = false, bool underline = false);
	std::string GetChatBoxText();
	void	CleanUpChatBox( const std::vector<TXT_TYPE> & removedText, int maxLineCount );
	void	ScrollToLastLine();
	bool	NeedsRepaint()  { return bNeedsRender; }

	bool	IsLoaded()		{ return bFinished; }

	CHttp&	GetHttp()		{ return cHttp; }

	void	showScrollbar(bool b)		{ bShowScrollbar = b; }
	// TODO: add function for getting individual text lines & their color
};

}; // namespace DeprecatedGUI

#endif  //  __CBROWSER_H__DEPRECATED_GUI__
