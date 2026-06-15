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


#include <assert.h>

#include "LieroX.h"
#include "CShootList.h"
#include "game/CWorm.h"
#include "Protocol.h"
#include "MathLib.h"
#include "WeaponDesc.h"


///////////////////
// Initialize the list
bool CShootList::Initialize()
{
	m_nNumShootings = 0;
	m_fStartTime = AbsTime();

	// TODO: use std::queue or std::list
	m_psShoot = new shoot_t[MAX_SHOOTINGS];
	if( m_psShoot == NULL )
		return false;

	return true;
}


///////////////////
// Shutdown the list
void CShootList::Shutdown()
{
	if( m_psShoot ) {
		delete[] m_psShoot;
		m_psShoot = NULL;
	}

	m_nNumShootings = 0;
	m_fStartTime = AbsTime();
}


///////////////////
// Clear the list
void CShootList::Clear()
{
	m_nNumShootings = 0;
	m_fStartTime = AbsTime();
}


///////////////////
// Return a shot
shoot_t *CShootList::getShot( int index )
{
	if( index >= 0 && index < m_nNumShootings )
		return &m_psShoot[index];

	return NULL;
}


///////////////////
// Add a shot to the list
// (done on server-side from GameServer::WormShoot)
bool CShootList::addShoot( int weaponID, TimeDiff fTime, float fSpeed, int nAngle, CWorm *pcWorm, bool release )
{
	assert( pcWorm );
	assert( m_psShoot );

	// Check if it isn't too full
	if( m_nNumShootings >= MAX_SHOOTINGS )
		return false;

	// If this is the first shot in the list, set the start time
	if( m_fStartTime == AbsTime())
		m_fStartTime = tLX->currentTime;

	shoot_t *psShot = &m_psShoot[ m_nNumShootings++ ];

	// Fill in the info
	psShot->cPos = pcWorm->getPos();
	psShot->cWormVel = gameSettings[FT_ProjRelativeVel] ? pcWorm->getVelocity() : CVec(0,0);
	psShot->fTime = fTime;
	psShot->nAngle = nAngle;
	psShot->nRandom = GetRandomInt(255);
	psShot->nSpeed = gameSettings[FT_ProjRelativeVel] ? (int)( fSpeed*100 ) : 0;
	psShot->nWeapon = weaponID;
	psShot->nWormID = pcWorm->getID();
	psShot->release = release;
	
	// Debuging
	psShot->devID = 0;
	if(m_nNumShootings > 1)
		psShot->devID = m_psShoot[m_nNumShootings-2].devID+1;

	return true;
}


///////////////////
// Write the shoot list to a bytestream
// Returns true if good (even if we didn't write anything)
// Returns false if the packet couldn't be written (overflow)
bool CShootList::writePacket( CBytestream *bs, const Version& receiverVer )
{
	CBytestream strm;

	// If no shots are in the list, just leave
	if( m_nNumShootings == 0)
		return true;

	// Single shot
	if( m_nNumShootings == 1 )
		writeSingle( &strm, receiverVer, 0 );
	else
		// Multiple shots
		writeMulti( &strm, receiverVer, 0 );


	// Otherwise, append the shootlist onto the bytestream
	// Don't append for now
	bs->Append( &strm );
	m_fLastWrite = tLX->currentTime;

	return true;
}


///////////////////
// A single shot packet
void CShootList::writeSingle( CBytestream *bs, const Version& receiverVer, int index )
{
	byte	flags = 0;
	shoot_t *psShot = &m_psShoot[index];

	// Set the flags
	if( psShot->nAngle > 255 )
		flags |= SMF_LARGEANGLE;			// Large angle
	if( psShot->nSpeed > 255 || psShot->nSpeed < -255)
		flags |= SMF_LARGESPEED;			// Large speed
	if( psShot->nSpeed < 0 )
		flags |= SMF_NEGSPEED;				// Negative speed


	short x = (short)psShot->cPos.x;
	short y = (short)psShot->cPos.y;
	short vx = (short)psShot->cWormVel.x;
	short vy = (short)psShot->cWormVel.y;

	// Write the packet
	bs->writeByte( S2C_SINGLESHOOT );
	bs->writeByte( flags );
	bs->writeByte( psShot->nWormID );
	bs->writeFloat( (float)psShot->fTime.seconds() );
	bs->writeByte( psShot->nWeapon );
	bs->write2Int12( x, y );
	bs->writeInt16( vx );
	bs->writeInt16( vy );
	bs->writeByte( psShot->nRandom );

	if( flags & SMF_LARGEANGLE )
		bs->writeByte( psShot->nAngle-255 );
	else
		bs->writeByte( psShot->nAngle );

	if( flags & SMF_LARGESPEED )
		bs->writeByte( psShot->nSpeed-255 );
	else if( flags & SMF_NEGSPEED ) {
		if(flags & SMF_LARGESPEED)
			bs->writeByte( (-psShot->nSpeed)-255 );
		else
			bs->writeByte( -psShot->nSpeed );
	}
	else
		bs->writeByte( psShot->nSpeed );
	
	if(receiverVer >= OLXBetaVersion(0,58,1)) {
		bs->ResetBitPos();
		bs->writeBit(psShot->release);
		bs->SkipRestBits();
	}
}


///////////////////
// A multiple shot packet
void CShootList::writeMulti( CBytestream *bs, const Version& receiverVer, int index )
{
	// If index is over the limit, exit
	if( index >= m_nNumShootings )
		return;

	// If index is the last shot, send a single shot & exit
	if( index == m_nNumShootings-1 ) {
		writeSingle( bs, receiverVer, index );
		return;
	}


	int		num = 1;
	int		wmid = m_psShoot[index].nWormID;
	int		wpid = m_psShoot[index].nWeapon;
	int		i;

	// Calculate how many shots we can/should send in a row
	// (based on worm id, weapon id, angle, time, number, speed & worm velocity)
	for(i=index+1; i<m_nNumShootings; i++) {
		if( m_psShoot[i].nWormID != wmid ||
			m_psShoot[i].nWeapon != wpid ||
			(m_psShoot[i].fTime - m_psShoot[i-1].fTime).milliseconds() > 255 ||
			abs(m_psShoot[i].nAngle - m_psShoot[i-1].nAngle) > 255 ||
			abs(m_psShoot[i].nSpeed - m_psShoot[i-1].nSpeed) > 255 ||
			abs((int)m_psShoot[i].cWormVel.x - (int)m_psShoot[i-1].cWormVel.x) > 255 ||
			abs((int)m_psShoot[i].cWormVel.y - (int)m_psShoot[i-1].cWormVel.y) > 255 ||
			num > 255) {

			break;
		}

		num++;
	}


	// If we can only send 1, send the 'single' packet
	if( num == 1 ) {
		writeSingle(bs, receiverVer, index);

		// If we could only send one shot, but we still have more shots to send, send them
		if( m_nNumShootings > 1)
			writeMulti(bs, receiverVer, index+1);
		return;
	}


	// Write out the header
	byte	flags = 0;
	shoot_t *psShot = &m_psShoot[index];

	// Set the flags
	if( psShot->nAngle > 255 )
		flags |= SMF_LARGEANGLE;			// Large angle
	if( psShot->nSpeed > 255 || psShot->nSpeed < -255)
		flags |= SMF_LARGESPEED;			// Large speed
	if( psShot->nSpeed < 0 )
		flags |= SMF_NEGSPEED;				// Negative speed


	short x = (short)psShot->cPos.x;
	short y = (short)psShot->cPos.y;
	short vx = (short)psShot->cWormVel.x;
	short vy = (short)psShot->cWormVel.y;

	// Write the packet
	bs->writeByte( S2C_MULTISHOOT );
	bs->writeByte( flags );
	bs->writeByte( psShot->nWormID );
	bs->writeFloat( (float)psShot->fTime.seconds() );
	bs->writeByte( psShot->nWeapon );
	bs->writeByte( num );
	bs->write2Int12( x, y );
	bs->writeInt16( vx );
	bs->writeInt16( vy );
	bs->writeByte( psShot->nRandom );

	if( flags & SMF_LARGEANGLE )
		bs->writeByte( psShot->nAngle-255 );
	else
		bs->writeByte( psShot->nAngle );

	int speed = psShot->nSpeed;

	if( flags & SMF_LARGESPEED )
		speed = psShot->nSpeed-255;
	if( flags & SMF_NEGSPEED ) {
		if(flags & SMF_LARGESPEED)
			speed = (-psShot->nSpeed)-255;
		else
			speed = -psShot->nSpeed;
	}

	bs->writeByte( speed );
	
	if(receiverVer >= OLXBetaVersion(0,58,1)) {
		bs->ResetBitPos();
		bs->writeBit(psShot->release);
		bs->SkipRestBits();
	}
	
	// Send out the following shots
	for(i=1; i<num; i++)
		writeSmallShot( &m_psShoot[index+i-1], bs, receiverVer, index+i );

	// Continue writing out multiple packets until all shots are written
	writeMulti( bs, receiverVer, index+num );
}


///////////////////
// Write a small shot out
void CShootList::writeSmallShot( shoot_t *psFirst, CBytestream *bs, const Version& receiverVer, int index )
{
	byte	flags = 0;
	byte	extraflags = 0;

	assert( index < m_nNumShootings );

	shoot_t	*psShot = &m_psShoot[index];


	int xoff = (int)( psShot->cPos.x - psFirst->cPos.x );
	int yoff = (int)( psShot->cPos.x - psFirst->cPos.x );
	xoff = abs(xoff);
	yoff = abs(yoff);

	// Set the flags
	if( psShot->fTime != psFirst->fTime )
		flags |= SHF_TIMEOFF;
	if( (int)psShot->cPos.x != (int)psFirst->cPos.x )
		flags |= SHF_XPOSOFF;
	if( (int)psShot->cPos.y != (int)psFirst->cPos.y )
		flags |= SHF_YPOSOFF;
	if( psShot->nAngle != psFirst->nAngle )
		flags |= SHF_ANGLEOFF;
	if( psShot->nSpeed != psFirst->nSpeed )
		flags |= SHF_SPEEDOFF;
	if( (int)psShot->cPos.x < (int)psFirst->cPos.x ) {
		flags |= SHF_NG_XPOSOFF;
		flags &= ~SHF_XPOSOFF;
	}
	if( (int)psShot->cPos.y < (int)psFirst->cPos.y ) {
		flags |= SHF_NG_YPOSOFF;
		flags &= ~SHF_YPOSOFF;
	}

	// Extra flags
	if( psShot->nAngle < psFirst->nAngle ) {
		extraflags |= SHF_NG_ANGLEOFF;
		flags &= ~SHF_ANGLEOFF;
	}
	if( psShot->nSpeed < psFirst->nSpeed ) {
		extraflags |= SHF_NG_SPEEDOFF;
		flags &= ~SHF_SPEEDOFF;
	}
	if( xoff >= 32 )
		extraflags |= SHF_LARGEXOFF;
	if( yoff >= 32 )
		extraflags |= SHF_LARGEYOFF;

	if( (int)psShot->cWormVel.x != (int)psFirst->cWormVel.x )
		flags |= SHF_XWRMVEL;
	if( (int)psShot->cWormVel.y != (int)psFirst->cWormVel.y )
		flags |= SHF_YWRMVEL;
	if( (int)psShot->cWormVel.x < (int)psFirst->cWormVel.x ) {
		extraflags &= ~SHF_XWRMVEL;
		extraflags |= SHF_NG_XWRMVEL;
	}
	if( (int)psShot->cWormVel.y < (int)psFirst->cWormVel.y ) {
		extraflags &= ~SHF_YWRMVEL;
		extraflags |= SHF_NG_YWRMVEL;
	}


	if( extraflags )
		flags |= SHF_EXTRAFLAGS;


	// For now, do some asserts for data that can't be written in the space
	assert( abs( psShot->nAngle - psFirst->nAngle ) <= 255 );
	//assert( abs( psShot->nSpeed - psFirst->nSpeed ) <= 255 );


	// Write the data
	bs->writeByte( flags );
	if( flags & SHF_EXTRAFLAGS )
		bs->writeByte( extraflags );
	if( flags & SHF_TIMEOFF )
		bs->writeByte( (int)( (psShot->fTime - psFirst->fTime).milliseconds()) );

	// X offset
	if( flags & SHF_XPOSOFF || flags & SHF_NG_XPOSOFF ) {
		if( extraflags & SHF_LARGEXOFF )
			bs->writeByte( xoff );
	}
	// Y offset
	if( flags & SHF_YPOSOFF || flags & SHF_NG_YPOSOFF ) {
		if( extraflags & SHF_LARGEYOFF )
			bs->writeByte( yoff );
	}

	// X & Y Small offset
	if( (flags & SHF_XPOSOFF || flags & SHF_NG_XPOSOFF) && (flags & SHF_YPOSOFF || flags & SHF_NG_YPOSOFF) ) {
		if( !(extraflags & SHF_LARGEXOFF) && !(extraflags & SHF_LARGEYOFF) )
			bs->write2Int4( xoff, yoff );
	}

	if( flags & SHF_ANGLEOFF )
		bs->writeByte( psShot->nAngle - psFirst->nAngle );
	if( flags & SHF_SPEEDOFF )
		bs->writeByte( psShot->nSpeed - psFirst->nSpeed );
	if( extraflags & SHF_NG_ANGLEOFF )
		bs->writeByte( psFirst->nAngle - psShot->nAngle );
	if( extraflags & SHF_NG_SPEEDOFF )
		bs->writeByte( psFirst->nSpeed - psShot->nSpeed );

	// Worm velocity
	if( extraflags & SHF_XWRMVEL )
		bs->writeByte( (int)psShot->cWormVel.x - (int)psFirst->cWormVel.x );
	if( extraflags & SHF_YWRMVEL )
		bs->writeByte( (int)psShot->cWormVel.y - (int)psFirst->cWormVel.y );
	if( extraflags & SHF_NG_XWRMVEL )
		bs->writeByte( (int)psFirst->cWormVel.x - (int)psShot->cWormVel.x );
	if( extraflags & SHF_NG_YWRMVEL )
		bs->writeByte( (int)psFirst->cWormVel.y - (int)psShot->cWormVel.y );

	if(receiverVer >= OLXBetaVersion(0,58,1)) {
		bs->ResetBitPos();
		bs->writeBit(psShot->release);
		bs->SkipRestBits();
	}	
}



///////////////////
// Read a single shot packet
void CShootList::readSingle( CBytestream *bs, const Version& senderVer, int max_weapon_id )
{
	// Clear the list
	Clear();

	shoot_t *psShot = &m_psShoot[0];


	// Read the packet
	byte flags = bs->readByte();
	psShot->nWormID = bs->readByte(); // Used for indexing
	psShot->fTime = MAX(bs->readFloat(), 0.0f);
	psShot->nWeapon = CLAMP((int)bs->readByte(), 0, max_weapon_id); // Used for indexing
	short x, y;
	bs->read2Int12( x, y );
	short vx = bs->readInt16();
	short vy = bs->readInt16();
	psShot->nRandom = bs->readByte();

	psShot->nAngle = bs->readByte();
	int speed = bs->readByte();
	psShot->nSpeed = speed;

	if( flags & SMF_LARGEANGLE )
		psShot->nAngle += 255;

	if( flags & SMF_LARGESPEED )
		psShot->nSpeed = speed+255;
	if( flags & SMF_NEGSPEED ) {
		if( flags & SMF_LARGESPEED )
			psShot->nSpeed = -(speed+255);
		else
			psShot->nSpeed = -speed;
	}


	// Convert the pos
	psShot->cPos = CVec( (float)x, (float)y );

	// Convert the velocity
	psShot->cWormVel = CVec( (float)vx, (float)vy );

	if(senderVer >= OLXBetaVersion(0,58,1)) {
		bs->ResetBitPos();
		psShot->release = bs->readBit();
		bs->SkipRestBits();
	}
	else
		psShot->release = false;
	
	m_nNumShootings++;
}


///////////////////
// Read a multi shot packet
void CShootList::readMulti( CBytestream *bs, const Version& senderVer, int max_weapon_id )
{
	// Clear the list
	Clear();

	// Read the header
	byte	flags = 0;
	short	x, y, vx, vy;
	int		i;
	int		num = 0;
	shoot_t *psShot = m_psShoot;

	flags = bs->readByte();
	psShot->nWormID = bs->readByte(); // Used for indexing
	psShot->fTime = MAX(bs->readFloat(), 0.0f);
	psShot->nWeapon = CLAMP((int)bs->readByte(), 0, max_weapon_id);
	num = (int) MIN(bs->readByte(), MAX_SHOOTINGS - 1);
	bs->read2Int12( x, y );
	vx = bs->readInt16();
	vy = bs->readInt16();
	psShot->nRandom = bs->readByte();

	psShot->nAngle = bs->readByte();
	int speed = bs->readByte();
	psShot->nSpeed = speed;

	if( flags & SMF_LARGEANGLE )
		psShot->nAngle += 255;

	if( flags & SMF_LARGESPEED )
		psShot->nSpeed = speed+255;
	if( flags & SMF_NEGSPEED ) {
		if( flags & SMF_LARGESPEED )
			psShot->nSpeed = -(speed+255);
		else
			psShot->nSpeed = -speed;
	}

	// Convert the pos
	psShot->cPos = CVec( (float)x, (float)y );

	// Convert the velocity
	psShot->cWormVel = CVec( (float)vx, (float)vy );

	if(senderVer >= OLXBetaVersion(0,58,1)) {
		bs->ResetBitPos();
		psShot->release = bs->readBit();
		bs->SkipRestBits();
	}
	else
		psShot->release = false;
	
	m_nNumShootings++;

	// Read the following shots
	for(i=1; i<num; i++)
		readSmallShot( &m_psShoot[i-1], bs, senderVer, i );
}


///////////////////
// Read a small shot
void CShootList::readSmallShot( shoot_t *psFirst, CBytestream *bs, const Version& senderVer, int index )
{
	byte	flags = 0;
	byte	extraflags = 0;
	int		x = 0;
	int		y = 0;

	shoot_t	*psShot = &m_psShoot[index];

	// Copy the base data, so we only apply the changes to the shot
	*psShot = *psFirst;


	flags = bs->readByte();
	if( flags & SHF_EXTRAFLAGS )
		extraflags = bs->readByte();

	// AbsTime offset
	if( flags & SHF_TIMEOFF ) {
		float time = (float)bs->readByte();
		psShot->fTime += TimeDiff(time/1000.0f);
	}

	// X Large offset
	if( flags & SHF_XPOSOFF || flags & SHF_NG_XPOSOFF ) {
		if( extraflags & SHF_LARGEXOFF )
			x = bs->readByte();
	}

	// Y Large offset
	if( flags & SHF_YPOSOFF || flags & SHF_NG_YPOSOFF ) {
		if( extraflags & SHF_LARGEYOFF )
			y = bs->readByte();
	}

	// X & Y Small offset
	if( (flags & SHF_XPOSOFF || flags & SHF_NG_XPOSOFF) && (flags & SHF_YPOSOFF || flags & SHF_NG_YPOSOFF) ) {
		if( !(extraflags & SHF_LARGEXOFF) && !(extraflags & SHF_LARGEYOFF) ) {
			short sx, sy;
			bs->read2Int4( sx, sy );
			x = sx; y = sy;
		}
	}

	// Negate the position offsets
	if( flags & SHF_NG_XPOSOFF )
		x = -x;
	if( flags & SHF_NG_YPOSOFF )
		y = -y;


	if( flags & SHF_ANGLEOFF ) {
		int angle = bs->readByte();
		psShot->nAngle += angle;
	}
	if( flags & SHF_SPEEDOFF ) {
		int speed = bs->readByte();
		psShot->nSpeed += speed;
	}

	if( extraflags & SHF_NG_ANGLEOFF ) {
		int angle = bs->readByte();
		psShot->nAngle -= angle;
	}
	if( extraflags & SHF_NG_SPEEDOFF ) {
		int speed = bs->readByte();
		psShot->nSpeed -= speed;
	}

	// Worm velocity
	if( extraflags & SHF_XWRMVEL ) {
		int velx = bs->readByte();
		psShot->cWormVel.x=( psShot->cWormVel.x + (float)velx );
	}
	if( extraflags & SHF_YWRMVEL ) {
		int vely = bs->readByte();
		psShot->cWormVel.y=( psShot->cWormVel.y + (float)vely );
	}
	if( extraflags & SHF_NG_XWRMVEL ) {
		int velx = bs->readByte();
		psShot->cWormVel.x=( psShot->cWormVel.x - (float)velx );
	}
	if( extraflags & SHF_NG_YWRMVEL ) {
		int vely = bs->readByte();
		psShot->cWormVel.y=( psShot->cWormVel.y - (float)vely );
	}

	if(senderVer >= OLXBetaVersion(0,58,1)) {
		bs->ResetBitPos();
		psShot->release = bs->readBit();
		bs->SkipRestBits();
	}
	else
		psShot->release = false;

	m_nNumShootings++;
}


bool CShootList::skipSingle(CBytestream *bs, const Version& senderVer)  {
	if(senderVer >= OLXBetaVersion(0,58,1))
		return bs->Skip(18);
	else
		return bs->Skip(17);
}
bool CShootList::skipMulti(CBytestream *bs, const Version& senderVer)  {
	bs->Skip(7);
	byte num = bs->readByte();
	bs->Skip(10);
	if(senderVer >= OLXBetaVersion(0,58,1)) bs->Skip(1);
	for (byte i = 0; i < num - 1;i++) skipSmallShot(bs, senderVer);
	return bs->isPosAtEnd();
}

////////////////
// Skip the small shot packet
bool CShootList::skipSmallShot(CBytestream *bs, const Version& senderVer)
{
	// For comments see the above function
	byte flags = bs->readByte();
	byte extraflags = 0;
	if (flags & SHF_EXTRAFLAGS)
		extraflags = bs->readByte();

	if (flags & SHF_TIMEOFF)
		bs->Skip(1);

	if( flags & SHF_XPOSOFF || flags & SHF_NG_XPOSOFF )
		if( extraflags & SHF_LARGEXOFF )
			bs->Skip(1);

	if( flags & SHF_YPOSOFF || flags & SHF_NG_YPOSOFF )
		if( extraflags & SHF_LARGEYOFF )
			bs->Skip(1);

	if( (flags & SHF_XPOSOFF || flags & SHF_NG_XPOSOFF) && (flags & SHF_YPOSOFF || flags & SHF_NG_YPOSOFF) )
		if( !(extraflags & SHF_LARGEXOFF) && !(extraflags & SHF_LARGEYOFF) )
			bs->Skip(1);

	if( flags & SHF_ANGLEOFF )
		bs->Skip(1);

	if( flags & SHF_SPEEDOFF )
		bs->Skip(1);

	if( extraflags & SHF_NG_ANGLEOFF )
		bs->Skip(1);

	if( extraflags & SHF_NG_SPEEDOFF )
		bs->Skip(1);

	if( extraflags & SHF_XWRMVEL )
		bs->Skip(1);

	if( extraflags & SHF_YWRMVEL )
		bs->Skip(1);

	if( extraflags & SHF_NG_XWRMVEL )
		bs->Skip(1);

	if( extraflags & SHF_NG_YWRMVEL )
		bs->Skip(1);

	if(senderVer >= OLXBetaVersion(0,58,1)) bs->Skip(1);
	
	return bs->isPosAtEnd();
}

AbsTime shoot_t::spawnTime() {
	// fServerTime is the time we calculated for the server,
	// shot->fTime was the fServerTime of the server when it added the shot
	// HINT: Though we are not using these because these times are not synchronised and sometimes
	// shot->fTime > fServerTime.
	// We are estimating the time with iMyPing. We divide it by 2 as iMyPing represents
	// the time of both ways (ping+pong).
	AbsTime fSpawnTime = tLX->currentTime - TimeDiff(((float)cClient->getMyPing() / 1000.0f) / 2.0f);

	AbsTime time = fSpawnTime;
	// HINT: Since Beta8 though, we have a good synchronisation of fServertime and we can actually use the provided sh->fTime
	if(cClient->getServerVersion() >= OLXBetaVersion(0,57,8))
		if(this->fTime <= game.serverTime()) // just a security check
			time = tLX->currentTime - (game.serverTime() - this->fTime);
	
	return time;
}
