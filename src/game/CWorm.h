/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Worm class
// Created 28/6/02
// Jason Boettcher


#ifndef __CWORM_H__
#define __CWORM_H__

#include <cassert>
#include "CodeAttributes.h"
#include "LieroX.h" // for MAX_WORMS, _AI_DEBUG
#include "CProjectile.h"
#include "CNinjaRope.h"
#include "CWpnRest.h"
#include "Options.h" // for control_t
#include "DeprecatedGUI/CBar.h"
#include "game/CMap.h"
#include "CGameSkin.h"
#include "Entity.h"
#include "Version.h"
#include "DynDraw.h"
#include "game/CGameObject.h"
#include "util/VecTimeRecorder.h"
#include "game/Attr.h"
#include "util/CustomVar.h"
#include "util/List.h"

// TODO: remove this after we changed network
#include "CBytestream.h"

// Gusanos related includes
#include "gusanos/luaapi/types.h"
#include "gusanos/netstream.h"
#include "CVec.h"
#include "util/angle.h"
#include <vector>


class CWormInputHandler;
class Weapon;
class WeaponType;
struct LuaEventDef;
class NetWormInterceptor;
#ifndef DEDICATED_ONLY
class SpriteSet;
class BaseAnimator;
class CViewport;
#endif
struct profile_t;


#ifdef _MSC_VER
// Warning: this used in member initializer list
#pragma warning(disable:4355)
#endif

// Direction
enum DIR_TYPE
{
	DIR_LEFT = 0, 
	DIR_RIGHT = 1 
};

INLINE DIR_TYPE OppositeDir(DIR_TYPE d) {
	switch(d) {
		case DIR_LEFT: return DIR_RIGHT;
		case DIR_RIGHT: return DIR_LEFT;
	}
	return DIR_LEFT;
}

enum WRM_LIVES
{
	WRM_OUT = -1,
	WRM_UNLIM = -2 
};


struct WormType;
class CWpnRest;
struct weapon_t;

// Weapon slot structure
struct wpnslot_t : CustomVar {
	ATTR(wpnslot_t, int32_t,	WeaponId,	1, { defaultValue = -1; serverside = false; })
	ATTR(wpnslot_t, float,		Charge,		2, { defaultValue = 1.f; serverside = true; })
	ATTR(wpnslot_t, bool,		Reloading,	3, { defaultValue = false; serverside = true; })
	ATTR(wpnslot_t, float,		LastFire,	4, { defaultValue = 0.f; serverside = true; })

	const weapon_t* weapon() const;

	wpnslot_t();
	void reset();

	virtual Result getAttrib(const ScriptVar_t& key, ScriptVar_t& value) const;
	virtual Result setAttrib(const ScriptVar_t& key, const ScriptVar_t& value);

	virtual BaseObject* parentObject() const;
	virtual std::string toString() const;
	virtual bool fromString(const std::string & str);
};

// Worm frame state
// Created 22/7/02
// Jason Boettcher
struct worm_state_t : CustomVar {
	bool	bShoot;
	bool	bCarve;
	bool	bMove;
	bool	bJump;

	worm_state_t();
	void reset();
	uint8_t asInt() const;
	void fromInt(uint8_t i);

	virtual BaseObject* parentObject() const;

	virtual Result getAttrib(const ScriptVar_t& key, ScriptVar_t& value) const;
	virtual Result setAttrib(const ScriptVar_t& key, const ScriptVar_t& value);

	virtual CustomVar* copy() const;
	virtual bool operator==(const CustomVar&) const;
	virtual bool operator<(const CustomVar&) const;
	virtual std::string toString() const;
	virtual bool fromString( const std::string & str);

	virtual void copyFrom(const CustomVar&);
	virtual Result toBytestream( CBytestream* bs, const CustomVar* diffTo ) const;
	virtual Result fromBytestream( CBytestream* bs, bool expectDiffToDefault );

	bool operator!=(const CustomVar& o) const { return !(*this == o); }
};


// the files have to be included yourself later
// they are used here; but their headers depends on this header
// TODO: remove the usage of these in this header
class CServerConnection;
class CBonus;



class CWormInputHandler;
struct WormJoinInfo;
class Client;
class Game;


class CWorm: public CGameObject {
	friend class CWormInputHandler;
	friend class CWormBotInputHandler; // TODO: remove
	friend class CWormHumanInputHandler; // TODO: remove
	friend struct WormJoinInfo;
	friend class Client;
	friend class Game;
	friend class PhysicsEngine;
public:
	CWorm();
	~CWorm();

	virtual bool weOwnThis() const;
	virtual CServerConnection* ownerClient() const;

	virtual Result getAttrib(const ScriptVar_t& key, ScriptVar_t& value) const;

private:
	// disallow these!
	CWorm(const CWorm&);
	CWorm& operator=(const CWorm&);
	
protected:
	// Attributes

	// General
	int			iID;
	WormType*	m_type;
	bool		bLocal;
	CServerConnection *cOwner;
	CWormInputHandler* m_inputHandler;
	bool            bPrepared;

public:
	ATTR(CWorm, int32_t,	iTeam, 1, {serverside = true;})
	ATTR(CWorm, std::string,	sName, 2, {serverside = false;})

	ATTR(CWorm, CGameSkin, cSkin, 3, {serverside = false;})

	// Game
	ATTR(CWorm, int32_t,	iLives, 5, { defaultValue = (int32_t)-2; })
	ATTR(CWorm, bool,	bAlive, 6, {})

	ATTR(CWorm, bool, bCanRespawnNow, 10, {serverside = true;})
	ATTR(CWorm, bool, bRespawnRequested, 11, {serverside = false;})
	ATTR(CWorm, bool, bSpawnedOnce, 12, {serverside = true;})
	ATTR(CWorm, bool, bSpectating, 13, {serverside = false;})

	// Arsenal
	ATTR(CWorm, bool,	bWeaponsReady,  20, {serverside = false; onUpdate = onWeaponsReadyUpdate; })
	ATTR(CWorm,	int32_t,	iCurrentWeapon,	21, {serverside = false;})
	ATTR(CWorm, List<wpnslot_t>,	weaponSlots, 22, { serverside = false; defaultValue = List<wpnslot_t>(5).getRefCopy(); })

	struct WeaponSlotWrapper {
		const CWorm* parent() const { return (const CWorm*)__OLX_BASETHIS(CWorm, tWeapons); }
		const wpnslot_t& operator[](size_t i) const {
			return parent()->weaponSlots.get()[i];
		}
		size_t size() const { return parent()->weaponSlots.get().size(); }
	}
	tWeapons;

	ATTR(CWorm, worm_state_t, tState, 23, {serverside = false;})

protected:
	SmartPointer<profile_t> tProfile; // used to read (AI)nDifficulty and read/write human player weapons
	
	// Simulation
	CVec		vLastPos;
	CVec		vDrawPos;
	bool		bOnGround;
	AbsTime		fLastInputTime;
	// last time we moved left or right
	AbsTime		lastMoveTime;

	
	TimeDiff	fServertime; // only for CServerConnection: when a wormupdate arrives, the servertime of client (>=beta8)
	
	AbsTime		fLastCarve;
	AbsTime		fLastShoot;
	bool		bTouchscreenWeaponCycle;

	Version		cClientVersion;

	AbsTime		fTimeofDeath;
	ATTR(CWorm, int32_t /*DIR_TYPE*/,	iFaceDirectionSide, 24, {serverside = false;})
	ATTR(CWorm, int32_t /*DIR_TYPE*/,	iMoveDirectionSide, 25, {serverside = false;})
	bool		bGotTarget;
	ATTR(CWorm, float,	fAngle, 26, {serverside = false; serverCanUpdate = false;})
    float       fAngleSpeed;
    float		fMoveSpeedX;
	
	ATTR(CWorm,	float,	fSpeedFactor, 30, { defaultValue = 1.0f; })
	ATTR(CWorm, bool,	bCanUseNinja, 31, { defaultValue = true; })
	ATTR(CWorm, float,	fDamageFactor, 32, { defaultValue = 1.0f; })
	ATTR(CWorm, float,	fShieldFactor, 33, { defaultValue = 1.0f; })
	ATTR(CWorm, bool,	bCanAirJump, 34, { defaultValue = false; }) // For instant air jump
	
	AbsTime		fLastAirJumpTime; // For relative air-jump
	float		fFrame;
public:
	ATTR(CWorm,	CNinjaRope,	cNinjaRope, 40, {serverside = false;})
protected:
	ATTR(CWorm, List<bool>, bVisibleForWorm, 41, {})

	ATTR(CWorm, bool,	bTagIT, 42, {})
	TimeDiff	fTagTime;

	ATTR(CWorm, int32_t,	iDirtCount, 43, {})

	AbsTime		fLastBlood;


	// server
	worm_state_t tLastState; // Used for checking if we need to send the packet
	float		fLastAngle;
	AbsTime		fLastUpdateWritten;
	CVec		vLastUpdatedPos; // last pos we have send to client
		// client
	AbsTime		fLastPosUpdate;  // Used for velocity calculations (client does not send velocity)
	CVec		vOldPosOfLastPaket;
	CVec		vPreOldPosOfLastPaket;
	AbsTime		fPreLastPosUpdate;
	CVec		vLastEstimatedVel;
	CVec		vPreLastEstimatedVel;
	int			iLastCharge;
	int			iLastCurWeapon;
	std::string	sLastWeaponHitBy;
	
	AbsTime		fSpawnTime;
	ATTR(CWorm, bool,	bLobbyReady, 45, {serverside = false;}) // Lobby Ready state

	// Graphics
	struct SkinDynDrawer; SkinDynDrawer* skinPreviewDrawerP;
	SmartPointer<DynDrawIntf> skinPreviewDrawer;
	SmartPointer<SDL_Surface> bmpGibs;
	DeprecatedGUI::CBar		cHealthBar;


    // Force the showing of the current weapon
    bool        bForceWeapon_Name;
    AbsTime       fForceWeapon_Time;

	bool		bDrawMuzzle;

	EntityEffect cSparkles;

	// Score
	ATTR(CWorm,	int, iKills, 50, {serverside=true;})
	ATTR(CWorm, int, iDeaths, 51, {serverside=true;})
	ATTR(CWorm,	int, iSuicides, 52, {serverside=true;})
	ATTR(CWorm,	int, iTeamkills, 53, {serverside=true;})
	ATTR(CWorm,	float, fDamage, 54, {serverside=true;})

	ATTR(CWorm, int, iTotalWins, 55, {serverside=true;})
	ATTR(CWorm, int, iTotalLosses, 56, {serverside=true;})
	ATTR(CWorm, int, iTotalKills, 57, {serverside=true;})
	ATTR(CWorm, int, iTotalDeaths, 58, {serverside=true;})
	ATTR(CWorm, int, iTotalSuicides, 59, {serverside=true;})

	ATTR(CWorm,	int,	iAFK,	100, {})
	ATTR(CWorm, std::string,	sAFKMessage, 101, {})

public:
	// Used to print damage numbers over the worm head
	struct DamageReport {		
		float damage;
		AbsTime lastTime;
		DamageReport(): damage(0.0f), lastTime(AbsTime()) {}
	};

private:
	std::map< int, DamageReport> cDamageReport; // WormID can be -1, then this is projectile generated by gamemode or level script

public:
	// Methods

	// TODO: move this out here (to network engine)
	//
	// Network
	//
	void		writeInfo(CBytestream *bs);
	void		updateCheckVariables();
	bool		checkPacketNeeded();
	void		writePacket(CBytestream *bs, bool fromServer, CServerConnection* receiver);
	void		readPacket(CBytestream *bs);
	void		net_updatePos(const CVec& newpos);
	bool		skipPacket(CBytestream *bs);
	void		readPacketState(CBytestream *bs);
	static bool	skipPacketState(CBytestream *bs);
	void		writeWeapons(CBytestream *bs);
	void		readWeapons(CBytestream *bs);
	static bool	skipWeapons(CBytestream *bs)  { return bs->Skip(5); } // 5 weapons
	void		writeStatUpdate(CBytestream *bs);
	void		updateStatCheckVariables();
	bool		checkStatePacketNeeded();
	void		readStatUpdate(CBytestream *bs);
	static bool	skipStatUpdate(CBytestream *bs) { return bs->Skip(2); } // Current weapon and charge
	int			GetMyPing();

	
	void		setupLobby();
	void		loadDetails();
	void		saveDetails();



	// Weapon
	void		GetRandomWeapons();
	void		CloneWeaponsFrom(CWorm* w);
	static void	onWeaponsReadyUpdate(BaseObject* obj, const AttrDesc* attrDesc, ScriptVar_t old);

	//
	// Graphics
	//
	bool		ChangeGraphics();
	void		FreeGraphics();
	SmartPointer<SDL_Surface> ColorizeImage(const std::string& filename) const;
	void		Draw(SDL_Surface * bmpDest, CViewport *v);
    void        DrawShadow(SDL_Surface * bmpDest, CViewport *v);
	void		UpdateDrawPos();

	IVec size() const { return IVec(4,4); }
	Color renderColorAt(/* relative coordinates */ int x, int y) const;
	
	//
	// Game
	//
	void		Prepare(); // weapon selection and so on
	void		Unprepare(); // after a game
	void		StartGame();
	void		Spawn(CVec position);
	void		Kill(bool serverside);
	bool		CheckBonusCollision(CBonus *b);
	bool		GiveBonus(CBonus *b);
	void		Hide(int forworm);
	void		Show(int forworm);


	void		getInput();
	void		clearInput();
	void		initWeaponSelection();
	void		doWeaponSelectionFrame(SDL_Surface * bmpDest, CViewport *v);

	void		setAiDiff(int aiDif);
	
	//
	// Misc.
	//
	bool		CanType();

	bool		isLocalHostWorm();
	bool		isFirstLocalHostWorm();
	bool		shouldDoOwnWeaponSelection();
	
	
    int         traceLine(CVec target, float *fDist, int *nType, int divs = 5);
	int			traceLine(CVec target, CVec start, int *nType, int divs = 5, uchar checkflag = PX_EMPTY);
	
    bool		CheckOnGround();
	

	//
	// Variables
	//

	const CNinjaRope*	getNinjaRope()				{ return &cNinjaRope.get(); }

	std::string getName()			{ return sName; }
	void		setName(const std::string& val) { sName = val; }
	Color		getGameColour() const;
	void		setColour(Color c);

	void		setLocal(bool _l)			{ bLocal = _l; }
	bool		getLocal() const			{ return bLocal; }

	bool		isPrepared() const { return bPrepared; }

	void		setSpawnedOnce()			{ bSpawnedOnce = true; }
	bool		haveSpawnedOnce()			{ return bSpawnedOnce; }
	
	int			getLives()				{ return iLives; }
	void		setLives(int l)				{ iLives = l; }

	float		getDamage()				{ return fDamage; }
	void		setDamage(float l)		{ fDamage = l; }
	void		addDamage(float damage, CWorm* victim, bool serverside);

	int			getKills() const		{ return iKills; }
    void        setKills(int k)			{ iKills = k; }
    void        addKill()				{ iKills++; }

	int			getScore() const;		// Not same as getKills, takes into account suicides and deaths

	int			getDeaths() const		{ return iDeaths; }
	void		setDeaths(int d)		{ iDeaths = d; }
	void		addDeath();

	int			getSuicides() const		{ return iSuicides; }
	void		setSuicides(int d)		{ iSuicides = d; }
	void		addSuicide();

	int			getTeamkills() const	{ return iTeamkills; }
	void		setTeamkills(int d)		{ iTeamkills = d; }
	void		addTeamkill();

	void		setID(int i)				{ iID = i; }
	int			getID()	const			{ return iID; }

	WormType*	getType()	const		{ return m_type; }
    void        setType(WormType* t)        { m_type = t; }

	const SmartPointer<profile_t>& getProfile() const;
	void setProfile(const SmartPointer<profile_t>& p);
	
	bool		getAlive() const		{ return bAlive; }

	AbsTime		getTimeofDeath()		{ return fTimeofDeath; }

	void		setClient(CServerConnection *cl)		{ cOwner = cl; }
	CServerConnection     *getClient()    const     { return cOwner; }

	bool		isOnGround()				{ return bOnGround; }
	void		setOnGround(bool g)			{ bOnGround = g; }

	bool		hasOwnServerTime();
	TimeDiff	serverTime()				{ return fServertime; }

	bool		isVisibleForWorm(int worm) const;
	void		setVisibleForWorm(int worm, bool visibility);
	bool		isVisibleForEverybody() const;
	bool		isVisible(const CViewport* v) const;
	bool		isVisible(CWorm* viewerWorm) const;
	
	float		getAngle()	const			{ return fAngle; }
	void		setAngle(float a)			{ fAngle = a; }
	void		resetAngleAndDir();
	DIR_TYPE	getFaceDirectionSide() const		{ return (DIR_TYPE)(int)iFaceDirectionSide; }
	void		setFaceDirectionSide(DIR_TYPE d)	{ iFaceDirectionSide = (int)d; }
	CVec		getFaceDirection() const {
		return CVec(cosf(getAngle() * ((float)PI/180)) * ((iFaceDirectionSide == DIR_LEFT) ? -1.0f : 1.0f),
					sinf(getAngle() * ((float)PI/180)) ); }
	DIR_TYPE	getMoveDirectionSide()				{ return (DIR_TYPE)(int)iMoveDirectionSide; }
	CVec		getMoveDirection() const {
		return CVec(cosf(getAngle() * ((float)PI/180)) * ((iMoveDirectionSide == DIR_LEFT) ? -1.0f : 1.0f),
					sinf(getAngle() * ((float)PI/180)) ); }

	void		setCanUseNinja(bool b) { bCanUseNinja = b; }
	bool		canUseNinja() const { return bCanUseNinja; }
	void		setSpeedFactor(float f) { fSpeedFactor = f; }
	float		speedFactor() const { return fSpeedFactor; }
	void		setDamageFactor(float f) { fDamageFactor = f; }
	float		damageFactor() const { return fDamageFactor; }
	void		setShieldFactor(float f) { fShieldFactor = f; }
	float		shieldFactor() const { return fShieldFactor; } 
	void		setCanAirJump(bool b) { bCanAirJump = b; }
	bool		canAirJump() const { return bCanAirJump; }
	void		setLastAirJumpTime(AbsTime t) { fLastAirJumpTime = t; }
	AbsTime		getLastAirJumpTime() { return fLastAirJumpTime; }
	void		setLastWeaponHitBy(std::string weapon) { sLastWeaponHitBy = weapon; }
	std::string	getLastWeaponHitBy() { return sLastWeaponHitBy; }
	
	void		setDrawMuzzle(bool _d)		{ bDrawMuzzle = _d; }

	const wpnslot_t	*getCurWeapon()	const	{
		if(tWeapons.size() == 0) {
			static wpnslot_t dummy;
			return &dummy; // it's a disabled wpn slot, thus a good fallback to return here
		}
		return &tWeapons[MIN(tWeapons.size()-1, (size_t)iCurrentWeapon)];
	}
	wpnslot_t	*writeCurWeapon()	{
		assert(tWeapons.size() > 0);
		return &weaponSlots.write()[MIN(tWeapons.size()-1, (size_t)iCurrentWeapon)];
	}
	int			getCurrentWeapon()		{
		if(tWeapons.size() == 0) return 0;
		return (int)MIN(tWeapons.size() - 1, (size_t)iCurrentWeapon);
	}
	void		setCurrentWeapon(int _w)	{ iCurrentWeapon = _w; }
	const wpnslot_t	*getWeapon(int id)		{
		if(id < 0 || (size_t)id >= tWeapons.size()) {
			static wpnslot_t dummy;
			return &dummy; // it's a disabled wpn slot, thus a good fallback to return here
		}
		return &tWeapons[id];
	}

	std::string getCurWeaponName() const;
	int getWeaponSlotsCount() const;

	void		setLobbyReady(bool _g)		{ bLobbyReady = _g; }
	bool		getLobbyReady() const		{ return bLobbyReady; }

	void		setTeam(int _t)				{ iTeam = _t; }
	int			getTeam() const				{ return iTeam; }

	SmartPointer<SDL_Surface> getGibimg()			{ return bmpGibs; }
	SmartPointer<DynDrawIntf> getPicimg()			{ return skinPreviewDrawer; }

	bool		getTagIT()				{ return bTagIT; }
	void		setTagIT(bool _t);

	AbsTime		getLastBlood()				{ return fLastBlood; }
	void		setLastBlood(const AbsTime& b)		{ fLastBlood = b; }
	EntityEffect * getSparklesEffect()		{ return &cSparkles; }

    void        incrementDirtCount(int d);
    int         getDirtCount()          { return iDirtCount; }

	void		setTarget(bool _t)			{ bGotTarget = _t; }

	TimeDiff	getTagTime()			{ return fTagTime; }
	void		setTagTime(const TimeDiff& _t)		{ fTagTime = _t; }
	void		incrementTagTime(const TimeDiff& dt)	{ fTagTime+=dt; }

	const CGameSkin&	getSkin()			{ return cSkin.get(); }
	CGameSkin&	writeSkin()					{ return cSkin.write(); }
	void		setSkin(const CGameSkin& skin)	{ cSkin = skin; }
	void		setSkin(const std::string& skin)	{ cSkin.write().Change(skin); }

	bool		isShooting()				{ return tState.get().bShoot; }
	bool		isWeaponReloading()			{ return getCurWeapon()->Reloading; }

	bool		isSpectating()				{ return bSpectating; }
	void		setSpectating(bool _s)		{ bSpectating = _s; }

	AFK_TYPE	getAFK()				{ return (AFK_TYPE)(int)iAFK; }
	std::string getAFKMessage()		{ return sAFKMessage; }
	void		setAFK(AFK_TYPE _f, const std::string & msg);

	void	addTotalWins(int _w = 1)		{ iTotalWins += _w; }
	int		getTotalWins()				{ return iTotalWins; }
	void	addTotalLosses(int _l = 1)		{ iTotalLosses += _l; }
	int		getTotalLosses()			{ return iTotalLosses; }
	void	addTotalKills(int _k = 1)		{ iTotalKills += _k; }
	int		getTotalKills()				{ return iTotalKills; }
	void	addTotalDeaths(int _d = 1)		{ iTotalDeaths += _d; }
	int		getTotalDeaths()			{ return iTotalDeaths; }
	void	addTotalSuicides(int _d = 1)	{ iTotalSuicides += _d; }
	int		getTotalSuicides()			{ return iTotalSuicides; }
	
	const Version & getClientVersion();
	void	setClientVersion(const Version & v);


	float&		frame()						{ return fFrame; }
	
	CWormInputHandler* inputHandler() { return m_inputHandler; }
	void reinitInputHandler();
	
	std::map< int, DamageReport> & getDamageReport() { return cDamageReport; }
	
	/* Keeps track over the positions over time.
	 This is used for LX56 physics to have better damage calculation. */
	VecTimeRecorder posRecordings;
	
	// --------------------------------------------------
	// --------------------- Gusanos --------------------
	
	
public:
	
	enum Actions
	{
		MOVELEFT,
		MOVERIGHT,
		//AIMUP,
		//AIMDOWN,
		FIRE,
		FIRE2,
		JUMP,
		DIG,
		NINJAROPE,
		CHANGEWEAPON,
		RESPAWN
	};
	
	enum Direction
	{
		Down = 0,
		Left,
		Up,
		Right,
		
		DirMax
	};
	
	static LuaReference metaTable;
	virtual LuaReference getMetaTable() const { return metaTable; }

	void draw(CViewport* viewport);
	
	void calculateReactionForce(VectorD2<long> origin, Direction dir);
	void calculateAllReactionForces(VectorD2<float>& nextPos, VectorD2<long>& inextPos);
	void processMoveAndDig(void);
	void processPhysics();
	void processJumpingAndNinjaropeControls();
	
	virtual void think();
	void actionStart( Actions action );
	void actionStop( Actions action );
	void addRopeLength(float distance);
	
#ifndef DEDICATED_ONLY
	Vec getRenderPos();
#endif
	
	bool isChangingWpn();
	
	virtual void damage( float amount, CWormInputHandler* damager );
	
	// This are virtual so that NetWorm can know about them and tell others over the network.
	virtual void respawn();
	
	virtual void dig();
	void dig(const Vec& digPos, Angle angle);
	
	virtual void die();
	void base_die();
	virtual void changeWeaponTo( unsigned int weapIndex );
	
	virtual void setWeapon(size_t index, WeaponType* type );
	void base_setWeapon(size_t index, WeaponType* type );
	virtual void setWeapons( std::vector<WeaponType*> const& weaps);
	virtual void clearWeapons();
	void base_clearWeapons();
	
	Weapon* getCurrentWeaponRef();
	const std::vector<Weapon*> getWeapons() const { return m_weapons; }
	std::vector<WeaponType*> getWeaponTypes() const;

	// getWeaponIndexOffset can be used to get the currentWeapon index or
	//to get the one to the right or the left or the one 1000 units to the
	//right ( it will wrap the value so that its always inside the worm's weapons size )
	int getWeaponIndexOffset( int offset );
	Angle getPointingAngle();
	void setDir(int d); // Only use this if you are going to sync it over netplay with an event
	int getDir()
	{
		return (iMoveDirectionSide == DIR_LEFT) ? -1 : 1;
	}
	bool isCollidingWith( const Vec& point, float radius );
	bool isActive();
	void removeRefsToPlayer( CWormInputHandler* player );
	
#ifndef DEDICATED_ONLY
	void showFirecone( SpriteSet* sprite, int frames, float distance );
#endif
		
	void setAimAngle(Angle a) { fAngle = (float)a.toDeg() - 90.f; }
	Angle getAimAngle() const { return Angle(fAngle + 90.f); }

	void sendWeaponMessage( int index, BitStream* data, Net_U8 repRules = Net_REPRULE_AUTH_2_ALL );
	
	virtual void finalize();
	
	void sendLuaEvent(LuaEventDef* event, eNet_SendMode mode, Net_U8 rules, BitStream* userdata, Net_ConnID connID);
		
protected:
	//LuaReference luaReference;
	
#ifndef DEDICATED_ONLY
	
	Vec renderPos;
#endif
	
	int reacts[DirMax];
	
	float aimRecoilSpeed;
	
#ifndef DEDICATED_ONLY
	
	int m_fireconeTime;
	float m_fireconeDistance;
#endif
	
	int m_timeSinceDeath; // Used for the min and max respawn time sv variables
		
	std::vector<Weapon*> m_weapons;
	
	CWormInputHandler* m_lastHurt;
	
#ifndef DEDICATED_ONLY
	
	SpriteSet *skin;
	SpriteSet *skinMask;
	SpriteSet *m_currentFirecone;
	BaseAnimator* m_fireconeAnimator;
	
	BaseAnimator* m_animator;
#endif
	// Smaller vars last to improve alignment and/or decrease structure size
	//bool m_isActive; -> alive
	bool movingLeft;
	bool movingRight;
	bool jumping;
	bool animate;
	bool changing; // This shouldnt be in the worm class ( its player stuff >:O )
	
	
	// ----------------------------------
	// ---------- Gusanos NetWorm -----------------
	// -------------------------------------
	
public:
	friend class NetWormInterceptor;
	
	enum NetEvents
	{
		Dig,
		ChangeWeapon,
		WeaponMessage,
		SetWeapon,
		ClearWeapons,
		SYNC,
		LuaEvent,
		EVENT_COUNT,
	};
	
	enum ReplicationItems
	{
		PlayerID = 0,
		Position,
		AIM
	};
	
	static Net_ClassID  classID;
	static const float MAX_ERROR_RADIUS;
	
	void NetWorm_Init();
	void NetWorm_Shutdown();
	
	void NetWorm_think();
	
	void sendSyncMessage( Net_ConnID id );
	
	eNet_NodeRole getRole()
	{
		if ( m_node )
		{
			return m_node->getRole();
		}else
			return eNet_RoleUndefined;
	}
		
	Net_NodeID getNodeID();
	Net_Node* getNode() { return m_node; }
	
	Vec lastPosUpdate;
	int timeSinceLastUpdate;
	
	bool gusSkinVisble;
	
private:
	
	void addEvent(BitStream* data, NetEvents event);
	
	Net_Node *m_node;
	NetWormInterceptor* m_interceptor;
	
};


int traceWormLine(CVec target, CVec start, CVec* collision = NULL);

#endif  //  __CWORM_H__
