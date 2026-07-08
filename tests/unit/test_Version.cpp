/*
 *  Unit tests for Version::setByString, the version-string parser.
 */

#include "unittest.h"
#include "Version.h"

void test_Version() {
	// Regression: the SemVer "+git.HASH" build-metadata suffix must not leak
	// into the parsed version. Before the fix "20260708.1+git.db202c4" parsed
	// as num.subnum.subsubnum = 20260708.1.202 (the "202" from the hash),
	// so a hashed build compared as a different version than a clean build.
	{
		Version v("OpenLieroX/20260708.1+git.db202c4");
		CHECK(v.gamename == "OpenLieroX");
		CHECK(v.num == 20260708);
		CHECK(v.subnum == 1);
		CHECK(v.subsubnum == 0);
		CHECK(v.releasetype == Version::RT_NORMAL);
		// Same release with and without the hash must compare equal.
		CHECK(v == Version("OpenLieroX/20260708.1"));
	}

	// The plain date-based version round-trips through asString().
	{
		Version v("OpenLieroX/20260708.1");
		CHECK(v.asString() == "OpenLieroX/20260708.1");
	}

	// A classic beta version still parses as before.
	{
		Version v("OpenLieroX/0.58_beta9");
		CHECK(v.num == 0);
		CHECK(v.subnum == 58);
		CHECK(v.subsubnum == 9);
		CHECK(v.releasetype == Version::RT_BETA);
	}
}
