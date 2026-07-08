#include "part_type.h"

#include "resource_list.h"

#include "gusgame.h"
#ifndef DEDICATED_ONLY
#include "sprite_set.h"
#include "sprite.h"
#include "gfx.h"
#include "distortion.h"
#include "animators.h"
#endif
#include "util/text.h"
#include "CVec.h"
#include "util/angle.h"
#include "util/macros.h"
#include "util/log.h"
#include "parser.h"
#include "detect_event.h"
#include "timer_event.h"
#include "network.h"
#include "luaapi/context.h"

#include "particle.h"
#include "simple_particle.h"
#include "game_actions.h"
#include "omfgscript/omfg_script.h"
#include "script.h"
#include "FindFile.h"
#include "game/Game.h"


#include "gusanos/allegro.h"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <list>
#include <functional>

using namespace std;

ResourceList<PartType> partTypeList;

LuaReference PartType::metaTable;

CGameObject* newParticle_requested( PartType* type, Vec pos_, Vec spd_, int dir, CWormInputHandler* owner, Angle angle )
{
	if(!type->needsNode) {
		errors << "newParticle_requested: type->needsNode = false" << endl;
		// proceed anyway, I have no idea what I should do here otherwise
	}
	
	Particle* particle = new Particle(type, pos_, spd_, dir, owner, angle);
	particle->assignNetworkRole( false );
	
	if(type->colLayer != Grid::NoColLayer)
		game.objects.insert( particle, type->colLayer, type->renderLayer);
	else
		game.objects.insert( particle, type->renderLayer);

	return particle;
}

CGameObject* newParticle_Particle(PartType* type, Vec pos_ = Vec(0.f, 0.f), Vec spd_ = Vec(0.f, 0.f), int dir = 1, CWormInputHandler* owner = NULL, Angle angle = Angle(0))
{
	if( type->needsNode && network.isClient() ) return 0;
	
	Particle* particle = new Particle(type, pos_, spd_, dir, owner, angle);
	
	if ( type->needsNode && network.isHost() )
	{
		particle->assignNetworkRole( true );
	}
	
	if(type->colLayer != Grid::NoColLayer)
		game.objects.insert( particle, type->colLayer, type->renderLayer);
	else
		game.objects.insert( particle, type->renderLayer);

	return particle;
}

template<class T>
CGameObject* newParticle_SimpleParticle(PartType* type, Vec pos_ = Vec(0.f, 0.f), Vec spd_ = Vec(0.f, 0.f), int dir = 1, CWormInputHandler* owner = NULL, Angle angle = Angle(0))
{
	int timeout = type->simpleParticle_timeout + rndInt(type->simpleParticle_timeoutVariation);
	
	CGameObject* particle = new T(pos_, spd_, owner, timeout, type->gravity, type->colour, type->wupixels);
	
	if(type->creation)
		type->creation->run(particle);
	
	game.objects.insert( particle, type->colLayer, type->renderLayer);
	return particle;
}

#ifdef DEDICATED_ONLY
CGameObject* newParticle_Dummy(PartType* type, Vec pos_ = Vec(0.f, 0.f), Vec spd_ = Vec(0.f, 0.f), int dir = 1, CWormInputHandler* owner = NULL, Angle angle = Angle(0))
{
	if(type->creation)
	{
		CGameObject particle(owner, pos_, spd_);
		type->creation->run(&particle);
	}
	return 0;
}
#endif

PartType::PartType()
: ResourceBase(), newParticle(0), wupixels(0)
, invisible(false)
{
	gravity			= 0;
	bounceFactor	= 1;
	groundFriction	= 1;
	colour			= -1;
	repeat			= 1;
	alpha			= 255;
	angularFriction = 0;
	animDuration	= 100;
	animType		= ANIM_LOOPRIGHT;
	animOnGround	= 1;
	damping			= 1;
	acceleration	= 0;
	maxSpeed		= -1;
	colLayer		= 0;
	health			= 100;
	radius			= 0;
	
	syncPos = false;
	syncSpd = false;
	syncAngle = false;
	needsNode = false;
	
	renderLayer = Grid::WormRenderLayer;
#ifndef DEDICATED_ONLY
	sprite = NULL;
	distortion = NULL;
	distortMagnitude = 0.8f;

	blender = BlitterContext::TNone;
	
	lightHax = NULL;
#endif
	
	line2Origin = false;
	culled = false;
	
	groundCollision = NULL;
	creation = NULL;
	death = NULL;
	
	for ( int i = 0; i < 16; ++i )
	{
		customEvents.push_back(NULL);
	}
}

PartType::~PartType()
{
	delete groundCollision;
	delete creation;
#ifndef DEDICATED_ONLY
	delete distortion;
	delete lightHax;
#endif
	for ( vector<TimerEvent*>::iterator i = timer.begin(); i != timer.end(); i++)
	{
		delete *i;
	}
	for ( vector<DetectEvent*>::iterator i = detectRanges.begin(); i != detectRanges.end(); i++)
	{
		delete *i;
	}
}

void PartType::touch()
{
#ifndef DEDICATED_ONLY
	// distort_size and light_size come from the mod,
	// which is downloaded from the server and thus untrusted.
	// Their products (width*height*4 and width*2) are computed in int
	// and overflow for huge or negative values,
	// which would under-size the buffers the loops below write into.
	// Reject implausible sizes.
	const int MaxGenSize = 4096;
	if(!distortion && !distortionGen.empty())
	{
		LuaReference f = distortionGen.get();
		if(f.isSet(luaIngame)
		   && distortionSize.x >= 0 && distortionSize.x <= MaxGenSize
		   && distortionSize.y >= 0 && distortionSize.y <= MaxGenSize)
		{
			DistortionMap* d = new DistortionMap;
			int width = distortionSize.x;
			int height = distortionSize.y;

			// DistortionMap is doubleRes
			d->map.resize(width * height * 4);
			d->width = width * 2;
			
			int hwidth = width / 2;
			int hheight = height / 2;
			
			for(int y = 0; y < height*2; ++y)
			for(int x = 0; x < width*2; ++x)
			{
				int n = (luaIngame.call(f, 2), x*0.5f - hwidth, y*0.5f - hheight, width, height)();
				if(n == 2)
				{
					d->map[y * width*2 + x] = Vec((float)lua_tonumber(luaIngame, -2), (float)lua_tonumber(luaIngame, -1)) * 2 /*doubleRes*/;
					luaIngame.pop(n);
				}
				else
					warnings << "PartType::touch with dissortion, expected 2 but got " << n << endl;
			}
			distortion = new Distortion(d);
		}
		// TODO: free distortionGen function
	}
	
	if(!lightHax && !lightGen.empty())
	{
		LuaReference f = lightGen.get();
		if(f.isSet(luaIngame)
		   && lightSize.x >= 0 && lightSize.x <= MaxGenSize
		   && lightSize.y >= 0 && lightSize.y <= MaxGenSize)
		{
			int width = lightSize.x;
			int height = lightSize.y;
			
			ALLEGRO_BITMAP* l = create_bitmap_ex(8, width*2, height*2);
			if(!l) return; // allocation failed; this is the last effect, so just bail
			
			int hwidth = width / 2;
			int hheight = height / 2;

			for ( int y = 0; y < height; ++y )
			for ( int x = 0; x < width; ++x )
			{
				int n = (luaIngame.call(f, 1), x - hwidth, y - hheight, width, height)();
				if(n == 1)
				{
					int v = lua_tointeger(luaIngame, -1);
					if(v < 0) v = 0;
					else if(v > 255) v = 255;
					putpixel_solid2x2(l, x*2, y*2, v);
					luaIngame.pop(1);
				}
				else
					assert(false);
			}
			
			lightHax = new Sprite(l, hwidth, hheight);
		}
		// TODO: free lightGen function
	}
#endif
}

bool PartType::isSimpleParticleType()
{
	if(repeat != 1 || alpha != 255
#ifndef DEDICATED_ONLY
	|| sprite || distortion || blender || lightHax
#endif
	|| damping != 1.f
	|| acceleration != 0.f || !groundCollision
	|| death || timer.size() > 1 || line2Origin
	|| detectRanges.size() > 0
	|| needsNode )
	{
		return false;
	}
		
	GameEvent::Actions::const_iterator i = groundCollision->actions.begin();
	for(; i != groundCollision->actions.end(); ++i)
	{
		Remove* event = dynamic_cast<Remove *>(&**i);
		if(!event)
			return false; // groundCollision contains non-remove actions
	}
	
	if(timer.size() == 1)
	{
		// triggerTimes is irrelevant since it will only trigger once anyway
		
		if(timer[0]->actions.size() != 1)
			return false;
			
		Remove* event = dynamic_cast<Remove *>(&*timer[0]->actions[0]);
		if(!event)
			return false; // timer event contains non-remove actions
		
		// One timer with one remove action

		simpleParticle_timeout = timer[0]->delay;
		simpleParticle_timeoutVariation = timer[0]->delayVariation;
	}
	else
	{
		simpleParticle_timeout = 0;
		simpleParticle_timeoutVariation = 0;
	}
		

	return true;
}

namespace GameEventID
{
enum type
{
	Creation,
	DetectRange,
	GroundCollision,
	Death,
	Timer,
	CustomEvent,
};
}


static void _reduceStack(std::function<void()> loadFunc) {
	// If we call this recursively, save loadFunc for later
	// and call it from the root call context.
	// Do this to avoid too deep stacks, which is a problem
	// for certain mods (e.g. Telek).
	// That will leave the object unloaded in the recursive context,
	// so we must not depend on it until we return from
	// all the loading code.
	// This should be the case for the particle type loading.

	// Note that we could also have more tricky code here
	// via clever co-routine stack dumping - then we would not
	// have any requirements. The code logic would look very
	// similar (see Git history for a draft).
	// However, for simplification, we don't do this for now.

	// We expect that this is always called from the same thread.
	
	static bool insideLoading = false;
	struct Stack {
		std::function<void()> loadFunc_;
		Stack(std::function<void()> f = NULL) : loadFunc_(f) {}
	};
	static std::list<Stack> stacks;
	
	if(insideLoading) {
		// Dump our stack + loadFunc.
		stacks.push_back(Stack(loadFunc));
		// It will get handled from the root call context.
		// Nothing to do here anymore.
		return;
	}
	
	insideLoading = true;
	{
		loadFunc();

		while(!stacks.empty()) {
			Stack top;
			std::swap(top, stacks.back());
			stacks.pop_back();
			
			top.loadFunc_();
		}
	}
	insideLoading = false;
}

bool PartType::load(std::string const& filename)
{
	if(!gusExistsFile(filename)) return false;
	
	_reduceStack([this,filename]() {
		_load(filename);
	});
	
	return true;
}

bool PartType::_load(std::string const& filename)
{
	std::ifstream fileStream;
	OpenGameFileR(fileStream, filename, std::ios::binary | std::ios::in);

	if(!fileStream) {
		notes << "PartType: cannot open " << filename << endl;
		return false;
	}
	
	OmfgScript::Parser parser(fileStream, gameActions, filename);
	
	namespace af = OmfgScript::ActionParamFlags;
		
	parser.addEvent("creation", GameEventID::Creation, af::Object);
	
	parser.addEvent("death", GameEventID::Death, af::Object);
	
	parser.addEvent("ground_collision", GameEventID::GroundCollision, af::Object);
	
	parser.addEvent("timer", GameEventID::Timer, af::Object)
		("delay")
		("delay_var")
		("max_trigger")
	;
	
	parser.addEvent("custom_event", GameEventID::CustomEvent, af::Object | af::Object2)
		("index", false)
	;
	
	parser.addEvent("detect_range", GameEventID::DetectRange, af::Object | af::Object2)
		("range")
		("detect_owner")
		("layers")
	;
		
	if(!parser.run())
	{
		if(parser.incomplete())
			parser.error("Trailing garbage");
		notes << "PartType: cannot parse " << filename << endl;
		return false;
	}
	
	//FLOG(parttypecrc, filename.string() << ": " << std::hex << parser.getCRC());
	crc = parser.getCRC();

#ifndef DEDICATED_ONLY
	{
		OmfgScript::TokenBase* v = parser.getProperty("sprite");
		if(!v->isDefault())
			sprite = spriteList.load(v->toString());
	}
	{
		// NOTE: this was deprecated in Gusanos. We don't mark it this way because it is used
		// quite often and the warning is annoying.
		OmfgScript::TokenBase* v = parser.getProperty("light_radius");
		if(!v->isDefault())
			lightHax = genLight(v->toInt(0));
	}
	
	if(OmfgScript::Function const* f = parser.getDeprFunction("distortion"))
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
	
	distortionGen = parser.getString("distort_gen", "");

	{
		OmfgScript::TokenBase* v = parser.getProperty("distort_size");
		if(v->isList())
		{
			std::list<OmfgScript::TokenBase*> const& c = v->toList();
			if(c.size() >= 2)
			{
				std::list<OmfgScript::TokenBase*>::const_iterator i = c.begin();
				distortionSize.x = (*i++)->toInt(0);
				distortionSize.y = (*i++)->toInt(0);
			}
		}
	}
	
	lightGen = parser.getString("light_gen", "");

	{
		OmfgScript::TokenBase* v = parser.getProperty("light_size");
		if(v->isList())
		{
			std::list<OmfgScript::TokenBase*> const& c = v->toList();
			if(c.size() >= 2)
			{
				std::list<OmfgScript::TokenBase*>::const_iterator i = c.begin();
				lightSize.x = (*i++)->toInt(0);
				lightSize.y = (*i++)->toInt(0);
			}
		}
	}
	
	distortMagnitude = (float)parser.getDouble("distort_magnitude", 1);
	
	std::string blenderstr = parser.getString("blender", "none");
	if(blenderstr == "add") blender = BlitterContext::Add;
	else if(blenderstr == "alpha") blender = BlitterContext::Alpha;
	else if(blenderstr == "alphach") blender = BlitterContext::AlphaChannel;
	else blender = BlitterContext::TNone;
#endif
	invisible = parser.getBool("invisible", false);
	culled = parser.getBool("occluded", false);
	animOnGround = parser.getBool("anim_on_ground", false);
	renderLayer = parser.getInt("render_layer", Grid::WormRenderLayer);
	colLayer = parser.getInt("col_layer", 0);
	
	syncPos = parser.getBool("sync_pos", false);
	syncSpd = parser.getBool("sync_spd", false);
	syncAngle = parser.getBool("sync_angle", false);
	
	animDuration = parser.getInt("anim_duration", 100);
	gravity = (float)parser.getDouble("gravity", 0.0);
	repeat = parser.getInt("repeat", 1);
	bounceFactor = (float)parser.getDouble("bounce_factor", 1.0);
	groundFriction = (float)parser.getDouble("ground_friction", 1.0);
	damping = (float)parser.getDouble("damping", 1.0);
	acceleration = (float)parser.getDouble("acceleration", 0.0);
	maxSpeed = (float)parser.getDouble("max_speed", -1.0);
	angularFriction = Angle(parser.getDouble("angular_friction", 0.0));
	health = (float)parser.getDouble("health", 100.0);
	radius = (float)parser.getDouble("radius", 0.0);
	line2Origin = parser.getBool("line_to_origin", false);
	//networkInitName = parser.getString("network_init", "");
	networkInit = parser.getString("network_init", "");
	
	std::string animtypestr = parser.getString("anim_type", "loop_right");
	if(animtypestr == "ping_pong") animType = ANIM_PINGPONG;
	else if(animtypestr == "loop_right") animType = ANIM_LOOPRIGHT;
	else if(animtypestr == "right_once") animType = ANIM_RIGHTONCE;
			
	alpha = parser.getInt("alpha", 255);
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
			
			case GameEventID::GroundCollision:
				groundCollision = new GameEvent(i.actions());
			break;
			
			case GameEventID::Death:
				death = new GameEvent(i.actions());
			break;
			
			case GameEventID::Timer:
				timer.push_back(new TimerEvent(i.actions(), p[0]->toInt(100), p[1]->toInt(0), p[2]->toInt(0)));
			break;
			
			case GameEventID::DetectRange:
			{
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
				detectRanges.push_back( new DetectEvent(i.actions(), (float)p[0]->toDouble(10.0), p[1]->toBool(true), detectFilter));
			}
			break;
			
			case GameEventID::CustomEvent:
			{
				size_t eventIndex = p[0]->toInt();
				if ( eventIndex < customEvents.size() )
				{
					customEvents[eventIndex] = new GameEvent(i.actions());
				}
			}
			break;
		}
	}
	
	needsNode = syncPos || syncSpd || syncAngle || !networkInit.empty();
	
	if(isSimpleParticleType())
	{
#ifndef DEDICATED_ONLY
		newParticle = newParticle_SimpleParticle<SimpleParticle>;
#else
		newParticle = newParticle_Dummy;
#endif
	}
	else
		newParticle = newParticle_Particle;
		
	if( colLayer >= 0 )
		colLayer = Grid::CustomColLayerStart + colLayer;
	else
		colLayer = Grid::NoColLayer;
			
	return true;
}


void PartType::finalize()
{
	delete groundCollision; groundCollision = 0;
	delete creation; creation = 0;
#ifndef DEDICATED_ONLY
	delete distortion; distortion = 0;
	delete lightHax; lightHax = 0;
#endif
	for ( vector<TimerEvent*>::iterator i = timer.begin(); i != timer.end(); i++)
	{
		delete *i;
	}
	timer.clear();
	
	for ( vector<DetectEvent*>::iterator i = detectRanges.begin(); i != detectRanges.end(); i++)
	{
		delete *i;
	}
	detectRanges.clear();
}

#ifndef DEDICATED_ONLY
BaseAnimator* PartType::allocateAnimator()
{
	switch ( animType )
	{
		case PartType::ANIM_PINGPONG : 
			return new AnimPingPong(sprite, animDuration);
		break;
		
		case PartType::ANIM_LOOPRIGHT : 
			return new AnimLoopRight(sprite, animDuration);
		break;
			
		case PartType::ANIM_RIGHTONCE : 
			return new AnimRightOnce(sprite, animDuration);
		break;
	}
	
	return 0;
}
#endif

