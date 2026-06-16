#include "gfx.h"
#include "gconsole.h"

#ifndef DEDICATED_ONLY
#include "blitters/blitters.h"
#include "blitters/colors.h"
#include "blitters/macros.h"
#include "sprite_set.h"
#include "sprite.h"
#endif
#include <boost/bind/bind.hpp>
#include <boost/assign/list_inserter.hpp>
using namespace boost::assign;

#include "gusanos/allegro.h"
#include "GfxPrimitives.h"
#include "AuxLib.h"

#include <string>
#include <algorithm>
#include <iostream>
#include <list>
#include <stdexcept>

using namespace std;

Gfx gfx;

namespace
{
#ifndef DEDICATED_ONLY
	static string screenShot(const list<string> &)
	{
		PushScreenshot("gus_scrshots", "");
		return "OK";
	}	
#endif
}


void Gfx::registerInConsole()
{
#ifndef DEDICATED_ONLY
	console.registerCommands()
		("SCREENSHOT", screenShot)
	;
	
	console.registerVariables()
		("VID_DISTORTION_AA", &m_distortionAA, 1)
	;
#endif
}

ALLEGRO_BITMAP* Gfx::loadBitmap( const string& filename, bool keepAlpha , bool stretch2)
{
	ALLEGRO_BITMAP* returnValue = NULL;
	/*
	int flags = COLORCONV_DITHER | COLORCONV_KEEP_TRANS;
	
	if(keepAlpha)
		flags |= COLORCONV_KEEP_ALPHA;
	else
		flags |= COLORCONV_TOTAL;
*/
	int flags = 0;
	
	LocalSetColorConversion cc(flags);
	
	if ( gusExists( filename ) )
	{
		returnValue = load_bitmap(filename, stretch2);
	}
	else
	{
		string tmp = filename;
		tmp += ".png";
		if ( gusExists( tmp ) )
		{
			returnValue = load_bitmap(tmp, stretch2);
		}
		else
		{
			tmp = filename;
			tmp += ".bmp";
			if ( gusExists( tmp ))
			{
				returnValue = load_bitmap(tmp, stretch2);
			}
		}
	}

#ifndef DEDICATED_ONLY
	if(returnValue && !keepAlpha && bitmap_color_depth(returnValue) == 32 && get_color_depth() == 32)
	{
		typedef Pixel32 pixel_t_1;
		APPLY_ON_BITMAP(returnValue,
			RECT_Y_LOOP(
				RECT_X_LOOP(
					//*p &= 0xFFFFFF;
					*p |= makecol(0,0,0);
				)
			)
		);
	}
#endif
	return returnValue;
}

SmartPointer<SDL_Surface> Gfx::loadBitmapSDL(const std::string& _filename, bool keepAlpha, bool stretch2) {
	std::string filename;
	if(IsFileAvailable(_filename)) filename = _filename;
	else if(IsFileAvailable(_filename + ".png")) filename = _filename + ".png";
	else if(IsFileAvailable(_filename + ".bmp")) filename = _filename + ".bmp";
	else return NULL;
	
	SmartPointer<SDL_Surface> img = load_bitmap__allegroformat(filename, stretch2);
	if(!img.get()) return NULL;
	
	if(!keepAlpha)
		// TODO: That is not exactly the same as above.
		// It probably does nothing if Amask = 0.
		ResetAlpha(img.get());

	return img;	
}


