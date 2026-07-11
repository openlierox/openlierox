/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Shoot list class
// Created 20/12/02
// Jason Boettcher


#ifndef __CSHOOTLIST_H__
#define __CSHOOTLIST_H__


class CBytestream;
class CWorm;
struct Version;


// Shoot Main flags
#define		SMF_LARGEANGLE	0x01
#define		SMF_LARGESPEED	0x02
#define		SMF_NEGSPEED	0x04

// Shoot Offsets
#define		SHF_TIMEOFF		0x01
#define		SHF_XPOSOFF		0x02
#define		SHF_YPOSOFF		0x04
#define		SHF_ANGLEOFF	0x08
#define		SHF_SPEEDOFF	0x10
#define		SHF_NG_XPOSOFF	0x20
#define		SHF_NG_YPOSOFF	0x40
#define		SHF_EXTRAFLAGS	0x80

// Shoot Extra offsets
#define		SHF_NG_ANGLEOFF	0x01
#define		SHF_NG_SPEEDOFF 0x02
#define		SHF_LARGEXOFF	0x04
#define		SHF_LARGEYOFF	0x08
#define		SHF_XWRMVEL		0x10
#define		SHF_YWRMVEL		0x20
#define		SHF_NG_XWRMVEL	0x40
#define		SHF_NG_YWRMVEL	0x80


#define		MAX_SHOOTINGS	256


// Weapon Shooting structure
struct shoot_t {
	TimeDiff	fTime;
	int		nWeapon;
	CVec	cPos;
	CVec	cWormVel;
	int		nAngle;
	int		nRandom;
	int		nSpeed;
	int		nWormID;

	int		devID;
	bool	release;
	
	AbsTime spawnTime();
};



// Shoot list class
class CShootList {
public:
	// Constructor
	CShootList() {
		m_nNumShootings = 0;
		m_psShoot = NULL;
		m_fStartTime = AbsTime();
		m_fLastWrite = AbsTime();
	}

	// Destructor
	~CShootList() {
		Shutdown();
	}


private:
	// Attributes

	int			m_nNumShootings;
	shoot_t		*m_psShoot;
	AbsTime		m_fStartTime;
	AbsTime		m_fLastWrite;



public:
	// Methods

	bool		Initialize();
	void		Shutdown();

	bool		addShoot(int weaponID, TimeDiff serverTime, float fSpeed, int nAngle, CWorm *pcWorm, bool release);

	bool		writePacket(CBytestream *bs, const Version& receiverVer);
	
private:
	void		writeSingle(CBytestream *bs, const Version& receiverVer, int index);
	void		writeMulti(CBytestream *bs, const Version& receiverVer, int index);
	void		writeSmallShot(shoot_t *psFirst, CBytestream *bs, const Version& receiverVer, int index);

public:
	void		readSingle(CBytestream *bs, const Version& senderVer, int max_weapon_id);
	static bool skipSingle(CBytestream *bs, const Version& senderVer);
	void		readMulti(CBytestream *bs, const Version& senderVer, int max_weapon_id);
	static bool skipMulti(CBytestream *bs, const Version& senderVer);
	void		readSmallShot(shoot_t *psFirst, CBytestream *bs, const Version& senderVer, int index);
	static bool	skipSmallShot(CBytestream *bs, const Version& senderVer);

	void		Clear();

	shoot_t		*getShot(int index);

	int			getNumShots()			{ return m_nNumShootings; }
	AbsTime		getStartTime()			{ return m_fStartTime; }
	AbsTime		getLastWrite()			{ return m_fLastWrite; }

	friend void test_CShootListSpeedRoundtrip();  // unit test: writeSingle/readSingle
};




#endif  //  __CSHOOTLIST_H__
