/*
 *  Unit tests for Version::setByString, the version-string parser.
 */

#include "unittest.h"
#include "Version.h"

void test_Version() {
	// The SemVer "+git.HASH" build-metadata suffix is kept as provenance but
	// must not leak into the parsed version. Before the fix
	// "20260708.1+git.db202c4" parsed as num.subnum.subsubnum = 20260708.1.202
	// (the "202" from the hash), so a hashed build compared as a different
	// version than a clean build.
	{
		Version v("OpenLieroX/20260708.1+git.db202c4");
		CHECK(v.gamename == "OpenLieroX");
		CHECK(v.num == 20260708);
		CHECK(v.subnum == 1);
		CHECK(v.subsubnum == 0);
		CHECK(v.releasetype == Version::RT_NORMAL);
		CHECK(v.buildmetadata == "git.db202c4");
		// Same release with and without the hash must compare equal.
		CHECK(v == Version("OpenLieroX/20260708.1"));
		// asString() round-trips the full string, hash included.
		CHECK(v.asString() == "OpenLieroX/20260708.1+git.db202c4");
		// asHumanString() shows the hash for dev builds.
		CHECK(v.asHumanString() == "OpenLieroX 20260708.1 (git.db202c4)");
	}

	// The plain date-based version round-trips through asString(), no metadata.
	{
		Version v("OpenLieroX/20260708.1");
		CHECK(v.buildmetadata == "");
		CHECK(v.asString() == "OpenLieroX/20260708.1");
	}

	// A "-dirty" build marker (uncommitted tree, see functions.sh) is kept and
	// round-trips: the hyphen lives in the metadata and is not parsed further.
	{
		Version v("OpenLieroX/20260708.1+git.db202c4-dirty");
		CHECK(v.buildmetadata == "git.db202c4-dirty");
		CHECK(v.subsubnum == 0);
		CHECK(v.asString() == "OpenLieroX/20260708.1+git.db202c4-dirty");
		CHECK(v == Version("OpenLieroX/20260708.1"));
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
