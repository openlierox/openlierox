#include "exp_type.h"

#include "resource_list.h"

#include "events.h"
#ifndef DEDICATED_ONLY
#include "sprite_set.h"
#include "distortion.h"
#include "gfx.h"
#include "sprite.h"
#endif //DEDICATED_ONLY
#include "util/text.h"
#include "util/macros.h"
#include "detect_event.h"
#include "object_grid.h"

#include "omfgscript/omfg_script.h"
#include "game_actions.h"
#include "FindFile.h"


#include "gusanos/allegro.h"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>

using namespace std;

ResourceList<ExpType> expTypeList;

ExpType::ExpType()
: wupixels(0), invisible(false)
{
	timeout = 0;
	timeoutVariation = 0;

	colour = -1;
	alpha = 255;
	destAlpha = -1;	
	renderLayer = Grid::WormRenderLayer;
#ifndef DEDICATED_ONLY
	sprite = NULL;
	lightHax = NULL;
	distortion = NULL;
	distortMagnitude = 0.8f;
	
	rockHidden = true;
	blender = BlitterContext::TNone;
#endif
	
	creation = NULL;
}

ExpType::~ExpType()
{
	delete creation;
#ifndef DEDICATED_ONLY
	delete distortion;
	delete lightHax;
#endif
	for ( vector<DetectEvent*>::iterator i = detectRanges.begin(); i != detectRanges.end(); i++)
	{
		delete *i;
	}
}

namespace GameEventID
{
enum type
{
	Creation,
	DetectRange,
};
}

bool ExpType::load(std::string const& filename)
{
	std::ifstream fileStream;
	OpenGameFileR(fileStream, filename, std::ios::binary | std::ios::in);

	if (!fileStream )
		return false;
	
	OmfgScript::Parser parser(fileStream, gameActions, filename);
	
	parser.addEvent("creation", GameEventID::Creation, OmfgScript::ActionParamFlags::Object);
	
	parser.addEvent("detect_range", GameEventID::DetectRange, OmfgScript::ActionParamFlags::Object | OmfgScript::ActionParamFlags::Object2)
		("range")
		("detect_owner")
		("layers")
	;
		
	if(!parser.run())
	{
		if(parser.incomplete())
			parser.error("Trailing garbage");
		return false;
	}
	
	crc = parser.getCRC();

#ifndef DEDICATED_ONLY
	{
		OmfgScript::TokenBase* v = parser.getProperty("sprite");
		if(!v->isDefault())
			sprite = spriteList.load(v->toString());
	}
#endif
	invisible = parser.getBool("invisible", false);
	timeout = parser.getInt("timeout", 0);
	timeoutVariation = parser.getInt("timeout_variation", 0);
	renderLayer = parser.getInt("render_layer", Grid::WormRenderLayer);

#ifndef DEDICATED_ONLY
	rockHidden = parser.getBool("rock_hidden", true);
	if(OmfgScript::Function const* f = parser.getFunction("distortion"))
	{
		if ( f->name == "lens" )
			distortion = new Distortion( lensMap( (*f)[0]->toInt() ));
		else if ( f->name == "swirl" )
			distortion = new Distortion( swirlMap( (*f)[0]->toInt() ));
		else if ( f->name == "ripple" )
			distortion = new Distortion( rippleMap( (*f)[0]->toInt() ));
		else if ( f->name == "random" )
			distortion = new Distortion( randomMap( (*f)[0]->toInt() ) );
		else if ( f->name == "spin" )
			distortion = new Distortion( spinMap( (*f)[0]->toInt() ) );
		else if ( f->name == "bitmap" )
			distortion = new Distortion( bitmapMap( (*f)[0]->toString() ) );
	}
	distortMagnitude = (float)parser.getDouble("distort_magnitude", 0.8f);
	
	std::string blenderstr = parser.getString("blender", "none");
	if(blenderstr == "add") blender = BlitterContext::Add;
	else if(blenderstr == "alpha") blender = BlitterContext::Alpha;
	else if(blenderstr == "alphach") blender = BlitterContext::AlphaChannel;
	else blender = BlitterContext::TNone;
#endif

	alpha = parser.getInt("alpha", 255);
	destAlpha = parser.getInt("dest_alpha", -1);
	wupixels = parser.getBool("wu_pixels", false);

	colour = parser.getProperty("color", "colour")->toColor(255, 255, 255);
	
	OmfgScript::Parser::GameEventIter i(parser);
	for(; i; ++i)
	{
		std::vector<OmfgScript::TokenBase*> const& p = i.params();
		switch(i.type())
		{
			case GameEventID::Creation:
				creation = new GameEvent(i.actions());
			break;
			
			case GameEventID::DetectRange:
				
				int detectFilter = 0;
				if(p[2]->isList())
				{
					const_foreach(i, p[2]->toList())
					{
						OmfgScript::TokenBase& v = **i;
						if ( v.isString() )
						{
							if( v.toString() == "worms" ) detectFilter |= 1;
						}
						else if ( v.isInt() )
							detectFilter |= 1 << (v.toInt() + 1);
					}
				}
				else
				{
					detectFilter = 1;
				}
				detectRanges.push_back( new DetectEvent(i.actions(), (float)p[0]->toDouble(0.0), p[1]->toBool(true), detectFilter));
			break;
		}
	}

	return true;
}

