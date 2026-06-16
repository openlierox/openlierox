/*
	OpenLieroX

	version parsing

	file created 29-02-2008 by albert
	code under LGPL
*/

#ifndef __VERSION_H__
#define __VERSION_H__

#include <string>
#include "StringUtils.h" // for itoa
#include "CodeAttributes.h"

struct Version;
extern const char* const fullGameName;
INLINE const char* GetFullGameName() { return fullGameName; }
const char* GetGameName();
const Version& GetGameVersion();

// The OLX version string without the git hash, e.g. "20260616.4". This is
// what should be shown in most user-facing places (menu, window title) — the
// git hash is noise there. Unlike Version::asHumanString() this is NOT
// re-parsed through the integer num.subnum.subsubnum scheme (which can't
// represent the date-based version system — see functions.sh), so it is
// shown verbatim.
const std::string& GetGameVersionString();

// The full version string including the "+git.HASH" suffix, e.g.
// "20260616.4+git.b547037" (matches get_version_full.sh). Use this only
// where the exact build provenance matters (crash reports, etc.).
const std::string& GetGameVersionStringFull();


struct Version {
	Version() { reset(); }
	Version(const std::string& versionStr) { setByString(versionStr); }
	
	void reset() { gamename = "LieroX"; num = 0; subnum = 56; subsubnum = 0; revnum = 0; releasetype = RT_NORMAL; }
	void setByString(const std::string& versionStr);
	std::string asString() const;
	std::string asHumanString() const;
	std::string releaseType() const;
	
	bool isBanned() const;
	
	int num; // 0
	int subnum; // 57
	int subsubnum; // 3 (Beta-version)
	int revnum; // 1007 (SVN)
	enum Releasetype {
		// HINT: keep in this order to allow direct comparsion here
		RT_UNKNOWN, RT_ALPHA, RT_BETA, RT_RC, RT_NORMAL
	} releasetype;
	std::string gamename; // OpenLieroX

};


// The following functions are declared INLINE because they are used very often.

// For comparision, we ignore the following: revnum, gamename
// That means, a special revision of a baseversion should not change the behaviour (and it's only for debugging).
// And another game like Hirudo should keep the same version-counting. We can start Hirudo at version 1.0 or 0.99.

INLINE bool operator<(const Version& ver1, const Version& ver2) {
	if(ver1.num != ver2.num) return ver1.num < ver2.num;
	if(ver1.subnum != ver2.subnum) return ver1.subnum < ver2.subnum;
	if(ver1.releasetype != ver2.releasetype) return ver1.releasetype < ver2.releasetype;
	if(ver1.subsubnum != ver2.subsubnum) return ver1.subsubnum < ver2.subsubnum;
	return false;
}

INLINE bool operator==(const Version& ver1, const Version& ver2) {
	return
	ver1.num == ver2.num &&
	ver1.subnum == ver2.subnum &&
	ver1.releasetype == ver2.releasetype &&
	ver1.subsubnum == ver2.subsubnum;
}

INLINE bool operator>(const Version& ver1, const Version& ver2) { return ver2 < ver1; }
INLINE bool operator<=(const Version& ver1, const Version& ver2) { return ver1 < ver2 || ver1 == ver2; }
INLINE bool operator>=(const Version& ver1, const Version& ver2) { return ver2 < ver1 || ver1 == ver2; }
INLINE bool operator!=(const Version& ver1, const Version& ver2) { return ! (ver1 == ver2); }

INLINE Version OLXBetaVersion(int num, int subnum, int betaversion) {
	Version v;
	v.gamename = fullGameName;
	v.num = num;
	v.subnum = subnum;
	v.subsubnum = betaversion;
	v.releasetype = Version::RT_BETA;
	return v;
}

INLINE Version OLXVersionBranchStart(int num, int subnum) {
	Version v;
	v.gamename = fullGameName;
	v.num = num;
	v.subnum = subnum;
	v.subsubnum = 0;
	v.releasetype = Version::RT_ALPHA; // lowest possible value
	return v;	
}

INLINE Version OLXRcVersion(int num, int subnum, int rcversion) {
	Version v;
	v.gamename = fullGameName;
	v.num = num;
	v.subnum = subnum;
	v.subsubnum = rcversion;
	v.releasetype = Version::RT_RC;
	return v;
}

#endif
