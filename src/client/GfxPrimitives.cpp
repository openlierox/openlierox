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


// Graphics primitives
// Created 12/11/01
// By Jason Boettcher

#ifndef DEDICATED_ONLY
#define USE_GD_FOR_IMAGE_LOADING 1
#endif

#include <cassert>
#include <SDL.h>
#include <algorithm>
#if USE_GD_FOR_IMAGE_LOADING
#include <gd.h>
#endif

#include "LieroX.h"
#include "Options.h"
#include "Debug.h"
#include "MathLib.h"
#include "GfxPrimitives.h"
#include "Cache.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "AuxLib.h"
#include "PixelFunctors.h"
#include "Utils.h"
#include "CViewport.h"
#include "Geometry.h"
#include "Timer.h"
#include "ReadWriteLock.h"
#include "Mutex.h"
#include "Condition.h"
#include "CVec.h"
#include "Cache.h"
#include "CodeAttributes.h"




// Used in various alpha-blending routines, internal
// TODO: remove this and use Color struct
struct RGBA  {
	Uint8 r, g, b, a;
};

/////////////////////////
//
// Misc routines
//
//////////////////////////

/////////////////
// Put the pixel alpha blended with the background
// TODO: this function is now obsolete, remove it
// TODO: why is this function obsolete? what should be used instead?
void PutPixelA(SDL_Surface * bmpDest, int x, int y, Uint32 colour, Uint8 a)  {
	Uint8* px = (Uint8*)bmpDest->pixels + y * bmpDest->pitch + x * bmpDest->format->BytesPerPixel;
	
	PixelPutAlpha& putter = getPixelAlphaPutFunc(bmpDest);
	Color c = Unpack_solid(colour, bmpDest->format); c.a = a;
	putter.put(px, bmpDest->format, c);
}


//////////////////////////
// Set the per-surface alpha
void SetPerSurfaceAlpha(SDL_Surface * dst, Uint8 a) {
	SDL_SetSurfaceAlphaMod(dst, a);
	// See SDL_BlitSurface(). We also need to enable SDL_BLENDMODE_BLEND.
	SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);
}


//////////////////////
// Set a color key for alpha surface (SDL_SetColorKey does not work for alpha surfaces)
static void SetColorKey_Alpha(SDL_Surface * dst, Uint8 r, Uint8 g, Uint8 b) {
	LOCK_OR_QUIT(dst);

	// Just set transparent alpha to pixels that match the color key
	Uint8* pxr = (Uint8*)dst->pixels;
	Uint8* px;
	int x, y;
	Uint32 colorkey = SDL_MapRGBA(dst->format, r, g, b, 0);

	for(y = 0; y < dst->h; y++, pxr += dst->pitch)  {
		px = pxr;
		for(x = 0; x < dst->w; x++, px += dst->format->BytesPerPixel)  {
			if(EqualRGB(
					colorkey,
					GetPixelFromAddr(px, dst->format->BytesPerPixel),
					dst->format))
				PutPixelToAddr(px, colorkey, dst->format->BytesPerPixel);
		}
	}

	UnlockSurface(dst);
}


///////////////////////
// Set a pink color key
void SetColorKey(SDL_Surface * dst)  {
	// If there's already a colorkey set, don't set it again
	if (Surface_HasColorKey(dst) &&
		((Surface_GetColorKey(dst) == SDL_MapRGB(dst->format, 255, 0, 255)) ||
		(Surface_GetColorKey(dst) == SDL_MapRGB(dst->format, 254, 0, 254))))
		return;

	// Because some graphic editors (old Photoshop and GIMP and possibly more) contain a bug that rounds 255 to 254
	// and some mods have such bugged images. To fix it we have to make a little hack here :(
	Uint32 bugged_colorkey = SDL_MapRGB(dst->format, 254, 0, 254);

	// If one of the pixels in edges matches the bugged colorkey, we suppose whole image is bugged
	// Not the best method, but it is fast and works for all cases I have discovered
	LOCK_OR_QUIT(dst);
	bool bugged =	EqualRGB(GetPixel(dst, 0, 0), bugged_colorkey, dst->format) ||
					EqualRGB(GetPixel(dst, dst->w - 1, 0), bugged_colorkey, dst->format) ||
					EqualRGB(GetPixel(dst, 0, dst->h - 1), bugged_colorkey, dst->format) ||
					EqualRGB(GetPixel(dst, dst->w - 1, dst->h - 1), bugged_colorkey, dst->format);
	UnlockSurface(dst);


	// Apply the colorkey
	if (Surface_HasBlendMode(dst)) {
		// see comment in other SetColorKey; the behaviour with Surface_HasBlendMode is different
		if (bugged)
			SetColorKey_Alpha(dst, 254, 0, 254);
		else
			SetColorKey_Alpha(dst, 255, 0, 255);
	}

	// set in both cases the colorkey (for alpha-surfaces just as a info, it's ignored there)
	if (bugged)
		SDL_SetColorKey(dst, 1, bugged_colorkey);
	else
		SDL_SetColorKey(dst, 1, SDL_MapRGB(dst->format, 255, 0, 255));
}

void SetColorKey(SDL_Surface * dst, Uint8 r, Uint8 g, Uint8 b) {
	if(r == 255 && g == 0 && b == 255) { // pink
		SetColorKey(dst); // use this function as it has a workaround included for some old broken mods
		return;
	}

	if (Surface_HasBlendMode(dst))
		// HINT: The behaviour with Surface_HasBlendMode is different
		// and not as you would expect it. The problem is that while blitting to a surface with RGBA,
		// the alpha channel of the destination is untouched (see manpage to SDL_SetAlpha).
		SetColorKey_Alpha(dst, r, g, b);

	// set in both cases the colorkey (for alpha-surfaces just as a info, it's ignored there)
	SDL_SetColorKey(dst, 1, SDL_MapRGB(dst->format, r, g, b));
}

struct VideoFormat {
	Uint32 bpp;
	Uint32 amask;
	Uint32 rmask;
	Uint32 gmask;
	Uint32 bmask;

	VideoFormat() {}
	VideoFormat(const SDL_PixelFormat* fmt) {
		bpp = fmt->BitsPerPixel;
		rmask = fmt->Rmask;
		gmask = fmt->Gmask;
		bmask = fmt->Bmask;
		amask = fmt->Amask;
	}
	
	bool operator==(const VideoFormat& f) const {
		return
		bpp == f.bpp &&
		amask == f.amask &&
		rmask == f.rmask &&
		gmask == f.gmask &&
		bmask == f.bmask;
	}
	bool operator!=(const VideoFormat& f) const { return !(*this == f); }
};

INLINE VideoFormat getMainVideoFormat() {
	return VideoFormat(getMainPixelFormat());
}


//////////////////////////////
// Create a surface without any alpha channel
SmartPointer<SDL_Surface> gfxCreateSurface(int width, int height, bool forceSoftware)
{
	if (width <= 0 || height <= 0) // Nonsense, can cause trouble
		return NULL;

	const VideoFormat fmt = getMainVideoFormat();

	SmartPointer<SDL_Surface> result = SDL_CreateRGBSurface(
			0,
			width, height,
			fmt.bpp, fmt.rmask, fmt.gmask, fmt.bmask, fmt.amask);
	if(!result.get()) return NULL;
	
	// Force to ignore alpha.
	SDL_SetSurfaceBlendMode(result.get(), SDL_BLENDMODE_NONE);

	// OpenGL strictly requires the surface to be cleared
	SDL_FillRect(result.get(), NULL, SDL_MapRGBA(result.get()->format, 0, 0, 0, SDL_ALPHA_OPAQUE));

	#ifdef DEBUG
	//printf("gfxCreateSurface() %p %i %i\n", result.get(), width, height );
	#endif

	return result;
}


static VideoFormat bestSoftwareAlphaFormat() {
	VideoFormat f;
	
	// This code is taken from SDL_DisplayFormatAlpha() in SDL_video.c (1.2.8)
	
	/* default to ARGB8888 */
	f.bpp = 32;
	f.amask = 0xff000000;
	f.rmask = 0x00ff0000;
	f.gmask = 0x0000ff00;
	f.bmask = 0x000000ff;
	
	SDL_PixelFormat* vf = getMainPixelFormat();
	
	switch(vf->BytesPerPixel) {
		case 2:
			/* For XGY5[56]5, use, AXGY8888, where {X, Y} = {R, B}.
			 For anything else (like ARGB4444) it doesn't matter
			 since we have no special code for it anyway */
			if ( (vf->Rmask == 0x1f) &&
				(vf->Bmask == 0xf800 || vf->Bmask == 0x7c00)) {
				f.rmask = 0xff;
				f.bmask = 0xff0000;
			}
			break;
			
		case 3:
		case 4:
			/* Keep the video format, as long as the high 8 bits are
			 unused or alpha */
			if ( (vf->Rmask == 0xff) && (vf->Bmask == 0xff0000) ) {
				f.rmask = 0xff;
				f.bmask = 0xff0000;
			}
			break;
			
		default:
			/* We have no other optimised formats right now. When/if a new
			 optimised alpha format is written, add the converter here */
			break;
	}
	
	return f;
}

INLINE VideoFormat getMainAlphaVideoFormat() {
	SDL_PixelFormat* fmt = getMainPixelFormat();
	
	if(fmt->Amask != 0) // the main pixel format supports alpha blending
		return VideoFormat(fmt);
	
	// no native alpha blending, so create a software alpha blended surface
	return bestSoftwareAlphaFormat();
}

INLINE VideoFormat getMainVideoFormat(bool withalpha) {
	if(withalpha) return getMainAlphaVideoFormat();
	else return getMainVideoFormat();
}

/////////////////////////
// Create a surface with an alpha channel
SmartPointer<SDL_Surface> gfxCreateSurfaceAlpha(int width, int height, bool forceSoftware)
{
	if (width <= 0 || height <= 0) // Nonsense, can cause trouble
		return NULL;

	const VideoFormat f = getMainAlphaVideoFormat();
	assert(f.amask != 0);
	SmartPointer<SDL_Surface> result = SDL_CreateRGBSurface(
				0,
				width, height,
				f.bpp, f.rmask, f.gmask, f.bmask, f.amask);
	if(!result.get()) return NULL;
	
	// The surface will have SDL_BLENDMODE_BLEND by default with Amask != 0.
	
	// OpenGL strictly requires the surface to be cleared
	SDL_FillRect( result.get(), NULL, SDL_MapRGB(result.get()->format, 0, 0, 0));

	return result;
}

//////////////////
// Resets the alpha-channel and the colorkey
void ResetAlpha(SDL_Surface * dst)
{
	SDL_SetColorKey(dst, 0, 0); // Remove the colorkey
	SDL_SetSurfaceAlphaMod(dst, SDL_ALPHA_OPAQUE); // Remove the persurface-alpha

	PixelPut& putter = getPixelPutFunc(dst);
	PixelGet& getter = getPixelGetFunc(dst);

	LOCK_OR_QUIT(dst);
	Uint8 *px = (Uint8 *)dst->pixels;
	int gap = dst->pitch - dst->w * dst->format->BytesPerPixel;

	int x, y;
	for(y = dst->h; y; --y, px += gap)
		for(x = dst->w; x; --x, px += dst->format->BytesPerPixel)
			putter.put(px, getter.get(px) | dst->format->Amask);

	UnlockSurface(dst);
}


/////////////////////////
//
// Clipping routines
//
/////////////////////////


//
// Line clipping - grabbed from SDL_gfx
//

#define CLIP_LEFT_EDGE   0x1
#define CLIP_RIGHT_EDGE  0x2
#define CLIP_BOTTOM_EDGE 0x4
#define CLIP_TOP_EDGE    0x8
#define CLIP_INSIDE(a)   (!a)
#define CLIP_REJECT(a,b) (a&b)
#define CLIP_ACCEPT(a,b) (!(a|b))

static INLINE int clipEncode(int x, int y, int left, int top, int right, int bottom)
{
    int code = 0;

    if (x < left) {
		code |= CLIP_LEFT_EDGE;
    } else if (x > right) {
		code |= CLIP_RIGHT_EDGE;
    }

    if (y < top) {
		code |= CLIP_TOP_EDGE;
    } else if (y > bottom) {
		code |= CLIP_BOTTOM_EDGE;
    }

    return code;
}

/////////////////////
// Clip the line to the surface
bool ClipLine(SDL_Surface * dst, int * x1, int * y1, int * x2, int * y2)
{
    int left, right, top, bottom;
    Uint32 code1, code2;
    bool draw = false;
    int swaptmp;
    float m;

	// No line
	if (*x1 == *x2 && *y1 == *y2)
		return false;

    // Get clipping boundary
    left = dst->clip_rect.x;
    right = dst->clip_rect.x + dst->clip_rect.w - 1;
    top = dst->clip_rect.y;
    bottom = dst->clip_rect.y + dst->clip_rect.h - 1;

    while (true) {
		code1 = clipEncode(*x1, *y1, left, top, right, bottom);
		code2 = clipEncode(*x2, *y2, left, top, right, bottom);
		if (CLIP_ACCEPT(code1, code2)) {
			draw = true;
			break;
		} else if (CLIP_REJECT(code1, code2))
			break;
		else {
			if (CLIP_INSIDE(code1)) {
				swaptmp = *x2;
				*x2 = *x1;
				*x1 = swaptmp;
				swaptmp = *y2;
				*y2 = *y1;
				*y1 = swaptmp;
				swaptmp = code2;
				code2 = code1;
				code1 = swaptmp;
			}

			if (*x2 != *x1) {
				m = (*y2 - *y1) / (float) (*x2 - *x1);
			} else {
				m = 1.0f;
			}

			if (code1 & CLIP_LEFT_EDGE) {
				*y1 += (int) ((left - *x1) * m);
				*x1 = left;
			} else if (code1 & CLIP_RIGHT_EDGE) {
				*y1 += (int) ((right - *x1) * m);
				*x1 = right;
			} else if (code1 & CLIP_BOTTOM_EDGE) {
				if (*x2 != *x1) {
					*x1 += (int) ((bottom - *y1) / m);
				}
				*y1 = bottom;
			} else if (code1 & CLIP_TOP_EDGE) {
				if (*x2 != *x1) {
					*x1 += (int) ((top - *y1) / m);
				}
				*y1 = top;
			}
		}
    }

    return draw && (*x1 != *x2 || *y1 != *y2);
}



//////////////////
//
// Blitting and drawing routines
//
//////////////////


// TODO: not used, remove?
INLINE void CopySurfaceFast(SDL_Surface * dst, SDL_Surface * src, int sx, int sy, int dx, int dy, int w, int h) {
	LOCK_OR_QUIT(dst);
	LOCK_OR_QUIT(src);

	// Initialize
	size_t byte_bound = w * src->format->BytesPerPixel;
	int src_pitch = src->pitch;
	int dst_pitch = dst->pitch;
	Uint8* srcrow = (Uint8 *)src->pixels
		+ (sy * src_pitch) + (sx * src->format->BytesPerPixel);
	Uint8* dstrow = (Uint8 *)dst->pixels
		+ (dy * dst_pitch) + (dx * dst->format->BytesPerPixel);

	// Copy row by row
	for (int i = 0; i < h; ++i)  {
		memcpy(dstrow, srcrow, byte_bound);
		dstrow += dst_pitch;
		srcrow += src_pitch;
	}

	UnlockSurface(src);
	UnlockSurface(dst);
}

SurfaceCopyScope::SurfaceCopyScope(SDL_Surface *src_) : src(src_) {
	// Copying is a normal blit without colorkey and alpha
	// If the surface has alpha or colorkey set, we have to remove them and then put them back

	// Save alpha values
	PerSurfaceAlpha = SDL_ALPHA_OPAQUE;
	SDL_GetSurfaceAlphaMod(src, &PerSurfaceAlpha);
	BlendMode = SDL_BLENDMODE_NONE;
	SDL_GetSurfaceBlendMode(src, &BlendMode);

	// Save colorkey values
	HasColorkey = true;
	Colorkey = 0;
	if (SDL_GetColorKey(src, &Colorkey) != 0)  {
		HasColorkey = false;
	}

	// Remove alpha and colorkey
	SDL_SetSurfaceAlphaMod(src, SDL_ALPHA_OPAQUE);
	SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
	SDL_SetColorKey(src, 0, 0);
}

SurfaceCopyScope::~SurfaceCopyScope() {
	// Return back alpha and colorkey
	if (PerSurfaceAlpha != SDL_ALPHA_OPAQUE)
		SDL_SetSurfaceAlphaMod(src, PerSurfaceAlpha);
	if (BlendMode != SDL_BLENDMODE_NONE)
		SDL_SetSurfaceBlendMode(src, BlendMode);
	if (HasColorkey)
		SDL_SetColorKey(src, 1, Colorkey);
}

///////////////////////
// Copies area from one image to another (not blitting so the alpha values are kept!)
void CopySurface(SDL_Surface * dst, SDL_Surface * src, int sx, int sy, int dx, int dy, int w, int h, bool stretch2)
{
	SurfaceCopyScope copyScope(src);

	// Blit
	if(stretch2)
		DrawImageStretch2(dst, src, sx, sy, dx, dy, w, h);
	else
		DrawImageAdv(dst, src, sx, sy, dx, dy, w, h);
}

SmartPointer<SDL_Surface> GetCopiedImage(SDL_Surface* bmpSrc) {
	assert(bmpSrc != NULL);
	SmartPointer<SDL_Surface> result = SDL_CreateRGBSurface(
															0,
															bmpSrc->w, bmpSrc->h,
															bmpSrc->format->BitsPerPixel,
															bmpSrc->format->Rmask,
															bmpSrc->format->Gmask,
															bmpSrc->format->Bmask,
															bmpSrc->format->Amask);
	if (result.get() == NULL) return NULL;

	CopySurface(result.get(), bmpSrc, 0, 0, 0, 0, bmpSrc->w, bmpSrc->h);
	
	// Colorkey.
	if(Surface_HasColorKey(bmpSrc)) {
		uint32_t colorkey = Surface_GetColorKey(bmpSrc);
		SDL_SetColorKey(result.get(), 1, colorkey);
	}
	
	// Per-surface alpha.
	{
		uint8_t alpha = SDL_ALPHA_OPAQUE;
		(void)SDL_GetSurfaceAlphaMod(bmpSrc, &alpha);
		if(alpha != SDL_ALPHA_OPAQUE) {
			SDL_SetSurfaceAlphaMod(result.get(), alpha);
		}
	}
	
	// (Per-pixel) alpha blend mode.
	{
		SDL_BlendMode blendMode = Surface_GetBlendMode(bmpSrc);
		SDL_SetSurfaceBlendMode(result.get(), blendMode);
	}
	
	assert(PixelFormatEqual(bmpSrc->format, result->format));

	return result;
}


SmartPointer<SDL_Surface> GetCopiedStretched2Image(SDL_Surface* bmpSrc) {
	assert(bmpSrc != NULL);
	SmartPointer<SDL_Surface> result = SDL_CreateRGBSurface(
															0,
															bmpSrc->w*2, bmpSrc->h*2,
															bmpSrc->format->BitsPerPixel,
															bmpSrc->format->Rmask,
															bmpSrc->format->Gmask,
															bmpSrc->format->Bmask,
															bmpSrc->format->Amask);
	if (result.get() == NULL) return NULL;

	CopySurface(result.get(), bmpSrc, 0, 0, 0, 0, bmpSrc->w, bmpSrc->h, true);

	// Colorkey.
	if(Surface_HasColorKey(bmpSrc)) {
		uint32_t colorkey = Surface_GetColorKey(bmpSrc);
		SDL_SetColorKey(result.get(), 1, colorkey);
	}
	
	// Per-surface alpha.
	{
		uint8_t alpha = SDL_ALPHA_OPAQUE;
		(void)SDL_GetSurfaceAlphaMod(bmpSrc, &alpha);
		if(alpha != SDL_ALPHA_OPAQUE) {
			SDL_SetSurfaceAlphaMod(result.get(), alpha);
		}
	}
	
	// (Per-pixel) alpha blend mode.
	{
		SDL_BlendMode blendMode = Surface_GetBlendMode(bmpSrc);
		SDL_SetSurfaceBlendMode(result.get(), blendMode);
	}

	assert(PixelFormatEqual(bmpSrc->format, result->format));

	return result;
}

template <
	bool colorkeycheck,
	bool alphablend,
	bool src_persurfacealpha,
	bool src_hasalpha, bool dst_hasalpha,
	int sbpp, int dbpp,
	bool sameformat
>
void _OperateOnSurfaces(SDL_PixelFormat* dstformat, SDL_PixelFormat* srcformat, SDL_Surface * bmpDest, SDL_Surface * bmpSrc, SDL_Rect& rDest, SDL_Rect& rSrc)
{
	assert(bmpSrc->format->BytesPerPixel == sbpp);
	assert(bmpDest->format->BytesPerPixel == dbpp);
	assert(!sameformat || sbpp == dbpp);
	
	// Clip
	{
		int old_x = rDest.x;
		int old_y = rDest.y;
		rDest.w = rSrc.w;
		rDest.h = rSrc.h;
		if (!ClipRefRectWith(rDest, (SDLRect&)bmpDest->clip_rect))
			return;
		
		rSrc.x += rDest.x - old_x;
		rSrc.y += rDest.y - old_y;
		if (!ClipRefRectWith(rSrc, (SDLRect&)bmpSrc->clip_rect))
			return;
		rDest.w = MIN(rSrc.w, rDest.w);
		rDest.h = MIN(rSrc.h, rDest.h);
	}
	
	LOCK_OR_QUIT(bmpDest);
	LOCK_OR_QUIT(bmpSrc);
	
	Uint8 *src = ((Uint8 *)bmpSrc->pixels + rSrc.y * bmpSrc->pitch + rSrc.x * sbpp);
	Uint8 *dst = ((Uint8 *)bmpDest->pixels + rDest.y * bmpDest->pitch + rDest.x * dbpp);
	int srcgap = bmpSrc->pitch - rDest.w * sbpp;
	int dstgap = bmpDest->pitch - rDest.w * dbpp;
	
	Color key = Unpack_<src_hasalpha>(Surface_GetColorKey(bmpSrc), bmpSrc->format);
	for (int y = rDest.h; y; --y, dst += dstgap, src += srcgap)
		for (int x = rDest.w; x; --x, dst += dbpp, src += sbpp) {
			if(sameformat && !alphablend && !colorkeycheck) {
				if(sbpp == 4) *(Uint32*)dst = *(Uint32*)src;
				else if(sbpp == 3) { *dst = *src; *(dst+1) = *(src+1); *(dst+2) = *(src+2); }
				else if(sbpp == 2) *(Uint16*)dst = *(Uint16*)src;
				else if(sbpp == 1) *dst = *src;
				else assert(false);
			} else {
				Color c = _GetPixel<src_hasalpha,sbpp>(srcformat, src);
				if(colorkeycheck && c.r == key.r && c.g == key.g && c.b == key.b)  // Colorkey check
					continue;
				if(src_persurfacealpha)
					c.a = ((int)c.a * Surface_GetAlpha(bmpSrc)) / 255;  // Add the per-surface alpha to the source pixel alpha
				_PutPixel<dst_hasalpha, alphablend, dbpp>(dstformat, dst, c);
			}
		}

	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);
}


/////////////////////
// Performs a "correct" blit of RGBA surfaces to RGB or RGBA surfaces
template<bool alphablend>
static void DrawRGBA(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, SDL_Rect& rDest, SDL_Rect& rSrc)
{
	const int sbpp = bmpSrc->format->BytesPerPixel;
	const int dbpp = bmpDest->format->BytesPerPixel;
	const bool src_hasalpha = bmpSrc->format->Amask != 0;
	const bool dst_hasalpha = bmpDest->format->Amask != 0;
	const bool src_persurfacealpha = alphablend && Surface_GetAlpha(bmpSrc) != SDL_ALPHA_OPAQUE;
	const bool colorkeycheck = Surface_HasColorKey(bmpSrc);
	const bool sameformat = PixelFormatEqual(bmpSrc->format, bmpDest->format);
	
#define ____DO_OP(_sbpp, _dbpp, _salpha, _dalpha, _spersurfalpha, _colkeycheck, _sameformat) \
	_OperateOnSurfaces< \
		_colkeycheck, \
		alphablend, \
		_spersurfalpha, \
		_salpha, _dalpha, \
		_sbpp, _dbpp, \
		_sameformat \
		>( \
		bmpDest->format, bmpSrc->format, \
		bmpDest, bmpSrc, rDest, rSrc)

#define ___DO_OP(_sbpp, _dbpp, _salpha, _dalpha, _spersurfalpha, _colkeycheck) \
	{ \
		if(/* this is an extra static(!) check for sameformat */ _sbpp == _dbpp && sameformat) \
			____DO_OP(_sbpp, _dbpp, _salpha, _dalpha, _spersurfalpha, _colkeycheck, true); \
		else /* !sameformat */ \
			____DO_OP(_sbpp, _dbpp, _salpha, _dalpha, _spersurfalpha, _colkeycheck, false); \
	}

#define __DO_OP(_sbpp, _dbpp, _salpha, _dalpha) \
	{ \
		if(src_persurfacealpha) { \
			if(colorkeycheck) ___DO_OP(_sbpp, _dbpp, _salpha, _dalpha, true, true) \
			else ___DO_OP(_sbpp, _dbpp, _salpha, _dalpha, true, false) \
		} \
		else /* !src_persurfacealpha */ { \
			if(colorkeycheck) ___DO_OP(_sbpp, _dbpp, _salpha, _dalpha, false, true) \
			else ___DO_OP(_sbpp, _dbpp, _salpha, _dalpha, false, false) \
		} \
	}
	
#define _DO_OP(_sbpp, _dbpp) \
	if(sbpp == _sbpp && dbpp == _dbpp) { \
		if(src_hasalpha && dst_hasalpha) __DO_OP(_sbpp, _dbpp, true, true) \
		else if(src_hasalpha && !dst_hasalpha) __DO_OP(_sbpp, _dbpp, true, false) \
		else if(!src_hasalpha && dst_hasalpha) __DO_OP(_sbpp, _dbpp, false, true) \
		else __DO_OP(_sbpp, _dbpp, false, false) \
	}
	
	_DO_OP(4,4) else
	_DO_OP(4,3) else
	_DO_OP(3,4) else
	_DO_OP(3,3) else
	_DO_OP(2,4) else
	_DO_OP(2,3) else
	_DO_OP(2,2) else
	_DO_OP(4,2) else
	_DO_OP(3,2) else
	_DO_OP(1,4) else
	_DO_OP(1,3) else
	_DO_OP(1,2) else
	_DO_OP(1,1) else
	_DO_OP(4,1) else
	_DO_OP(3,1) else
	_DO_OP(2,1) else
	assert(false);
}

/////////////////////
// Draws the image
void DrawImageAdv(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, SDL_Rect& rDest, SDL_Rect& rSrc)
{
	if(!bmpSrc) {
		errors << "DrawImageAdv: source-image not set" << endl;
		return;
	}
	
	if(!bmpDest) {
		errors << "DrawImageAdv: destination-image not set" << endl;
		return;
	}

	SDL_BlitSurface(bmpSrc, &rSrc, bmpDest, &rDest);

	return; // XXX: for now. I hope that SDL2 is faster than our code... :P Also, I didn't checked our code with SDL2 yet
	
	if(Surface_HasBlendMode(bmpSrc))
		DrawRGBA<true>(bmpDest, bmpSrc, rDest, rSrc);
	else
		DrawRGBA<false>(bmpDest, bmpSrc, rDest, rSrc);

	return; // TODO: for now. and if the above is fast enough anyway, we don't need the code below (which is buggy)
	
	bool src_isrgba = bmpSrc->format->Amask != 0 && Surface_HasBlendMode(bmpSrc);
	bool dst_isrgba = bmpDest->format->Amask != 0 && Surface_HasBlendMode(bmpDest);

	// RGB -> RGB
	// RGB -> RGBA
	if (!src_isrgba)  {
		if(bmpDest->format->BitsPerPixel == 8 && bmpSrc->format->BitsPerPixel == 8)
			CopySurfaceFast(bmpDest, bmpSrc, rSrc.x, rSrc.y, rDest.x, rDest.y, rSrc.w, rSrc.h);
		else if(dst_isrgba)
			SDL_BlitSurface(bmpSrc, &rSrc, bmpDest, &rDest);
		else
			DrawRGBA<false>(bmpDest, bmpSrc, rDest, rSrc);
		
	// RGBA -> RGB
	} else if (src_isrgba && !dst_isrgba)  {
		switch (Surface_GetAlpha(bmpSrc))  {
		case SDL_ALPHA_OPAQUE:
			SDL_BlitSurface(bmpSrc, &rSrc, bmpDest, &rDest);
		break;
		case SDL_ALPHA_TRANSPARENT:
		return;
		default:
			DrawRGBA<true>(bmpDest, bmpSrc, rDest, rSrc); // To handle the per-surface alpha correctly
		}

	// RGBA -> RGBA
	} else {
		DrawRGBA<true>(bmpDest, bmpSrc, rDest, rSrc);
	}
}



/////////////////////////
// Draw the image tiled on the dest surface
void DrawImageTiled(SDL_Surface *bmpDest, SDL_Surface *bmpSrc, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh)
{
	SDL_Rect newClip = { (SDLRect::Type) dx, (SDLRect::Type) dy, (SDLRect::TypeS) dw, (SDLRect::TypeS) dh };
	ScopedSurfaceClip clip(bmpDest,newClip);	
	MOD(sx, (int)bmpSrc->w); MOD(sy, (int)bmpSrc->h);
	
	// Place the tiles
	for (int y = dy; y < dy + dh; y += sh)
		for (int x = dx; x < dx + dw; x += sw) {
			int w1 = (sx + sw > bmpSrc->w) ? bmpSrc->w - sx : sw;
			int h1 = (sy + sh > bmpSrc->h) ? bmpSrc->h - sy : sh;
			DrawImageAdv(bmpDest, bmpSrc, sx, sy, x, y, w1, h1);
			if(sw != w1) DrawImageAdv(bmpDest, bmpSrc, 0, sy, x+w1, y, sw-w1, h1);
			if(sh != h1) DrawImageAdv(bmpDest, bmpSrc, sx, 0, x, y+h1, w1, sh-h1);
			if(sh != h1 && sw != w1) DrawImageAdv(bmpDest, bmpSrc, 0, 0, x+w1, y+h1, sw-w1, sh-h1);
		}									  
}

/////////////////////////
// Draw the image tiled on the dest surface, only in the X direction
void DrawImageTiledX(SDL_Surface *bmpDest, SDL_Surface *bmpSrc, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh)
{
	// Place the tiles
	for (int x = 0; x < dw; x += sw)
		DrawImageAdv(bmpDest, bmpSrc, sx, sy, dx + x, dy, sw, MIN(dh, sh));
}

/////////////////////////
// Draw the image tiled on the dest surface, only in the Y direction
void DrawImageTiledY(SDL_Surface *bmpDest, SDL_Surface *bmpSrc, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh)
{
	// Place the tiles
	for (int y = 0; y < dh; y += sh)
		DrawImageAdv(bmpDest, bmpSrc, sx, sy, dx, dy + y, MIN(dw, sw), sh);
}


///////////////////
// Draw the image mirrored with a huge amount of options. horiz, i.e. x is flipped
void DrawImageAdv_Mirror(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, int sx, int sy, int dx, int dy, int w, int h)
{
	// TODO: fix this
	// Warning: Both surfaces have to have same bpp!
	if(bmpDest->format->BytesPerPixel != bmpSrc->format->BytesPerPixel) {
		errors << "DrawImageAdv_Mirror: dst bpp " << bmpDest->format->BytesPerPixel << " != " << " src bpp " << bmpSrc->format->BytesPerPixel << endl;
		return;
	}

	// Clipping on source surface
	if (!ClipRefRectWith(sx, sy, w, h, (SDLRect&)bmpSrc->clip_rect))
		return;

	// Clipping on dest surface
	if (!ClipRefRectWith(dx, dy, w, h, (SDLRect&)bmpDest->clip_rect))
		return;

	int x,y;

	// Lock the surfaces
	LOCK_OR_QUIT(bmpDest);
	LOCK_OR_QUIT(bmpSrc);


	Uint8 *TrgPix = (Uint8 *)bmpDest->pixels + dy*bmpDest->pitch + dx*bmpDest->format->BytesPerPixel;
	Uint8 *SrcPix = (Uint8 *)bmpSrc->pixels +  sy*bmpSrc->pitch + sx*bmpSrc->format->BytesPerPixel;

	short bpp = bmpDest->format->BytesPerPixel;

	Uint8 *sp,*tp;
	for(y = h; y; --y) {

		sp = SrcPix;
		tp = TrgPix + w*bpp;
		for(x = w; x; --x) {
			// Copy the pixel
			tp -= bpp;
			memcpy(tp, sp, bpp);
			sp += bpp;
		}

		SrcPix += bmpSrc->pitch;
		TrgPix += bmpDest->pitch;
	}

	// Unlock em
	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);
}


///////////////////
// Draws a sprite doubly stretched
void DrawImageStretch2(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, int sx, int sy, int dx, int dy, int w, int h)
{
	// HINT: since the new copy-functors have been available, this function accepts surfaces of any format
	// However, passing surfaces with same format (bpp) makes this function faster

	// Clipping
	{
		int dw = w * 2;
		int dh = h * 2;

		// Source clipping
		if (!ClipRefRectWith(sx, sy, w, h, (SDLRect&)bmpSrc->clip_rect))
			return;

		// Dest clipping
		if (!ClipRefRectWith(dx, dy, dw, dh, (SDLRect&)bmpDest->clip_rect))
			return;

		w = MIN(w, dw/2);
		h = MIN(h, dh/2);
	}

	// Lock the surfaces
	LOCK_OR_QUIT(bmpDest);
	LOCK_OR_QUIT(bmpSrc);

	Uint8 *TrgPix = (Uint8 *)bmpDest->pixels + dy*bmpDest->pitch + dx*bmpDest->format->BytesPerPixel;
	Uint8 *SrcPix = (Uint8 *)bmpSrc->pixels +  sy*bmpSrc->pitch + sx*bmpSrc->format->BytesPerPixel;

	int doublepitch = bmpDest->pitch*2;
	int sbpp = bmpSrc->format->BytesPerPixel;
	int dbpp = bmpDest->format->BytesPerPixel;
	PixelCopy& copier = getPixelCopyFunc(bmpSrc, bmpDest);

    for(int y = h; y; --y) {

		Uint8 *sp = SrcPix;
		Uint8 *tp_x1 = TrgPix;
		Uint8 *tp_x2 = tp_x1 + bmpDest->pitch;
		for(int x = w; x; --x) {
            // Copy the 1 source pixel into a 4 pixel block on the destination surface
			copier.copy(tp_x1, sp); tp_x1 += dbpp;
			copier.copy(tp_x1, sp); tp_x1 += dbpp;
			copier.copy(tp_x2, sp); tp_x2 += dbpp;
			copier.copy(tp_x2, sp); tp_x2 += dbpp;
			sp += sbpp;
		}
		TrgPix += doublepitch;
		SrcPix += bmpSrc->pitch;
	}

	// Unlock em
	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);
}


///////////////////
// Draws a sprite doubly stretched with colour key
// HINT: doesn't work with alpha-surfaces
void DrawImageStretch2Key(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, int sx, int sy, int dx, int dy, int w, int h)
{
	// HINT: since the new copy-functors have been available, this function accepts surfaces of any format
	// However, passing surfaces with same format (bpp) makes this function faster

	// Clipping
	{
		int dw = w * 2;
		int dh = h * 2;

		// Source clipping
		if (!ClipRefRectWith(sx, sy, w, h, (SDLRect&)bmpSrc->clip_rect))
			return;

		// Dest clipping
		if (!ClipRefRectWith(dx, dy, dw, dh, (SDLRect&)bmpDest->clip_rect))
			return;

		w = MIN(w, dw/2);
		h = MIN(h, dh/2);
	}

	// Lock the surfaces
	LOCK_OR_QUIT(bmpDest);
	LOCK_OR_QUIT(bmpSrc);

	Uint8 *TrgPix = (Uint8 *)bmpDest->pixels + dy*bmpDest->pitch + dx*bmpDest->format->BytesPerPixel;
	Uint8 *SrcPix = (Uint8 *)bmpSrc->pixels +  sy*bmpSrc->pitch + sx*bmpSrc->format->BytesPerPixel;

	int doublepitch = bmpDest->pitch*2;
	int sbpp = bmpSrc->format->BytesPerPixel;
	int dbpp = bmpDest->format->BytesPerPixel;
	PixelCopy& copier = getPixelCopyFunc(bmpSrc, bmpDest);
	PixelGet& getter = getPixelGetFunc(bmpSrc);

    for(int y = h; y; --y) {

		Uint8 *sp = SrcPix;
		Uint8 *tp_x1 = TrgPix;
		Uint8 *tp_x2 = tp_x1 + bmpDest->pitch;
		for(int x = w; x; --x) {
			if (IsTransparent(bmpSrc, getter.get(sp)))  {
				// Skip the transparent pixel
				tp_x1 += dbpp * 2;
				tp_x2 += dbpp * 2;
			} else {
				// Copy the 1 source pixel into a 4 pixel block on the destination surface
				copier.copy(tp_x1, sp); tp_x1 += dbpp;
				copier.copy(tp_x1, sp); tp_x1 += dbpp;
				copier.copy(tp_x2, sp); tp_x2 += dbpp;
				copier.copy(tp_x2, sp); tp_x2 += dbpp;
			}
			sp += sbpp;
		}
		TrgPix += doublepitch;
		SrcPix += bmpSrc->pitch;
	}

	// Unlock em
	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);
}


///////////////////
// Draws a sprite mirrored doubly stretched with colour key
void DrawImageStretchMirrorKey(SDL_Surface *bmpDest, SDL_Surface * bmpSrc, int sx, int sy, int dx, int dy, int w, int h)
{
	// Clipping
	{
		int dw = w * 2;
		int dh = h * 2;

		// Source clipping
		if (!ClipRefRectWith(sx, sy, w, h, (SDLRect&)bmpSrc->clip_rect))
			return;

		// Clipping on dest surface
		if (!ClipRefRectWith(dx, dy, dw, dh, (SDLRect&)bmpDest->clip_rect))
			return;

		// Clipping could change w or h
		w = MIN(w, dw / 2);
		h = MIN(h, dh / 2);
	}


	// Lock the surfaces
	LOCK_OR_QUIT(bmpDest);
	LOCK_OR_QUIT(bmpSrc);


	Uint8 *TrgPix = (Uint8 *)bmpDest->pixels + dy*bmpDest->pitch + dx*bmpDest->format->BytesPerPixel;
	Uint8 *SrcPix = (Uint8 *)bmpSrc->pixels + sy*bmpSrc->pitch + sx*bmpSrc->format->BytesPerPixel;

	// Pre-calculate some things, so the loop is faster
	int doublepitch = bmpDest->pitch*2;
	int sbpp = bmpSrc->format->BytesPerPixel;
	int dbpp = bmpDest->format->BytesPerPixel;
	
	PixelGet& getter = getPixelGetFunc(bmpSrc);
	PixelCopy& copier = getPixelCopyFunc(bmpSrc, bmpDest);

    for(int y = h; y; --y) {

		Uint8 *sp = SrcPix;
		Uint8 *tp_x1 = TrgPix + w * 2 * dbpp;
		Uint8 *tp_x2 = tp_x1 + bmpDest->pitch;
		for(int x = w; x; --x) {
			if (!IsTransparent(bmpSrc, getter.get(sp)))  {
				// Non-transparent
				// Copy the 1 source pixel into a 4 pixel block on the destination surface
				copier.copy(tp_x1, sp); tp_x1 -= dbpp;
				copier.copy(tp_x1, sp); tp_x1 -= dbpp;
				copier.copy(tp_x2, sp); tp_x2 -= dbpp;
				copier.copy(tp_x2, sp); tp_x2 -= dbpp;
			} else {
				// Skip to next pixel
				tp_x1 -= dbpp * 2;
				tp_x2 -= dbpp * 2;
			}
			sp += sbpp;
		}
		TrgPix += doublepitch;
		SrcPix += bmpSrc->pitch;
	}


	// Unlock em
	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);
}

/////////////////////////
// Draw the image resized
void DrawImageResizedAdv(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, int sx, int sy, int dx, int dy, int sw, int sh, int dw, int dh)
{
	// Source clipping
	if (!ClipRefRectWith(sx, sy, sw, sh, (SDLRect&)bmpSrc->clip_rect))
		return;

	// Dest clipping
	if (!ClipRefRectWith(dx, dy, dw, dh, (SDLRect&)bmpDest->clip_rect))
		return;

	float xstep = (float)sw/(float)dw; // X step we'll do on the source surface
	float ystep = (float)sh/(float)dh; // Y step we'll do on the source surface

	// Update the widths/heights according to clipping
	dw = MIN(dw, Round((float)sw * xstep));
	dh = MIN(dh, Round((float)sh * ystep));

	// Lock the surfaces
	LOCK_OR_QUIT(bmpSrc);
	LOCK_OR_QUIT(bmpDest);

	// Pixels
	Uint8 *dst_px = NULL;
	Uint8 *dst_pxrow = (Uint8 *)bmpDest->pixels + (dy * bmpDest->pitch) + (dx * bmpDest->format->BytesPerPixel);
	int sbpp = bmpSrc->format->BytesPerPixel;
	int dbpp = bmpDest->format->BytesPerPixel;
	PixelCopy& copier = getPixelCopyFunc(bmpSrc, bmpDest);

	// Resize
	int dest_y = 0;
	for (float src_y = (float)sy; dest_y < dh; dest_y++)  {
		Uint8 *src_pxrow = (Uint8 *)bmpSrc->pixels + (int)(src_y) * bmpSrc->pitch;
		dst_px = dst_pxrow;

		int dest_x = dw;

		// Copy the row
		for (float src_x = (float)sx; dest_x; --dest_x)  {
			const Uint8 *src_px = src_pxrow + (int)(src_x) * sbpp;
			copier.copy(dst_px, src_px);
			src_x += xstep;
			dst_px += dbpp;
		}

		src_y += ystep;
		dst_pxrow += bmpDest->pitch;
	}

	// Unlock
	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);
}

/////////////////////////
// Draw the image resized
void DrawImageResizedAdv(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, int sx, int sy, int dx, int dy, int sw, int sh, float xratio, float yratio)
{
	int dw = Round((float)sw * xratio);
	int dh = Round((float)sh * yratio);
	DrawImageResizedAdv(bmpDest, bmpSrc, sx, sy, dx, dy, sw, sh, dw, dh);
}






// Copied over from SDL_stretch.c from SDL-1.2.9
// This is a safe, esp multithreadingsafe version of SDL_SoftStretch without any ASM magic.

static INLINE void copy_row(PixelCopy& copier, Uint8 *src, int src_w, int sbpp, Uint8 *dst, int dst_w, int dbpp)
{                                                                                                               
        int pos = 0x10000;                              
        int inc = (src_w << 16) / dst_w;                
        for ( int i=dst_w; i>0; --i ) {                 
                while ( pos >= 0x10000L ) {             
                        src += sbpp;                 
                        pos -= 0x10000L;                
                } 
				copier.copy(dst, src - sbpp);
                dst += dbpp;                         
                pos += inc;                             
        }                                               
}

/* Perform a stretch blit between two surfaces of the same format.
 */
int SafeSoftStretch(SDL_Surface *src, SDL_Rect *srcrect,
                    SDL_Surface *dst, SDL_Rect *dstrect)
{
	int src_locked;
	int dst_locked;
	int pos, inc;
	int dst_maxrow;
	int src_row, dst_row;
	Uint8 *srcp = NULL;
	Uint8 *dstp;
	SDL_Rect full_src;
	SDL_Rect full_dst;
	const int sbpp = src->format->BytesPerPixel;
	const int dbpp = dst->format->BytesPerPixel;
		
	/* Verify the blit rectangles */
	if ( srcrect ) {
		if ( (srcrect->x < 0) || (srcrect->y < 0) ||
			((srcrect->x+srcrect->w) > src->w) ||
			((srcrect->y+srcrect->h) > src->h) ) {
			SDL_SetError("Invalid source blit rectangle");
			return(-1);
		}
	} else {
		full_src.x = 0;
		full_src.y = 0;
		full_src.w = src->w;
		full_src.h = src->h;
		srcrect = &full_src;
	}
	if ( dstrect ) {
		if ( (dstrect->x < 0) || (dstrect->y < 0) ||
			((dstrect->x+dstrect->w) > dst->w) ||
			((dstrect->y+dstrect->h) > dst->h) ) {
			SDL_SetError("Invalid destination blit rectangle");
			return(-1);
		}
	} else {
		full_dst.x = 0;
		full_dst.y = 0;
		full_dst.w = dst->w;
		full_dst.h = dst->h;
		dstrect = &full_dst;
	}
	
	/* Lock the destination if it's in hardware */
	dst_locked = 0;
	if ( SDL_MUSTLOCK(dst) ) {
		if ( SDL_LockSurface(dst) < 0 ) {
			SDL_SetError("Unable to lock destination surface");
			return(-1);
		}
		dst_locked = 1;
	}
	/* Lock the source if it's in hardware */
	src_locked = 0;
	if ( SDL_MUSTLOCK(src) ) {
		if ( SDL_LockSurface(src) < 0 ) {
			if ( dst_locked ) {
				SDL_UnlockSurface(dst);
			}
			SDL_SetError("Unable to lock source surface");
			return(-1);
		}
		src_locked = 1;
	}
	
	/* Set up the data... */
	pos = 0x10000;
	inc = (srcrect->h << 16) / dstrect->h;
	src_row = srcrect->y;
	dst_row = dstrect->y;

	PixelCopy& copier = getPixelCopyFunc(src, dst);

	/* Perform the stretch blit */
	for ( dst_maxrow = dst_row+dstrect->h; dst_row<dst_maxrow; ++dst_row ) {
		dstp = (Uint8 *)dst->pixels + (dst_row*dst->pitch)
		+ (dstrect->x*dbpp);
		while ( pos >= 0x10000L ) {
			srcp = (Uint8 *)src->pixels + (src_row*src->pitch)
			+ (srcrect->x*sbpp);
			++src_row;
			pos -= 0x10000L;
		}
		
		copy_row(copier, srcp, srcrect->w, sbpp, dstp, dstrect->w, dbpp);
		pos += inc;
	}
	
	/* We need to unlock the surfaces if they're locked */
	if ( dst_locked ) {
		SDL_UnlockSurface(dst);
	}
	if ( src_locked ) {
		SDL_UnlockSurface(src);
	}
	return(0);
}





////////////////////////
// Draws the image nicely resampled
void DrawImageResampledAdv(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, int sx, int sy, int dx, int dy, int sw, int sh, float xratio, float yratio)
{
	if(!bmpSrc || !bmpDest) return;
	
	SDL_Rect src = { (SDLRect::Type) sx, (SDLRect::Type) sy, (SDLRect::TypeS) sw, (SDLRect::TypeS) sh };
	SDL_Rect dst = { (SDLRect::Type) dx, (SDLRect::Type) dy, (SDLRect::TypeS) ((float)sw * xratio), (SDLRect::TypeS) ((float)sh * yratio) };
	
	// Source clipping
	if (!ClipRefRectWith((SDLRect&)src, (SDLRect&)bmpSrc->clip_rect))
		return;

	// Dest clipping
	if (!ClipRefRectWith((SDLRect&)dst, (SDLRect&)bmpDest->clip_rect))
		return;
	
	if (dst.w != 0 && dst.h != 0)
		SafeSoftStretch(bmpSrc, &src, bmpDest, &dst);
}

////////////////////////
// Draws the image nicely resampled
void DrawImageResampledAdv(SDL_Surface * bmpDest, SDL_Surface * bmpSrc, int sx, int sy, int dx, int dy, int sw, int sh, int dw, int dh)
{
	if(!bmpSrc || !bmpDest) return;

	SDL_Rect src = { (SDLRect::Type) sx, (SDLRect::Type) sy, (SDLRect::TypeS) sw, (SDLRect::TypeS) sh };
	SDL_Rect dst = { (SDLRect::Type) dx, (SDLRect::Type) dy, (SDLRect::TypeS) dw, (SDLRect::TypeS) dh };
	
	// Source clipping
	if (!ClipRefRectWith((SDLRect&)src, (SDLRect&)bmpSrc->clip_rect))
		return;

	// Dest clipping
	if (!ClipRefRectWith((SDLRect&)dst, (SDLRect&)bmpDest->clip_rect))
		return;
	
	if (dw != 0 && dh != 0)
		SafeSoftStretch(bmpSrc, &src, bmpDest, &dst);
}


// horiz, i.e. y flipped
SmartPointer<SDL_Surface> GetHMirroredImage(const SmartPointer<SDL_Surface> & bmpSrc) {
	SmartPointer<SDL_Surface> result = SDL_CreateRGBSurface(
															bmpSrc->flags,
															bmpSrc->w, bmpSrc->h,
															bmpSrc->format->BitsPerPixel,
															bmpSrc->format->Rmask,
															bmpSrc->format->Gmask,
															bmpSrc->format->Bmask,
															bmpSrc->format->Amask);

	LockSurface(bmpSrc); LockSurface(result);
	PixelCopy& copier = getPixelCopyFunc(bmpSrc.get(), result.get());
	for(int y = 0; y < result->h; ++y) {
		for(int x = 0; x < result->w; ++x) {
			copier.copy(GetPixelAddr(result.get(), x, y), GetPixelAddr(bmpSrc.get(), x, bmpSrc->h - y - 1));
		}
	}
	UnlockSurface(result); UnlockSurface(bmpSrc);
	return result;
}

// vert, i.e. x flipped
SmartPointer<SDL_Surface> GetVMirroredImage(const SmartPointer<SDL_Surface> & bmpSrc) {
	SmartPointer<SDL_Surface> result = SDL_CreateRGBSurface(
															bmpSrc->flags,
															bmpSrc->w, bmpSrc->h,
															bmpSrc->format->BitsPerPixel,
															bmpSrc->format->Rmask,
															bmpSrc->format->Gmask,
															bmpSrc->format->Bmask,
															bmpSrc->format->Amask);

	LockSurface(bmpSrc); LockSurface(result);
	PixelCopy& copier = getPixelCopyFunc(bmpSrc.get(), result.get());
	for(int y = 0; y < result->h; ++y) {
		for(int x = 0; x < result->w; ++x) {
			copier.copy(GetPixelAddr(result.get(), x, y), GetPixelAddr(bmpSrc.get(), bmpSrc->w - x - 1, y));
		}
	}
	UnlockSurface(result); UnlockSurface(bmpSrc);
	return result;
}

// rotate clockwise
SmartPointer<SDL_Surface> GetRotatedImage(const SmartPointer<SDL_Surface> & bmpSrc) {
	SmartPointer<SDL_Surface> result = SDL_CreateRGBSurface(
															bmpSrc->flags,
															bmpSrc->h, bmpSrc->w, // flipped
															bmpSrc->format->BitsPerPixel,
															bmpSrc->format->Rmask,
															bmpSrc->format->Gmask,
															bmpSrc->format->Bmask,
															bmpSrc->format->Amask);
	LockSurface(bmpSrc); LockSurface(result);
	PixelCopy& copier = getPixelCopyFunc(bmpSrc.get(), result.get());
	for(int y = 0; y < result->h; ++y) {
		for(int x = 0; x < result->w; ++x) {
			copier.copy(GetPixelAddr(result.get(), x, y),
						GetPixelAddr(bmpSrc.get(), y, bmpSrc->h - x - 1)
						);
		}
	}
	UnlockSurface(result); UnlockSurface(bmpSrc);
	return result;
}


//
// Helper functions for the Scale2x algorithm
//

/*
	[A] [B] [C]
	[D] [E] [F]
	[G] [H] [I]
*/

// HINT: the A, C, G, I pixels are not used by the Scale2x algorithm
enum { B = 0, D, E, F, H };

////////////////////
// Does the source and destination surface clipping
static bool Clip2x(SDL_Surface* bmpDest, SDL_Surface* bmpSrc, int& sx, int& sy, int& dx, int& dy, int& w, int& h)
{
	if (!ClipRefRectWith(sx, sy, w, h, (SDLRect&)bmpSrc->clip_rect))
		return false;

	int dw = w * 2;
	int dh = h * 2;

	if (!ClipRefRectWith(dx, dy, dw, dh, (SDLRect&)bmpDest->clip_rect))
		return false;

	w = MIN(w, dw/2);
	h = MIN(h, dh/2);

	return true;
}

static void Scale2xPixel(SDL_Surface *bmpDest, SDL_Surface *bmpSrc, int dx, int dy, Uint32 *colors, PixelPut& putter)
{
	// Put the 2x2 square on the dest surface
	const int bpp = bmpDest->format->BytesPerPixel;

	Uint8 *pixel = GetPixelAddr(bmpDest, dx, dy);

	// Get the four colors to put on the dest surface
	Uint32 dstE[4];
	if (colors[B] != colors[H] && colors[D] != colors[F]) {
		dstE[0] = (colors[D] == colors[B]) ? colors[D] : colors[E];
		dstE[1] = (colors[B] == colors[F]) ? colors[F] : colors[E];
		dstE[2] = (colors[D] == colors[H]) ? colors[D] : colors[E];
		dstE[3] = (colors[H] == colors[F]) ? colors[F] : colors[E];

		// Put the four pixels
		putter.put(pixel, dstE[0]);
		putter.put(pixel + bpp, dstE[1]);

		pixel += bmpDest->pitch;
		putter.put(pixel, dstE[2]);
		putter.put(pixel + bpp, dstE[3]);
	} else {
		// One, solid pixel
		putter.put(pixel, colors[E]);
		putter.put(pixel + bpp, colors[E]);

		pixel += bmpDest->pitch;
		putter.put(pixel, colors[E]);
		putter.put(pixel + bpp, colors[E]);
	}
}

static void Scale2xPixel_FF(SDL_Surface *bmpDest, SDL_Surface *bmpSrc, 
							int sx, int sy, int dx, int dy, PixelGet& getter, PixelPut& putter)
{
	Uint8 *addr = GetPixelAddr(bmpSrc, sx, sy);
	Uint32 colors[5];
	colors[B] = colors[D] = colors[E] = getter.get(addr);
	colors[F] = getter.get(addr + bmpSrc->format->BytesPerPixel); // x + 1
	colors[H] = getter.get(addr + bmpSrc->pitch);  // y + 1
	Scale2xPixel(bmpDest, bmpSrc, dx, dy, colors, putter);
}

static void Scale2xPixel_LF(SDL_Surface *bmpDest, SDL_Surface *bmpSrc,
							int sx, int sy, int dx, int dy, PixelGet& getter, PixelPut& putter)
{
	Uint8 *addr = GetPixelAddr(bmpSrc, sx, sy);
	Uint32 colors[5];
	colors[B] = colors[E] = colors[F] = getter.get(addr);
	colors[D] = getter.get(addr - bmpSrc->format->BytesPerPixel);
	colors[H] = getter.get(addr + bmpSrc->pitch);
	Scale2xPixel(bmpDest, bmpSrc, dx, dy, colors, putter);
}

static void Scale2xPixel_FL(SDL_Surface *bmpDest, SDL_Surface *bmpSrc,
							int sx, int sy, int dx, int dy, PixelGet& getter, PixelPut& putter)
{
	Uint8 *addr = GetPixelAddr(bmpSrc, sx, sy);
	Uint32 colors[5];
	colors[D] = colors[E] = colors[H] = getter.get(addr);
	colors[B] = getter.get(addr - bmpSrc->pitch);
	colors[F] = getter.get(addr + bmpSrc->format->BytesPerPixel);
	Scale2xPixel(bmpDest, bmpSrc, dx, dy, colors, putter);
}

static void Scale2xPixel_LL(SDL_Surface *bmpDest, SDL_Surface *bmpSrc,
							int sx, int sy, int dx, int dy, PixelGet& getter, PixelPut& putter)
{
	Uint8 *addr = GetPixelAddr(bmpSrc, sx, sy);
	Uint32 colors[5];
	colors[E] = colors[F] = colors[H] = getter.get(addr);
	colors[B] = getter.get(addr - bmpSrc->pitch);
	colors[D] = getter.get(addr - bmpSrc->format->BytesPerPixel);
	Scale2xPixel(bmpDest, bmpSrc, dx, dy, colors, putter);
}

static void Scale2xPixel_F(SDL_Surface *bmpDest, SDL_Surface *bmpSrc,
						   int sx, int sy, int dx, int dy, PixelGet& getter, PixelPut& putter)
{
	Uint8 *addr = GetPixelAddr(bmpSrc, sx, sy);
	Uint32 colors[5];
	colors[D] = colors[E] = getter.get(addr);
	colors[B] = getter.get(addr - bmpSrc->pitch);
	colors[F] = getter.get(addr + bmpSrc->format->BytesPerPixel);
	colors[H] = getter.get(addr + bmpSrc->pitch);
	Scale2xPixel(bmpDest, bmpSrc, dx, dy, colors, putter);
}

static void Scale2xPixel_L(SDL_Surface *bmpDest, SDL_Surface *bmpSrc,
						   int sx, int sy, int dx, int dy, PixelGet& getter, PixelPut& putter)
{
	Uint8 *addr = GetPixelAddr(bmpSrc, sx, sy);
	Uint32 colors[5];
	colors[E] = colors[F] = getter.get(addr);
	colors[B] = getter.get(addr - bmpSrc->pitch);
	colors[D] = getter.get(addr - bmpSrc->format->BytesPerPixel);
	colors[H] = getter.get(addr + bmpSrc->pitch);
	Scale2xPixel(bmpDest, bmpSrc, dx, dy, colors, putter);
}

/////////////////////////
// Draws the image double-sized using the scale2x algorithm
// This algo is taken from http://scale2x.sourceforge.net/algorithm.html
// Thanks go to the AdvanceMAME team!
void DrawImageScale2x(SDL_Surface* bmpDest, SDL_Surface* bmpSrc, int sx, int sy, int dx, int dy, int w, int h)
{
	PixelPut& putter = getPixelPutFunc(bmpDest);
	PixelGet& getter = getPixelGetFunc(bmpSrc);
	const unsigned sbpp = bmpSrc->format->BytesPerPixel;
	Uint8 *px1, *px2;

	// Lock
	LOCK_OR_QUIT(bmpDest);
	LOCK_OR_QUIT(bmpSrc);

	// Clipping
	if (!Clip2x(bmpDest, bmpSrc, sx, sy, dx, dy, w, h))
		return;

	if (w < 2 || h < 2)  {
		DrawImageStretch2(bmpDest, bmpSrc, sx, sy, dx, dy, w, h);
		return;
	}

	// Variables
	int sx2 = sx + w - 1;
	int sy2 = sy + h - 1;

	Uint32 colors[5];

	// First pixel, first line
	Scale2xPixel_FF(bmpDest, bmpSrc, sx, sy, dx, dy, getter, putter);

	// Last pixel, first line
	Scale2xPixel_LF(bmpDest, bmpSrc, sx2, sy, dx + w * 2 - 2, dy, getter, putter);

	// First line
	px1 = GetPixelAddr(bmpSrc, sx + 1, sy);
	px2 = px1 + bmpSrc->pitch;
	for(int x = 1; x < w - 1; ++x, px1 += sbpp, px2 += sbpp)  {
		colors[B] = colors[E] = getter.get(px1);
		colors[D] = getter.get(px1 - sbpp);
		colors[F] = getter.get(px1 + sbpp);
		colors[H] = getter.get(px2);

		Scale2xPixel(bmpDest, bmpSrc, dx + x * 2, dy, colors, putter);
	}

	// First pixel, last line
	Scale2xPixel_FL(bmpDest, bmpSrc, sx, sy2, dx, dy + h * 2 - 2, getter, putter);

	// Last pixel, last line
	Scale2xPixel_LL(bmpDest, bmpSrc, sx2, sy2, dx + w * 2 - 2, dy + h * 2 - 2, getter, putter);


	// Last line
	px1 = GetPixelAddr(bmpSrc, sx + 1, sy2 - 1);
	px2 = px1 + bmpSrc->pitch;
	for(int x = 1; x < w - 1; ++x, px1 += sbpp, px2 += sbpp)  {
		colors[E] = colors[H] = getter.get(px2);
		colors[B] = getter.get(px1);
		colors[D] = getter.get(px2 - sbpp);
		colors[F] = getter.get(px2 + sbpp);

		Scale2xPixel(bmpDest, bmpSrc, dx + x * 2, dy + h * 2 - 2, colors, putter);
	}

	// Rest of the image
	for(int y = 1; y < h - 1; ++y) {
		const int y2 = y * 2;

		// First & last pixel
		Scale2xPixel_F(bmpDest, bmpSrc, sx, sy + y, dx, dy + y2, getter, putter);
		Scale2xPixel_L(bmpDest, bmpSrc, sx2, sy + y, dx + w * 2 - 2, dy + y2, getter, putter);

		// Rest of the line
		Uint8 *px = GetPixelAddr(bmpSrc, sx + w - 2, sy + y);
		for(int x = w - 2; x; --x, px -= sbpp) {
			colors[B] = getter.get(px - bmpSrc->pitch);
			colors[D] = getter.get(px - sbpp);
			colors[E] = getter.get(px);
			colors[F] = getter.get(px + sbpp);
			colors[H] = getter.get(px + bmpSrc->pitch);

			Scale2xPixel(bmpDest, bmpSrc, dx + x * 2, dy + y2, colors, putter);
		}
	}

	// Unlock
	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);
}

///////////////////////
// Helper function for HalfImageScaleHalf
static Uint32 HalfBlendPixel(Uint32 p1, Uint32 p2, Uint32 p3, Uint32 p4, SDL_PixelFormat *fmt)
{
/*#define PIXEL_IMPACT(pixel, channel) (((pixel & fmt->##channel##mask) >> fmt->##channel##shift)/4)
	return ((PIXEL_IMPACT(p1, R) + PIXEL_IMPACT(p2, R) + PIXEL_IMPACT(p3, R) + PIXEL_IMPACT(p4, R)) << fmt->Rshift) |
		((PIXEL_IMPACT(p1, G) + PIXEL_IMPACT(p2, G) + PIXEL_IMPACT(p3, G) + PIXEL_IMPACT(p4, G)) << fmt->Gshift) |
		((PIXEL_IMPACT(p1, B) + PIXEL_IMPACT(p2, B) + PIXEL_IMPACT(p3, B) + PIXEL_IMPACT(p4, B)) << fmt->Bshift);*/

	// TODO: does this work on big endian systems? (there could be an overflow in the red channel)
#define PIXEL_CHANNEL(pixel, channel) ( (Uint32) (pixel & fmt-> channel##mask) )
	return (Uint32) (
		((((PIXEL_CHANNEL(p1, R) + PIXEL_CHANNEL(p2, R) + PIXEL_CHANNEL(p3, R) + PIXEL_CHANNEL(p4, R))) / 4) & fmt->Rmask) |
		((((PIXEL_CHANNEL(p1, G) + PIXEL_CHANNEL(p2, G) + PIXEL_CHANNEL(p3, G) + PIXEL_CHANNEL(p4, G))) / 4) & fmt->Gmask) |
		((((PIXEL_CHANNEL(p1, B) + PIXEL_CHANNEL(p2, B) + PIXEL_CHANNEL(p3, B) + PIXEL_CHANNEL(p4, B))) / 4) & fmt->Bmask) ) |
		fmt->Amask; // HINT: this doesn't support semi-transparent surfaces
}

void DrawImageScaleHalf(SDL_Surface* bmpDest, SDL_Surface* bmpSrc) {

	// Lock
	LOCK_OR_QUIT(bmpDest);
	LOCK_OR_QUIT(bmpSrc);

	short bpp = bmpDest->format->BytesPerPixel;
	Uint8 *srcpx_1 = (Uint8 *)bmpSrc->pixels;
	Uint8 *srcpx_2 = (Uint8 *)bmpSrc->pixels + bmpSrc->pitch;
	int srcgap = bmpSrc->pitch * 2 - bmpSrc->w * bmpSrc->format->BytesPerPixel; // *2 because we skip two lines on the source surface
	Uint8 *dstpx = (Uint8 *)bmpDest->pixels;
	int dstgap = bmpDest->pitch - bmpDest->w * bmpDest->format->BytesPerPixel;

	int w = bmpSrc->w / 2;
	int h = bmpSrc->h / 2;
	for(int y = h; y; --y, srcpx_1 += srcgap, srcpx_2 += srcgap, dstpx += dstgap)
		for(int x = w; x; --x, srcpx_1 += bpp * 2, srcpx_2 += bpp * 2, dstpx += bpp) {
			const Uint32 px1 = GetPixelFromAddr(srcpx_1, bpp);  // x, y
			const Uint32 px2 = GetPixelFromAddr(srcpx_1 + bpp, bpp); // x + 1, y
			const Uint32 px3 = GetPixelFromAddr(srcpx_2, bpp);  // x, y + 1
			const Uint32 px4 = GetPixelFromAddr(srcpx_2 + bpp, bpp);  // x + 1, y + 1
			PutPixelToAddr(dstpx, HalfBlendPixel(px1, px2, px3, px4, bmpSrc->format), bpp);
		}

	// Unlock
	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);

}

static Color HalfBlendPixelAdv(Uint32 s1, Uint32 s2, Uint32 s3, Uint32 s4, bool usekey, const SDL_Surface *src)
{
	const SDL_PixelFormat *format = src->format;
	Color c1(format, s1);
	Color c2(format, s2);
	Color c3(format, s3);
	Color c4(format, s4);
	if (usekey)  {
		Uint32 colorkey = Surface_GetColorKey(src);
		if (EqualRGB(s1, colorkey, format))
			c1.a = 0;
		if (EqualRGB(s2, colorkey, format))
			c2.a = 0;
		if (EqualRGB(s3, colorkey, format))
			c3.a = 0;
		if (EqualRGB(s4, colorkey, format))
			c4.a = 0;
	}

#define CHPART(var, chan) ((var.chan * var.a) >> 10)

	Color res(CHPART(c1, r) + CHPART(c2, r) + CHPART(c3, r) + CHPART(c4, r),
				CHPART(c1, g) + CHPART(c2, g) + CHPART(c3, g) + CHPART(c4, g),
				CHPART(c1, b) + CHPART(c2, b) + CHPART(c3, b) + CHPART(c4, b),
				(c1.a + c2.a + c3.a + c4.a) >> 2);
	return res;
}

/////////////////////////
// Draws the image half-scaled onto the destination surface
// Handles colorkey and alpha correctly
void DrawImageScaleHalfAdv(SDL_Surface* bmpDest, SDL_Surface* bmpSrc, int sx, int sy, int dx, int dy, int sw, int sh)
{
	// Lock
	LOCK_OR_QUIT(bmpDest);
	LOCK_OR_QUIT(bmpSrc);

	// Clip
	int dw = sw / 2;
	int dh = sh / 2;
	if (!ClipRefRectWith(dx, dy, dw, dh, (SDLRect&)bmpDest->clip_rect))
		return;
	sw = dw * 2;
	sh = dh * 2;
	if (!ClipRefRectWith(sx, sy, sw, sh, (SDLRect&)bmpSrc->clip_rect))
		return;

	if (sh < 2 || sw < 2)
		return;
	dw = sw / 2;
	dh = sh / 2;

	const short bpp = bmpDest->format->BytesPerPixel;
	Uint8 *srcrow_1 = GetPixelAddr(bmpSrc, sx, sy);
	Uint8 *srcrow_2 = GetPixelAddr(bmpSrc, sx, sy + 1);
	Uint8 *dstrow = GetPixelAddr(bmpDest, dx, dy);
	PixelPutAlpha& put = getPixelAlphaPutFunc(bmpDest);
	const bool usekey = Surface_HasColorKey(bmpSrc);

	for(int y = sy + dh; y != sy; --y)  {
		Uint8 *srcpx_1 = srcrow_1;
		Uint8 *srcpx_2 = srcrow_2;
		Uint8 *dstpx = dstrow;
		for(int x = sx + dw; x != sx; --x, srcpx_1 += bpp * 2, srcpx_2 += bpp * 2, dstpx += bpp) {
			Uint32 px1 = GetPixelFromAddr(srcpx_1, bpp);  // x, y
			Uint32 px2 = GetPixelFromAddr(srcpx_1 + bpp, bpp); // x + 1, y
			Uint32 px3 = GetPixelFromAddr(srcpx_2, bpp);  // x, y + 1
			Uint32 px4 = GetPixelFromAddr(srcpx_2 + bpp, bpp);  // x + 1, y + 1
			const Color pxBg(bmpDest->format, GetPixelFromAddr(dstpx, bpp));

			put.put(dstpx, bmpDest->format, HalfBlendPixelAdv(px1, px2, px3, px4, usekey, bmpSrc));
			//PutPixelToAddr(dstpx, HalfBlendPixel(px1, px2, px3, px4, bmpSrc->format), bpp);
		}

		srcrow_1 += 2*bmpSrc->pitch;
		srcrow_2 += 2*bmpSrc->pitch;
		dstrow += bmpDest->pitch;
	}

	// Unlock
	UnlockSurface(bmpDest);
	UnlockSurface(bmpSrc);

}


//////////////////////
// Generates a shadow for the given object
SmartPointer<SDL_Surface> GenerateShadowSurface(SDL_Surface *object, unsigned char opacity)
{
	if (object == NULL)
		return NULL;

	SmartPointer<SDL_Surface> res = gfxCreateSurfaceAlpha(object->w, object->h);
	if (!LockSurface(res) || !LockSurface(object))
		return NULL;

	Uint8 *objrow = (Uint8 *)object->pixels;
	Uint8 *shrow = (Uint8 *)res->pixels;
	const Uint32 shadow = Color(0, 0, 0, opacity).get(res->format);
	const Uint32 transparent = Color(0, 0, 0, 0).get(res->format);
	for (int y = object->h; y; --y)  {
		Uint8 *objpx = objrow;
		Uint8 *shpx = shrow;
		for (int x = object->w; x; --x, objpx += object->format->BytesPerPixel, shpx += res->format->BytesPerPixel)  {
			if (IsTransparent(object, GetPixelFromAddr(objpx, object->format->BytesPerPixel)))
				PutPixelToAddr(shpx, transparent, res->format->BytesPerPixel);
			else
				PutPixelToAddr(shpx, shadow, res->format->BytesPerPixel);
		}

		objrow += object->pitch;
		shrow += res->pitch;
	}

	UnlockSurface(res);
	UnlockSurface(object);

	return res;
}




/////////////////
//
// Special line and pixel drawing
//
/////////////////


int ropecolour = 0;
int ropealt = 0;

///////////////////
// Put a pixel on the surface (while checking for clipping)
void RopePutPixelA(SDL_Surface * bmpDest, int x, int y, Uint32 colour, Uint8 alpha)
{
	// Warning: lock the surface before calling this!
	// Warning: passing NULL surface will cause a segfault

	ropealt++;
	ropealt %= 3;

	if (ropealt == 2)
		ropecolour = !ropecolour;
	colour = tLX->clRopeColors[ropecolour].get(bmpDest->format);

	// No alpha weight, use direct pixel access (faster)
	if (alpha == 255)  {
		colour = SDLColourToNativeColour(colour, bmpDest->format->BytesPerPixel);
		Uint8 *px = (Uint8 *)bmpDest->pixels + bmpDest->pitch*y + x*bmpDest->format->BytesPerPixel;
		Uint8 *px2 = px+bmpDest->pitch;
		memcpy(px,&colour,bmpDest->format->BytesPerPixel);
		memcpy(px+bmpDest->format->BytesPerPixel,&colour,bmpDest->format->BytesPerPixel);
		memcpy(px2,&colour,bmpDest->format->BytesPerPixel);
		memcpy(px2+bmpDest->format->BytesPerPixel,&colour,bmpDest->format->BytesPerPixel);
	} else { // Alpha, use special routine
		PutPixelA(bmpDest, x, y, colour, alpha);
		PutPixelA(bmpDest, x+1, y, colour, alpha);
		PutPixelA(bmpDest, x, y+1, colour, alpha);
		PutPixelA(bmpDest, x+1, y+1, colour, alpha);
	}
}

// For compatibility with perform_line
// NOTE: it's slightly different, to keep the rope looking the same
void RopePutPixel(SDL_Surface * bmpDest, int x, int y, Uint32 colour) {
	ropealt = !ropealt;

	if (ropealt)
		ropecolour = !ropecolour;
	colour = tLX->clRopeColors[ropecolour].get(bmpDest->format);
	

	// Put the pixel
	colour = SDLColourToNativeColour(colour, bmpDest->format->BytesPerPixel);
	Uint8 *px = (Uint8 *)bmpDest->pixels + bmpDest->pitch*y + x*bmpDest->format->BytesPerPixel;
	Uint8 *px2 = px+bmpDest->pitch;
	memcpy(px,&colour,bmpDest->format->BytesPerPixel);
	memcpy(px+bmpDest->format->BytesPerPixel,&colour,bmpDest->format->BytesPerPixel);
	memcpy(px2,&colour,bmpDest->format->BytesPerPixel);
	memcpy(px2+bmpDest->format->BytesPerPixel,&colour,bmpDest->format->BytesPerPixel);
}


///////////////////
// Put a pixel on the surface (while checking for clipping)
void BeamPutPixelA(SDL_Surface * bmpDest, int x, int y, Uint32 colour, Uint8 alpha)
{
	// No alpha, use direct pixel access
	if (alpha == 255)  {
		colour = SDLColourToNativeColour(colour, bmpDest->format->BytesPerPixel);
		Uint8 *px = (Uint8 *)bmpDest->pixels+bmpDest->pitch*y+x*bmpDest->format->BytesPerPixel;
		Uint8 *px2 = px+bmpDest->pitch;
		memcpy(px,&colour,bmpDest->format->BytesPerPixel);
		memcpy(px+bmpDest->format->BytesPerPixel,&colour,bmpDest->format->BytesPerPixel);
		memcpy(px2,&colour,bmpDest->format->BytesPerPixel);
		memcpy(px2+bmpDest->format->BytesPerPixel,&colour,bmpDest->format->BytesPerPixel);
	} else { // Alpha weight, use special routine
		PutPixelA(bmpDest, x, y, colour, alpha);
		PutPixelA(bmpDest, x+1, y, colour, alpha);
		PutPixelA(bmpDest, x, y+1, colour, alpha);
		PutPixelA(bmpDest, x+1, y+1, colour, alpha);
	}
}

void BeamPutPixel(SDL_Surface * bmpDest, int x, int y, Uint32 colour) { // For compatibility with perform_line
	BeamPutPixelA(bmpDest, x, y, colour, 255);
}



///////////////////
// Put a laser-sight pixel on the surface
void LaserSightPutPixel(SDL_Surface * bmpDest, int x, int y, Uint32 colour)
{
	static int laseralt = 0;
	laseralt++;
	laseralt %= GetRandomInt(35)+1;

	if(laseralt != 0)
		return;

	colour = tLX->clLaserSightColors[ GetRandomInt(1) ].get(bmpDest->format);

	// Snap to nearest 2nd pixel
	x -= x % 2;
	y -= y % 2;

	// Put the pixel (laser sight is never antialiased)
	colour = SDLColourToNativeColour(colour, bmpDest->format->BytesPerPixel);
	Uint8 *px = (Uint8 *)bmpDest->pixels+bmpDest->pitch*y+x*bmpDest->format->BytesPerPixel;
	Uint8 *px2 = px+bmpDest->pitch;
	memcpy(px,&colour,bmpDest->format->BytesPerPixel);
	memcpy(px+bmpDest->format->BytesPerPixel,&colour,bmpDest->format->BytesPerPixel);
	memcpy(px2,&colour,bmpDest->format->BytesPerPixel);
	memcpy(px2+bmpDest->format->BytesPerPixel,&colour,bmpDest->format->BytesPerPixel);
}

////////////////////
// Perform a line draw using a put pixel callback
// Grabbed from allegro
INLINE void perform_line(SDL_Surface * bmp, int x1, int y1, int x2, int y2, Color col, void (*proc)(SDL_Surface * , int, int, Uint32, Uint8))
{
	int dx = x2-x1;
	int dy = y2-y1;
	int i1, i2;
	int x, y;
	int dd;
	
	Uint32 d = col.get(bmp->format);

   LOCK_OR_QUIT(bmp);

   /* worker macro */
   #define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)     \
   {                                                                         \
      if (d##pri_c == 0) {                                                   \
		 proc(bmp, x1, y1, d, col.a);                                               \
		 return;                                                             \
      }                                                                      \
									     \
      i1 = 2 * d##sec_c;                                                     \
      dd = i1 - (sec_sign (pri_sign d##pri_c));                              \
      i2 = dd - (sec_sign (pri_sign d##pri_c));                              \
									     \
      x = x1;                                                                \
      y = y1;                                                                \
									     \
      while (pri_c pri_cond pri_c##2) {                                      \
		 proc(bmp, x, y, d, col.a);                                                 \
										 \
		 if (dd sec_cond 0) {                                                \
			sec_c sec_sign##= 1;                                             \
			dd += i2;                                                        \
		 }                                                                   \
		 else                                                                \
			dd += i1;                                                        \
										 \
		 pri_c pri_sign##= 1;                                                \
      }                                                                      \
   }

   if (dx >= 0) {
      if (dy >= 0) {
	 if (dx >= dy) {
	    /* (x1 <= x2) && (y1 <= y2) && (dx >= dy) */
	    DO_LINE(+, x, <=, +, y, >=);
	 }
	 else {
	    /* (x1 <= x2) && (y1 <= y2) && (dx < dy) */
	    DO_LINE(+, y, <=, +, x, >=);
	 }
      }
      else {
	 if (dx >= -dy) {
	    /* (x1 <= x2) && (y1 > y2) && (dx >= dy) */
	    DO_LINE(+, x, <=, -, y, <=);
	 }
	 else {
	    /* (x1 <= x2) && (y1 > y2) && (dx < dy) */
	    DO_LINE(-, y, >=, +, x, >=);
	 }
      }
   }
   else {
      if (dy >= 0) {
	 if (-dx >= dy) {
	    /* (x1 > x2) && (y1 <= y2) && (dx >= dy) */
	    DO_LINE(-, x, >=, +, y, >=);
	 }
	 else {
	    /* (x1 > x2) && (y1 <= y2) && (dx < dy) */
	    DO_LINE(+, y, <=, -, x, <=);
	 }
      }
      else {
	 if (-dx >= -dy) {
	    /* (x1 > x2) && (y1 > y2) && (dx >= dy) */
	    DO_LINE(-, x, >=, -, y, <=);
	 }
	 else {
	    /* (x1 > x2) && (y1 > y2) && (dx < dy) */
	    DO_LINE(-, y, >=, -, x, <=);
	 }
      }
   }

#undef DO_LINE
	
   UnlockSurface(bmp);
}

INLINE void perform_line(SDL_Surface * bmp, int x1, int y1, int x2, int y2, Color col, void (*proc)(SDL_Surface * , int, int, Uint32))
{
	int dx = x2-x1;
	int dy = y2-y1;
	int i1, i2;
	int x, y;
	int dd;
	
	Uint32 d = col.get(bmp->format);
	
	LOCK_OR_QUIT(bmp);
	
	/* worker macro */
	#define DO_LINE(pri_sign, pri_c, pri_cond, sec_sign, sec_c, sec_cond)     \
	{                                                                         \
	if (d##pri_c == 0) {                                                   \
	proc(bmp, x1, y1, d);                                               \
	return;                                                             \
	}                                                                      \
	\
	i1 = 2 * d##sec_c;                                                     \
	dd = i1 - (sec_sign (pri_sign d##pri_c));                              \
	i2 = dd - (sec_sign (pri_sign d##pri_c));                              \
	\
	x = x1;                                                                \
	y = y1;                                                                \
	\
	while (pri_c pri_cond pri_c##2) {                                      \
	proc(bmp, x, y, d);                                                 \
	\
	if (dd sec_cond 0) {                                                \
	sec_c sec_sign##= 1;                                             \
	dd += i2;                                                        \
	}                                                                   \
	else                                                                \
	dd += i1;                                                        \
	\
	pri_c pri_sign##= 1;                                                \
	}                                                                      \
	}
		
	if (dx >= 0) {
		if (dy >= 0) {
			if (dx >= dy) {
				/* (x1 <= x2) && (y1 <= y2) && (dx >= dy) */
				DO_LINE(+, x, <=, +, y, >=);
			}
			else {
				/* (x1 <= x2) && (y1 <= y2) && (dx < dy) */
				DO_LINE(+, y, <=, +, x, >=);
			}
		}
		else {
			if (dx >= -dy) {
				/* (x1 <= x2) && (y1 > y2) && (dx >= dy) */
				DO_LINE(+, x, <=, -, y, <=);
			}
			else {
				/* (x1 <= x2) && (y1 > y2) && (dx < dy) */
				DO_LINE(-, y, >=, +, x, >=);
			}
		}
	}
	else {
		if (dy >= 0) {
			if (-dx >= dy) {
				/* (x1 > x2) && (y1 <= y2) && (dx >= dy) */
				DO_LINE(-, x, >=, +, y, >=);
			}
			else {
				/* (x1 > x2) && (y1 <= y2) && (dx < dy) */
				DO_LINE(+, y, <=, -, x, <=);
			}
		}
		else {
			if (-dx >= -dy) {
				/* (x1 > x2) && (y1 > y2) && (dx >= dy) */
				DO_LINE(-, x, >=, -, y, <=);
			}
			else {
				/* (x1 > x2) && (y1 > y2) && (dx < dy) */
				DO_LINE(-, y, >=, -, x, <=);
			}
		}
	}
	
	#undef DO_LINE
	
	UnlockSurface(bmp);
}



INLINE void secure_perform_line(SDL_Surface * bmpDest, int x1, int y1, int x2, int y2, Color color, void (*proc)(SDL_Surface *, int, int, Uint32, Uint8)) {
	if (!ClipLine(bmpDest, &x1, &y1, &x2, &y2)) // Clipping
		return;

	perform_line(bmpDest, x1, y1, x2, y2, color, proc);
}

INLINE void secure_perform_line(SDL_Surface * bmpDest, int x1, int y1, int x2, int y2, Color color, void (*proc)(SDL_Surface *, int, int, Uint32)) {
	if (!ClipLine(bmpDest, &x1, &y1, &x2, &y2)) // Clipping
		return;
	
	perform_line(bmpDest, x1, y1, x2, y2, color, proc);
}


////////////////////////
// Draw horizontal line
void DrawHLine(SDL_Surface * bmpDest, int x, int x2, int y, Color colour) {

	// Swap the ends if necessary
	if (x2 < x)  {
		int tmp;
		tmp = x;
		x = x2;
		x2 = tmp;
	}

	const SDL_Rect& r = bmpDest->clip_rect;

	// Clipping
	if (y < r.y) return;
	if (y >= (r.y + r.h)) return;

	if (x < r.x)
		x = r.x;
	else if (x >= (r.x + r.w))
		return;

	if (x2 < r.x)
		return;
	else if (x2 >= (r.x + r.w))
		x2 = r.x + r.w - 1;

	// Lock
	LOCK_OR_QUIT(bmpDest);
	byte bpp = (byte)bmpDest->format->BytesPerPixel;
	Uint8 *px2 = (uchar *)bmpDest->pixels+bmpDest->pitch*y+bpp*x2;

	// Draw depending on the alpha
	switch (colour.a)  {
	case SDL_ALPHA_OPAQUE:  
	{
		// Solid (no alpha) drawing
		PixelPut& putter = getPixelPutFunc(bmpDest);
		Uint32 packed_cl = Pack(colour, bmpDest->format);
		for (Uint8* px = (Uint8*)bmpDest->pixels + bmpDest->pitch * y + bpp * x; px <= px2; px += bpp)
			putter.put(px, packed_cl);
	} break;
	case SDL_ALPHA_TRANSPARENT:
	break;
	default:
	{
		// Draw the line alpha-blended with the background
		PixelPutAlpha& putter = getPixelAlphaPutFunc(bmpDest);
		for (Uint8* px = (Uint8*)bmpDest->pixels + bmpDest->pitch * y + bpp * x; px <= px2; px += bpp)
			putter.put(px, bmpDest->format, colour);
	}
	}

	UnlockSurface(bmpDest);

}

// Draw vertical line
void DrawVLine(SDL_Surface * bmpDest, int y, int y2, int x, Color colour) {

	// Swap the ends if necessary
	if (y2 < y)  {
		int tmp;
		tmp = y;
		y = y2;
		y2 = tmp;
	}

	const SDL_Rect& r = bmpDest->clip_rect;

	// Clipping
	if (x < r.x) return;
	if (x >= (r.x + r.w)) return;

	if (y < r.y)
		y = r.y;
	else if (y >= (r.y + r.h))
		return;

	if (y2 < r.y)
		return;
	else if (y2 >= (r.y + r.h))
		y2 = r.y + r.h - 1;

	LOCK_OR_QUIT(bmpDest);
	ushort pitch = (ushort)bmpDest->pitch;
	byte bpp = (byte)bmpDest->format->BytesPerPixel;
	Uint8 *px2 = (Uint8 *)bmpDest->pixels+pitch*y2+bpp*x;

	// Draw depending on the alpha
	switch (colour.a)  {
	case SDL_ALPHA_OPAQUE:  
	{
		// Solid (no alpha) drawing
		PixelPut& putter = getPixelPutFunc(bmpDest);
		Uint32 packed_cl = Pack(colour, bmpDest->format);
		for (Uint8 *px= (Uint8 *)bmpDest->pixels+pitch*y + bpp*x; px <= px2; px+=pitch)
			putter.put(px, packed_cl);
	} break;
	case SDL_ALPHA_TRANSPARENT:
	break;
	default:
	{
		// Draw the line alpha-blended with the background
		PixelPutAlpha& putter = getPixelAlphaPutFunc(bmpDest);
		for (Uint8 *px= (Uint8 *)bmpDest->pixels+pitch*y + bpp*x; px <= px2; px+=pitch)
			putter.put(px, bmpDest->format, colour);
	}
	}

	UnlockSurface(bmpDest);
}

///////////////////
// Line drawing
void DrawLine(SDL_Surface * dst, Sint16 x1, Sint16 y1, Sint16 x2, Sint16 y2, Color color) {
	secure_perform_line(dst, x1, y1, x2, y2, color, PutPixelA);
}

//////////////////
// Fast routine for drawing 2x2 filled rects
void DrawRectFill2x2_NoClip(SDL_Surface *bmpDest, int x, int y, Color color)
{
	LOCK_OR_QUIT(bmpDest);

	Uint8 *row1 = (Uint8 *)bmpDest->pixels + y * bmpDest->pitch + x * bmpDest->format->BytesPerPixel;
	Uint8 *row2 = row1 + bmpDest->pitch;

	PixelPutAlpha& putter = getPixelAlphaPutFunc(bmpDest);
	putter.put(row1, bmpDest->format, color);
	putter.put(row1 + bmpDest->format->BytesPerPixel, bmpDest->format, color);
	putter.put(row2, bmpDest->format, color);
	putter.put(row2 + bmpDest->format->BytesPerPixel, bmpDest->format, color);

	UnlockSurface(bmpDest);
}

///////////////////////
// Draws the 2x2 filled rectangle, does clipping
void DrawRectFill2x2(SDL_Surface *bmpDest, int x, int y, Color color)
{
	if(color.a == SDL_ALPHA_TRANSPARENT) return;
	
	if (x < 0 || x + 2 >= bmpDest->clip_rect.x + bmpDest->clip_rect.w)
		return;
	if (y < 0 || y + 2 >= bmpDest->clip_rect.y + bmpDest->clip_rect.h)
		return;

	DrawRectFill2x2_NoClip(bmpDest, x, y, color);
}

//////////////////////
// Draw antialiased line with a putpixel callback
// Code basis taken from CTGraphics by darkoman (http://www.codeproject.com/gdi/CTGraphics.asp)
void AntiAliasedLine(SDL_Surface * dst, int x1, int y1, int x2, int y2, Color color, void (*proc)(SDL_Surface *, int, int, Uint32, Uint8))
{
	// Clipping
	if (!ClipLine(dst, &x1, &y1, &x2, &y2))
		return;

	// Calculate line params
	int dx = (x2 - x1);
	int dy = (y2 - y1);
	int k;
	Uint16 distance;
	Uint8 alpha = color.a;
	color.a = SDL_ALPHA_OPAQUE;
	LOCK_OR_QUIT(dst);

	Uint32 packed_c = color.get(dst->format);

	// Set start pixel
	proc(dst, x1, y1, packed_c, alpha);

	// X-dominant line
	if (abs(dx) > abs(dy))
	{
		// Ex-change line end points
		if (dx < 0)
		{
			int temp;

			temp = x1;
			x1 = x2;
			x2 = temp;

			temp = y1;
			y1 = y2;
			y2 = temp;
		}
		k = (dy * 255) / dx;

		// Set middle pixels
		int xs;
		int yt = y1 * 256 + k;
		distance = 0;
		for (xs = x1 + 1; xs < x2; ++xs)
		{
			proc(dst, xs, yt / 256, packed_c, (Uint8) ((255 - distance) * alpha / 255));
			proc(dst, xs, yt / 256 + 1, packed_c, (Uint8) (distance * alpha / 255));

			yt += k;
			distance = yt & 255; // Same as: yt % 256
		}
	}
	// Y-dominant line
	else
	{
		// Ex-change line end points
		if (dy < 0)
		{
			int temp;

			temp = x1;
			x1 = x2;
			x2 = temp;

			temp = y1;
			y1 = y2;
			y2 = temp;
		}
		k = (dx * 255) / dy;

		// Set middle pixels
		int ys;
		int xt = x1 * 256 + k;
		distance = 0;
		for (ys=y1+1; ys<y2; ++ys)
		{
			proc(dst, xt / 256, ys, packed_c, (Uint8) ((255 - distance) * alpha / 255));
			proc(dst, xt / 256 + 1, ys, packed_c, (Uint8) (distance * alpha / 255));

			xt += k;
			distance = xt & 255;  // Same as: xt % 256
		}
	}

	UnlockSurface(dst);
}

static void DrawRectFill_Overlay(SDL_Surface *bmpDest, const SDL_Rect& r, Color color)
{
	// Clipping
	if (!ClipRefRectWith((SDLRect&)r, (SDLRect&)bmpDest->clip_rect))
		return;

	const int bpp = bmpDest->format->BytesPerPixel;
	Uint8 *px = (Uint8 *)bmpDest->pixels + r.y * bmpDest->pitch + r.x * bpp;
	int step = bmpDest->pitch - r.w * bpp;

	// Draw the fill rect
	PixelPutAlpha& putter = getPixelAlphaPutFunc(bmpDest);
	for (int y = r.h; y; --y, px += step)
		for (int x = r.w; x; --x, px += bpp)  {
			putter.put(px, bmpDest->format, color);
		}

}

//////////////////////
// Draws a filled rectangle
void DrawRectFill(SDL_Surface *bmpDest, int x, int y, int x2, int y2, Color color)
{
	SDL_Rect r = { (SDLRect::Type) x, (SDLRect::Type) y, (SDLRect::TypeS) (x2 - x), (SDLRect::TypeS) (y2 - y) };

	switch (color.a)  {
		case SDL_ALPHA_OPAQUE:
			SDL_FillRect(bmpDest,&r,color.get(bmpDest->format));
			break;
		case SDL_ALPHA_TRANSPARENT:
			break;
		default:
			DrawRectFill_Overlay(bmpDest, r, color);
	}
}


void DrawCircleFilled(SDL_Surface* bmpDest, int x, int y, int rx, int ry, Color color) {
	if(color.a == SDL_ALPHA_TRANSPARENT) return;
	if(rx <= 0 || ry <= 0) return;
	if(rx == 1) { DrawVLine(bmpDest, y - ry, y + ry, x, color); return; }
	if(ry == 1) { DrawHLine(bmpDest, x - rx, x + rx, y, color); return; }
	
	int innerRectW = int(rx / sqrt(2.0));
	int innerRectH = int(ry / sqrt(2.0));
	DrawRectFill(bmpDest, x - innerRectW, y - innerRectH, x + innerRectW + 1, y + innerRectH + 1, color);
	
	float f = float(rx) / float(ry);
	for(int _y = innerRectH + 1; _y < ry; _y++) {
		int w = int(f * sqrt(float(ry*ry - _y*_y))) - 1;
			
		DrawHLine(bmpDest, x - w, x + w, y - _y, color);
		DrawHLine(bmpDest, x - w, x + w, y + _y, color);
	}

	f = 1.0f / f;
	for(int _x = innerRectW + 1; _x < rx; _x++) {
		int h = int(f * sqrt(float(rx*rx - _x*_x))) - 1;
			
		DrawVLine(bmpDest, y - h, y + h, x - _x, color);
		DrawVLine(bmpDest, y - h, y + h, x + _x, color);
	}
}




/////////////////////
// Draws a simple linear gradient
void DrawLinearGradient(SDL_Surface *bmpDest, int x, int y, int w, int h, Color cl1, Color cl2, GradientDirection dir)
{
	if (!ClipRefRectWith(x, y, w, h, (SDLRect&)bmpDest->clip_rect))
		return;

	float rstep, gstep, bstep, astep;

	switch (dir)  {
		case grdVertical:
			rstep = (float)(cl2.r - cl1.r) / (float)h;
			gstep = (float)(cl2.g - cl1.g) / (float)h;
			bstep = (float)(cl2.b - cl1.b) / (float)h;
			astep = (float)(cl2.a - cl1.a) / (float)h;

			for (int gy = 0; gy < h; gy++)  {
				Uint8 r = cl1.r + Round(gy * rstep);
				Uint8 g = cl1.g + Round(gy * gstep);
				Uint8 b = cl1.b + Round(gy * bstep);
				Uint8 a = cl1.a + Round(gy * astep);
				DrawHLine(bmpDest, x, x + w - 1, y + gy, Color(r, g, b, a));
			}
		break;

		case grdHorizontal:
			rstep = (float)(cl2.r - cl1.r) / (float)w;
			gstep = (float)(cl2.g - cl1.g) / (float)w;
			bstep = (float)(cl2.b - cl1.b) / (float)w;
			astep = (float)(cl2.a - cl1.a) / (float)w;

			for (int gx = 0; gx < w; gx++)  {
				Uint8 r = cl1.r + Round(gx * rstep);
				Uint8 g = cl1.g + Round(gx * gstep);
				Uint8 b = cl1.b + Round(gx * bstep);
				Uint8 a = cl1.a + Round(gx * astep);
				DrawVLine(bmpDest, y, y + h - 1, x + gx, Color(r, g, b, a));
			}
		break;
	}
}


///////////////////
// Draws a rope line
void DrawRope(SDL_Surface * bmp, int x1, int y1, int x2, int y2, Color color)
{
	ropealt = 0;
	ropecolour = 0;

	// Clipping
	if (!ClipLine(bmp, &x1, &y1, &x2, &y2))
		return;

	// Because we are drawing 2x stretched, we have to make sure we don't draw the boundary pixels
	x1 = MIN(x1, bmp->w - 3);
	x2 = MIN(x2, bmp->w - 3);
	y1 = MIN(y1, bmp->h - 3);
	y2 = MIN(y2, bmp->h - 3);

	if (tLXOptions->bAntiAliasing)
		AntiAliasedLine(bmp, x1, y1, x2, y2, color, RopePutPixelA);
	else
		perform_line(bmp, x1, y1, x2, y2, color, RopePutPixel);
}


///////////////////
// Draws a beam
void DrawBeam(SDL_Surface * bmp, int x1, int y1, int x2, int y2, Color color)
{
	// Clipping
	if (!ClipLine(bmp, &x1, &y1, &x2, &y2))
		return;

	// Because we are drawing 2x stretched, we have to make sure we don't draw the boundary pixels
	x1 -= x1 & 1;
	y1 -= y1 & 1;
	x2 -= x2 & 1;
	y2 -= y2 & 1;

	if (tLXOptions->bAntiAliasing)
		AntiAliasedLine(bmp, x1, y1, x2, y2, color, BeamPutPixelA);
	else {
		if(color.a != SDL_ALPHA_OPAQUE)
			perform_line(bmp, x1, y1, x2, y2, color, BeamPutPixelA);
		else
			perform_line(bmp, x1, y1, x2, y2, color, BeamPutPixel);
	}
}


///////////////////
// Draws a laser sight
void DrawLaserSight(SDL_Surface * bmp, int x1, int y1, int x2, int y2, Color color)
{
	// Clipping
	if (!ClipLine(bmp, &x1, &y1, &x2, &y2))
		return;

	// Because we are drawing 2x stretched, we have to make sure we don't draw the boundary pixels
	x1 -= x1 & 1;
	y1 -= y1 & 1;
	x2 -= x2 & 1;
	y2 -= y2 & 1;

	perform_line(bmp, x1, y1, x2, y2, color, LaserSightPutPixel);
}



void DrawCross(SDL_Surface* bmpDest, int x, int y, int w, int h, Color c) {
	DrawLine(bmpDest, x, y, x + w, y + h, c);
	DrawLine(bmpDest, x, y + h, x + w, y, c);	
}



void Polygon2D::drawFilled(SDL_Surface* bmpDest, int _x, int _y, Color col) {
	if (lines.size() + horizLines.size() < 3)
		return;

	// Check for clipping
	SDLRect r = (SDLRect)overlay;
	r.x() += _x;
	r.y() += _y;
	if (!r.clipWith((SDLRect&)bmpDest->clip_rect))
		return;

	assert(!addingPoints);

	LOCK_OR_QUIT(bmpDest);

	PixelPutAlpha& putter = getPixelAlphaPutFunc(bmpDest);

	// Run the scanline algorithm
	const int maxy = MIN(overlay.y + _y + overlay.h, bmpDest->clip_rect.y + bmpDest->clip_rect.h - 1);
	std::vector<int> isc;
	isc.reserve(lines.size());
	
	for (int y = MAX(overlay.y + _y + 1, (int)bmpDest->clip_rect.y); y <= maxy; y++)  {
		

		// Get intersections
		for (Lines::const_iterator it = lines.begin(); it != lines.end(); ++it)  {
			assert(it->start.y <= it->end.y);

			// Check for an intersection
			if (it->start.y + _y < y && it->end.y + _y >= y)  {
				const float slope = (float)(it->start.x - it->end.x) / (it->start.y - it->end.y);
				isc.push_back((int)(slope * (y - _y - it->start.y)) + it->start.x + _x); // Calculate the intersection
			}
		}


		// Make sure the intersection count is even and not zero
		assert(isc.size() >= 2 && ((isc.size() & 1) == 0));

		// Sort by X
		std::sort(isc.begin(), isc.end());

		// Draw the scanline using even-odd rule
		for (unsigned i = 0; i < isc.size(); i += 2)  {
			const int maxx = MIN(isc[i + 1], bmpDest->clip_rect.x + bmpDest->clip_rect.w - 1);
			int x = MAX(isc[i], (int)bmpDest->clip_rect.x);
			Uint8 *addr = GetPixelAddr(bmpDest, x, y);
			for (; x <= maxx; x++, addr += bmpDest->format->BytesPerPixel)
				putter.put(addr, bmpDest->format, col);
		}

		isc.clear();
	}

	UnlockSurface(bmpDest);

	// Draw horizontal lines (special cases, cannot be checked for intersections)
	for (Lines::const_iterator it = horizLines.begin(); it != horizLines.end(); ++it)
		DrawHLine(bmpDest, it->start.x + _x, it->end.x + _x, it->start.y + _y, col);
}

void Polygon2D::drawFilled(SDL_Surface* bmpDest, int x, int y, CViewport* v, Color col) {
	SDL_Rect r = minOverlayRect(v); r.x += x*2; r.y += y*2; 
	
	int wx = v->GetWorldX();
	int wy = v->GetWorldY();
	int l = v->GetLeft();
	int t = v->GetTop();
	

#define Tx(x) ((x - wx) * 2 + l)
#define Ty(y) ((y - wy) * 2 + t)

	Polygon2D big;
	big.startPointAdding();
	for (std::list< VectorD2<int> >::iterator p = points.begin(); p != points.end(); p++)
		big.addPoint(VectorD2<int>(Tx(p->x), Ty(p->y)));
	big.endPointAdding();

	big.drawFilled(bmpDest, x * 2, y * 2, col);
	
#undef Tx
#undef Ty
}

#include "InputEvents.h"
void TestPolygonDrawing(SDL_Surface* surf) {
	Color on(255, 0, 0, 128);
	Color off(0, 255, 0, 128); 

	// star
	Polygon2D p;
	p.startPointAdding();
	p.addPoint( VectorD2<int>(100, 0) );
	p.addPoint( VectorD2<int>(110, 90) );
	p.addPoint( VectorD2<int>(200, 100) );
	p.addPoint( VectorD2<int>(110, 110) );
	p.addPoint( VectorD2<int>(100, 200) );
	p.addPoint( VectorD2<int>(90, 110) );
	p.addPoint( VectorD2<int>(0, 100) );
	p.addPoint( VectorD2<int>(90, 90) );
	p.addPoint( VectorD2<int>(100, 0) );
	p.endPointAdding();
	p.drawFilled(surf, 0, 0, p.isInside(GetMouse()->X, GetMouse()->Y) ? on : off);
	
	// triangle
	Polygon2D q;
	q.startPointAdding();
	q.addPoint( VectorD2<int>(310, 10) );
	q.addPoint( VectorD2<int>(400, 20) );
	q.addPoint( VectorD2<int>(390, 100) );
	q.addPoint( VectorD2<int>(310, 10) );
	q.endPointAdding();
	q.drawFilled(surf, 0, 0, q.isInside(GetMouse()->X, GetMouse()->Y) ? on : off);
	
	// rectangle
	Polygon2D r;
	r.startPointAdding();
	r.addPoint( VectorD2<int>(10, 240) );
	r.addPoint( VectorD2<int>(200, 240) );
	r.addPoint( VectorD2<int>(200, 400) );
	r.addPoint( VectorD2<int>(10, 400) );
	r.addPoint( VectorD2<int>(10, 240) );
	r.endPointAdding();
	r.drawFilled(surf, 0, 0, r.isInside(GetMouse()->X, GetMouse()->Y) ? on : off);

	/*Line l1(VectorD2<int>(10, 10), VectorD2<int>(200, 200));
	Line l2(VectorD2<int>(10, 200), VectorD2<int>(200, 10));
	assert(l1.intersects(l2));*/

	// quadrangle (like worms beam)
	Polygon2D s;
	s.startPointAdding();
	s.addPoint( VectorD2<int>(230, 240) );
	s.addPoint( VectorD2<int>(250, 230) );
	s.addPoint( VectorD2<int>(300, 380) );
	s.addPoint( VectorD2<int>(260, 400) );
	s.addPoint( VectorD2<int>(230, 240) );
	s.endPointAdding();
	s.drawFilled(surf, 0, 0, s.isInside(GetMouse()->X, GetMouse()->Y) ? on : off);

	// rotating long triangle
	Polygon2D t;
	const float ta = (GetTime() - AbsTime()).seconds();
	MatrixD2<float> rot = MatrixD2<float>::Rotation(cosf(ta), sinf(ta));
	VectorD2<int> t1(0, -20); t1 = rot * t1;
	VectorD2<int> t2(200, 0); t2 = rot * t2;
	VectorD2<int> t3(0, 20); t3 = rot * t3;
	t.startPointAdding();
	t.addPoint(t1);
	t.addPoint(t2);
	t.addPoint(t3);
	t.addPoint(t1);
	t.endPointAdding();
	t.drawFilled(surf, 70, 400, p.isInside(GetMouse()->X, GetMouse()->Y) ? on : Color(0, 0, 255, 160));
	
	// a crown
	Polygon2D u;
	u.startPointAdding();
	{
		const int x = 400, y = 100, w = 200, h = 300;
		const int C = 10;
		u.addPoint( VectorD2<int>(x,y) );
		u.addPoint( VectorD2<int>(x+w,y) );
		for(int i = C - 1; i >= 0; --i) {
			const int _x = x + w * i / C;
			u.addPoint( VectorD2<int>(_x + w/(C*2),y+h) );
			u.addPoint( VectorD2<int>(_x,y + h/4) );
		}
	}
	u.endPointAdding();
	u.drawFilled(surf, 0,0, u.isInside(GetMouse()->X, GetMouse()->Y) ? on : off);
}






void DumpPixelFormat(const SDL_PixelFormat* format, const std::string& prefix) {
	std::ostringstream str;
	str
	<< prefix
	<< "PixelFormat:" << std::endl
	<< prefix
	<< "  BitsPerPixel: " << (int)format->BitsPerPixel << ","
	<< "  BytesPerPixel: " << (int)format->BytesPerPixel << std::endl
	<< prefix
	<< "  R/G/B/A mask: " << std::hex
	<< (uint)format->Rmask << "/"
	<< (uint)format->Gmask << "/"
	<< (uint)format->Bmask << "/"
	<< (uint)format->Amask << std::endl
	<< prefix
	<< "  R/G/B/A loss: "
	<< (uint)format->Rloss << "/"
	<< (uint)format->Gloss << "/"
	<< (uint)format->Bloss << "/"
	<< (uint)format->Aloss << std::endl;
	notes << str.str() << endl;
}

void DumpSurfaceInfo(const SDL_Surface* s, const char* name, bool dumpAFewPixels) {
	notes << "Surface";
	if(name) notes << " '" << name << "'";
	notes << " (" << s << ")" << endl;
	if(!s) return;

	notes << "  Size (WxH): " << s->w << " x " << s->h << endl;
	notes << "  Pitch: " << s->pitch << endl;
	
	DumpPixelFormat(s->format, "  ");

	std::ostringstream str;
	str << "  ColorKey: ";
	if(Surface_HasColorKey(s))
		str << " yes, " << std::hex << Surface_GetColorKey(s);
	else
		str << " no";
	str << std::endl;
	
	{
		str << "  AlphaBlendMode: ";
		SDL_BlendMode m;
		int r = SDL_GetSurfaceBlendMode((SDL_Surface*)s, &m);
		if(r != 0)
			str << "ERROR: " << SDL_GetError();
		else
			switch(m) {
				case SDL_BLENDMODE_NONE: str << "none"; break;
				case SDL_BLENDMODE_ADD: str << "add"; break;
				case SDL_BLENDMODE_BLEND: str << "blend"; break;
				case SDL_BLENDMODE_MOD: str << "mod"; break;
				default: str << "invalid"; break;
			}
		str << std::endl;
	}
	
	{
		str << "  AlphaMod: ";
		Uint8 a = SDL_ALPHA_OPAQUE;
		int r = SDL_GetSurfaceAlphaMod((SDL_Surface*)s, &a);
		if(r != 0)
			str << "ERROR: " << SDL_GetError();
		else
			str << std::dec << a;
		str << std::endl;
	}
	
	if(dumpAFewPixels) {
		std::string pfmt = " %." + itoa(s->format->BytesPerPixel*2) + "X";
		static constexpr int MaxY = 2, MaxX = 10;
		for(int y = 0; y < MIN(MaxY, s->h); ++y) {
			for(int x = 0; x < MIN(MaxX, s->w); ++x) {
				char buffer[10];
				snprintf(buffer, sizeof(buffer), pfmt.c_str(), GetPixel(s, x, y));
				buffer[sizeof(buffer)-1] = 0;
				notes << buffer;
			}
			if(MaxX < s->w) notes << " ...";
			notes << endl;
		}
		if(MaxY < s->h) notes << "..." << endl;
	}
}

void DumpSurface(const SDL_Surface* s) {
	DumpPixelFormat(s->format);
	std::string pfmt = " %." + itoa(s->format->BytesPerPixel*2) + "X";
	for(int y = 0; y < MIN(10, s->h); ++y) {
		for(int x = 0; x < MIN(10, s->w); ++x)
			printf(pfmt.c_str(), GetPixel(s, x, y));
		printf("\n");
	}
}

bool PixelFormatEqual(const SDL_PixelFormat* fm1, const SDL_PixelFormat* fm2) {
	return
	fm1->BitsPerPixel == fm2->BitsPerPixel &&
	fm1->Rmask == fm2->Rmask &&
	fm1->Gmask == fm2->Gmask &&
	fm1->Bmask == fm2->Bmask &&
	fm1->Amask == fm2->Amask;
}

bool IsCorrectSurfaceFormat(const SDL_PixelFormat* format) {
	// gfxCreateSurface() uses exactly the same format as getMainPixelFormat()
	return PixelFormatEqual(getMainPixelFormat(), format);
}


#if USE_GD_FOR_IMAGE_LOADING
///////////////////////
// Converts the gdImagePtr to SDL_surface
SmartPointer<SDL_Surface> GDImage2SDLSurface(gdImagePtr src, bool withalpha, bool keep8bit) {
	SmartPointer<SDL_Surface> dst;
	if(withalpha)
		dst = gfxCreateSurfaceAlpha(gdImageSX(src), gdImageSY(src));
	else if(!keep8bit || gdImageTrueColor(src))
		dst = gfxCreateSurface(gdImageSX(src), gdImageSY(src));
	else // keep8bit && gd img is 8bit
		dst = SDL_CreateRGBSurface(SDL_SWSURFACE, gdImageSX(src), gdImageSY(src), 8, 0,0,0,0);	
	if(!dst.get()) return NULL;
	
	if (!LockSurface(dst)) return NULL;
	
	if(dst->format->BytesPerPixel > 1)
		for(int y = 0; y < dst->h; y++) 
			for(int x = 0; x < dst->w; ++x) {
				Uint32 px = gdImageGetTrueColorPixel(src, x, y);
				Color c;
				c.r = px >> 16;
				c.g = px >> 8;
				c.b = px;
				c.a = 0x7f - (px >> 24);
				c.a *= 2;
				if(c.a > 0x7f && c.a <= 0xfe) c.a++;
				PutPixel(dst.get(), x, y, Pack(c, dst->format));
			}
	else
		for(int y = 0; y < dst->h; y++) 
			for(int x = 0; x < dst->w; ++x) {
				Uint8 px = gdImageGetPixel(src, x, y);
				PutPixel(dst.get(), x, y, px);
			}

	UnlockSurface(dst);
	
	return dst;
}

SmartPointer<SDL_Surface> LoadGameImage_viaGd(const std::string& _filename, bool withalpha, bool keep8bit) {
	/* WARNING: Don't use gd-functions which take a FILE* argument on Windows!
	 * The DLL doesn't work anymore with recent MSVC versions.
	 * See here: http://stackoverflow.com/questions/9945648/crash-when-calling-gd-function
	 * Or: http://www.gamedev.net/topic/479025-libgd-problem/
	*/

	std::string fileContent = GetFileContents(_filename);
	if(fileContent.empty()) return NULL;

	std::string ext = GetFileExtensionWithDot(_filename);
	stringlwr(ext);
	gdImagePtr src = NULL;

	if(ext == ".bmp") {
		SDL_Surface* s = SDL_LoadBMP_RW(SDL_RWFromConstMem(&fileContent[0], (int)fileContent.size()), true);
		if(s == NULL) {
			errors << "LoadGameImage: cannot load bmp: " << _filename << endl;
			return NULL;
		}
		return s;
	}
	else if(ext == ".jpg" || ext == ".jpeg")
		src = gdImageCreateFromJpegPtr((int)fileContent.size(), &fileContent[0]);
	else if(ext == ".png")
		src = gdImageCreateFromPngPtr((int)fileContent.size(), &fileContent[0]);
	else if(ext == ".gif")
		src = gdImageCreateFromGifPtr((int)fileContent.size(), &fileContent[0]);
	else {
		errors << "LoadGameImage: file extension unknown: " << _filename << endl;
		return NULL;
	}
	
	if(src == NULL) {
		errors << "LoadGameImage: cannot load: " << _filename << endl;
		return NULL;
	}	
	SmartPointer<SDL_Surface> Image = GDImage2SDLSurface(src, withalpha, keep8bit);
	gdImageDestroy(src);
	
	if(Image.get() == NULL)
		errors << "LoadGameImage: error in GDImage2SDLSurface for: " << _filename << endl;
	return Image;	
}
#endif

SmartPointer<SDL_Surface> LoadGameImage_viaSdlImage(const std::string& _filename, bool withalpha, bool keep8bit) {
	// Load the image
	std::string fullfname = GetFullFileName(_filename);
	if(fullfname.size() == 0)
		return NULL;
	
	return IMG_Load(Utf8ToSystemNative(fullfname).c_str());	
}


////////////////////////
//
//  Image loading/saving routines
//
////////////////////////

SmartPointer<SDL_Surface> LoadGameImage_unaltered(const std::string& _filename, bool withalpha, bool keep8bit) {
#if USE_GD_FOR_IMAGE_LOADING
	return LoadGameImage_viaGd(_filename, withalpha, keep8bit);
#else
	return LoadGameImage_viaSdlImage(_filename, withalpha, keep8bit);
#endif
}

///////////////////
// Loads an image, and converts it to the same colour depth as the screen (speed)
SmartPointer<SDL_Surface> LoadGameImage(const std::string& _filename, bool withalpha)
{
	ScopedLock lock(cCache.mutex);
	
	{
		// Try cache first
		SmartPointer<SDL_Surface> ImageCache = cCache.GetImage__unsafe(_filename);
		if( ImageCache.get() )
			return ImageCache;
	}
	
#if USE_GD_FOR_IMAGE_LOADING
	SmartPointer<SDL_Surface> img = LoadGameImage_viaGd(_filename, withalpha, false);	
#else
	SmartPointer<SDL_Surface> img = LoadGameImage_viaSdlImage(_filename, withalpha, false);
#endif
	if(!img.get()) return NULL;
	
	if(!withalpha)
		// Force no blend mode.
		SDL_SetSurfaceBlendMode(img.get(), SDL_BLENDMODE_NONE);
	
	const VideoFormat wantedFormat = getMainVideoFormat(withalpha);
	if(wantedFormat != VideoFormat(img->format)) {
		SmartPointer<SDL_Surface> converted;
		if(bDedicated || !VideoPostProcessor::videoSurface().get()) {
			if(!bDedicated)
				// we haven't initialized the screen yet
				warnings << "LoadGameImage: screen not initialized yet while loading image" << endl;	
			
			if (withalpha)
				converted = gfxCreateSurfaceAlpha(img.get()->w, img.get()->h);
			else
				converted = gfxCreateSurface(img.get()->w, img.get()->h);			
			CopySurface(converted.get(), img, 0, 0, 0, 0, img.get()->w, img.get()->h);
		}
		else {
			// Convert the image to the screen's colour depth
			if (withalpha)  {
				converted = gfxCreateSurfaceAlpha(img.get()->w, img.get()->h);
				CopySurface(converted.get(), img, 0, 0, 0, 0, img.get()->w, img.get()->h);
			} else {
				SDL_PixelFormat fmt = *(getMainPixelFormat());
				SDL_SetSurfaceBlendMode(img.get(), SDL_BLENDMODE_NONE); // Remove the alpha flag here, ConvertSurface will remove the alpha completely later
				SDL_SetColorKey(img.get(), 0, 0); // Remove the colorkey here, we don't want it (normally it shouldn't be activated here, so only for safty)
				converted = SDL_ConvertSurface(img.get(), &fmt, 0);
				SDL_SetSurfaceBlendMode(converted.get(), SDL_BLENDMODE_NONE); // we explicitly said that we don't want alpha, so remove it
			}
		}
		
		if(!converted.get()) {
			errors << "LoadGameImage: cannot create new surface" << endl;
			return NULL;
		}
		img = converted;
	}
	
	// Save to cache
	#ifdef DEBUG
	//printf("LoadImage() %p %s\n", Image.get(), _filename.c_str() );
	#endif
	cCache.SaveImage__unsafe(_filename, img);
	return img;
}

void test_Clipper() {
	SDL_Rect r1 = {52, 120, 7, 14};
	SDL_Rect r2 = {52, 162, 558, 258};

	ClipRefRectWith(r1, (SDLRect&)r2);

	notes << r1.x << "," << r1.y << "," << r1.w << "," << r1.h << endl;
}

template <> void SmartPointer_ObjectDeinit<SDL_Surface> ( SDL_Surface * obj )
{
	#ifdef DEBUG
	//printf("SmartPointer_ObjectDeinit<SDL_Surface>() %p\n", obj);
	#endif

	SDL_FreeSurface(obj);
}

template <> void SmartPointer_ObjectDeinit<SDL_Texture> ( SDL_Texture * obj ) {
	SDL_DestroyTexture(obj);
}

template <> void SmartPointer_ObjectDeinit<SDL_Renderer> ( SDL_Renderer * obj ) {
	SDL_DestroyRenderer(obj);
}

template <> void SmartPointer_ObjectDeinit<SDL_Window> ( SDL_Window * obj ) {
	SDL_DestroyWindow(obj);
}





void TestCircleDrawing(SDL_Surface* s) {
	for(int i = 0; i < 10; i += 3) {
		DrawCircleFilled(s, i*50, i*25, i*25, i*25, Color(i*20,255,0,128));
	}
}

