#include "level_effect.h"

#include "resource_list.h"

//#include "gfx.h"
//#include "util/text.h"
#include "sprite_set.h"
#include "sprite.h"
#include "omfgscript/omfg_script.h"
#include "game_actions.h"
#include "FindFile.h"

#include "gusanos/allegro.h"
#include <string>
//#include <vector>
#include <fstream>
#include <iostream>

using namespace std;

ResourceList<LevelEffect> levelEffectList;

LevelEffect::LevelEffect()
: ResourceBase()
, mask(NULL)
{
	
}

LevelEffect::~LevelEffect()
{
}

bool LevelEffect::load(std::string const& filename)
{
	std::ifstream fileStream;
	OpenGameFileR(fileStream, filename, std::ios::binary | std::ios::in);

	if (!fileStream )
		return false;
	
	OmfgScript::Parser parser(fileStream, gameActions, filename);
	
	if(!parser.run())
	{
		if(parser.incomplete())
			parser.error("Trailing garbage");
		return false;
	}

	{
		if(OmfgScript::TokenBase* v = parser.getRawProperty("mask"))
			mask = spriteList.load(v->toString());
		//if(!v->isDefault())
	}

	if(mask)
	{
		uint32_t bits = 0;
		int c = 8;
		ALLEGRO_BITMAP* b = mask->getSprite()->m_bitmap;
		for( int y = 0; y < b->h; ++y )
		for( int x = 0; x < b->w; ++x )
		{
			unsigned int colour = getpixel( b, x, y);
			if( colour == 0 )
				bits = (bits << 1) + 1;
			else
				bits = (bits << 1);
			if(--c == 0)
			{
				parser.crcProcessByte(bits);
				c = 8;
				bits = 0;
			}
		}
		
		parser.crcProcessByte(bits);
	}
	else
	{
		parser.crcProcessByte(1);
	}
	
	crc = parser.getCRC();

	return true;
}


