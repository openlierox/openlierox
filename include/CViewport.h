/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Viewport class
// Created 21/1/02
// Jason Boettcher


#ifndef __CVIEWPORT_H__
#define	__CVIEWPORT_H__

#include <SDL.h>

#include "CInput.h"
#include "CVec.h"
#include "Options.h" // for control_t
#include "SmartPointer.h"

#include "CVec.h"
#include "gusanos/luaapi/types.h"
#include "util/BaseObject.h"

// Viewport types
// !!! Don't change the order !!!
enum {
    VW_FOLLOW = 0,
    VW_CYCLE,
    VW_FREELOOK,
    VW_ACTIONCAM
};


class CWorm;
struct Listener;
struct ALLEGRO_BITMAP;
class CWormInputHandler;


class CViewport : BaseObject {
private:
	void _init() {
		bUsed = false;
		nID = 0;
		Left = Top = 0;
		Width = 320;
		Height = 280;
		VirtWidth = 640;
		VirtHeight = 480;
		WorldX = WorldY = 0;

		bShaking = false;
		iShakeAmount = 0;

		m_origTargetWorm = pcTargetWorm = NULL;
		nType = VW_FOLLOW;
		fTimer = AbsTime();
		bSmooth = false;
		m_listener = NULL;

		gusInit();
	}

public:
	CViewport() { _init(); }
	~CViewport() { gusReset(); }
	
	CViewport(const CViewport& o) { _init(); operator=(o); }
	CViewport& operator=(const CViewport& o);

private:
	// Attributes

	bool	bUsed;
    int     nID;
	int		Left,Top;
	int		Width,Height;

	int		VirtWidth, VirtHeight;

	int  	WorldX, WorldY;

    CVec    curPos, tgtPos;

	AbsTime	fShakestart;
	bool	bShaking;
	int		iShakeAmount;

    int     nType;
    CWorm   *pcTargetWorm;

	CWorm*	m_origTargetWorm;
	
    AbsTime   fTimer;

	// HINT: we have to use pointers here as we have CViewport in a global variable
    SmartPointer<CInput>  cUp, cRight, cDown, cLeft;
    SmartPointer<CInput>  gpUp, gpRight, gpDown, gpLeft;
	
	bool	bSmooth;
	CVec	cSmoothVel, cSmoothAccel;
	
	
public:
	// Methods

	void	Setup(int l, int t, int vw, int vh, int type);

	void	Process(CViewport *pcViewList, int MWidth, int MHeight, int iGameMode);
	void	Clamp(int MWidth, int MHeight);

	bool	inView(CVec pos);	
	void	Shake(int amount);

    void    setupInputs(const PlyControls& Inputs);
    void    setupGamepadInputs(const PlyControls& Inputs);

    void    reset();

    CWorm   *findTarget(CViewport *pcViewList, bool bAlive);

	SDL_Rect getRect();
	
	void	setSmoothPosition( float X, float Y, TimeDiff DeltaTime );

	//
	// Variables
	//
	int		GetLeft() const	{ return Left; }
	int		GetTop() const	{ return Top; }
	int		GetWidth() const { return Width; }
	int		GetHeight()	const { return Height; }
	int		GetWorldX()	const { return WorldX; }
	int		GetWorldY()	const { return WorldY; }
	int		GetVirtW() const { return VirtWidth; }
	int		GetVirtH() const { return VirtHeight; }

	void	SetLeft(int _l)		{ Left = _l; }
	void	SetTop(int _t)		{ Top = _t; }
	void	SetVirtWidth(int _w)	{ Width = _w/2; VirtWidth = _w; }
	void	SetVirtHeight(int _h)	{ Height = _h/2; VirtHeight = _h; }
	void	SetWorldX(int _x)	{ WorldX = _x; }
	void	SetWorldY(int _y)	{ WorldY = _y; }

	bool	getUsed() const	{ return bUsed; }
	void	shutdown();

	void	setOrigTarget(CWorm* w) { m_origTargetWorm = w; }
	CWorm	*getOrigTarget() { return m_origTargetWorm; }
    void    setTarget(CWorm *w) { pcTargetWorm = w; }
    CWorm   *getTarget() const { return pcTargetWorm; }

    int     getType() const { return nType; }
	void	setType(int _t)		{ nType = _t; }

    void    setID(int id)       { nID = id; }

    void    setSmooth(bool _b);

	
	VectorD2<int>	physicToReal(VectorD2<int> p, bool wrapAround = false, long mapW = 0, long mapH = 0) const {
		const int wx = GetWorldX();
		const int wy = GetWorldY();
		const int l = GetLeft();
		const int t = GetTop();
		int x = p.x - wx;
		int y = p.y - wy;
		if(wrapAround) {
			x %= mapW; if(x < 0) x += mapW;
			y %= mapH; if(y < 0) y += mapH;
		}
		return VectorD2<int>(x * 2 + l, y * 2 + t);
	}
	
	bool	posInside(VectorD2<int> p) const {
		const int l = GetLeft();
		const int t = GetTop();
		if((p.x<l || p.x>l+GetVirtW()))
			return false;
		if((p.y<t || p.y>t+GetVirtH()))
			return false;
		return true;
	}
	
	bool	physicsInside(VectorD2<int> p, bool wrapAround = false, long mapW = 0, long mapH = 0) const {
		return posInside(physicToReal(p, wrapAround, mapW, mapH));
	}
	
	
	
	// ---------------------------------------------------
	// ---------------- Gusanos --------------------------
	
	void gusInit();
	void gusReset();
	
	void setDestination(int w, int h);
	void gusRender(const SmartPointer<SDL_Surface>& bmpDest);
		
	void drawLight(IVec const& v); // TEMP
	
	IVec getPos()
	{
		return IVec(WorldX,WorldY);
	}
	
	IVec convertCoords( IVec const & coord )
	{
		return (coord - IVec(WorldX,WorldY)) * 2 /* doubleRes */;
	}
	
	Vec convertCoordsPrec( Vec const & coord )
	{
		return (coord - Vec((float)WorldX,(float)WorldY)) * 2 /* doubleRes */;
	}
			
	ALLEGRO_BITMAP* dest;
	ALLEGRO_BITMAP* fadeBuffer;

	static LuaReference metaTable;
	virtual LuaReference getMetaTable() const { return metaTable; }

private:
	Listener* m_listener;
};



#endif  //  __CVIEWPORT_H__
