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


// Font class
// Created 15/7/01
// Jason Boettcher


#include "LieroX.h"

#include "GfxPrimitives.h"
#include "PixelFunctors.h"
#include "Unicode.h"
#include "MathLib.h"


//
// IMPORTANT: all surfaces in CFont are alpha blended which means they can (and mostly do) have another format than screen.
// Because of this we cannot use almost any of the color handling functions from GfxPrimitives
// Format of the surfaces is guaranted to be 32bit ARGB
//


// For font drawing use FontGenerator tool in /tools/fontgenerator

///////////////////
// Load a font
int CFont::Load(const std::string& fontname, bool _colour) {

	// Load the font
	LOAD_IMAGE_WITHALPHA(bmpFont, fontname);

	// Set the color key for this alpha surface
	SetColorKey(bmpFont.get(), 255, 0, 255);

	Colorize = _colour;

	bmpWhite = gfxCreateSurfaceAlpha(bmpFont.get()->w, bmpFont.get()->h);
	bmpGreen = gfxCreateSurfaceAlpha(bmpFont.get()->w, bmpFont.get()->h);

	// Calculate the width of each character and number of characters
	Parse();

	// Pre-calculate some colours
	f_white = tLX->clNormalLabel;
	f_green = tLX->clChatText;

	// Precache some common font colors (but only if this font should be colorized)
	if (Colorize) {
		PreCalculate(bmpWhite, f_white);
		PreCalculate(bmpGreen, f_green);
	}

	return true;
}


///////////////////
// Shutdown the font
void CFont::Shutdown() {
}


//////////////////
// Helper function for CalculateWidth
// Checks, whether a vertical line is free
// NOTE: bmpFont must be locked before calling this
bool CFont::IsColumnFree(int x) {
	// it's only completelly see through
	for (int y = 0; y < bmpFont.get()->h; y++) {
		if ((GetPixel(bmpFont.get(), x, y) & bmpFont->format->Amask) != 0)
			return false;
	}

	return true;
}

///////////////////
// Calculate character widths, number of characters and offsets
void CFont::Parse() {
	int x;
	UnicodeChar CurChar = FIRST_CHARACTER;
	int cur_w;

	// Lock the surface
	LOCK_OR_QUIT(bmpFont);

	Uint32 blue = SDL_MapRGB(bmpFont.get()->format, 0, 0, 255);

	// a blue pixel always indicates a new char exept for the first

	uint char_start = 0;
	for (x = 0; x < bmpFont.get()->w; x++) {
		// x is always one pixel behind a blue line or x==0

		// Ignore any free pixel columns before the character
		char_start = x;
		while (x < bmpFont.get()->w && IsColumnFree(x))
			++x;
		if(x >= bmpFont.get()->w) break;
		
		// If we stopped at next blue line/end, this must be a kind of space
		if (GetPixel(bmpFont.get(), x, 0) == blue || x == bmpFont.get()->w)  {
			cur_w = x - char_start;

		// Non-blank character
		} else {
			char_start = x;
			++x;

			// Read until a blue pixel or end of the image
			cur_w = 1;
			while (GetPixel(bmpFont.get(), x, 0) != blue && x < bmpFont.get()->w) {
				++x;
				++cur_w;
			}

			// Ignore any free pixel columns *after* the character
			uint tmp_x = x - 1; // last line before blue line or end
			while (IsColumnFree(tmp_x) && cur_w) {
				--cur_w;
				--tmp_x;
			}

			if(cur_w == 0)
				warnings << "CFont cur_w == 0" << endl;
		}

		// Add the character
		FontWidth.push_back(cur_w);
		CharacterOffset.push_back(char_start);
		NumCharacters++;
		CurChar++;
	}



	// Unlock the surface
	UnlockSurface(bmpFont);
	
	if(NumCharacters <= 10)
		warnings << "CFont num chars = " << NumCharacters << " is much too low" << endl;
}

///////////////////
// Precalculate a font's colour
void CFont::PreCalculate(const SmartPointer<SDL_Surface> & bmpSurf, Color colour)
{
	FillSurface(bmpSurf.get(), SDL_MapRGBA(bmpSurf.get()->format, 255, 0, 255, 0));

	// Lock the surfaces
	LOCK_OR_QUIT(bmpSurf);
	LOCK_OR_QUIT(bmpFont);

	Uint8 R, G, B, A;
	const Uint8 sr = colour.r, sg = colour.g, sb = colour.b;

	// Outline font: replace white pixels with appropriate color, put black pixels
	if (OutlineFont) {
		for (int y = 0; y < bmpSurf.get()->h; y++) {
			for (int x = 0; x < bmpSurf.get()->w; x++) {
				Uint32 pixel = GetPixel(bmpFont.get(), x, y);
				GetColour4(pixel, bmpFont.get()->format, &R, &G, &B, &A);

				if (R == 255 && G == 255 && B == 255)    // White
					PutPixel(bmpSurf.get(), x, y,
					         SDL_MapRGBA(bmpSurf.get()->format, sr, sg, sb, A));
				else if (!R && !G && !B)   // Black
					PutPixel(bmpSurf.get(), x, y,
					         SDL_MapRGBA(bmpSurf.get()->format, 0, 0, 0, A));
			}
		}
	// Not outline: replace black pixels with appropriate color
	} else {
		for (int y = 0; y < bmpSurf.get()->h; y++) {
			for (int x = 0; x < bmpSurf.get()->w; x++) {
				Uint32 pixel = GetPixel(bmpFont.get(), x, y);
				GetColour4(pixel, bmpFont.get()->format, &R, &G, &B, &A);

				if (!R && !G && !B)   // Black
					PutPixel(bmpSurf.get(), x, y,
					         SDL_MapRGBA(bmpSurf.get()->format, sr, sg, sb, A));
			}
		}
	}


	// Unlock the surfaces
	UnlockSurface(bmpSurf);
	UnlockSurface(bmpFont);
}


////////////////////
// Get height of multiline text
int CFont::GetHeight(const std::string& buf) {
	int numlines = 1;
	for (std::string::const_iterator i = buf.begin(); i != buf.end(); i++)
		if (*i == '\n') numlines++;
	return numlines * (bmpFont.get()->h + VSpacing);
}

///////////////////
// Draws a font at X, Y, but visible only in specified rect
// HINT: not thread-safe
void CFont::DrawInRect(SDL_Surface * dst, int x, int y, int rectX, int rectY, int rectW, int rectH, Color col, const std::string &txt)  {
	// Set the special clipping rectangle and then draw the font
	SDL_Rect newrect;
	newrect.x = rectX;
	newrect.y = rectY;
	newrect.w = rectW;
	newrect.h = rectH;
	ScopedSurfaceClip clip(dst, newrect);

	// Blit the font
	DrawAdv(dst, x, y, 9999, col, txt);
}

///////////////////
// Draw a font (advanced)
void CFont::DrawAdv(SDL_Surface * dst, int x, int y, int max_w, Color col, const std::string& txt) {

	if (txt.size() == 0)
		return;

	if(dst == NULL) {
		errors << "CFont::DrawAdv(" << txt << "): dst == NULL" << endl;
		return;
	}
	
	if(dst->format == NULL) {
		errors << "CFont::DrawAdv(" << txt << "): dst->format == NULL" << endl;
		return;
	}
	
	// Set the newrect width and use this newrect temporarily to draw the font
	// We use this rect because of precached fonts which use SDL_Blit for drawing (and it takes care of cliprect)
	SDL_Rect newrect = dst->clip_rect;
	newrect.w = MIN(newrect.w, max_w);
	newrect.x = MAX(newrect.x, x);
	newrect.y = MAX(newrect.y, y);
	if (!ClipRefRectWith(newrect.x, newrect.y, newrect.w, newrect.h, (SDLRect&)dst->clip_rect))
		return;

	ScopedSurfaceClip clip(dst, newrect);

	// Look in the precached fonts if there's some for this color
	SmartPointer<SDL_Surface> bmpCached = NULL;
	if (Colorize) {
		// HINT: if we leave this disabled, the drawing will always be done manually
		// this is a bit (not not much) slower but prevents from the usual errors with CFont (wrong color, invisible, so on)
		// TODO: should we completly remove the caches? how much speed improvements do they give?
		// under MacOSX, it doesn't seem to give any performance improvement at all, at least no change in FPS
		// I have activated it again now as it seem to work at the moment (perhabps because of my change in gfxCreateSurfaceAlpha)
		if (col == f_white)
			bmpCached = bmpWhite;
		else if (col == tLX->clBlack)
			bmpCached = bmpFont;
		else if (col == f_green)
			bmpCached = bmpGreen;
	}
	// Not colourize, bmpFont itself should be blitted without any changes, so it's precached
	else {
		bmpCached = bmpFont;
	}


	// Lock the surfaces
	// If we use cached font, we do not access the pixels
	if (!bmpCached.get())  {
		LOCK_OR_QUIT(dst);
		LOCK_OR_QUIT(bmpFont);
	}

	// Get the color values
	// TODO: this function should accept a device-independent color, change it asap!
	Color font_cl(col);

	// Position at destination surface
	int char_y = y;
	int char_h = bmpFont.get()->h;

	// Get the putpixel & getpixel functors
	PixelPutAlpha& putter = getPixelAlphaPutFunc(dst);
	PixelGet& getter = getPixelGetFunc(bmpFont.get());

	// Vertical clipping
	OneSideClip(char_y, char_h, newrect.y, newrect.h);

	// Get the correct drawing function
	typedef void(CFont::*GlyphBlitter)(SDL_Surface *dst, const SDL_Rect& r, int sx, int sy, Color col, int glyph_index, PixelPutAlpha& putter, PixelGet& getter);
	GlyphBlitter func = &CFont::DrawGlyphNormal_Internal;
	if (OutlineFont)
		func = &CFont::DrawGlyphOutline_Internal;

	for (std::string::const_iterator p = txt.begin(); p != txt.end();) {

		// Line break
		if (*p == '\n') {
			y += bmpFont.get()->h + VSpacing;
			char_y = y;
			x = newrect.x;
			p++;

			// If any further text wouldn't be drawn, just stop
			if (char_y >= (newrect.y + newrect.h))
				break;
			else  {
				OneSideClip(char_y, char_h, newrect.y, newrect.h);
				continue;
			}
		}

		// Translate and ignore unknown
		int l = TranslateCharacter(p, txt.end()); // HINT: increases the iterator
		if (l == -1)
			continue;


		// Check if the current line will be drawn, if not, just proceed
		if (char_h == 0)
			continue;

		// Horizontal clipping
		int char_x = x;
		int char_w = FontWidth[l];
		if (!OneSideClip(char_x, char_w, newrect.x, newrect.w))  {
			x += FontWidth[l] + Spacing;
			continue;
		}

		// Precached fonts
		if (bmpCached.get()) {
			DrawImageAdv(dst, bmpCached, CharacterOffset[l], 0, x, y, FontWidth[l], bmpFont.get()->h);
			x += FontWidth[l] + Spacing;
			continue;
		}

		// Draw the glyph
		(this->*(func))(dst, MakeRect(char_x, char_y, char_w, char_h), char_x - x, char_y - y, font_cl, l, putter, getter);

		x += FontWidth[l] + Spacing;
	}


	// Unlock the surfaces
	if (!bmpCached.get())  {
		UnlockSurface(dst);
		UnlockSurface(bmpFont);
	}
}

/////////////////////////
// Draws an outlined character
void CFont::DrawGlyphOutline_Internal(SDL_Surface *dst, const SDL_Rect& r, int sx, int sy, Color col, int glyph_index, PixelPutAlpha& putter, PixelGet& getter)
{
	const short bpp = bmpFont->format->BytesPerPixel;
	Uint8 *src = (Uint8 *) bmpFont.get()->pixels + bmpFont.get()->pitch * sy +
					(CharacterOffset[glyph_index] + sx) * bpp;

	// Draw the glyph
	for (int j = 0; j < r.h; ++j) {
		Uint8 *px = src;
		for (int i = 0; i < r.w; ++i, px += bpp) {
			Uint8 R, G, B, A;
			GetColour4(getter.get(px), bmpFont->format, &R, &G, &B, &A);

			// Put black pixels and colorize white ones
			if (R == 255 && G == 255 && B == 255)  {    // White
				Color current_cl = col;
				current_cl.a = (current_cl.a * A) / 255; // Add the alpha from the font
				putter.put(GetPixelAddr(dst, r.x + i, r.y + j), dst->format, current_cl);
			} else if (!R && !G && !B)    // Black
				putter.put(GetPixelAddr(dst, r.x + i, r.y + j), dst->format, Color(0, 0, 0, (A * col.a) / 255));
		}
		src += bmpFont.get()->pitch;
	}
}

//////////////////////////
// Draws a character
void CFont::DrawGlyphNormal_Internal(SDL_Surface *dst, const SDL_Rect& r, int sx, int sy, Color col, int glyph_index, PixelPutAlpha& putter, PixelGet& getter)
{
	if(glyph_index < 0 || (size_t)glyph_index >= CharacterOffset.size()) {
		errors << "CFont::DrawGlyphNormal_Internal: glyph_index " << glyph_index << " is invalid" << endl;
		return;
	}
	
	const short bpp = bmpFont->format->BytesPerPixel;
	Uint8 *src = (Uint8 *) bmpFont.get()->pixels + bmpFont.get()->pitch * sy +
					(CharacterOffset[glyph_index] + sx) * bpp;

	// Draw the glyph
	for (int j = 0; j < r.h; ++j) {
		Uint8 *px = src;
		for (int i = 0; i < r.w; ++i, px += bpp) {
			Uint8 R, G, B, A;
			GetColour4(getter.get(px), bmpFont.get()->format, &R, &G, &B, &A);

			// Put only black pixels
			if (!R && !G && !B)  {
				Color current_cl = col;
				current_cl.a = (current_cl.a * A) / 255; // Add the alpha from the font
				putter.put(GetPixelAddr(dst, r.x + i, r.y + j), dst->format, current_cl);
			}
		}
		src += bmpFont.get()->pitch;
	}
}


/////////////////////
// Draws one character to the dest surface
void CFont::DrawGlyph(SDL_Surface *dst, int x, int y, Color col, UnicodeChar c)
{
	// Don't draw spaces
	if (c == ' ' || c == '\r' || c == '\n' || c == '\t')
		return;

	int index = TranslateCharacter(c);
	if (index == -1) // Unknown character
		return;

	// Clipping
	SDL_Rect r = { (SDLRect::Type) x, (SDLRect::Type) y, (SDLRect::TypeS) FontWidth[index], (SDLRect::TypeS) bmpFont->h };
	if (!ClipRefRectWith((SDLRect &)r, (SDLRect &)dst->clip_rect))
		return;

	// Get the putpixel & getpixel functors
	PixelPutAlpha& putter = getPixelAlphaPutFunc(dst);
	PixelGet& getter = getPixelGetFunc(bmpFont.get());

	// Draw the glyph
	if (OutlineFont)
		DrawGlyphOutline_Internal(dst, r, x - r.x, y - r.y, col, index, putter, getter);
	else
		DrawGlyphNormal_Internal(dst, r, x - r.x, y - r.y, col, index, putter, getter);
}


///////////////////
// Calculate the width of a string of text
int CFont::GetWidth(const std::string& buf) {
	int length = 0, maxlength = 0;
	short l;

	// Calculate the length of the text
	for (std::string::const_iterator p = buf.begin(); p != buf.end();) {
		if( *p == '\n' ) {
			length = 0;
			p++;
			continue;
		}
		l = TranslateCharacter(p, buf.end());
		if (l != -1)
			length += FontWidth[l] + Spacing;
		if( length > maxlength )
			maxlength = length;
	}

	return maxlength;
}

/////////////////
// Get width of a single character
int CFont::GetCharacterWidth(UnicodeChar c)
{
	int l = TranslateCharacter(c);
	if (l != -1)
		return FontWidth[l];
	else
		return 0;
}

///////////////////
// Draws the text in centre alignment
void CFont::DrawCentre(SDL_Surface * dst, int x, int y, Color col, const std::string& txt) {
	Draw(dst, x - GetWidth(txt) / 2, y, col, txt);
}

///////////////////
// Draw's the text in centre alignment
void CFont::DrawCentreAdv(SDL_Surface * dst, int x, int y, int min_x, int max_w, Color col, const std::string& txt) {
	DrawAdv(dst, MAX(min_x, x - GetWidth(txt) / 2), y, max_w, col, txt);
}
