/////////////////////////////////////////
//
//   OpenLieroX
//
//   Auxiliary Software class library
//
//   based on the work of JasonB
//   enhanced by Dark Charlie and Albert Zeyer
//
//   code under LGPL
//
/////////////////////////////////////////


// File finding routines
// Created 30/9/01
// By Jason Boettcher




#ifdef _MSC_VER
#pragma warning(disable: 4786)  // WARNING: identifier XXX was truncated to 255 characters in the debug info
#pragma warning(disable: 4503)  // WARNING: decorated name length exceeded, name was truncated
#endif

#include "FindFile.h"
#include "StringUtils.h"
#include "Options.h"
#include "Debug.h"


#ifdef WIN32
#	ifndef _WIN32_IE
// Some functions that are required are unavailable if this is not defined:
// TODO: which functions?
#   define  _WIN32_IE  0x0400  // Because of Dev-cpp
#	endif

#	include <shlobj.h>

#else // WIN32

// not defined for older GCC versions; we check only for >=4.3 anyway
# ifndef __GNUC_PREREQ
#  define __GNUC_PREREQ(maj, min) (0)
# endif

// include hash_set support
#	if !defined(STLPORT)
#		if defined(__GNUC__) &&  __GNUC_PREREQ(4,3)
#			include <tr1/unordered_set>
#			define hash_set std::tr1::unordered_set
#		else
#			include <ext/hash_set>
#			ifdef _GLIBCXX_DEBUG
				using namespace __gnu_debug_def;
#			else
				using namespace __gnu_cxx;
#			endif
#		endif
#	else // STLPORT
#		include <hash_set>
		using std::hash_set;
#	endif

// for getpwduid
#	include <pwd.h>

// for realpath
#	include <sys/param.h>
#	include <unistd.h>

#endif


searchpathlist tSearchPaths;



bool IsFileAvailable(const std::string& f, bool absolute) {
	std::string abs_f;
	if(absolute) {
		abs_f = f;
	} else
		if((abs_f = GetFullFileName(f)) == "") return false;

	// remove trailing slashes
	// don't remove them on WIN, if it is a drive-letter
	while(abs_f.size() > 0 && (abs_f[abs_f.size()-1] == '\\' || abs_f[abs_f.size()-1] == '/')) {
#ifdef WIN32
		if(abs_f.size() > 2 && abs_f[abs_f.size()-2] == ':') break;
#endif
		abs_f.erase(abs_f.size()-1);
	}

	abs_f = Utf8ToSystemNative(abs_f);

	// HINT: this should also work on WIN32, as we have _stat here
	struct stat s;
	if(stat(abs_f.c_str(), &s) != 0 || !S_ISREG(s.st_mode)) {
		// it's not stat-able or not a reg file
		return false;
	}

	// it's stat-able and a file
	return true;
}



//////////////////////
// Replaces backward slashes with forward slashes (windows only)
// Used when comparing two paths
static void ReplaceSlashes(std::string& path)  {
#ifdef WIN32
	for (std::string::iterator it = path.begin(); it != path.end(); it++)
		if (*it == '\\') *it = '/';
#endif
}

bool EqualPaths(const std::string& path1, const std::string& path2)  {
	std::string p1 = path1;
	std::string p2 = path2;

	ReplaceSlashes(p1);
	ReplaceSlashes(p2);

	if (*p1.rbegin() != '/')
		p1 += '/';
	if (*p2.rbegin() != '/')
		p2 += '/';

	return stringcaseequal(p1, p2);
}

/*

	Drives

*/

////////////////////
//
drive_list GetDrives()
{
static drive_list list;
list.clear();
#ifdef WIN32
	static char drives[34];
	int len = GetLogicalDriveStrings(sizeof(drives),drives); // Get the list of drives
	drive_t tmp;
	if (len)  {
		for (int i=0; i<len; i+=(int)strnlen(&drives[i],4)+1)  {
			// Create the name (for example: C:\)
			tmp.name = &drives[i];
			// Get the type
			tmp.type = GetDriveType((LPCTSTR)tmp.name.c_str());
			// Add to the list
			list.push_back(tmp);
		}
	}


#else
	// there are not any drives on Linux/Unix/MacOSX/...
	// it's only windows which uses this crazy drive-letters

	// perhaps not the best way
	// home-dir of user is in other applications the default
	// but it's always possible to read most other stuff
	// and it's not uncommon that a user hase a shared dir like /mp3s
	drive_t tmp;
	tmp.name = "/";
	tmp.type = 0;
	list.push_back(tmp);

	// we could communicate with dbus and ask it for all connected
	// and mounted hardware-stuff
#endif

	return list;
}


#ifndef WIN32

// checks, if path is statable (that means, it's existing)
// HINT: absolute path and there is not case fixing
// (used by GetExactFileName)
bool IsPathStatable(const std::string& f) {
	std::string abs_f = f;

	// remove trailing slashes
	// don't remove them on WIN, if it is a drive-letter
	while(abs_f.size() > 0 && (abs_f[abs_f.size()-1] == '\\' || abs_f[abs_f.size()-1] == '/')) {
#ifdef WIN32
		if(abs_f.size() > 2 && abs_f[abs_f.size()-2] == ':') break;
#endif
		abs_f.erase(abs_f.size()-1);
	}

	// HINT: this should also work on WIN32, as we have _stat here
	struct stat s;
#ifdef WIN32  // uses UTF16
	return (wstat(Utf8ToUtf16(abs_f).c_str(), &s) == 0); // ...==0, if successfull
#else // other systems
	return (stat(abs_f.c_str(), &s) == 0); // ...==0, if successfull
#endif
}


// used by unix-GetExactFileName
// HINT: it only reads the first char of the seperators
// it returns the start of the subdir (the pos _after_ the sep.)
// HINT: it returns position in bytes, not in characters
size_t GetNextName(const std::string& fullname, const char** seperators, std::string& nextname)
{
	std::string::const_iterator pos;
	size_t p = 0;
	unsigned short i;

	for(pos = fullname.begin(); pos != fullname.end(); pos++, p++) {
		for(i = 0; seperators[i] != NULL; i++)
			if(*pos == seperators[i][0]) {
				nextname = fullname.substr(0, p);
				return p + 1;
			}
	}

	nextname = fullname;
	return 0;
}

// get ending filename of a path
size_t GetLastName(const std::string& fullname, const char** seperators)
{
	std::string::const_reverse_iterator pos;
	size_t p = fullname.size()-1;
	unsigned short i;

	for(pos = fullname.rbegin(); pos != fullname.rend(); pos++, p--) {
		for(i = 0; seperators[i] != NULL; i++)
			if(*pos == seperators[i][0]) {
				return p;
			}
	}

	// indicates that there is no more sep
	return (size_t)(-1);
}



struct strcasecomparer {
	bool operator()(const std::string& str1, const std::string& str2) const {
		return stringcaseequal(str1, str2);
	}
};

typedef hash_set<std::string, simple_reversestring_hasher, strcasecomparer> exactfilenamecache_t;
struct ExactFilenameCache {
	exactfilenamecache_t cache;
	Mutex mutex;
}
exactfilenamecache;

bool is_searchname_in_exactfilenamecache(
										 const std::string& searchname,
										 std::string& exactname
) {
	Mutex::ScopedLock lock(exactfilenamecache.mutex);
	exactfilenamecache_t::iterator it = exactfilenamecache.cache.find(searchname);
	if(it != exactfilenamecache.cache.end()) {
		exactname = *it;
		return true;
	} else
		return false;
}

void add_searchname_to_exactfilenamecache(const std::string& exactname) {
	Mutex::ScopedLock lock(exactfilenamecache.mutex);
	exactfilenamecache.cache.insert(exactname);
}


// used by unix-GetExactFileName
// does a case insensitive search for searchname in dir
// sets filename to the first search result
// returns true, if any file found
bool CaseInsFindFile(const std::string& dir, const std::string& searchname, std::string& filename) {
	if(searchname == "") {
		filename = "";
		return true;
	}

	// Check first if searchname perhaps exists with exactly this name.
	// This check is also needed in the case if we cannot read dir (-r) but we can access files (+x) in it.
	if(IsPathStatable((dir == "") ? searchname : (dir + "/" + searchname))) {
		filename = searchname;
		return true;
	}

	DIR* dirhandle = opendir((dir == "") ? "." : dir.c_str());
	if(dirhandle == NULL) return false;

	dirent* direntry;
	while((direntry = readdir(dirhandle))) {
		if(strcasecmp(direntry->d_name, searchname.c_str()) == 0) {
			filename = direntry->d_name;
			closedir(dirhandle);
#ifdef DEBUG
			// HINT: activate this warning temporarly when you want to fix some filenames
			//if(filename != searchname)
			//	cerr << "filename case mismatch: " << searchname << " <-> " << filename << endl;
#endif
			return true;
		}
		add_searchname_to_exactfilenamecache((dir == "") ? direntry->d_name : (dir + "/" + direntry->d_name));
	}

	closedir(dirhandle);
	return false;
}



// does case insensitive search for file
bool GetExactFileName(const std::string& abs_searchname, std::string& filename) {
	const char* seps[] = {"\\", "/", (char*)NULL};
	if(abs_searchname.size() == 0) {
		filename = "";
		return false;
	}

	std::string sname = abs_searchname;
	ReplaceFileVariables(sname);

	std::string nextname = "";
	std::string nextexactname = "";
	size_t pos;

	bool first_iter = true; // this is used in the bottom loop

	// search in cache

	// sname[0..pos-1] is left rest, excluding the /
	pos = sname.size();
	std::string rest;
	while(true) {
		rest = sname.substr(0,pos);
		if(is_searchname_in_exactfilenamecache(rest, filename)) {
			if(IsPathStatable(filename)) {
				if(pos == sname.size()) // do we got the whole filename?
					return true;

				// filename is the correct one here
				sname.erase(0,pos+1);
				first_iter = false; // prevents the following loop from not adding a "/" to filename
				break;
			}
		}
		pos = GetLastName(rest, seps);
		if(pos == (size_t)(-1)) {
			first_iter = false;
			if(rest == "." || rest == "..") {
				filename = rest;
				sname.erase(0,rest.size()+1);
				break;
			}
			filename = ".";
			break;
		}
		if(pos == 0) {
			filename = "/";
			sname.erase(0,1);
			break;
		}
	}



	// search the filesystem for the name

	// sname contains the rest of the path
	// filename contains the start (including a "/" if necces.)
	// if first_iter is set to true, don't add leading "/"
	while(true) {
		pos = GetNextName(sname, seps, nextname);
		// pos>0  => found a sep (pos is right behind the sep)
		// pos==0  => none found
		if(pos > 0) sname.erase(0,pos);

		if(nextname == "") {
			// simply ignore this case
			// (we accept sth like /usr///share/)
			if(pos == 0) break;
			continue;
		} else if(!CaseInsFindFile(
				filename, // dir
				nextname, // ~name
				nextexactname // resulted name
		)) {
			// we doesn't get any result
			// just add rest to it
			if(!first_iter) filename += "/";
			filename += nextname;
			if(pos > 0) filename += "/" + sname;
			return false; // error (not found)
		}

		if(!first_iter) filename += "/";
		filename += nextexactname;
		if(nextexactname != "")
			add_searchname_to_exactfilenamecache(filename);

		if(pos == 0) break;
		first_iter = false;
	}

	// we got here after the full path was resolved successfully
	return true;
}

#endif // not WIN32


searchpathlist	basesearchpaths;
void InitBaseSearchPaths() {
	basesearchpaths.clear();
#if defined(__APPLE__)
	AddToFileList(&basesearchpaths, "${HOME}/Library/Application Support/OpenLieroX");
	AddToFileList(&basesearchpaths, "Contents/Resources/gamedir");
	AddToFileList(&basesearchpaths, ".");
	AddToFileList(&basesearchpaths, SYSTEM_DATA_DIR"/OpenLieroX");
#elif defined(WIN32)
	AddToFileList(&basesearchpaths, "${HOME}/OpenLieroX");
	AddToFileList(&basesearchpaths, ".");
	AddToFileList(&basesearchpaths, "${BIN}");
#else // all other systems (Linux, *BSD, OS/2, ...)
	AddToFileList(&basesearchpaths, "${HOME}/.OpenLieroX");
	AddToFileList(&basesearchpaths, ".");
	AddToFileList(&basesearchpaths, SYSTEM_DATA_DIR"/OpenLieroX"); // no use of ${SYSTEM_DATA}, because it is uncommon and could cause confusion to the user
	AddToFileList(&basesearchpaths, "${BIN}/gamedir");
#endif
}

void CreateRecDir(const std::string& abs_filename, bool last_is_dir) {
	std::string tmp;
	std::string::const_iterator f = abs_filename.begin();
	for(tmp = ""; f != abs_filename.end(); f++) {
		if(*f == '\\' || *f == '/')
			mkdir(tmp.c_str(), 0777);
		tmp += *f;
	}
	if(last_is_dir)
		mkdir(tmp.c_str(), 0777);
}

std::string GetFirstSearchPath() {
	if(tSearchPaths.size() > 0)
		return tSearchPaths.front();
	else if(basesearchpaths.size() > 0)
		return basesearchpaths.front();
	else
		return GetHomeDir();
}

size_t FileSize(const std::string& path)
{
	FILE *fp = OpenGameFile(path, "rb");
	if (fp == NULL)
		return 0;
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fclose(fp);
	return size;
}

// Checks if the given path is absolute
bool IsAbsolutePath(const std::string& path)
{
#ifdef WIN32
	// The path must start with a drive letter
	if (path.size() < 2)
		return false;

	return (isalpha((uchar)path[0]) && path[1] == ':');
#else
	// Must start with a slash
	if (!path.size())
		return false;

	return path[0] == '/';
#endif
}


static std::string specialSearchPathForTheme = "";

void initSpecialSearchPathForTheme() {
	if(tLXOptions->sTheme != "") {
		specialSearchPathForTheme = GetFullFileName("themes/" + tLXOptions->sTheme);
	} else
		specialSearchPathForTheme = "";
}

const std::string* getSpecialSearchPathForTheme() {
	if(specialSearchPathForTheme == "")
		return NULL;
	else
		return &specialSearchPathForTheme;
}


	class CheckSearchpathForFile { public:
		const std::string& filename;
		std::string* result;
		std::string* searchpath;
		CheckSearchpathForFile(const std::string& f, std::string* r, std::string* s) : filename(f), result(r), searchpath(s) {}

		bool operator() (const std::string& spath) {
			std::string tmp = spath + filename;
			if(GetExactFileName(tmp, *result)) {
				// we got here, if the file exists
				if(searchpath) *searchpath = spath;
				return false; // stop checking next searchpaths
			}

			// go to the next searchpath
			return true;
		}
	};

std::string GetFullFileName(const std::string& path, std::string* searchpath) {
	if(searchpath) *searchpath = "";
	if(path == "") return GetFirstSearchPath();

	// Check if we have an absolute path
	if(IsAbsolutePath(path)) {
		std::string tmp;
		GetExactFileName(path, tmp);
		return tmp;
	}
	
	std::string fname;
	CheckSearchpathForFile checker(path, &fname, searchpath);
	ForEachSearchpath(checker);
	return fname;
}

std::string GetWriteFullFileName(const std::string& path, bool create_nes_dirs) {
	std::string tmp;
	std::string fname;

	// get the dir, where we should write into
	if(tSearchPaths.size() == 0 && basesearchpaths.size() == 0) {
		errors << "we want to write somewhere, but don't know where => we are writing to your temp-dir now..." << endl;
		tmp = GetTempDir() + "/" + path;
	} else {
		GetExactFileName(GetFirstSearchPath(), tmp);

		CreateRecDir(tmp);
		if(!CanWriteToDir(tmp)) {
			errors << "we cannot write to " << tmp << " => we are writing to your temp-dir now..." << endl;
			tmp = GetTempDir();
		}

		tmp += "/";
		tmp += path;
	}

	GetExactFileName(tmp, fname);
	if(create_nes_dirs) CreateRecDir(fname, false);
	return tmp;
}

FILE* OpenAbsFile(const std::string& path, const char *mode) {
	std::string exactfn;
	if(!GetExactFileName(path, exactfn))
		return NULL;
	return fopen(Utf8ToSystemNative(exactfn).c_str(), mode);
}

FILE *OpenGameFile(const std::string& path, const char *mode) {
	if(path.size() == 0)
		return NULL;

	std::string fullfn = GetFullFileName(path);

	bool write_mode = strchr(mode, 'w') != 0;
	bool append_mode = strchr(mode, 'a') != 0;
	if(write_mode || append_mode) {
		std::string writefullname = GetWriteFullFileName(path, true);
		if(append_mode && fullfn != "") { // check, if we should copy the file
			if(IsFileAvailable(fullfn, true)) { // we found the file
				// GetWriteFullFileName ensures an exact filename,
				// so no case insensitive check is needed here
				if(fullfn != writefullname) {
					// it is not the file, we would write to, so copy it to the wanted destination
					if(!FileCopy(fullfn, writefullname)) {
						errors << "problems while copying, so I cannot open this file in append-mode somewhere else" << endl;
						return NULL;
					}
				}
			}
		}
		//errors << "opening file for writing (mode %s): %s\n", mode, writefullname);
		return fopen(Utf8ToSystemNative(writefullname).c_str(), mode);
	}

	if(fullfn.size() != 0) {
		return fopen(Utf8ToSystemNative(fullfn).c_str(), mode);
	}

	return NULL;
}



std::ifstream* OpenGameFileR(const std::string& path) {
	if(path.size() == 0)
		return NULL;

	std::string fullfn = GetFullFileName(path);
	if(fullfn.size() != 0) {
		try {
			std::ifstream* f = new std::ifstream(Utf8ToSystemNative(fullfn).c_str(), std::ios::in | std::ios::binary);
			if (f->is_open())
				return f;
			else {
				delete f;
				return NULL;
			}
		} catch(...) {}
		return NULL;
	}

	return NULL;
}



void AddToFileList(searchpathlist* l, const std::string& f) {
	if(!FileListIncludesExact(l, f)) l->push_back(f);
}

void removeEndingSlashes(std::string& s)
{
	while(s.size() > 0 && (*s.rbegin() == '\\' || *s.rbegin() == '/'))
		s.erase(s.size() - 1);
}

/////////////////
// Returns true, if the list contains the path
bool FileListIncludesExact(const searchpathlist* l, const std::string& f) {
	std::string tmp1 = f;
	removeEndingSlashes(tmp1);
	ReplaceFileVariables(tmp1);
	replace(tmp1,"\\","/");

	// Go through the list, checking each item
	for(searchpathlist::const_iterator i = l->begin(); i != l->end(); i++) {
		std::string tmp2 = *i;
		removeEndingSlashes(tmp2);
		ReplaceFileVariables(tmp2);
		replace(tmp2,"\\","/");
		if(stringcaseequal(tmp1, tmp2))
			return true;
	}

	return false;
}

std::string GetHomeDir() {
#ifndef WIN32
	char* home = getenv("HOME");
	if(home == NULL || home[0] == '\0') {
		passwd* userinfo = getpwuid(getuid());
		if(userinfo)
			return userinfo->pw_dir;
		return ""; // both failed, very strange system...
	}
	return home;
#else
	static std::string result = "";
	if (result.size() == 0)  {  // Only do this once
		char tmp[1024];
		if (!SHGetSpecialFolderPath(NULL, tmp, CSIDL_PERSONAL,FALSE))  {
			// TODO: get dynamicaly another possible path
			// the following is only a workaround!
			return "C:\\OpenLieroX";
		}
		fix_markend(tmp);

		result = SystemNativeToUtf8(tmp);
	}
	return result;
#endif
}


std::string GetSystemDataDir() {
#ifndef WIN32
	return SYSTEM_DATA_DIR;
#else
	// windows don't have such dir, don't it?
	// or should we return windows/system32 (which is not exactly intended here)?
	return "";
#endif
}


std::string	binary_dir; // given by argv[0], set by main()

std::string GetBinaryDir() {
	return binary_dir;
}

std::string GetTempDir() {
#ifndef WIN32
	return "/tmp"; // year, it's so simple :)
#else
	static char buf[1024] = "";
	if(buf[0] == '\0') { // only do this once
		GetTempPath(sizeof(buf), buf);
		fix_markend(buf);
	}
	return SystemNativeToUtf8(buf);
#endif
}

void ReplaceFileVariables(std::string& filename) {
	if(filename.compare(0,2,"~/")==0
	|| filename.compare(0,2,"~\\")==0
	|| filename == "~") {
		filename.erase(0,1);
		filename.insert(0,GetHomeDir());
	}
	replace(filename, "${HOME}", GetHomeDir());
	replace(filename, "${SYSTEM_DATA}", GetSystemDataDir());
	replace(filename, "${BIN}", GetBinaryDir());
}

// WARNING: not multithreading aware
// HINT: uses absolute paths
// returns true, if successfull
bool FileCopy(const std::string& src, const std::string& dest) {
	static char tmp[2048];

	notes << "FileCopy: " << src << " -> " << dest << endl;

	FILE* src_f = fopen(Utf8ToSystemNative(src).c_str(), "rb");

	if(!src_f) {
		errors << "FileCopy: cannot open source" << endl;
		return false;
	}

	FILE* dest_f = fopen(Utf8ToSystemNative(dest).c_str(), "wb");

	if(!dest_f) {
		fclose(src_f);
		errors << "FileCopy: cannot open destination" << endl;
		return false;
	}

	bool success = true;
	unsigned short count = 0;
	notes << "FileCopy: |" << flush;
	size_t len = 0;
	while((len = fread(tmp, 1, sizeof(tmp), src_f)) > 0) {
		if(count == 0) notes << "." << flush;
		count++;
		count %= 20;
		if(len != fwrite(tmp, 1, len, dest_f)) {
			errors << "FileCopy: problem while writing" << endl;
			success = false;
			break;
		}
		if(len != sizeof(tmp)) break;
	}
	notes << endl;
	if(success) {
		success = feof(src_f) != 0;
		if(!success) errors << "FileCopy: problem while reading" << endl;
	}

	fclose(src_f);
	fclose(dest_f);
	if(success)	notes << "FileCopy: success :)" << endl;
	return success;
}

bool CanWriteToDir(const std::string& dir) {
	// TODO: we have to make this a lot better!
	std::string fname = dir + "/.some_stupid_temp_file";

	FILE* fp = fopen(Utf8ToSystemNative(fname).c_str(), "w");

	if(fp) {
		fclose(fp);
		remove(Utf8ToSystemNative(fname).c_str());

		return true;
	}
	return false;
}


std::string GetAbsolutePath(const std::string& path) {
#ifdef WIN32
	std::string exactpath;
	if (!GetExactFileName(path, exactpath))
		exactpath = path;

	char buf[2048];
	int len = GetFullPathName(Utf8ToSystemNative(exactpath).c_str(), sizeof(buf), buf, NULL);
	fix_markend(buf);
	if (len)
		return SystemNativeToUtf8(buf);
	else  // Failed
		return path;
#else
	std::string exactpath;
	if(GetExactFileName(path, exactpath)) {
		char buf[PATH_MAX];
		if(realpath(exactpath.c_str(), buf) != NULL) {
			fix_markend(buf);
			return buf;
		} else
			return exactpath;
	} else
		return path;
#endif
}

bool PathListIncludes(const std::list<std::string>& pathlist, const std::string& path) {
	std::string abs_path;
	abs_path = GetAbsolutePath(path);

	// Go through the list, checking each item
	for(std::list<std::string>::const_iterator i = pathlist.begin(); i != pathlist.end(); i++) {
		if(EqualPaths(abs_path, GetAbsolutePath(*i))) {
			return true;
		}
	}

	return false;
}

///////////////////////
// Returns the file contents as a string
std::string GetFileContents(const std::string& path, bool absolute)
{
	FILE *fp = NULL;
	if (absolute)
		fp = fopen(Utf8ToSystemNative(path).c_str(), "rb");
	else
		fp = OpenGameFile(path, "rb");

	if (!fp)
		return "";

	fseek(fp, 0, SEEK_END);
	long size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	if (!size || size < 0 || size > 100000000)  {
		fclose(fp);
		return "";
	}

	char *buf = new char[size];
	size = fread(buf, 1, size, fp);
	if (!size)  {
		delete[] buf;
		fclose(fp);
		return "";
	}

	std::string result;
	result.append(buf, size);
	delete[] buf;
	fclose(fp);

	return result;
}

////////////////
// Extract the directory part from a path
std::string ExtractDirectory(const std::string& path)
{
	if (path.size() == 0)
		return "";

	size_t pos = findLastPathSep(path);
	if (pos == std::string::npos)
		return path;
	else
		return path.substr(0, pos);
}


std::string GetScriptInterpreterCommandForFile(const std::string& filename) {
	FILE* f = OpenGameFile(filename, "r");	
	if(f) {
		std::string line = ReadUntil(f);
		if(line.size() > 2 && line[0] == '#' && line[1] == '!') {
			std::string cmd = line.substr(2);
			TrimSpaces(cmd);
			fclose(f);
			return cmd;
		}
		fclose(f);
		return "";
	}
	return "";
}


///////////////////
// Merges two parts of a path into one, for example JoinPath("./test/", "/file.fil") gives "./test/file.fil"
std::string JoinPaths(const std::string& path1, const std::string& path2)
{
	if (path1.size() == 0)
		return path2;
	if (path2.size() == 0)
		return path1;

	std::string result = path1;
	if (*path1.rbegin() == '/' || *path1.rbegin() == '\\')  {
		if (*path2.begin() == '/' || *path2.begin() == '\\')  {
			result.erase(result.size() - 1);
			result += path2;
			return result;
		} else {
			result += path2;
			return result;
		}
	} else {
		if (*path2.begin() == '/' || *path2.begin() == '\\')  {
			result += path2;
			return result;
		} else {
			result += '/';
			result += path2;
			return result;
		}
	}
}

//////////////////////////////
// Creating SDL_RWops structure from a file pointer
// We cannot use SDL's function for this under WIN32 because it doesn't allow
// passing file pointers to a dll

#include <SDL.h>

// These are taken from SDL_rwops.c
#ifdef WIN32
static int stdio_seek(SDL_RWops *context, int offset, int whence)
{
	if ( fseek(context->hidden.stdio.fp, offset, whence) == 0 ) {
		return(ftell(context->hidden.stdio.fp));
	} else {
		SDL_Error(SDL_EFSEEK);
		return(-1);
	}
}
static int stdio_read(SDL_RWops *context, void *ptr, int size, int maxnum)
{
	size_t nread;

	nread = fread(ptr, size, maxnum, context->hidden.stdio.fp);
	if ( nread == 0 && ferror(context->hidden.stdio.fp) ) {
		SDL_Error(SDL_EFREAD);
	}
	return (int)(nread);
}
static int stdio_write(SDL_RWops *context, const void *ptr, int size, int num)
{
	size_t nwrote;

	nwrote = fwrite(ptr, size, num, context->hidden.stdio.fp);
	if ( nwrote == 0 && ferror(context->hidden.stdio.fp) ) {
		SDL_Error(SDL_EFWRITE);
	}
	return (int)(nwrote);
}
static int stdio_close(SDL_RWops *context)
{
	if ( context ) {
		if ( context->hidden.stdio.autoclose ) {
			/* WARNING:  Check the return value here! */
			fclose(context->hidden.stdio.fp);
		}
		free(context);
	}
	return(0);
}
#endif

////////////////
// Creates SDL_RWops from a file pointer
SDL_RWops *RWopsFromFP(FILE *fp, bool autoclose)  {
#ifdef WIN32
	// Taken from SDL code
	SDL_RWops *rwops = SDL_AllocRW();
	if ( rwops != NULL ) {
		rwops->seek = stdio_seek;
		rwops->read = stdio_read;
		rwops->write = stdio_write;
		rwops->close = stdio_close;
		rwops->hidden.stdio.fp = fp;
		rwops->hidden.stdio.autoclose = (int)autoclose;
	}
	return(rwops);

#else
	return SDL_RWFromFP(fp, (int)autoclose);
#endif
}

