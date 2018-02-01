/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Font Handling
// Created 22/7/08
// Karel Petranek

#include "FontHandling.h"
#include "StringUtils.h"
#include "GfxPrimitives.h"
#include "CFont.h"
#include "MathLib.h"
#include "LieroX.h"


///////////////////
// Apply CSS to the font style
void CFontStyle::ApplySelector(const CSSParser::Selector& sel, const std::string& prefix)
{
	FOR_EACH_ATTRIBUTE(sel, it)  {
		if (it->getName() == prefix + "font-family")  {
			sFontName.set(it->getFirstValue().getString("default"), it->getPriority());
		} else if (it->getName() == prefix + "color" || it->getName() == prefix + "colour")  {
			iColor.set(it->getFirstValue().getColor(iColor), it->getPriority());
		} else if (it->getName() == prefix + "font-background-color" || it->getName() == prefix + "font-background-colour")  {
			iBackgroundColor.set(it->getFirstValue().getColor(iBackgroundColor), it->getPriority());
		} else if (it->getName() == prefix + "font-weight")  {
			if (stringcaseequal(it->getFirstValue().getString(), "bold"))
				bBold.set(true, it->getPriority());
			else
				bBold.set(false, it->getPriority());
		} else if (it->getName() == prefix + "text-decoration")  {
			// Go through the values
			for (std::vector<CSSParser::Attribute::Value>::const_iterator val = it->getParsedValue().begin(); val != it->getParsedValue().end(); val++)  {
				if (stringcaseequal(val->getString(), "underline"))
					bUnderline.set(true, it->getPriority());
				else if (stringcaseequal(val->getString(), "overline"))
					bOverline.set(true, it->getPriority());
				else if (stringcaseequal(val->getString(), "line-through"))
					bStrikeThrough.set(true, it->getPriority());
			}
		} else if (it->getName() == prefix + "font-style")  {
			if (stringcaseequal(it->getFirstValue().getString(), "italics"))  {
				bItalics.set(true, it->getPriority());
			} else {
				bItalics.set(false, it->getPriority());
			}
		} else if (it->getName() == prefix + "font-size")  {
			iSize.set(it->getFirstValue().getInteger(12), it->getPriority());
		} else if (it->getName() == prefix + "vertical-spacing")  {
			iVSpacing.set(it->getFirstValue().getInteger(2), it->getPriority());
		} else if (it->getName() == prefix + "horizontal-spacing")  {
			iHSpacing.set(it->getFirstValue().getInteger(0), it->getPriority());
		} else if (it->getName() == prefix + "font-texture")  {
			bmpTexture.set(LoadGameImage(it->getFirstValue().getURL()), it->getPriority());
		}
	}
}

////////////////////
// Apply a selector to the given text
void CTextProperties::ApplySelector(const CSSParser::Selector &sel, const std::string &prefix)
{
	FOR_EACH_ATTRIBUTE(sel, it)  {
		if (it->getName() == prefix + "text-align")  {
			if (stringcaseequal(it->getFirstValue().getString(), "left"))
				iHAlignment.set(algLeft, it->getPriority());
			else if (stringcaseequal(it->getFirstValue().getString(), "center"))
				iHAlignment.set(algCenter, it->getPriority());
			else if (stringcaseequal(it->getFirstValue().getString(), "right"))
				iHAlignment.set(algRight, it->getPriority());
			else if (stringcaseequal(it->getFirstValue().getString(), "justify"))
				iHAlignment.set(algJustify, it->getPriority());
		} else if (it->getName() == prefix + "text-valign")  {
			if (stringcaseequal(it->getFirstValue().getString(), "top"))
				iVAlignment.set(algTop, it->getPriority());
			else if (stringcaseequal(it->getFirstValue().getString(), "middle"))
				iVAlignment.set(algMiddle, it->getPriority());
			else if (stringcaseequal(it->getFirstValue().getString(), "bottom"))
				iVAlignment.set(algBottom, it->getPriority());
		} else if (it->getName() == prefix + "overflow")  {
			if (stringcaseequal(it->getFirstValue().getString(), "clip"))  {
				// This is default, just remove any other clipping possibilities
				bThreeDotsEnd.set(false, it->getPriority());
				bWrap.set(false, it->getPriority());
			} else if (stringcaseequal(it->getFirstValue().getString(), "strip"))  {
				bThreeDotsEnd.set(true, it->getPriority());
			} else if (stringcaseequal(it->getFirstValue().getString(), "wrap"))  {
				bWrap.set(true, it->getPriority());
			}
		}
	}
}

/**************************
*
* Font handling
*
***************************/

#include <map>

// A very simple cache, should be improved and moved to CCache later
std::map<std::string, CFont *> fontCache;

CFont *GetFont(const std::string& name)
{
	std::map<std::string, CFont *>::iterator it = fontCache.find(name);
	if (it == fontCache.end())  {
		CFont *tmp = new CFont();
		if (!tmp->Load(name, true))  {
			delete tmp;
			tmp = NULL;
		}
		fontCache[name] = tmp;
		return tmp;
	}

	return it->second;
}

void ShutdownFontCache()
{
	for (std::map<std::string, CFont *>::iterator it = fontCache.begin(); it != fontCache.end(); it++)  {
		it->second->Shutdown();
		delete it->second;
	}
	fontCache.clear();
}


static void StripTextWithDots(std::string& buf, CFont *font, int width)
{
	if (buf.size() == 0)
		return;

	int dotwidth = font->GetWidth("...");
	bool stripped = false;
	for(size_t j=buf.length()-1; font->GetWidth(buf) > width && j != 0; j--)  {
		buf.erase(buf.length()-1);
		stripped = true;
	}

	// Make space for the dots
	if (stripped)  {
		for(size_t j=buf.length()-1; font->GetWidth(buf) > width-dotwidth && j != 0; j--)
			buf.erase(buf.length()-1);
	}

	if(stripped)
		buf += "...";
}

/////////////////////
// Draws the text to the surface according to the style & positions given
void DrawGameText(SDL_Surface *bmpDest, const std::string& text, const CFontStyle& style, const CTextProperties& prop)
{
	CFont *fnt = GetFont(style.sFontName);
	if (!fnt)  {
		return;
	}

	fnt->SetVSpacing(style.iVSpacing);
	fnt->SetSpacing(style.iHSpacing);

	// Save the clipping rect
	SDL_Rect oldrect, fontrect;
	SDL_GetClipRect(bmpDest, &oldrect);
	fontrect = oldrect;
	if (prop.tFontRect)  {
		fontrect = *prop.tFontRect;
	}
	ScopedSurfaceClip clip(bmpDest, fontrect);
	
	int x = fontrect.x;
	switch (prop.iHAlignment) {
		case algLeft:
			x = fontrect.x;
			break;
		case algCenter:
			x = MAX(prop.iCentreMinX, fontrect.x + (fontrect.w - fnt->GetWidth(text)) / 2);
			break;
		case algRight:
			x = fontrect.x + fontrect.w - fnt->GetWidth(text);
			break;
		case algJustify:
		// TODO: justify
			break;
	}

	int y = fontrect.y;
	switch (prop.iVAlignment)  {
		case algTop:
			y = fontrect.y;
			break;
		case algMiddle:
			y = MAX(prop.iCentreMinY, fontrect.y + (fontrect.h - fnt->GetHeight(text)) / 2);
			break;
		case algBottom:
			y = fontrect.y + fontrect.h - fnt->GetHeight(text);
			break;
	}

	std::string txt = text;
	if (prop.bThreeDotsEnd)  {
		StripTextWithDots(txt, fnt, fontrect.w);
	}

	// Draw it
	fnt->Draw(bmpDest, x, y, style.iColor, txt);
}

///////////////////
// Draw a text onto a surface
void DrawGameText(SDL_Surface *bmpDest, int x, int y, const std::string& text)
{
	SDL_Rect r = { (SDLRect::Type) x, (SDLRect::Type) y, 9999, 9999 };
	DrawGameText(bmpDest, text, CFontStyle(), CTextProperties(&r));
}


/////////////////////
// Get a width of the given text (handles newlines correctly)
int GetTextWidth(const CFontStyle& style, const std::string& text)
{
	CFont *fnt = GetFont(style.sFontName);
	if (!fnt)  {
		return 0;
	}

	fnt->SetVSpacing(style.iVSpacing);
	fnt->SetSpacing(style.iHSpacing);

	int w = fnt->GetWidth(text);
	return w;
}

///////////////////
// Get the height of the text
int GetTextHeight(const CFontStyle& style, const std::string& text)
{
	CFont *fnt = GetFont(style.sFontName);
	if (!fnt)  {
		return 0;
	}

	fnt->SetVSpacing(style.iVSpacing);
	fnt->SetSpacing(style.iHSpacing);

	int h = fnt->GetHeight(text);
	return h;
}

///////////////////////////////
// Returns position in the text specified by the substring pixel width
size_t GetPosByTextWidth(const std::string& text, int width, CFont *fnt)
{
	std::string::const_iterator it = text.begin();
	int w = 0;
	int next_w = 0;
	size_t res = 0;
	while (it != text.end())  {

		UnicodeChar c = GetNextUnicodeFromUtf8(it, text.end());
		w = next_w;
		next_w += fnt->GetCharacterWidth(c) + fnt->GetSpacing();

		if (w <= width && width <= next_w)
			break;

		++res;
	}

	return res;
}



static bool strip(std::string::const_iterator& it, const std::string::const_iterator& end, size_t width, std::string& res, size_t& resw)
{
	// TODO: this width depends on tLX->cFont; this is no solution, fix it
	resw = 0;
	res = "";
	if(it == end) return false;
	
	while(it != end) {
		UnicodeChar c = GetNextUnicodeFromUtf8(it, end);
		int cw = tLX->cFont.GetCharacterWidth(c) + tLX->cFont.GetSpacing();
		if(cw + resw > width) return true;
		resw += cw;
		res += GetUtf8FromUnicode(c);
	}

	return false;
}

bool stripdot(std::string& buf, int width)
{
	if (buf.size() == 0)
		return false;
	
	// TODO: this width depends on tLX->cFont; this is no solution, fix it	
	
	width += tLX->cFont.GetSpacing(); // add one spacing because we can ignore the last
	
	int dotwidth = tLX->cFont.GetCharacterWidth('.') + tLX->cFont.GetSpacing();
	if(dotwidth * 3 > width) {
		const int dotcount = width / dotwidth;
		buf = std::string(dotcount, '.');
		return true;
	}
	dotwidth *= 3;
	
	std::string::const_iterator it = buf.begin();
	size_t resw = 0;
	std::string res;
	if(!strip(it, buf.end(), width - dotwidth, res, resw))
		return false;
	
	std::string rest;
	if(!strip(it, buf.end(), dotwidth, rest, resw))
		return false;
	
	buf = res + "...";
	return true;
}


//////////////////
// Splits the str in two pieces, part before space and part after space, if no space is found, the second string is empty
// Used internally by splitstring
static void split_by_space(const std::string& str, std::string& before_space, std::string& after_space)
{
	size_t spacepos = str.rfind(' ');
	if (spacepos == std::string::npos || spacepos == str.size() - 1 || str == "")  {
		before_space = str;
		after_space = "";
	} else {
		before_space = str.substr(0, spacepos);
		after_space = str.substr(spacepos + 1); // exclude the space
	}
}


//////////////////////
// Splits the string to pieces that none of the pieces can be longer than maxlen and wider than maxwidth
// TODO: maxlen is the raw len of the next, not the unicode len
std::vector<std::string> splitstring(const std::string& str, size_t maxlen, size_t maxwidth, CFont& font)
{
	std::vector<std::string> result;
	
	// Check
	if (!str.size())
		return result;

	std::string::const_iterator it = str.begin();
	std::string::const_iterator last_it = str.begin();
	size_t i = 0;
	std::string token;

	for (it++; it != str.end(); i += IncUtf8StringIterator(it, str.end()))  {

		// Check for maxlen
		if( i > maxlen )  {
			std::string before_space;
			split_by_space(token, before_space, token);
			result.push_back(before_space);
			i = token.size();
		}

		// Check for maxwidth
		if( (size_t)font.GetWidth(token) <= maxwidth && (size_t)font.GetWidth(token + std::string(last_it, it)) > maxwidth ) {
			std::string before_space;
			split_by_space(token, before_space, token);
			result.push_back(before_space);
			i = token.size();
		}

		// handle newline in str
		if( *it == '\n' ) {
			result.push_back(token + std::string(last_it, it));
			token = "";
			i = 0;
			last_it = it;
			++last_it; // don't add the '\n' to token
			continue;
		}
		
		// Add the current bytes to token
		token += std::string(last_it, it);

		last_it = it;
	}

	 // Last token
	result.push_back(token + std::string(last_it, it));

	return result;
}

std::string splitStringWithNewLine(const std::string& str, size_t maxlen, size_t maxwidth, class CFont& font) {
	std::vector<std::string> lines = splitstring(str, maxlen, maxwidth, font);
	std::string ret = "";
	for(std::vector<std::string>::iterator i = lines.begin(); i != lines.end(); ++i) {
		if(i != lines.begin()) ret += "\n";
		ret += *i;
	}
	return ret;
}
