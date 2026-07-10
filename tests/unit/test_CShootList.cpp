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

// Regression for #1115:
// a single shot with a large negative relative speed (nSpeed < -255)
// sets both SMF_LARGESPEED and SMF_NEGSPEED.
// writeSingle() used an else-if that hid the large-and-negative branch,
// so it sent nSpeed-255 while readSingle() expects (-nSpeed)-255,
// and the shot decoded with the wrong speed.
// writeSingle() and readSingle() must round-trip every speed.
void test_CShootListSpeedRoundtrip() {
	const Version ver = OLXBetaVersion(0,57,0);  // before the release bit; keeps the packet simple
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
