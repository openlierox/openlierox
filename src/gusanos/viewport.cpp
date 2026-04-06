#include "CViewport.h"

#ifndef DEDICATED_ONLY

#include "gusgame.h"
#include "sound/sfx.h"
#include "gfx.h"
#include "gusanos/allegro.h"
#include "game/CWorm.h"
#include "game/WormInputHandler.h"
#include "CWormHuman.h"
#include "LuaCallbacks.h"
#include "lua/bindings-gfx.h"
#include "blitters/blitters.h"
#include "culling.h"
#include "game/CMap.h"
#include "game/Game.h"
#include "FlagInfo.h"
#include "CGameMode.h"
#include <list>

#include "sprite_set.h" // TEMP
#include "sprite.h" // TEMP
#include "CGameScript.h"

#include <iostream>

LuaReference CViewport::metaTable;

using namespace std;

void CViewport::gusInit()
{
	dest = 0;
	fadeBuffer = 0;
}

void CViewport::gusReset()
{
	destroy_bitmap(dest); dest = 0;
	destroy_bitmap(fadeBuffer); fadeBuffer = 0;
}

struct TestCuller : public Culler<TestCuller>
{
	TestCuller(ALLEGRO_BITMAP* dest_, ALLEGRO_BITMAP* src_, int scrOffX_, int scrOffY_, int destOffX_, int destOffY_, Rect const& rect)
			: Culler<TestCuller>(rect), dest(dest_), src(src_),
			scrOffX(scrOffX_), scrOffY(scrOffY_), destOffX(destOffX_), destOffY(destOffY_)
	{
	}

	bool block(int64_t x, int64_t y)
	{
		// doubleRes to singleRes coords
		return !game.gameMap()->unsafeGetMaterial(uint32_t(x/2), uint32_t(y/2)).worm_pass;
	}

	void line(int64_t y, int64_t x1, int64_t x2)
	{
		drawSpriteLine_add(
		    dest,
		    src,
		    int(x1 + scrOffX),
		    int(y + scrOffY),
		    int(x1 + destOffX),
		    int(y + destOffY),
		    int(x2 + destOffX + 1),
		    255);
	}

	ALLEGRO_BITMAP* dest;
	ALLEGRO_BITMAP* src;

	int scrOffX;
	int scrOffY;
	int destOffX;
	int destOffY;
};

static Sprite* testLight = 0;

void CViewport::setDestination(int width, int height)
{
	destroy_bitmap(fadeBuffer);
	fadeBuffer = create_bitmap_ex(8, width, height);
}

void CViewport::drawLight(IVec const& v)
{
	ALLEGRO_BITMAP* renderBitmap = testLight->m_bitmap;
	IVec off = getPos() * 2;
	IVec loff(v*2 - IVec(testLight->m_xPivot, testLight->m_yPivot));

	Rect r(0, 0, game.gameMap()->GetWidth()*2 - 1, game.gameMap()->GetHeight()*2 - 1);
	r &= Rect(renderBitmap) + loff;

	// we use drawSpriteLine_add which requires the same bit depths
	assert(renderBitmap->surf->format->BitsPerPixel == 8);
	TestCuller testCuller(fadeBuffer, renderBitmap, -off.x, -off.y, -loff.x, -loff.y, r);

	testCuller.cullOmni(v.x*2, v.y*2);
}


void CViewport::gusRender(const SmartPointer<SDL_Surface>& bmpDest)
{
	{
		int destw = Width*2;
		int desth = Height*2;
		bool needDestReset = false;
		if(!dest || !fadeBuffer || !testLight)
			needDestReset = true;
		else if(fadeBuffer->w != destw || fadeBuffer->h != desth)
			needDestReset = true;
		
		if(needDestReset)
			setDestination(destw, desth);
		
		destroy_bitmap(dest);
		dest = create_bitmap_from_sdl(bmpDest, Left, Top, Width*2, Height*2);
	}

	{
		int r = game.darkMode_wormLightRadius;
		if(r < 1) r = 1;
		if(r > 500) r = 500; // sane maximum. dont crash OLX

		bool needLightReset = false;
		if(!testLight)
			needLightReset = true;
		else if(testLight->m_xPivot != r * 2 /*doubleRes*/)
			needLightReset = true;

		if(needLightReset)
			testLight = genLight(r);
	}

	game.gameMap()->gusDraw(dest, WorldX, WorldY);

	if ( game.isLevelDarkMode() && game.gameMap()->lightmap )
		blit( game.gameMap()->lightmap, fadeBuffer, WorldX*2, WorldY*2, 0, 0, fadeBuffer->w, fadeBuffer->h );

	if (game.state == Game::S_Playing)  {
		// update the drawing position
		for_each_iterator(CWorm*, w, game.aliveWorms())
			w->get()->UpdateDrawPos();

		if( tLXOptions->bShadows ) {
			// Draw the projectile shadows
			cClient->DrawProjectileShadows(bmpDest.get(), this);

			// Draw the worm shadows
			for_each_iterator(CWorm*, w, game.aliveWorms())
				w->get()->DrawShadow(bmpDest.get(), this);
		}

		// Draw the entities
		DrawEntities(bmpDest.get(), this);

		// Draw the projectiles
		cClient->DrawProjectiles(bmpDest.get(), this);

		// Draw the bonuses
		cClient->DrawBonuses(bmpDest.get(), this);

		// draw unattached flags and flag spawnpoints
		cClient->flagInfo()->draw(bmpDest.get(), this);

		// draw worms
		for_each_iterator(CWorm*, w, game.aliveWorms())
			w->get()->Draw(bmpDest.get(), this);
	}

	for ( Grid::iterator iter = game.objects.beginAll(); iter; ++iter)
		iter->draw(this);

	if(game.isLevelDarkMode() && pcTargetWorm) {
		if(pcTargetWorm->isActive())
			drawLight(pcTargetWorm->pos().get());
	}

	if(game.isLevelDarkMode())
		drawSprite_mult_8(dest, fadeBuffer, 0, 0);

	// only use the player/worm specific drawings in gus mods
	if(game.gameScript()->gusEngineUsed()) {
		CWormInputHandler* targetPlayer = pcTargetWorm ? pcTargetWorm->inputHandler() : NULL;
		
		// Note that we only process worms in the Lua callbacks which have a player set.
		// Most Lua code depends on this assumption so it would break otherwise.
		
		for(CWormInputHandler* player : game.players) {
			CWorm* worm = player->getWorm();
			if(!worm) continue;
			if(!worm->isActive()) continue;
			if(!worm->inputHandler()) continue;

			IVec renderPos( worm->getRenderPos() );
			int x = renderPos.x - WorldX;
			int y = renderPos.y - WorldY;
			LuaReferenceLazy ownerRef;
			if(targetPlayer)
				ownerRef = targetPlayer->getLuaReference();

			LUACALLBACK(wormRender).call()
				((lua_Number)x)
				((lua_Number)y)
				(worm->getLuaReference())
				(getLuaReference())
				(ownerRef)
				();
		}

		// draw viewport specific stuff only for human worms
		if(pcTargetWorm && dynamic_cast<CWormHumanInputHandler*>(pcTargetWorm->inputHandler()) != NULL) {
			LUACALLBACK(viewportRender).call()(getLuaReference())(pcTargetWorm->getLuaReference())();
		}
	}

	if(getTarget() && game.gameMode() && game.gameMode()->HaveTargetPos(getTarget())) {
		CVec wormPos = convertCoordsPrec(getTarget()->pos());
		CVec targetPos = convertCoordsPrec(game.gameMode()->TargetPos(getTarget()));
		CVec dir = targetPos - wormPos;

		if(dir.length() > 1) {
			CVec dirNormal = dir.normal();
			dir -= dirNormal * 50.f;

			static const int arrowW = 30;
			float f = 1.f;
			if(dir.x && (wormPos + dir * f).x - arrowW/2 < 0)
				f = CLAMP((arrowW/2 - wormPos.x) / dir.x, 0.f, f);
			if(dir.y && (wormPos + dir * f).y - arrowW/2 < 0)
				f = CLAMP((arrowW/2 - wormPos.y) / dir.y, 0.f, f);
			if(dir.x && (wormPos + dir * f).x + arrowW/2 > dest->w)
				f = CLAMP((dest->w - arrowW/2 - wormPos.x) / dir.x, 0.f, f);
			if(dir.y && (wormPos + dir * f).y + arrowW/2 > dest->h)
				f = CLAMP((dest->h - arrowW/2 - wormPos.y) / dir.y, 0.f, f);

			dir *= f;
			float diffToTarget = (float)(wormPos - targetPos).length();
			if(diffToTarget > 1) {
				CVec p = wormPos + dir;

				int alpha = 200;
				if(diffToTarget < 100)
					alpha = CLAMP(int(alpha * (diffToTarget - 50.f) / 50.f), 0, alpha);

				DrawArrow(dest->surf.get(), (int)p.x-arrowW/2, (int)p.y-arrowW/2, arrowW, arrowW, dirNormal.getAngle(), Color(0,255,30,alpha));
			}
		}
	}
}

#else
// Stubs
LuaReference CViewport::metaTable;

void CViewport::gusInit()
{
}

void CViewport::gusReset()
{
}

void CViewport::gusRender(const SmartPointer<SDL_Surface>& bmpDest)
{
}
#endif
