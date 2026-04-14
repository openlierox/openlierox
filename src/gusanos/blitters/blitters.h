#ifndef OMFG_BLITTERS_BLITTERS_H
#define OMFG_BLITTERS_BLITTERS_H

#ifdef DEDICATED_ONLY
#error "Can't use this in dedicated server"
#endif //DEDICATED_ONLY

#include <SDL.h>
#include "gusanos/allegro.h"
#include "gusanos/blitters/types.h"
#include "mmx.h"
#include "CodeAttributes.h"

// TODO: correct check if we should include MMX/SSE code
#if !defined(WIN32) && (SDL_BYTEORDER == SDL_LIL_ENDIAN) && (defined(__i386__) || defined(__x86_64__))
#define HAS_MMX (cpu_capabilities & CPU_MMX)
#define HAS_SSE (cpu_capabilities & CPU_SSE)
#define HAS_MMXSSE (cpu_capabilities & CPU_MMXPLUS)
#define BUILTIN_MMXSSE
#else  // TODO: currently buggy on Windows and unavailable on non-x86 archs
#define HAS_MMX (false)
#define HAS_SSE (false)
#define HAS_MMXSSE (false)

#if defined(WIN32)
// built in anyway, it is still needed for linking
#define BUILTIN_MMXSSE
#endif

#endif

#define FOR_MMX(x_) if(HAS_MMX) { x_ }

namespace Blitters
{

	
/*
	Naming:
	
	function [ - filter] - bitdepth [ - parallelism] [ - variant]
	
	e.g.:
	rectfill_blend_32_mmx
	
	defaults:
		parallelism = 1
		variant = C
*/

INLINE Pixel getpixel_32(ALLEGRO_BITMAP* where, int x, int y)
{
	return ((Pixel32 *)where->line[y])[x];
}

INLINE void putpixel_solid_32(ALLEGRO_BITMAP* where, int x, int y, Pixel color1)
{
	((Pixel32 *)where->line[y])[x] = color1;
}

void putpixel_add_32(ALLEGRO_BITMAP* where, int x, int y, Pixel color1);
void putpixel_addFact_32(ALLEGRO_BITMAP* where, int x, int y, Pixel color1, int fact);
void putpixelwu_add_32(ALLEGRO_BITMAP* where, float x, float y, Pixel color1, int fact);
void putpixel_blendHalf_32(ALLEGRO_BITMAP* where, int x, int y, Pixel color1);
void putpixel_blend_32(ALLEGRO_BITMAP* where, int x, int y, Pixel color1, int fact);
void putpixelwu_blend_32(ALLEGRO_BITMAP* where, float x, float y, Pixel color1, int fact);

INLINE void putpixel_solid_16(ALLEGRO_BITMAP* where, int x, int y, Pixel color1)
{
	((Pixel16 *)where->line[y])[x] = (Pixel16)color1;
}

void putpixel_add_16(ALLEGRO_BITMAP* where, int x, int y, Pixel color1);
void putpixel_addFact_16(ALLEGRO_BITMAP* where, int x, int y, Pixel color1, int fact);
void putpixelwu_add_16(ALLEGRO_BITMAP* where, float x, float y, Pixel color1, int fact);
void putpixel_blendHalf_16(ALLEGRO_BITMAP* where, int x, int y, Pixel color1);
void putpixel_blend_16(ALLEGRO_BITMAP* where, int x, int y, Pixel color1, int fact);
void putpixelwu_blend_16(ALLEGRO_BITMAP* where, float x, float y, Pixel color1, int fact);

INLINE void putpixel_solid_8(ALLEGRO_BITMAP* where, int x, int y, Pixel color1)
{
	((Pixel8 *)where->line[y])[x] = (Pixel8)color1;
}

void rectfill_add_16(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour, int fact);
void rectfill_blend_16(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour, int fact);

void rectfill_add_32(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour, int fact);
void rectfill_add_32_mmx(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour, int fact);
void rectfill_blend_32(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour, int fact);

void hline_add_16(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour, int fact);
void hline_add_32(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour, int fact);
void hline_add_32_mmx(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour, int fact);
void hline_blend_16(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour, int fact);
void hline_blend_32(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour, int fact);

bool linewu_blend(ALLEGRO_BITMAP* where, float x, float y, float destx, float desty, Pixel colour, int fact);
bool linewu_add(ALLEGRO_BITMAP* where, float x, float y, float destx, float desty, Pixel colour, int fact);

void line_blend(ALLEGRO_BITMAP* where, int x, int y, int destx, int desty, Pixel colour, int fact);
void line_add(ALLEGRO_BITMAP* where, int x, int y, int destx, int desty, Pixel colour, int fact);

void drawSprite_add_16(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_add_16_mmx_sse(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blend_16(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blend_16_mmx_sse(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blendalpha_32_to_16(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blendtint_8_to_16(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact, int color);
void drawSprite_multsec_32_with_8(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, ALLEGRO_BITMAP* secondary, int x, int y, int sx, int sy, int cutl, int cutt, int cutr, int cutb);
void drawSprite_mult_8_to_16(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb);

void drawSprite_add_32(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_add_32_mmx_sse(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blend_32(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blend_32_mmx_sse(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blendalpha_32_to_32(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blendalpha_32_to_32_mmx_sse(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact);
void drawSprite_blendtint_8_to_32(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact, int color);
void drawSprite_mult_8_to_32(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb);
void drawSprite_mult_8_to_32_mmx_sse(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb);

void drawSpriteLine_add_32(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int x1, int y1, int x2, int fact);
void drawSpriteLine_add_16(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int x1, int y1, int x2, int fact);
void drawSpriteLine_add_8(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int x1, int y1, int x2, int fact);
void drawSpriteLine_add_8_mmx_sse(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int x1, int y1, int x2, int fact);
void drawSpriteRotate_solid_32(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, double angle);
} // namespace Blitters

using Blitters::linewu_blend;
using Blitters::linewu_add;
using Blitters::line_blend;
using Blitters::line_add;


// Automatic color depth and variant selecting functions
#define SELECT(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: Blitters::f_##_16 x_ ; break; \
		case 32: Blitters::f_##_32 x_ ; break; }
		
#define SELECT_ALL(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: Blitters::f_##_16 x_ ; break; \
		case 32: Blitters::f_##_32 x_ ; break; \
		case 8: Blitters::f_##_8 x_ ; break; }
		
#define SELECT_ALL_SSE8(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: Blitters::f_##_16 x_ ; break; \
		case 32: Blitters::f_##_32 x_ ; break; \
		case 8: \
			if(HAS_SSE) Blitters::f_##_8_sse x_ ; \
			else Blitters::f_##_8 x_ ; \
		break; }
		
#define SELECT_ALL_MMX_SSE8(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: Blitters::f_##_16 x_ ; break; \
		case 32: Blitters::f_##_32 x_ ; break; \
		case 8: \
			if(HAS_MMXSSE || HAS_SSE) Blitters::f_##_8_mmx_sse x_ ; \
			else Blitters::f_##_8 x_ ; \
		break; }
		
#define SELECT_MMX32(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: Blitters::f_##_16 x_ ; break; \
		case 32: \
			if(HAS_MMX) Blitters::f_##_32_mmx x_ ; \
			else Blitters::f_##_32 x_ ; \
		break; }
			
#define SELECT_SSE32(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: Blitters::f_##_16 x_ ; break; \
		case 32: \
			if(HAS_SSE) Blitters::f_##_32_sse x_ ; \
			else Blitters::f_##_32 x_ ; \
		break; }
		
#define SELECT_MMX_SSE_32(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: Blitters::f_##_16 x_ ; break; \
		case 32: \
			if(HAS_MMXSSE || HAS_SSE) Blitters::f_##_32_mmx_sse x_ ; \
			else Blitters::f_##_32 x_ ; \
		break; }
		
#define SELECT_MMX_SSE(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: \
			if(HAS_MMXSSE || HAS_SSE) Blitters::f_##_16_mmx_sse x_ ; \
			else Blitters::f_##_16 x_ ; break; \
		case 32: \
			if(HAS_MMXSSE || HAS_SSE) Blitters::f_##_32_mmx_sse x_ ; \
			else Blitters::f_##_32 x_ ; \
		break; }
		
#define SELECT_SSE(f_, x_) \
	switch(bitmap_color_depth(where)) { \
		case 16: \
			if(HAS_SSE) Blitters::f_##_16_sse x_ ; \
			else Blitters::f_##_16 x_ ; \
		break; \
		case 32: \
			if(HAS_SSE) Blitters::f_##_32_sse x_ ; \
			else Blitters::f_##_32 x_ ; \
		break; }
		
#define SELECT2(f_, a_, b_) \
	switch(bitmap_color_depth(where)) { \
		case 16: Blitters::f_##_16 b_ ; break; \
		case 32: Blitters::f_##_32 a_ ; break; }

#define CHECK_RANGE() \
	if((unsigned int)x >= (unsigned int)where->w \
	|| (unsigned int)y >= (unsigned int)where->h) \
		return
		
INLINE void putpixel_add(ALLEGRO_BITMAP* where, int x, int y, Pixel color1, int fact)
{
	CHECK_RANGE();
	SELECT2(putpixel_addFact, (where, x, y, color1, fact), (where, x, y, color1, (fact + 4) / 8));
}

INLINE void putpixel_addFull(ALLEGRO_BITMAP* where, int x, int y, Pixel color1)
{
	CHECK_RANGE();
	SELECT(putpixel_add, (where, x, y, color1));
}

INLINE void putpixelwu_add(ALLEGRO_BITMAP* where, float x, float y, Pixel color1, int fact)
{
	//blendwu blender checks range
	SELECT2(putpixelwu_add, (where, x, y, color1, fact), (where, x, y, color1, (fact + 4) / 8));
}

INLINE void putpixel_blendHalf(ALLEGRO_BITMAP* where, int x, int y, Pixel color1)
{
	CHECK_RANGE();
	SELECT(putpixel_blendHalf, (where, x, y, color1));
}

INLINE void putpixel_blend(ALLEGRO_BITMAP* where, int x, int y, Pixel color1, int fact)
{
	CHECK_RANGE();
	SELECT2(putpixel_blend, (where, x, y, color1, fact), (where, x, y, color1, (fact + 4) / 8));
}

INLINE void putpixel_blendalpha(ALLEGRO_BITMAP* where, int x, int y, Pixel color1, int fact)
{
	CHECK_RANGE();
	SELECT2(putpixel_blend, (where, x, y, color1, fact), (where, x, y, color1, (fact + 4) / 8));
}

INLINE void putpixelwu_blend(ALLEGRO_BITMAP* where, float x, float y, Pixel color1, int fact)
{
	//blendwu blender checks range
	SELECT2(putpixelwu_blend, (where, x, y, color1, fact), (where, x, y, color1, (fact + 4) / 8));
}

INLINE void putpixelwu_blendalpha(ALLEGRO_BITMAP* where, float x, float y, Pixel color1, int fact)
{
	//blendwu blender checks range
	SELECT2(putpixelwu_blend, (where, x, y, color1, fact), (where, x, y, color1, (fact + 4) / 8));
}

INLINE void putpixel_solid(ALLEGRO_BITMAP* where, int x, int y, Pixel color1)
{
	CHECK_RANGE();
	SELECT_ALL(putpixel_solid, (where, x, y, color1));
}

INLINE void putpixel_solid2x2(ALLEGRO_BITMAP* where, int x, int y, Pixel color1) {
	putpixel_solid(where, x, y, color1);
	putpixel_solid(where, x+1, y, color1);
	putpixel_solid(where, x, y+1, color1);
	putpixel_solid(where, x+1, y+1, color1);
}

INLINE void putpixelwu_solid(ALLEGRO_BITMAP* where, float x, float y, Pixel color1)
{
	//blendwu blender checks range
	SELECT2(putpixelwu_blend, (where, x, y, color1, 256), (where, x, y, color1, 32));
}

INLINE void rectfill_add(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour, int fact)
{
	SELECT_MMX32(rectfill_add, (where, x1, y1, x2, y2, colour, fact));
}

INLINE void rectfill_blend(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour, int fact)
{
	SELECT(rectfill_blend, (where, x1, y1, x2, y2, colour, fact));
}

INLINE void rectfill_blendalpha(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour, int fact)
{
	SELECT(rectfill_blend, (where, x1, y1, x2, y2, colour, fact));
}

INLINE void rectfill_solid(ALLEGRO_BITMAP* where, int x1, int y1, int x2, int y2, Pixel colour)
{
	rectfill(where, x1, y1, x2, y2, colour); //TODO: Make own
}

INLINE void hline_add(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour, int fact)
{
	SELECT_MMX32(hline_add, (where, x1, y1, x2, colour, fact));
}

INLINE void hline_blend(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour, int fact)
{
	SELECT(hline_blend, (where, x1, y1, x2, colour, fact));
}

INLINE void hline_blendalpha(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour, int fact)
{
	SELECT(hline_blend, (where, x1, y1, x2, colour, fact));
}

INLINE void hline_solid(ALLEGRO_BITMAP* where, int x1, int y1, int x2, Pixel colour)
{
	hline(where, x1, y1, x2, colour);
}

INLINE void linewu_blendalpha(ALLEGRO_BITMAP* where, float x, float y, float destx, float desty, Pixel colour, int fact)
{
	Blitters::linewu_blend(where, x, y, destx, desty, colour, fact);
}

INLINE void linewu_solid(ALLEGRO_BITMAP* where, float x, float y, float destx, float desty, Pixel colour)
{
	Blitters::linewu_blend(where, x, y, destx, desty, colour, 256);
}

INLINE void line_solid(ALLEGRO_BITMAP* where, int x, int y, int destx, int desty, Pixel colour)
{
	line(where, x, y, destx, desty, colour);
}

INLINE void line_blendalpha(ALLEGRO_BITMAP* where, int x, int y, int destx, int desty, Pixel colour, int fact)
{
	Blitters::line_blend(where, x, y, destx, desty, colour, fact);
}

INLINE void drawSprite_add(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int fact)
{
	SELECT_MMX_SSE(drawSprite_add, (where, from, x, y, 0, 0, 0, 0, fact));
}

INLINE void drawSprite_blend(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int fact)
{
	SELECT_MMX_SSE(drawSprite_blend, (where, from, x, y, 0, 0, 0, 0, fact));
}

INLINE void drawSprite_blendalpha(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int fact)
{
	SELECT_MMX_SSE_32(drawSprite_blendalpha_32_to, (where, from, x, y, 0, 0, 0, 0, fact));
}

INLINE void drawSprite_blendtint(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int fact, int color)
{
	SELECT(drawSprite_blendtint_8_to, (where, from, x, y, 0, 0, 0, 0, fact, color));
}

INLINE void drawSprite_solid(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y)
{
	draw_sprite(where, from, x, y);
}

INLINE void drawSprite_mult_8(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y)
{
	SELECT_MMX_SSE_32(drawSprite_mult_8_to, (where, from, x, y, 0, 0, 0, 0));
}

INLINE void drawSpriteCut_add(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact)
{
	SELECT_MMX_SSE(drawSprite_add, (where, from, x, y, cutl, cutt, cutr, cutb, fact));
}

INLINE void drawSpriteCut_blend(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact)
{
	SELECT_MMX_SSE(drawSprite_blend, (where, from, x, y, cutl, cutt, cutr, cutb, fact));
}

INLINE void drawSpriteCut_blendalpha(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb, int fact)
{
	SELECT_MMX_SSE_32(drawSprite_blendalpha_32_to, (where, from, x, y, cutl, cutt, cutr, cutb, fact));
}

INLINE void drawSpriteCut_solid(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int cutl, int cutt, int cutr, int cutb)
{
	masked_blit(from, where, cutl, cutt, x+cutl, y+cutt
		, from->w - (cutl + cutr)
		, from->h - (cutt + cutb));
}

INLINE void drawSpriteLine_add(ALLEGRO_BITMAP* where, ALLEGRO_BITMAP* from, int x, int y, int x1, int y1, int x2, int fact)
{
	SELECT_ALL_MMX_SSE8(drawSpriteLine_add, (where, from, x, y, x1, y1, x2, fact));
}

#undef SELECT

#endif //OMFG_BLITTERS_BLITTERS_H
