/*
 *  Unit test for CShootList shot encoding.
 */

#include "unittest.h"

// CShootList.h is not self-contained; pull in its types first, as the .cpp does.
#include "olx-types.h"
#include "CVec.h"
#include "Version.h"
#include "CBytestream.h"
#include "CShootList.h"
#include "Protocol.h"  // S2C_SINGLESHOOT, S2C_MULTISHOOT

// Regression for #1115:
// a single shot with a large negative speed (nSpeed < -255)
// sets both SMF_LARGESPEED and SMF_NEGSPEED,
// which writeSingle() once mis-encoded.
// Every speed must round-trip writeSingle() -> readSingle().
void test_CShootListSpeedRoundtrip() {
	// before the release bit; keeps the packet simple
	const Version ver = OLXBetaVersion(0,57,0);
	const int max_weapon_id = 10;

	const int speeds[] = { 0, 50, 255, 400, -100, -255, -300, -510 };
	for(size_t i = 0; i < sizeof(speeds)/sizeof(speeds[0]); ++i) {
		CShootList src;
		CHECK(src.Initialize());
		shoot_t& s = src.m_psShoot[0];
		s.fTime = TimeDiff();
		s.nWeapon = 5;
		s.cPos = CVec(10, 20);
		s.cWormVel = CVec(0, 0);
		s.nAngle = 90;
		s.nRandom = 42;
		s.nSpeed = speeds[i];
		s.nWormID = 3;
		s.release = false;
		src.m_nNumShootings = 1;

		CBytestream bs;
		src.writeSingle(&bs, ver, 0);

		bs.ResetPosToBegin();
		bs.readByte();  // skip the S2C_SINGLESHOOT id

		CShootList dst;
		CHECK(dst.Initialize());
		dst.readSingle(&bs, ver, max_weapon_id);

		CHECK(dst.m_psShoot[0].nSpeed == speeds[i]);
		CHECK(dst.m_psShoot[0].nAngle == 90);
		CHECK(dst.m_psShoot[0].nWormID == 3);
	}
}

// Regression for #1128:
// a run of 256 similar shots overflowed the one-byte run count (256 -> 0),
// so writeMulti() still wrote the small shots
// while readMulti() dropped them and desynced the stream.
// A full shoot list must round-trip.
void test_CShootListMultiRoundtrip() {
	const Version ver = OLXBetaVersion(0,57,0);
	const int max_weapon_id = 10;
	const int N = MAX_SHOOTINGS;  // 256: exactly the run-count overflow

	// All shots identical, so they run-length encode into one run.
	CShootList src;
	CHECK(src.Initialize());
	for(int i = 0; i < N; ++i) {
		shoot_t& s = src.m_psShoot[i];
		s.fTime = TimeDiff();
		s.nWeapon = 5;
		s.cPos = CVec(10, 20);
		s.cWormVel = CVec(0, 0);
		s.nAngle = 90;
		s.nRandom = 0;
		s.nSpeed = 100;
		s.nWormID = 3;
		s.release = false;
	}
	src.m_nNumShootings = N;

	CBytestream bs;
	src.writeMulti(&bs, ver, 0);

	// Read the sub-packets back and count every shot that arrives.
	bs.ResetPosToBegin();
	int got = 0;
	while(!bs.isPosAtEnd()) {
		const byte type = bs.readByte();
		CShootList dst;
		CHECK(dst.Initialize());
		if(type == S2C_SINGLESHOOT)
			dst.readSingle(&bs, ver, max_weapon_id);
		else if(type == S2C_MULTISHOOT)
			dst.readMulti(&bs, ver, max_weapon_id);
		else {
			// A small-shot byte parsed as a packet id: the stream desynced.
			CHECK(type == S2C_SINGLESHOOT || type == S2C_MULTISHOOT);
			break;
		}
		got += dst.getNumShots();
	}
	CHECK(got == N);
}
