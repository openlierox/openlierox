/*
 *  GfxUtils.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 03.12.09.
 *  code under LGPL
 *
 */


#include "GfxPrimitives.h"
#include "Geometry.h"
#include "Timer.h"
#include "Mutex.h"
#include "LieroX.h"
#include "AuxLib.h"
#include "Condition.h"
#include "InputEvents.h"


void DrawLoadingAni(SDL_Surface* bmpDest, int x, int y, int rx, int ry, Color fg, Color bg, LoadingAniType type) {
	switch(type) {
		case LAT_CIRCLES: {
			static const int STEPS = 12;
			int cur = int(((GetTime().milliseconds() % 1000)) * STEPS * 0.001f);
			for(int i = 0; i < STEPS; ++i) {
				const float a = (float)PI * 2.0f * i / STEPS;
				VectorD2<float> p( cosf(a) * rx, sinf(a) * ry );
				
				DrawCircleFilled(bmpDest, int(x + p.x), int(y + p.y), rx / 5, ry / 5, (i == cur) ? fg : bg);
			}
			break;
		}
		case LAT_CAKE: {
			static const int STEPS = 6;
			int cur = int(((GetTime().milliseconds() % 500)) * STEPS * 0.002f);
			for(int i = 0; i < STEPS; ++i) {
				const float angle = (float)PI * 2.0f * i / STEPS;
				const float a = angle + (float)PI * 2.0f * -0.5f / STEPS;
				const float b = angle + (float)PI * 2.0f * 0.5f / STEPS;
				Polygon2D poly;
				poly.startPointAdding();
				poly.addPoint(VectorD2<float>(cosf(a) * rx, sinf(a) * ry));
				poly.addPoint(VectorD2<float>(cosf(b) * rx, sinf(b) * ry));
				poly.addPoint(VectorD2<int>(0,0));
				poly.endPointAdding();
				float colf = float((STEPS + cur - i) % STEPS) / float(STEPS - 1);
				Color col = bg * colf + fg * (1.0f - colf);
				poly.drawFilled(bmpDest, x, y, col);
			}
			break;
		}
	}
}


struct ScopedBackgroundLoadingAni::Data {
	SmartPointer<ThreadPoolItem> thread;
	Mutex mutex;
	bool quit;
	Condition breakSig;
	Data() : thread(NULL), quit(false) {}
};

ScopedBackgroundLoadingAni::ScopedBackgroundLoadingAni(int x, int y, int rx, int ry, Color fg, Color bg, LoadingAniType type) {
	data = NULL;
	if(bDedicated) return;
#ifdef SINGLETHREADED
	return;
#endif

	// Note: Earlier, we made a backup copy of the video surface.
	// This is not really needed, because we will redraw in the next frame after this anyway.
	// Also, this caused about 25% overhead in the whole loading process!
	// So, to avoid flickering, just copy to our back buffer:
	CopySurface(VideoPostProcessor::videoBufferSurface().get(), VideoPostProcessor::videoSurface().get(), 0,0,0,0,640,480);
	
	data = new Data();
	
	struct Animator : Action {
		int x, y, rx, ry;
		LoadingAniType type;
		Color fg, bg;
		Data* data;
		
		Result handle() {
			Mutex::ScopedLock lock(data->mutex);
			while(!data->quit) {
				// Kind of a hack: we update the time so that mainlockdetector does not break this
				// loading, no matter how slow it is.
				// As we are only loading the mod/map, we can savly access tLX->currentTime.
				tLX->currentTime = GetTime();
				
				DrawLoadingAni(VideoPostProcessor::videoSurface().get(), x, y, rx, ry, fg, bg, type);
				doVideoFrameInMainThread();
				
				data->breakSig.wait(data->mutex, 50);
			}
			
			
			return true;
		}
	};
	
	Animator* anim = new Animator();
	anim->x = x;
	anim->y = y;
	anim->rx = rx;
	anim->ry = ry;
	anim->fg = fg;
	anim->bg = bg;
	anim->type = type;
	anim->data = data;
	data->thread = threadPool->start(anim, "Background Loading animation");
}

ScopedBackgroundLoadingAni::~ScopedBackgroundLoadingAni() {
	if(data) {
		{
			Mutex::ScopedLock lock(data->mutex);
			data->quit = true;
			data->breakSig.signal();
		}
		threadPool->wait(data->thread);
		delete data;
		data = NULL;
		
		// force a redraw
		WakeupIfNeeded();
	}
}



void DrawArrow(SDL_Surface* bmpDest, int x, int y, int w, int h, Angle dir, Color c) {
	VectorD2<float> angleVec(dir + Angle(90.)); // thus the 0-angle is the normal. see VectorD2 from angle construction.
	MatrixD2<float> transMatrix = MatrixD2<float>::Rotation(angleVec.x, angleVec.y);
	transMatrix.v1.x *= w;
	transMatrix.v2.x *= w;
	transMatrix.v1.y *= h;
	transMatrix.v2.y *= h;

#define P(_x,_y) VectorD2<int>(transMatrix * VectorD2<float>(float(_x)-0.5f,float(_y)-0.5f) + VectorD2<float>(float(w),float(h))*0.5f)
	Polygon2D poly;
	poly.startPointAdding();
	{
		poly.addPoint(P(0.5, 0));
		poly.addPoint(P(2.0/3, 1.0/3));
		poly.addPoint(P(3.0/5, 1.0/3));
		poly.addPoint(P(3.0/5, 1));
		poly.addPoint(P(2.0/5, 1));
		poly.addPoint(P(2.0/5, 1.0/3));
		poly.addPoint(P(1.0/3, 1.0/3));
	}
	poly.endPointAdding();
	poly.drawFilled(bmpDest, x, y, c);
#undef P
}

