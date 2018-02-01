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


#include "LieroX.h"

#include <stack>
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "GuiPrimitives.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "StringBuf.h"
#include "DeprecatedGUI/CBrowser.h"
#include "XMLutils.h"
#include "Clipboard.h"
#include "Cursor.h"
#include "Timer.h"
#include "AuxLib.h"


namespace DeprecatedGUI {

#define LIST_SPACING 10
#define DEFAULT_LINE_HEIGHT (tLX->cFont.GetHeight())

//
// Text object
//
class CTextObject : public CBrowser::CBrowserObject {
public:
	CTextObject() { iType = objText; bSelectable = true; }
	CTextObject(CBrowser *parent, const std::string& text, const FontFormat& f, int x, int y) : 
	CBrowser::CBrowserObject(parent, MakeRect(x, y, tLX->cFont.GetWidth(text), tLX->cFont.GetHeight()))
		{ sText = text; iType = objText; tFormat = f; bSelectable = true; }

	CBrowser::CBrowserObject *Clone() const { 
		CTextObject *res = new CTextObject();
		CopyPropertiesTo(res);
		res->sText = sText;
		res->tFormat = tFormat;
		return res;
	}

	virtual ~CTextObject() {}

protected:
	std::string sText;
	FontFormat	tFormat;

public:
	const std::string& getText() const	{ return sText; }
	void setText(const std::string& text)	{ tRect.w = tLX->cFont.GetWidth(text); sText = text; }

	// Render the text object
	virtual void Render(SDL_Surface *dest, int x, int y)  
	{
		tLX->cFont.Draw(dest, x, y, tFormat.color, sText);
		if (tFormat.bold)
			tLX->cFont.Draw(dest, x + 1, y, tFormat.color, sText);
		if (tFormat.underline)
			DrawHLine(dest, x, x + tRect.w, y + tRect.h - 2, tFormat.color);
	}

	virtual void Clear()  { CBrowser::CBrowserObject::Clear(); setText(""); }

	// Split the text object in two objects, the first object won't be wider than the specified width
	// WARNING: if the length of this object is less than maxwidth, the second object will be set to NULL
	// WARNING 2: the first object always points to this object, so don't free it!
	std::pair<CTextObject *, CTextObject *> Split(int maxwidth)
	{
		std::pair<CTextObject *, CTextObject *> result((CTextObject *)NULL, (CTextObject *)NULL);

		if (tRect.w <= maxwidth)  {
			result.first = this;
			return result;
		}

		int w = 0;
		std::string::const_iterator it = sText.begin();

		while (it != sText.end())  {

			// If the next word overflows, make the split
			std::string word = GetNextWord(it, sText);
			int word_w = tLX->cFont.GetWidth(word);
			w += word_w + tLX->cFont.GetSpacing();
			if (w > maxwidth)  {
				result.first = this;
				if (word_w > maxwidth)  {
					// Hard break
					size_t res = GetPosByTextWidth(sText, maxwidth, &tLX->cFont);
					result.second = new CTextObject(tParent, sText.substr(res), tFormat, tRect.x + w - word_w, tRect.y);
					sText.erase(res);
					tRect.w = tLX->cFont.GetWidth(sText);
				} else {
					if (it == sText.end() || it == sText.begin())  {
						result.second = NULL;
					} else {
						result.second = new CTextObject(tParent, std::string(it, std::string::const_iterator(sText.end())), 
															tFormat, tRect.x + w - word_w, tRect.y);
						setText(std::string(std::string::const_iterator(sText.begin()), it - 1));
					}
				}

				return result;
			}

			it += word.size();
			if (it != sText.end())  {
				w += tLX->cFont.GetCharacterWidth((uchar)*it) + tLX->cFont.GetSpacing(); // The space
				it++;
			}
		}

		// Should not happen
		result.first = this;
		result.second = NULL;

		return result;
	}
	
};

//
// List item object
//
#define ARROW_W 3
#define ARROW_H 5
class CListItemObject : public CBrowser::CBrowserObject {
public:
	CListItemObject() { iType = objList; }
	CListItemObject(CBrowser *parent, int x, int y) : 
	CBrowser::CBrowserObject(parent, MakeRect(x, y, LIST_SPACING + ARROW_W * 2, tLX->cFont.GetHeight())) { iType = objList; }

	CBrowser::CBrowserObject *Clone() const { 
		CListItemObject *res = new CListItemObject();
		CopyPropertiesTo(res);
		return res;
	}

public:

	// Render the list item object
	virtual void Render(SDL_Surface *dest, int x, int y)  
	{
		DrawSimpleArrow(dest, x, y + (tRect.h - ARROW_H)/2, ARROW_W, ARROW_H, ardLeft, tLX->clNormalText);
		DrawSimpleArrow(dest, x + ARROW_W, y + (tRect.h - ARROW_H)/2, ARROW_W, ARROW_H, ardRight, tLX->clNormalText);
	}
};

//
// Hrizontal rule object
//
class CHorizontalRuleObject : public CBrowser::CBrowserObject {
public:
	CHorizontalRuleObject() { iType = objHr; tColor = Color(0, 0, 0); }
	CHorizontalRuleObject(CBrowser *parent, Color color, const SDL_Rect& rect) : 
	CBrowser::CBrowserObject(parent, rect) { iType = objHr; tColor = color; }

	CBrowser::CBrowserObject *Clone() const { 
		CHorizontalRuleObject *res = new CHorizontalRuleObject();
		CopyPropertiesTo(res);
		res->tColor = tColor;
		return res;
	}

private:
	Color tColor;

public:

	// Render the horizontal rule object
	virtual void Render(SDL_Surface *dest, int x, int y)  
	{
		DrawHLine(dest, x, x + tRect.w, y + tRect.h / 2, tColor);
	}
};

//
// Link group
//
class CLinkGroup : public CBrowser::CObjectGroup  {
public:
	CLinkGroup(const std::string& url) : sURL(url) {}
	
	CBrowser::CObjectGroup *Clone() const  {
		CLinkGroup *res = new CLinkGroup(sURL);
		CopyPropertiesTo(res);
		return res;
	}

private:
	std::string sURL;
public:
	void DoMouseMove(int x, int y)		{ SetGameCursor(CURSOR_HAND); }
	void DoClick(int x, int y)			{ OpenLinkInExternBrowser(sURL); }
};


////////////////////////
// Add an object to browser line
void CBrowser::CBrowserLine::addObject(CBrowser::CBrowserObject *obj)
{
	tObjects.push_back(obj);
	if (obj->getHeight() > iHeight)
		iHeight = obj->getHeight();
	if (obj->getType() == CBrowserObject::objText)
		sPureText += ((CTextObject *)obj)->getText();
}

///////////////////////
// Browser line destructor
CBrowser::CBrowserLine::~CBrowserLine()
{
	for (std::list<CBrowserObject *>::iterator i = tObjects.begin(); i != tObjects.end(); i++)
		delete (*i);
	tObjects.clear();
}


///////////////////////////
// Returns true if the cursor is inside the group
bool CBrowser::CObjectGroup::InBox(int x, int y)
{
	for (std::list<CBrowserObject *>::const_iterator it = tObjects.begin(); it != tObjects.end(); it++)  {
		if ((*it)->InBox(x, y))
			return true;
	}

	return false;
}

///////////////////
// The create event
void CBrowser::Create()
{
	tHtmlDocument = NULL;
	tRootNode = NULL;

	bUseScroll = false;
	bNeedsRender = true;

	// Setup the scrollbar
	cScrollbar.Create();
	cScrollbar.setMin(0);
	cScrollbar.setValue(0);
	cScrollbar.setItemsperbox(1);
	cScrollbar.setMax(0);

	// Setup the cursor blink timer
	if (tTimer == NULL)  {
		tTimer = new Timer;
		if (tTimer)  {
			tTimer->name = "CBrowser cursor blinker";
			tTimer->interval = 500;
			tTimer->once = false;
			tTimer->onTimer.handler() = getEventHandler(this, &CBrowser::OnTimerEvent);
			tTimer->start();
		}
	}
}


///////////////////
// Destroy event
void CBrowser::Destroy()
{
	if (tHtmlDocument)
		xmlFreeDoc(tHtmlDocument);
	if (tTimer)  {
		tTimer->stop();
		delete tTimer;
		tTimer = NULL;
	}

	ClearParser();

	tCurrentLine = NULL;
	tHtmlDocument = NULL;
	tRootNode = NULL;
}

///////////////////
// Load the HTML file
void CBrowser::LoadFromHTTP(const std::string& url)
{
	// Send the HTTP request
	bFinished = false;
	cHttp.RequestData(url, tLXOptions->sHttpProxy);
	sHostName = cHttp.GetHostName();
	sURL = cHttp.GetUrl();
}

/////////////////////////
// Load the contents from a file
void CBrowser::LoadFromFile(const std::string& file)
{
	bFinished = false;
	std::ifstream *fp = OpenGameFileR(file);
	if (!fp)
		return;

	sURL = file;
	sHostName = "";
	std::string data;
	while (!fp->eof())  {
		std::string tmp;
		std::getline(*fp, tmp);
		data += tmp;
	}

	fp->close();
	delete fp;

	bFinished = true;
	LoadFromString(data);
}

////////////////////////
// Load the contents from a data string
void CBrowser::LoadFromString(const std::string& data)
{
	sHostName = "";
	sURL = "";
	ClearDocument();
	AppendData(data);
	bFinished = true;

	cScrollbar.setValue(0);
	bUseScroll = false;
}

/////////////////////////
// Append data to the browser
void CBrowser::AppendData(const std::string& data)
{
	htmlDocPtr doc = ParseHTML(data);
	if (!doc)  {
		// Append as pure text
		xmlNodePtr node = xmlNewNode(NULL, (xmlChar *)"span");
		xmlNodeAddContent(node, (const xmlChar *)data.c_str());
		AppendNode(node);
		xmlFreeNode(node);
		return;
	}
	AppendDocument(doc);
}

///////////////////
// Process the HTTP downloading
void CBrowser::ProcessHTTP()
{
	if (bFinished)
		return;

	int res = cHttp.ProcessRequest();
	switch (res)  {
	case HTTP_PROC_PROCESSING:
		break;
	case HTTP_PROC_ERROR:
		if (cHttp.GetData().empty())
			LoadFromString("An error occured while loading: " + cHttp.GetError().sErrorMsg);
		else
			LoadFromString(cHttp.GetData());
		break;
	case HTTP_PROC_FINISHED:
		LoadFromString(cHttp.GetData());
		break;
	}
}

//////////////////////
// Clears the parser to prepare it for new parsing
void CBrowser::ClearParser()
{
	while (tContextStack.size()) tContextStack.pop();
	tCurrentContext.tCurrentGroup = NULL;
	tCurrentContext.tFormat = FontFormat();
	tCurrentContext.tFormat.color = tLX->clNormalText;
	curX = iBorderSize;
	curY = iBorderSize;
	iDocumentHeight = 0;

	// Free the groups
	for (std::list<CObjectGroup *>::iterator it = tGroups.begin(); it != tGroups.end(); it++)
		delete (*it);

	// Free the lines
	for (std::vector<CBrowserLine *>::iterator it = tLines.begin(); it != tLines.end(); it++)
		delete (*it);

	tGroups.clear();
	tLines.clear();
}

//////////////////////
// Initialization before changing the DOM tree
void CBrowser::InitNodeWalk()
{
	curX = iBorderSize;
	curY = iDocumentHeight;

	// Prepare the first line
	tCurrentLine = new CBrowserLine();
	tCurrentLine->setMaxWidth(iWidth - iBorderSize * 2 - cScrollbar.getWidth());
	tCurrentLine->setHeight(0);
	tCurrentLine->setLeftMargin(0);
	tCurrentLine->setDocumentY(curY);

	tCurrentContext.tCurrentGroup = NULL;
	tCurrentContext.tFormat = FontFormat();
	tCurrentContext.tFormat.color = tLX->clNormalText;
}

////////////////////////
// Finalization of the DOM tree walk
void CBrowser::EndNodeWalk()
{
	if (tCurrentLine)  {
		if (!tCurrentLine->isEmpty())
			EndLine();
		delete tCurrentLine;
		tCurrentLine = NULL;
	}
}

////////////////////////
// Converts the HTML data to XML document tree
// Returns NULL on failure
htmlDocPtr CBrowser::ParseHTML(const std::string &data)
{
	// Get the context
	htmlParserCtxtPtr context = htmlCreateMemoryParserCtxt(data.data(), data.size());
	if (!context)
		return NULL;

	// Parse the file
	htmlDocPtr document = htmlCtxtReadDoc(context, (xmlChar *)data.data(), NULL, NULL, /*HTML_PARSE_RECOVER |*/ HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);
	htmlFreeParserCtxt(context);
	if (!document)  {
		return NULL;
	}

	// Get the root element
	htmlNodePtr node = xmlDocGetRootElement(document);
	if (!node || !node->children)  {
		xmlFreeDoc(document);
		return NULL;
	}

	// Success
	return document;
}

//////////////////////
// Appends a document and all its subnodes at the end of the current document
// WARNING: the document gets freed in this function, don't use it after passing it here
void CBrowser::AppendDocument(htmlDocPtr doc)
{
	if (!doc)
		return;

	// Make sure the given document contains nodes
	xmlNodePtr node = xmlDocGetRootElement(doc);
	if (!node)  {
		xmlFreeDoc(doc);
		return;
	}

	// If there's no document to append to, just make the given one the default document
	if (!tHtmlDocument)  {
		tHtmlDocument = doc;
		tRootNode = node;
		UpdateRenderObjects();
		return;
	}

	// Append the root node and all its subnodes
	AppendNode(node);

	xmlFreeDoc(doc);
}

/////////////////////////
// Appends a node at the end of the current document
void CBrowser::AppendNode(xmlNodePtr node)
{
	if (!tHtmlDocument || !node)
		return;

	xmlNodePtr doc_node = xmlDocGetRootElement(tHtmlDocument);
	if (!doc_node)
		return;

	// Find the BODY node
	while (doc_node)  {
		if (xmlStrcmp(doc_node->name, (xmlChar *)"body") == 0)
			break;
		if (doc_node->next)
			doc_node = doc_node->next;
		else
			doc_node = doc_node->children;
	}

	if (!doc_node)
		doc_node = xmlDocGetRootElement(tHtmlDocument); // No body tag - jsut take the root element
	
	// Recursively append the node
	xmlNodePtr copy = xmlCopyNode(node, true);
	if (copy)
		xmlAddChild(doc_node, copy);

	// Append the lines
	InitNodeWalk();
	WalkNodes(node);
	EndNodeWalk();

	ResetScrollbar();

	bNeedsRender = true;
}

/////////////////////////
// Synchronizes the XML tree with rendering lines (tLines)
// Should be called anytime the XML tree is changed
void CBrowser::UpdateRenderObjects()
{
	ClearParser();

	if (!tHtmlDocument)
		return;

	InitNodeWalk();
	WalkNodes(xmlDocGetRootElement(tHtmlDocument));
	EndNodeWalk();

	ResetScrollbar();

	bNeedsRender = true;
}


///////////////////
// Mouse down event
int CBrowser::MouseDown(mouse_t *tMouse, int nDown)
{
	// Do nothing if not focused
	if (!bFocused)
		return BRW_NONE;

	if(bUseScroll && (tMouse->X > iX+iWidth-20 || cScrollbar.getGrabbed()) && bShowScrollbar)
	{
		cScrollbar.MouseDown(tMouse, nDown);
		bNeedsRender = true; // Always redraw, scrollbar may change its image
		return BRW_NONE;
	}

	MousePosToCursorPos(tMouse->X, tMouse->Y, iCursorColumn, iCursorLine);

	// Copy to clipboard with 2-nd or 3-rd mouse button
	std::string sel = GetSelectedText();
	if( sel.size() != 0 && 
		(tMouse->FirstDown & SDL_BUTTON(2) || tMouse->FirstDown & SDL_BUTTON(3)) )
	{
        copy_to_clipboard(sel);
		bSelectionGrabbed = false;
		return BRW_NONE;
	}

	// Clear the selection when clicked
	if (sel.size() != 0 && tMouse->FirstDown & SDL_BUTTON(1))  {
		ClearSelection();
		bSelectionGrabbed = false;
	}

	if (bSelectionGrabbed)  {
		iSelectionStartColumn = iSelectionGrabColumn;
		iSelectionStartLine = iSelectionGrabLine;
		iSelectionEndLine = iCursorLine;
		iSelectionEndColumn = iCursorColumn;

		// Swap the ends if necessary
		SwapSelectionEnds();

		bCanLoseFocus = false;
	} else {
		iSelectionGrabColumn = iCursorColumn;
		iSelectionGrabLine = iCursorLine;

		// Destroy any previous selection
		ClearSelection();
	}

	// Scroll if necessary
	AdjustScrollbar(true);

	bSelectionGrabbed = true;
	bNeedsRender = true;

	return BRW_NONE;
}


///////////////////
// Mouse over event
int CBrowser::MouseOver(mouse_t *tMouse)
{
	if(bUseScroll && tMouse->X > iX+iWidth-20 && bShowScrollbar)
	{
		if( cScrollbar.MouseOver(tMouse) == SCR_CHANGE )
			bNeedsRender = true;
		return BRW_NONE;
	}

	// Check if any of the active areas has been clicked
	if (!tMouse->Down)  {
		int scroll = bUseScroll ? cScrollbar.getValue() : 0;
		for (std::list<CObjectGroup *>::iterator it = tGroups.begin(); it != tGroups.end(); it++) {
			if ((*it)->InBox(tMouse->X - iX, tMouse->Y - iY + scroll * DEFAULT_LINE_HEIGHT))  {
				(*it)->DoMouseMove(tMouse->X, tMouse->Y - iY + scroll * DEFAULT_LINE_HEIGHT);
				break;
			}
		}
	}

	return BRW_NONE;
}

//////////////////
// Mouse wheel down event
int CBrowser::MouseWheelDown(mouse_t *tMouse)
{
	if(bUseScroll)  {
		if( cScrollbar.MouseWheelDown(tMouse) == SCR_CHANGE )
			bNeedsRender = true;
	}
	return BRW_NONE;
}

//////////////////
// Mouse wheel up event
int CBrowser::MouseWheelUp(mouse_t *tMouse)
{
	if(bUseScroll)  {
		if( cScrollbar.MouseWheelUp(tMouse) == SCR_CHANGE )
			bNeedsRender = true;
	}
	return BRW_NONE;
}

//////////////////
// Mouse up event
int CBrowser::MouseUp(mouse_t *tMouse, int nDown)
{
	if(bUseScroll && (tMouse->X > iX+iWidth-20 || cScrollbar.getGrabbed()) && bShowScrollbar)
	{
		cScrollbar.MouseUp(tMouse, nDown);
		bNeedsRender = true; // Always redraw, scrollbar may change it's image
		return BRW_NONE;
	}

	std::string sel = GetSelectedText();

	// Copy to clipboard with 2-nd or 3-rd mouse button
	if( sel.size() != 0 && bSelectionGrabbed && 
		(tMouse->Up & SDL_BUTTON(2) || tMouse->Up & SDL_BUTTON(3)) )
        copy_to_clipboard(sel);

	// Check if any of the active areas has been clicked
	if (sel.size() == 0)  {
		int scroll = bUseScroll ? cScrollbar.getValue() : 0;
		for (std::list<CObjectGroup *>::iterator it = tGroups.begin(); it != tGroups.end(); it++) {
			if ((*it)->InBox(tMouse->X - iX, tMouse->Y - iY + scroll * DEFAULT_LINE_HEIGHT))  {
				(*it)->DoClick(tMouse->X, tMouse->Y - iY + scroll * DEFAULT_LINE_HEIGHT);
				break;
			}
		}
	}

	bSelectionGrabbed = false;
	bCanLoseFocus = true;

	return BRW_NONE;
}

//////////////////
// Mouse wheel down event
int CBrowser::KeyDown(UnicodeChar c, int keysym, const ModifiersState& modstate)
{
	size_t oldline = iCursorLine;
	size_t oldcol = iCursorColumn;

	bNeedsRender = true;

	switch (keysym)  {
	case SDLK_DOWN:
		if (iCursorLine < tLines.size() - 1)  {
			++iCursorLine;
			iCursorColumn = MIN(Utf8StringSize(tLines[iCursorLine]->getPureText()), iCursorColumn);
			AdjustScrollbar();

			if (modstate.bShift)  {
				// There is a selection and the cursor is at the beginning of it
				if (iSelectionStartLine == oldline && iSelectionStartColumn == oldcol && !IsSelectionEmpty())  {
					iSelectionStartLine = iCursorLine;
					iSelectionStartColumn = MIN(Utf8StringSize(tLines[iCursorLine]->getPureText()), iCursorColumn);
					SwapSelectionEnds();

				// No selection or cursor at the end of the selection
				} else {
					iSelectionEndLine = iCursorLine;
					iSelectionEndColumn = MIN(Utf8StringSize(tLines[iCursorLine]->getPureText()), iCursorColumn);
				}
			} else {
				ClearSelection();
			}
		}
	return BRW_KEY_PROCESSED;
	case SDLK_UP:
		if (iCursorLine > 0 && iCursorLine < tLines.size())  {
			--iCursorLine;
			iCursorColumn = MIN(Utf8StringSize(tLines[iCursorLine]->getPureText()), iCursorColumn);
			AdjustScrollbar();

			if (modstate.bShift)  {
				// There is a selection and the cursor is at the end of it
				if (iSelectionEndLine == oldline && iSelectionEndColumn == oldcol && !IsSelectionEmpty())  {
					iSelectionEndLine = iCursorLine;
					iSelectionEndColumn = MIN(Utf8StringSize(tLines[iCursorLine]->getPureText()), iCursorColumn);
					SwapSelectionEnds();

				// No selection or cursor at the end of the selection
				} else {
					iSelectionStartLine = iCursorLine;
					iSelectionStartColumn = MIN(Utf8StringSize(tLines[iCursorLine]->getPureText()), iCursorColumn);
				}
			} else {
				ClearSelection();
			}
		}
	return BRW_KEY_PROCESSED;

	case SDLK_RIGHT:
		if (iCursorLine < tLines.size())  {
			if (iCursorColumn >= Utf8StringSize(tLines[iCursorLine]->getPureText()))  {
				if  (iCursorLine < tLines.size() - 1)  {
					iCursorColumn = 0;
					++iCursorLine;
					AdjustScrollbar();
				}
			} else {
				++iCursorColumn;
			}

			if (modstate.bShift)  {
				// There is a selection and the cursor is at the beginning of it
				if (iSelectionStartLine == oldline && iSelectionStartColumn == oldcol && !IsSelectionEmpty())  {
					iSelectionStartLine = iCursorLine;
					iSelectionStartColumn = iCursorColumn;

				// No selection or cursor at the end of the selection
				} else {
					iSelectionEndLine = iCursorLine;
					iSelectionEndColumn = iCursorColumn;
				}
			} else {
				ClearSelection();
			}
		}
	return BRW_KEY_PROCESSED;

	case SDLK_LEFT:
		if (tLines.size() > 0)  {
			if (iCursorColumn == 0)  {
				if  (iCursorLine > 0)  {
					--iCursorLine;
					iCursorColumn = MAX(0, (int)Utf8StringSize(tLines[MIN(tLines.size() - 1, iCursorLine)]->getPureText()));
					AdjustScrollbar();
				}
			} else {
				--iCursorColumn;
			}

			if (modstate.bShift)  {
				// There is a selection and the cursor is at the end of it
				if (iSelectionEndLine == oldline && iSelectionEndColumn == oldcol && !IsSelectionEmpty())  {
					iSelectionEndLine = iCursorLine;
					iSelectionEndColumn = iCursorColumn;

				// No selection or cursor at the end of the selection
				} else {
					iSelectionStartLine = iCursorLine;
					iSelectionStartColumn = iCursorColumn;
				}
			} else {
				ClearSelection();
			}
		}
	return BRW_KEY_PROCESSED;

	case SDLK_HOME:
		if (tLines.size() > 0 && iCursorLine < tLines.size())  {
			iCursorColumn = 0;

			if (modstate.bShift)  {
				// There is a selection and the cursor is at the end of it
				if (iSelectionEndLine == oldline && iSelectionEndColumn == oldcol && !IsSelectionEmpty())  {
					iSelectionEndColumn = iCursorColumn;
					SwapSelectionEnds();

				// No selection or cursor at the end of the selection
				} else {
					iSelectionStartLine = iCursorLine;
					iSelectionStartColumn = iCursorColumn;
					if (IsSelectionEmpty())  { // If there's no selection, the new selection can be only one-line
						iSelectionEndLine = iCursorLine;
						iSelectionEndColumn = oldcol;
					}
				}
			} else {
				ClearSelection();
			}
		}
	return BRW_KEY_PROCESSED;

	case SDLK_END:
		if (tLines.size() > 0 && iCursorLine < tLines.size())  {
			iCursorColumn = Utf8StringSize(tLines[iCursorLine]->getPureText());

			if (modstate.bShift)  {
				// There is a selection and the cursor is at the beginning of it
				if (iSelectionStartLine == oldline && iSelectionStartColumn == oldcol && !IsSelectionEmpty())  {
					iSelectionStartColumn = iCursorColumn;
					SwapSelectionEnds();

				// No selection or cursor at the end of the selection
				} else {
					iSelectionEndLine = iCursorLine;
					iSelectionEndColumn = iCursorColumn;
					if (IsSelectionEmpty())  { // If there's no selection, the new selection can be only one-line
						iSelectionStartLine = iCursorLine;
						iSelectionStartColumn = oldcol;
					}
				}
			} else {
				ClearSelection();
			}
		}
	return BRW_KEY_PROCESSED;
	}

    // Ctrl-c or Super-c or Ctrl-Insert (copy)
    if (((modstate.bCtrl || modstate.bGui) && keysym == SDLK_c ) ||
		(((modstate.bCtrl || modstate.bGui) && keysym == SDLK_INSERT ))) {
		if (!IsSelectionEmpty())
			copy_to_clipboard(GetSelectedText());
        return BRW_KEY_PROCESSED;
    }

    // Ctrl-a or Super-a (select all)
    if ((modstate.bCtrl || modstate.bGui) && keysym == SDLK_a) {
		iSelectionStartLine = 0;
		iSelectionStartColumn = 0;
		if (tLines.size())  {
			iCursorLine = iSelectionEndLine = tLines.size() - 1;
			iCursorColumn = iSelectionEndColumn = Utf8StringSize(tLines[iSelectionEndLine]->getPureText());
		} else {
			iCursorLine = iCursorColumn = iSelectionEndLine = iSelectionEndColumn = 0;
		}
        return BRW_KEY_PROCESSED;
    }

	// If a mod key has been pressed, pretend we handled it to prevent taking off focus
	if (keysym == SDLK_LSHIFT || keysym == SDLK_RSHIFT || keysym == SDLK_LCTRL || keysym == SDLK_RCTRL ||
		keysym == SDLK_LALT || keysym == SDLK_RALT)
		return BRW_KEY_PROCESSED;

	return BRW_KEY_NOT_PROCESSED;
}

////////////////////
// Allocates the buffer surface and/or resizes it
void CBrowser::AdjustBuffer()
{
	// Allocate
	if (!bmpBuffer.get())  {
		bmpBuffer = gfxCreateSurfaceAlpha(iWidth, iHeight);
		if (!bmpBuffer.get())
			return;
	}

	// Adjust the buffer size if necessary
	if (bmpBuffer->w != iWidth || bmpBuffer->h != iHeight)  {
		bmpBuffer = NULL;
		bmpBuffer = gfxCreateSurfaceAlpha(iWidth, iHeight);
		if (!bmpBuffer.get())
			return;
	}
}

////////////////////////
// Draws the cursor
void CBrowser::DrawCursor(SDL_Surface *bmpDest)
{
	if (iCursorLine >= tLines.size())
		return;

	// Check if the cursor is displayed
	if (bUseScroll)  {
		int scroll_y = cScrollbar.getValue() * DEFAULT_LINE_HEIGHT;
		int line_y = tLines[iCursorLine]->getDocumentY();

		if (line_y < scroll_y || line_y > scroll_y + iHeight)
			return;
	}

	if (bFocused && bDrawCursor)  {
		int x, y;
		CursorPosToMousePos(iCursorColumn, iCursorLine, x, y);
		DrawVLine(bmpDest, y, y + tLines[iCursorLine]->getHeight(), x, tLX->clTextboxCursor);
	}
}


///////////////////
// Render the browser
void CBrowser::Draw(SDL_Surface * bmpDest)
{
	if (bNeedsRender)
		ReRender();

	DrawImage(bmpDest, bmpBuffer.get(), iX, iY);
	DrawCursor(bmpDest);

	// Scrollbar
	if (bUseScroll && bShowScrollbar)  {
		cScrollbar.Draw(bmpDest);
	}
}

/////////////////////
// Converts mouse position to cursor position
void CBrowser::MousePosToCursorPos(int ms_x, int ms_y, size_t& cur_x, size_t& cur_y)
{
	cur_x = cur_y = 0;

	if (tLines.empty())
		return;

	// Get the line
	size_t line = 0;
	std::vector<CBrowserLine *>::const_iterator ln = tLines.begin();
	int scroll_y = bUseScroll ? cScrollbar.getValue() * DEFAULT_LINE_HEIGHT : 0;
	int doc_y = scroll_y + ms_y - iY - iBorderSize;

	for (; ln != tLines.end(); ln++)  {
		if (doc_y < (*ln)->getDocumentY() + (*ln)->getHeight())
			break;
		++line;
	}

	// If clicked below all lines, take the last one
	if (ln == tLines.end())  {
		line--;
		ln--;
	}
	
	int x = ms_x - iX;

	// Get the column
	size_t column = 0;
	if (x > 0)  {
		std::list<CBrowserObject *>::iterator obj = (*ln)->getObjects().begin();
		for (; obj != (*ln)->getObjects().end(); obj++)  {
			bool text = (*obj)->getType() == CBrowserObject::objText;
			if (x >= (*obj)->tRect.x && x < (*obj)->tRect.x + (*obj)->tRect.w)  {
				if (text)
					column += GetPosByTextWidth(((CTextObject *)(*obj))->getText(), x - (*obj)->tRect.x, &tLX->cFont);
				break;
			}

			if (text)
				column += Utf8StringSize(((CTextObject *)(*obj))->getText());
		}
	}

	cur_x = column;
	cur_y = line;
}


////////////////////////
// Converts cursor position to a mouse (screen) position
void CBrowser::CursorPosToMousePos(size_t cur_x, size_t cur_y, int& ms_x, int& ms_y)
{
	ms_x = iX + iBorderSize;
	ms_y = iY + iBorderSize;

	// Get the Y coordinate
	if (cur_y >= tLines.size())
		return;

	int scroll = (bUseScroll ? cScrollbar.getValue() : 0);
	int y = 0;
	size_t i = 0;
	for (std::vector<CBrowserLine *>::iterator ln = tLines.begin(); i < cur_y; ln++)  {
		y += (*ln)->getHeight();
		++i;
	}
	y -= scroll * DEFAULT_LINE_HEIGHT;

	// Get the X coordinate
	size_t count = MIN(cur_x, Utf8StringSize(tLines[cur_y]->getPureText()));
	int x = tLines[cur_y]->getLeftMargin();
	size_t count_i = 0;
	for (std::list<CBrowserObject *>::const_iterator obj = tLines[cur_y]->getObjects().begin();
		obj != tLines[cur_y]->getObjects().end(); obj++)  {
			if ((*obj)->getType() == CBrowserObject::objText)  {
				CTextObject *o = (CTextObject *)(*obj);
				size_t txt_size = Utf8StringSize(o->getText());
				if (txt_size + count_i <= count)  {
					count_i += txt_size;
				} else {
					x = o->tRect.x + tLX->cFont.GetWidth(Utf8SubStr(o->getText(), 0, count - count_i));
					break;
				}
			}

			x = (*obj)->tRect.x + (*obj)->tRect.w;
	}

	ms_x = iX + x;
	ms_y = iY + y;
}

////////////////////
// Creates a new line and pushes the current one to the pure text stack
void CBrowser::EndLine()
{
	if(tCurrentLine == NULL) {
		errors << "CBrowser::EndLine: currentline == NULL" << endl;
		return;
	}
	
	// Add the line to the pure text and start a new line
	tLines.push_back(tCurrentLine);

	curY += tCurrentLine->getHeight();
	curX = iBorderSize + iCurIndent;
	iDocumentHeight += tCurrentLine->getHeight();

	tCurrentLine = new CBrowserLine(iCurIndent);

	tCurrentLine->setDocumentY(iDocumentHeight);
	tCurrentLine->setHeight(tLX->cFont.GetHeight());
	tCurrentLine->setMaxWidth(iWidth - 2*iBorderSize - iCurIndent - cScrollbar.getWidth());
}

////////////////////
// Converts the given URL to a standard format (accepts both absolute and relative URLs)
std::string CBrowser::GetFullURL(const std::string& url)
{
	// Absolute URL
	if (url.size() > 8)  {
		if (stringcaseequal(url.substr(0, 7), "http://") ||
			stringcaseequal(url.substr(0, 8), "https://") || stringcaseequal(url.substr(0, 6), "ftp://") ||
			stringcaseequal(url.substr(0, 7), "mailto:"))
			return url;

		if (stringcaseequal(url.substr(0, 4), "www."))
			return "http://" + url;
	}

	// Relative URL

	// Remove any slashes at the beginning of the given URL
	std::string link_url = url;
	if (link_url.size())
		if (*link_url.begin() == '/')
			link_url.erase(0);

	// Find the last slash of the base URL
	size_t slashpos = sURL.rfind('/');
	if (slashpos == std::string::npos)
		link_url = sHostName + "/" + sURL + "/" + url; // No slash in the base URL, just append the given URL
	else
		link_url = sHostName + "/" + sURL.substr(0, slashpos) + url; // Remove the filename from the base URL and append the given URL instead

	// Prepend HTTP if not present
	if (link_url.size() > 7)
		if (!stringcaseequal(link_url.substr(0, 7), "http://"))
			link_url = "http://" + link_url;

	return link_url;
}

//////////////////////////
// Starts a link area
void CBrowser::StartLink(const std::string& url)
{
	tCurrentContext.tCurrentGroup = new CLinkGroup(GetFullURL(url));
}

//////////////////////////
// Ends the link area
void CBrowser::EndLink()
{
	tGroups.push_back(tCurrentContext.tCurrentGroup);
}

/////////////////////////////
// Newly renders the content
void CBrowser::ReRender()
{
	AdjustBuffer();

	// Clear the surface
	FillSurfaceTransparent(bmpBuffer.get());

	// 3D Box
	if (iBorderSize > 0)  {
		DrawRect(bmpBuffer.get(), 0, 0, iWidth - 1, iHeight - 1, tLX->clBoxDark);
		DrawRect(bmpBuffer.get(), 1, 1, iWidth - 2, iHeight - 2, tLX->clBoxLight);
	}
	
	// Render the content
	RenderContent();

	bNeedsRender = false;
}


//////////////////////
// Add an object to the current line and group
void CBrowser::AddObject(CBrowser::CBrowserObject *obj)
{
	tCurrentLine->addObject(obj);
	if (tCurrentContext.tCurrentGroup)
		tCurrentContext.tCurrentGroup->addObject(obj);
	curX += obj->getWidth();
}

////////////////////////
// Adds a text to the line and performs necessary wrapping
void CBrowser::AddTextObject(const std::string& text)
{
	if (text.empty())
		return;

	// Create the text object and split it into more lines if necessary
	CTextObject *txt = new CTextObject(this, text, tCurrentContext.tFormat, curX, curY);
	while (curX + txt->getWidth() > tCurrentLine->getMaxWidth())  {
		std::pair<CTextObject *, CTextObject *> split = txt->Split(tCurrentLine->getMaxWidth() - curX);
		AddObject(split.first);

		if (split.second)  {
			txt = split.second;
			EndLine();
			txt->tRect.x = curX;
			txt->tRect.y = curY;
		} else  {
			txt = NULL;
			break;
		}
	}

	if (txt) // Add the last line
		AddObject(txt);
}

///////////////////////////
// Renders a chunk of text with the given style
void CBrowser::ParseText(const std::string& text)
{
	if(!text.size()) return;

	const FontFormat& fmt = tCurrentContext.tFormat;
	if (fmt.preformatted)  { // Keep newlines
		StringBuf str(text);
		std::vector<std::string> lines = str.splitBy('\n');
		for (std::vector<std::string>::iterator it = lines.begin(); it != lines.end(); it++)  {
			AddTextObject(*it);
			EndLine();
		}
	} else {
		std::string tmp;
		replace(text, "\r\n", " ", tmp);
		replace(tmp, "\n", " ", tmp);
		replace(tmp, "\t", " ", tmp);
		while (replace(tmp, "  ", " ", tmp)) {}  // Get rid of doubled blanks

		// Don't add blanks at the beginning of the line
		if (tCurrentLine->isEmpty())  {
			while (tmp.size() && isspace((uchar)*tmp.begin()))
				tmp.erase(0, 1);
		}

		AddTextObject(tmp);
	}
}

////////////////////
// Returns true if the given position is inside the selection
bool CBrowser::InSelection(size_t line, size_t column)
{
	if (line >= iSelectionStartLine && line <= iSelectionEndLine)  {
		if (line == iSelectionStartLine && line == iSelectionEndLine)
			return column >= iSelectionStartColumn && column < iSelectionEndColumn;
		if (line == iSelectionStartLine)
			return column >= iSelectionStartColumn;
		if (line == iSelectionEndLine)
			return column < iSelectionEndColumn;
		return true;
	}
	return false;
}

////////////////////
// Get text of the current selection
std::string CBrowser::GetSelectedText()
{
	// One line selection
	if (iSelectionStartLine == iSelectionEndLine)  {
		if (iSelectionStartLine < tLines.size() && iSelectionStartColumn < iSelectionEndColumn)  {
			size_t len = Utf8StringSize(tLines[iSelectionStartLine]->getPureText());
			if (iSelectionStartColumn < len && iSelectionEndColumn <= len)
				return Utf8SubStr(tLines[iSelectionStartLine]->getPureText(), iSelectionStartColumn, iSelectionEndColumn - iSelectionStartColumn);
		}

	// More lines
	} else {
		// Safety checks
		if (iSelectionStartLine > iSelectionEndLine || iSelectionStartLine >= tLines.size() ||
			iSelectionEndLine >= tLines.size())
			return "";

		std::string res;

		// First line
		if (iSelectionStartColumn < Utf8StringSize(tLines[iSelectionStartLine]->getPureText()))  {
			res += Utf8SubStr(tLines[iSelectionStartLine]->getPureText(), iSelectionStartColumn);
			res += '\n';
		}

		// All other lines
		int diff = iSelectionEndLine - iSelectionStartLine;
		for (int i = 1; i < diff; ++i)  {
			res += tLines[iSelectionStartLine + i]->getPureText() + "\n";
		}

		// Last line
		if (iSelectionEndColumn <= Utf8StringSize(tLines[iSelectionEndLine]->getPureText()))
			res += Utf8SubStr(tLines[iSelectionEndLine]->getPureText(), 0, iSelectionEndColumn);

		return res;
	}

	return "";
}


//////////////////////
// Destroys any selection
void CBrowser::ClearSelection()
{
	iSelectionStartLine = iSelectionEndLine = iSelectionGrabLine = iCursorLine;
	iSelectionStartColumn = iSelectionEndColumn = iSelectionGrabColumn = iCursorColumn;
	bNeedsRender = true;
}

///////////////////////
// Returns true if no text is selected
bool CBrowser::IsSelectionEmpty()
{
	return iSelectionStartLine == iSelectionEndLine && iSelectionStartColumn == iSelectionEndColumn;
}


////////////////////////
// Exchanges the start and end points of the selection of necessary
void CBrowser::SwapSelectionEnds()
{
	if (iSelectionStartLine > iSelectionEndLine)  {
		size_t tmp = iSelectionStartLine;
		iSelectionStartLine = iSelectionEndLine;
		iSelectionEndLine = tmp;

		tmp = iSelectionStartColumn;
		iSelectionStartColumn = iSelectionEndColumn;
		iSelectionEndColumn = tmp;
	} else if (iSelectionStartLine == iSelectionEndLine && iSelectionStartColumn > iSelectionEndColumn)  {
		size_t tmp = iSelectionStartColumn;
		iSelectionStartColumn = iSelectionEndColumn;
		iSelectionEndColumn = tmp;
	}
}

////////////////////////
// Makes sure that the line with the cursor is visible
void CBrowser::AdjustScrollbar(bool mouse)
{
	if (!bUseScroll)
		return;

	// If scrolling using mouse, don't scroll so fast
	if (mouse && tLX->currentTime - fLastMouseScroll <= 0.1f)
		return;

	if (iCursorLine >= tLines.size())
		return;

	int scroll_y = cScrollbar.getValue() * DEFAULT_LINE_HEIGHT;
	int line_y = tLines[iCursorLine]->getDocumentY();
	int line_h = tLines[iCursorLine]->getHeight();

	// Scroll up if necessary
	while (scroll_y > line_y && cScrollbar.getValue() > 0)  {
		cScrollbar.setValue(cScrollbar.getValue() - 1);
		scroll_y -= DEFAULT_LINE_HEIGHT;
	}

	// Scroll down if necessary
	while (scroll_y + iHeight < line_y + line_h && cScrollbar.getValue() <= cScrollbar.getMax())  {
		cScrollbar.setValue(cScrollbar.getValue() + 1);
		scroll_y += DEFAULT_LINE_HEIGHT;
	}

	if (mouse)
		fLastMouseScroll = tLX->currentTime;

	bNeedsRender = true;
}


//////////////////////
// Helper function for WalkNodes below
void CBrowser::WalkChildren(xmlNodePtr node)
{
	// Process the children
	if (node->children)  {
		xmlNodePtr child = node->children;
		while (child)  {
			WalkNodes(child);
			child = child->next;
		}
	}
}


////////////////////////
// Recursivelly walks through the nodes
void CBrowser::WalkNodes(xmlNodePtr node)
{
	if (!node)
		return;

	if (!xmlStrcasecmp(node->name, (xmlChar *)"style")) return;
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"script")) return;
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"embed")) return;
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"object")) return;
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"head")) return;
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"title")) return;

	// Bold
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"b") || !xmlStrcasecmp(node->name, (xmlChar *)"strong"))  {
		PushContext();
		tCurrentContext.tFormat.bold = true;
		WalkChildren(node);
		PopContext();
		return;
	}

	// Underline
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"u"))  {
		PushContext();
		tCurrentContext.tFormat.underline = true;
		WalkChildren(node);
		PopContext();
		return;
	}

	// Color
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"font"))  {
		PushContext();
		tCurrentContext.tFormat.color = xmlGetColour(node, "color", tCurrentContext.tFormat.color);
		WalkChildren(node);
		PopContext();
		return;
	}

	// Newline
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"br"))  {
		EndLine();
		return;
	}

	// Common block elements
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"p") || !xmlStrcasecmp(node->name, (xmlChar *)"div")
		|| !xmlStrcasecmp(node->name, (xmlChar *)"ul") || !xmlStrcasecmp(node->name, (xmlChar *)"ol"))  {
		// Don't create a blank newline (classical web browsers don't do that either)
		if (!tCurrentLine->isEmpty())
			EndLine();
		WalkChildren(node);
		return;
	}

	// List item
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"li"))  {
		CListItemObject *li = new CListItemObject(this, curX, curY);
		AddObject(li);
		iCurIndent = li->getWidth();
		WalkChildren(node);
		iCurIndent = 0; // Back to normal indentation
		EndLine();
		return;
	}

	// Pre
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"pre"))  {
		if (!tCurrentLine->isEmpty())
			EndLine();
		PushContext();
		tCurrentContext.tFormat.preformatted = true;
		WalkChildren(node);
		EndLine();
		PopContext();
		return;
	}

	// Headings (h1 - h6)
	else if (xmlStrlen(node->name) == 2 && node->name[0] == 'h' && node->name[1] >= '1' && node->name[1] <= '6')  {
		PushContext();
		tCurrentContext.tFormat.bold = true;
		EndLine();
		WalkChildren(node);
		EndLine();
		PopContext();
		return;
	}

	// Link (<a href= ...)
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"a"))  {
		std::string url = xmlGetString(node, "href");
		if (url.size())  {	
			// Setup the format (blue underlined text by default)
			PushContext();
			tCurrentContext.tFormat.color = Color(0, 0, 255);
			tCurrentContext.tFormat.underline = true;

			tCurrentContext.tCurrentGroup = new CLinkGroup(GetFullURL(url));

			// Get the content of the link
			WalkChildren(node);

			tGroups.push_back(tCurrentContext.tCurrentGroup);

			// Restore the old format
			PopContext();
		}

		return;
	}

	// Horizontal rule
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"hr"))  {
		if (!tCurrentLine->isEmpty())
			EndLine();
		AddObject(new CHorizontalRuleObject(this, xmlGetColour(node, "color", Color(0, 0, 0)), 
			MakeRect(curX + 5, curY, tCurrentLine->getMaxWidth() - 11, tCurrentLine->getHeight())));
		EndLine();
		return;
	}

	// Body
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"body"))  {
		tBgColor = xmlGetColour(node, "bcolor", tBgColor);
	}

	// Plain text
	else if (!xmlStrcasecmp(node->name, (xmlChar *)"text") && node->content)
		ParseText((const char *)node->content);

	WalkChildren(node);
}

////////////////////////
// Helper function for DrawSelection
void CBrowser::DrawSelectionForText(CBrowser::CBrowserObject *obj, size_t& line, size_t& column, int scroll_y)
{
	assert(obj->getType() == CBrowserObject::objText);

	CTextObject *o = (CTextObject *)obj;
	size_t len = Utf8StringSize(o->getText());
	int x2 = o->tRect.x + o->tRect.w;
	int x = o->tRect.x;

	bool sel = false; // False if the selection should not be drawn at all

	// Beginning in selection?
	if (!InSelection(line, column + len))
		x2 = o->tRect.x + tLX->cFont.GetWidth(Utf8SubStr(o->getText(), 0, iSelectionEndColumn - column));
	else
		sel = true;

	// End in selection?
	if (!InSelection(line, column))
		x = o->tRect.x + tLX->cFont.GetWidth(Utf8SubStr(o->getText(), 0, iSelectionStartColumn - column));
	else
		sel = true;

	// Even if the beginning and end are not in the selection, the object can contain a selection inside, for example:
	// This is some |selected| text
	if (sel || (iSelectionStartColumn >= column && iSelectionEndColumn <= column + len && iSelectionStartLine == iSelectionEndLine))
		DrawRectFill(bmpBuffer.get(), x, o->tRect.y - scroll_y, x2, 
			o->tRect.y + o->tRect.h - scroll_y, tLX->clSelection);

	column += len;
}

///////////////////////////
// Helper function for DrawSelection
// Gets the upper and lower lines of the selection, that are visible
// Returns false if no lines are visible
bool CBrowser::GetDrawSelectionBounds(size_t &top_line, size_t &bot_line)
{
	// No selection or lines out of bounds
	if (IsSelectionEmpty() || iSelectionStartLine >= tLines.size() || iSelectionEndLine >= tLines.size())
		return false;

	int scroll_y = (bUseScroll ? cScrollbar.getValue() * DEFAULT_LINE_HEIGHT : 0);
	top_line = iSelectionStartLine;
	bot_line = iSelectionEndLine;

	// Exclude the lines that are out of the box
	while (tLines[top_line]->getDocumentY() - scroll_y < 0 && top_line < tLines.size() - 1)
		++top_line;
	while (tLines[bot_line]->getDocumentY() - scroll_y > iHeight && bot_line)
		--bot_line;

	return bot_line >= top_line;
}

////////////////////////
// Draws the selection
void CBrowser::DrawSelection()
{
	size_t sel_start_line, sel_end_line;

	// Don't draw if no selection
	if (!GetDrawSelectionBounds(sel_start_line, sel_end_line))
		return;

	int scroll_y = (bUseScroll ? cScrollbar.getValue() * DEFAULT_LINE_HEIGHT : 0);

#define FOR_EACH_OBJECT(line) for (std::list<CBrowserObject *>::const_iterator obj = (line)->getObjects().begin(); obj != (line)->getObjects().end(); obj++)

	std::vector<CBrowserLine *>::iterator it = tLines.begin() + sel_start_line;
	std::vector<CBrowserLine *>::iterator end = tLines.begin() + sel_end_line + 1;

	// Draw all lines of the selection
	size_t line = sel_start_line;
	for (; it != end; it++)  {
		size_t column = 0;
		FOR_EACH_OBJECT(*it)  {
			if (!(*obj)->isSelectable())
				continue;

			// Text object selection
			if ((*obj)->getType() == CBrowserObject::objText)  {
				DrawSelectionForText((*obj), line, column, scroll_y); // HINT: column gets updated here
				continue;
			}

			// Don't draw if not in selection
			if (!InSelection(line, column))
				continue;

			// Blue filled rect under the object
			DrawRectFill(bmpBuffer.get(), (*obj)->tRect.x, (*obj)->tRect.y - scroll_y, (*obj)->tRect.x + (*obj)->tRect.w, 
				(*obj)->tRect.y + (*obj)->tRect.h - scroll_y, tLX->clSelection);
		}

		++line;
	}
}

//////////////////
// Renders the textual content
void CBrowser::RenderContent()
{
	// Nothing to draw
	if (!tHtmlDocument || !tRootNode)
	{
		// Background (transparent by default)
		//DrawRectFill(bmpDest, iX + iBorderSize, iY + iBorderSize, iX + iWidth, iY + iHeight, tLX->clChatBoxBackground);
		return;
	}

	// Background
	DrawRectFill(bmpBuffer.get(), iBorderSize, iBorderSize, iWidth, iHeight, tBgColor);

	// Setup the clipping
	SDL_Rect clipRect = {(SDLRect::Type) iBorderSize, (SDLRect::Type) iBorderSize,
						(SDLRect::TypeS) (iWidth - iBorderSize * 2), (SDLRect::TypeS) (iHeight - iBorderSize * 2)};
	ScopedSurfaceClip clip(bmpBuffer.get(), clipRect);

	// Selection
	DrawSelection();

	size_t start_y = bUseScroll ? cScrollbar.getValue() * DEFAULT_LINE_HEIGHT : 0;
	start_y += iBorderSize;

	size_t y = iBorderSize;
	size_t line = 0;
	for (std::vector<CBrowserLine *>::iterator it = tLines.begin(); it != tLines.end(); it++, line++)  {
		// Don't draw above the window area
		if (y < start_y)  {
			y += (*it)->getHeight();
			continue;
		}

		// Render the objects
		for (std::list<CBrowserObject *>::const_iterator obj = (*it)->getObjects().begin(); obj != (*it)->getObjects().end(); obj++)  {
			(*obj)->Render(bmpBuffer.get(), (*obj)->tRect.x, y - start_y);
		}

		y += (*it)->getHeight();

		// Don't draw below the window area
		if (y - start_y > (size_t)iHeight)
			break;
	}

	ResetScrollbar();
}

///////////////////////
// Shows or hides the scrollbar
void CBrowser::ResetScrollbar() {
	int linesperbox = iHeight / DEFAULT_LINE_HEIGHT;
	if (iDocumentHeight > (size_t)(iHeight - 2 * iBorderSize))  {
		cScrollbar.Setup(0, iX + iWidth - 14 - iBorderSize - 1, iY + iBorderSize, 14, iHeight - 2*iBorderSize);
		cScrollbar.setMax((iDocumentHeight / DEFAULT_LINE_HEIGHT));
		cScrollbar.setItemsperbox(linesperbox);
		bUseScroll = true;
	} else  {
		cScrollbar.setValue(0);
		bUseScroll = false;	
	}
}

/////////////////////////
// Cursor blink timer handler
void CBrowser::OnTimerEvent(Timer::EventData ev)
{
	bDrawCursor = !bDrawCursor;
}

//
// Chatbox routines
//

static const char * txtTypeNames[] = {
	"CHAT",
	"NORMAL",
	"NOTICE",
	"IMPORTANT",
	"NETWORK",
	"PRIVATE",
	"TEAMPM"
};

//////////////////////////
// Initialize as a chatbox
void CBrowser::InitializeChatBox( const std::string & initText )
{
	if (initText.size())
		LoadFromString(initText);
	else
		LoadFromString("<html><body bgcolor=\"transparent\"></body></html>");
}

///////////////////////////////
// Add a new line to the browser
void CBrowser::AddChatBoxLine(const std::string & text, Color color, TXT_TYPE textType, bool bold, bool underline)
{
	bool shouldScrollDown = false;
	// If we have scrolled nearly down, scroll down to the new added line.
	// If we are in the middle, don't scroll to the end. (That is the default
	// behaviour in most application and it makes sense at it would be impossible
	// to read old text if frequenlty new messages arrive.)
	// TODO: If you want to have it different, leave this the default and make an option.
	// TODO: getMax() is not what one would expect (see comment in CScrollbar). Indead
	// getMax()-getItemsperbox() is hopefully the real max value if I am not wrong.
	static const int maxLineDiffToEnd = 5;
	if( cScrollbar.getMax() - cScrollbar.getItemsperbox() - cScrollbar.getValue() < maxLineDiffToEnd )
		shouldScrollDown = true;
	
	if (textType < TXT_CHAT || textType > TXT_TEAMPM)
		textType = TXT_NOTICE; // Default

	// Format
	char string_color[20];
	// TODO: don't use sprintf!
	sprintf(string_color, "#%02X%02X%02X", (unsigned)color.r, (unsigned)color.g, (unsigned)color.b);

	std::string code = "<font color=\"" + std::string(string_color) + "\">" + 
		AutoDetectLinks(HtmlEntityUnpairedBrackets(text)) + "</font>";

	if (bold)
		code = "<b>" + code + "</b>";
	if (underline)
		code = "<u>" + code + "</u>";

	// Wrap it to a div for better management
	code = "<html><body><div class=\"" + std::string(txtTypeNames[textType]) + "\">" + code + "</div></body></html>";

	// Parse the line as HTML and insert the nodes
	AppendData(code);
	
	// Scroll to last line
	if (bUseScroll && shouldScrollDown) {
		ScrollToLastLine();
	}
}

//////////////////////
// Get all the text from the chatbox browser, as HTML
std::string CBrowser::GetChatBoxText()
{
	if (!tHtmlDocument || !tRootNode)
		return "";

	xmlChar *xmlbuff = NULL;
	int buffersize = 0;
	xmlDocDumpFormatMemory(tHtmlDocument, &xmlbuff, &buffersize, 1);
	std::string ret( (const char *) xmlbuff );
	xmlFree(xmlbuff);

	return ret;
}

//////////////////////
// Remove all network messages from chatbox, and remove extra lines if more than 127 lines
void CBrowser::CleanUpChatBox(const std::vector<TXT_TYPE> & removedText, int maxLineCount)
{
	if (!tHtmlDocument || !tRootNode)
		return;
	
	int lineCount = 0;
	for (xmlNodePtr node = tRootNode->children; node != NULL; )  {
		xmlNodePtr nextNode = node->next;
		lineCount++;

		// Remove all elements that are not in a div (these should not appear in the chatbox)
		if (xmlStrcasecmp(node->name, (xmlChar *)"div") != 0 )  {
			xmlUnlinkNode(node);
			xmlFreeNode(node);
			lineCount --;
		} else {
			std::string divClass = xmlGetString(node, "class", "CHAT");

			// Unlink all the non-chat nodes
			for (size_t i = 0; i < removedText.size(); i++ )
				if (txtTypeNames[removedText[i]] == divClass)  {
					xmlUnlinkNode(node);
					xmlFreeNode(node);
					lineCount--;
					break;
				}
		}
	
		node = nextNode;
	}
	
	// Free lines that exceed the maximum allowed chatbox size
	for (xmlNodePtr node = tRootNode->children; node != NULL && lineCount > maxLineCount; lineCount--)  {
		xmlUnlinkNode(node);
		xmlFreeNode(node);
	}

	tRootNode = xmlDocGetRootElement(tHtmlDocument); // Safety

	// Synchronize the XML tree and the rendering objects
	UpdateRenderObjects();

	ScrollToLastLine();
}

////////////////////////
// Scrolls to the end of the document
void CBrowser::ScrollToLastLine()
{
	if (bUseScroll)  {
		cScrollbar.setValue(cScrollbar.getMax());
		bNeedsRender = true;
	}
}

}; // namespace DeprecatedGUI
