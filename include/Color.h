/*
	OpenLieroX

	color type and related functions

	code under LGPL
	created 10-01-2007
*/

#ifndef __COLOR_H__
#define __COLOR_H__

#include <SDL.h>
#include <cassert>

#include "MathLib.h"
#include "CodeAttributes.h"


///////////////////
// If you want to use the adress of some Uint32 directly with memcpy or similar, use this
INLINE Uint32 SDLColourToNativeColour(Uint32 pixel, short bpp) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return (pixel << (32 - 8 * bpp));
#else
	return pixel;
#endif
}

/////////////////
// If you copied some data directly with memcpy into an Uint32, use this
INLINE Uint32 NativeColourToSDLColour(Uint32 pixel, short bpp) {
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	return (pixel >> (32 - 8 * bpp));
#else
	return pixel;
#endif
}



///////////////
// Get the specified component from the pixel (grabbed from SDL)
INLINE Uint8 GetR(Uint32 pixel, SDL_PixelFormat *fmt)  {
	return  (Uint8)((((pixel & fmt->Rmask) >> fmt->Rshift) << fmt->Rloss) +
			(((pixel & fmt->Rmask) >> fmt->Rshift) >> (8 - (fmt->Rloss << 1))));
}

INLINE Uint8 GetG(Uint32 pixel, SDL_PixelFormat *fmt)  {
	return  (Uint8)((((pixel & fmt->Gmask) >> fmt->Gshift) << fmt->Gloss) +
			(((pixel & fmt->Gmask) >> fmt->Gshift) >> (8 - (fmt->Gloss << 1))));
}

INLINE Uint8 GetB(Uint32 pixel, SDL_PixelFormat *fmt)  {
	return  (Uint8)((((pixel & fmt->Bmask) >> fmt->Bshift) << fmt->Bloss) +
			(((pixel & fmt->Bmask) >> fmt->Bshift) >> (8 - (fmt->Bloss << 1))));
}

INLINE Uint8 GetA(Uint32 pixel, SDL_PixelFormat *fmt)  {
	return  (Uint8)((((pixel & fmt->Amask) >> fmt->Ashift) << fmt->Aloss) +
			(((pixel & fmt->Amask) >> fmt->Ashift) >> (8 - (fmt->Aloss << 1))));
}

extern SDL_PixelFormat mainPixelFormat;

INLINE SDL_PixelFormat* getMainPixelFormat() {
	return &mainPixelFormat;
}

///////////////
// Returns true if the two colors are the same, ignoring the alpha
// HINT: both colors have to be in the same pixelformat
INLINE bool EqualRGB(Uint32 p1, Uint32 p2, const SDL_PixelFormat* fmt) {
	return ((p1&(fmt->Rmask|fmt->Gmask|fmt->Bmask)) == (p2&(fmt->Rmask|fmt->Gmask|fmt->Bmask)));
}

///////////////
// Creates a int colour based on the 3 components
// HINT: format is that one from videosurface!
INLINE Uint32 MakeColour(Uint8 r, Uint8 g, Uint8 b) {
	return SDL_MapRGB(getMainPixelFormat(), r, g, b);
}

///////////////
// Creates a int colour based on the 4 components
// HINT: format is that one from videosurface!
INLINE Uint32 MakeColour(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
	return SDL_MapRGBA(getMainPixelFormat(), r, g, b, a);
}



// Device-independent color
struct Color {
	INLINE Color() : r(0), g(0), b(0), a(SDL_ALPHA_OPAQUE) {}
	INLINE Color(Uint8 _r, Uint8 _g, Uint8 _b) : r(_r), g(_g), b(_b), a(SDL_ALPHA_OPAQUE) {}
	INLINE Color(Uint8 _r, Uint8 _g, Uint8 _b, Uint8 _a) : r(_r), g(_g), b(_b), a(_a) {}
	INLINE Color(const SDL_PixelFormat *f, Uint32 cl) { SDL_GetRGBA(cl, f, &r, &g, &b, &a); }
	INLINE Color(const SDL_Color& cl) : r(cl.r), g(cl.g), b(cl.b), a(SDL_ALPHA_OPAQUE) {}
	static Color fromDefault(Uint32 cl) { return Color(Uint8(cl >> 24), Uint8(cl >> 16), Uint8(cl >> 8), Uint8(cl)); }
	static Color fromMainFormat(Uint32 i) { return Color(getMainPixelFormat(), i); }
	
	Uint8 r;
	Uint8 g;
	Uint8 b;
	Uint8 a;

	INLINE Uint32 get(const SDL_PixelFormat *f) const { return SDL_MapRGBA(f, r, g, b, a); }
	Uint32 getDefault() const { return (Uint32(r) << 24) | (Uint32(g) << 16) | (Uint32(b) << 8) | Uint32(a); }
	Color derived(Sint16 _r, Sint16 _g, Sint16 _b, Sint16 _a) const {
		return Color(
					 Uint8(CLAMP(_r + r, 0, 255)),
					 Uint8(CLAMP(_g + g, 0, 255)),
					 Uint8(CLAMP(_b + b, 0, 255)),
					 Uint8(CLAMP(_a + a, 0, 255)));
	}
	void set(const SDL_PixelFormat *f, Uint32 cl) { SDL_GetRGBA(cl, f, &r, &g, &b, &a); }
	
	INLINE bool operator == ( const Color & c ) const { return r == c.r && g == c.g && b == c.b && a == c.a; };
	INLINE bool operator != ( const Color & c ) const { return ! ( *this == c ); };
	
	Color operator * ( float f ) const { return Color( Uint8(CLAMP(r*f,0.0f,255.0f)), Uint8(CLAMP(g*f,0.0f,255.0f)), Uint8(CLAMP(b*f,0.0f,255.0f)), a ); };
	Color operator + ( const Color & c ) const { return Color( (Uint8)CLAMP(Uint16(r)+c.r,0,255), (Uint8)CLAMP(Uint16(g)+c.g,0,255), (Uint8)CLAMP(Uint16(b)+c.b,0,255), (Uint8)CLAMP(Uint16(a)+c.a,0,255)); };
	bool operator<(const Color& c) const {
		if(r != c.r) return r < c.r;
		if(g != c.g) return g < c.g;
		if(b != c.b) return b < c.b;
		return a < c.a;
	}
	Uint8& operator[](int i) { switch(i) { case 0: return r; case 1: return g; case 2: return b; case 3: return a; default: assert(false); } return r; }
	Uint8 operator[](int i) const { switch(i) { case 0: return r; case 1: return g; case 2: return b; case 3: return a; default: assert(false); } return 0; }
};

#endif
