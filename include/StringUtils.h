/*
	OpenLieroX

	string utilities
	
	code under LGPL
	created 01-05-2007
	by Albert Zeyer and Dark Charlie
*/

#ifndef __STRINGUTILS_H__
#define __STRINGUTILS_H__

#include <SDL.h> // for Uint32
#include <cstdio> // for FILE
#include <string>
#include <sstream>
#include <vector>
#include <cassert>
#include <list>
#include <limits.h>
#include <set>
#include "olx-types.h"
#include "Color.h" // for StrToCol
#include "Iter.h"
#include "util/IPrintOutFct.h"
#include "CodeAttributes.h"

//
// C-string handling routines
//
// HINT: these are obsolete, use std::string where possible!!!

// Secure c-string handling macros
// WARNING: don't use expressions like buf[i++] with the macros, because the "i" variable will be incremented twice in some macros!
#define		fix_markend(chrarray) \
				chrarray[sizeof(chrarray)-1] = '\0';
#define		fix_strnlen(chrarray) \
				strnlen(chrarray,sizeof(chrarray))
#define		fix_strncpy(chrarray, src) \
			{	strncpy(chrarray, src, sizeof(chrarray)); \
			 	chrarray[sizeof(chrarray)-1] = '\0'; }
#define		fix_strncat(chrarray, src) \
			{	size_t destlen = strnlen(chrarray, sizeof(chrarray)); \
				strncpy(&chrarray[destlen], src, sizeof(chrarray)-destlen); \
				chrarray[sizeof(chrarray)-1] = '\0'; }
#define		dyn_markend(dest, len) \
				dest[len-1] = '\0';
#define		dyn_strncpy(dest, src, len) \
			{	strncpy(dest, src, len); \
				dest[len-1] = '\0'; }
#define		dyn_strncat(dest, src, len) \
			{	size_t destlen = strnlen(dest, len); \
				strncpy(&dest[destlen], src, len-destlen); \
				dest[len-1] = '\0'; }


// Strnlen definition for compilers that don't have it
#if !defined(__USE_GNU) && _MSC_VER <= 1200
	INLINE size_t strnlen(const char *str, size_t maxlen) {
		size_t i;
		for(i = 0; (i < maxlen) && str[i]; ++i) {}
		return i;
	}
#endif

// Misc cross-compiler compatibility problem solutions
#ifdef WIN32
#if (defined(_MSC_VER) && (_MSC_VER <= 1200))
	INLINE int strncasecmp(const char *str1, const char *str2, size_t l) {
		return _strnicmp(str1, str2, l);
	}
#endif
#	define vsnprintf _vsnprintf
#	define snprintf	 _snprintf
#	define stricmp _stricmp
#	define fcloseall _fcloseall
#	define strcasecmp	stricmp
#elif defined(__EMSCRIPTEN__)
// Emscripten ships <compat/string.h> with `extern char* strlwr(char*)`,
// which clashes with the void-returning helper below. Use the system one
// (it's a thin wrapper around tolower() too) and discard the return.
#include <compat/string.h>
#else
INLINE void strlwr(char* string) {
	if(string)
		while( *string ) {
			*string = (char)tolower( *string );
			string++;
		}
}
#endif


/////////////
// Case-insensitive comparison of two chars, behaves like stringcasecmp
int chrcasecmp(const char c1, const char c2);


//
// C++ string (std::string) routines
//
// HINT: use these where possible

void			TrimSpaces(std::string& szLine);
INLINE std::string	Trimmed(const std::string& s) { std::string ret(s); TrimSpaces(ret); return ret; }
bool			replace(const std::string& text, const std::string& what, const std::string& with, std::string& result);
bool			replace(std::string& text, const std::string& what, const std::string& with);
std::string		Replace(const std::string & text, const std::string& what, const std::string& with);
std::string		replacemax(const std::string& text, const std::string& what, const std::string& with, std::string& result, int max);
std::string		replacemax(const std::string& text, const std::string& what, const std::string& with, int max);
std::string		strip(const std::string& text, int width);
bool			stripdot(std::string& text, int width);
void			ucfirst(std::string& text);
std::string		ReadUntil(const std::string& text, char until_character = '\n'); // will return whole text if not found
std::string		ReadUntil(const std::string& text, std::string::const_iterator& start, char until_character, const std::string& alternative = "");
std::string		ReadUntil(FILE* fp, char until_character = '\n');
Color			StrToCol(const std::string& str);
Color			StrToCol(const std::string& str, bool& fail);
std::vector<std::string> explode(const std::string& str, const std::string& delim);
void			freadstr(std::string& result, size_t maxlen, FILE *fp);
size_t			fwrite(const std::string& txt, size_t len, FILE* fp);
size_t			findLastPathSep(const std::string& path);
void			stringlwr(std::string& txt);
std::string		stringtolower(const std::string& txt);
bool			strincludes(const std::string& str, const std::string& what);
short			stringcasecmp(const std::string& s1, const std::string& s2);
bool			stringcaseequal(const std::string& s1, const std::string& s2);
bool			subStrEqual(const std::string& s1, const std::string s2, size_t p);
bool			subStrCaseEqual(const std::string& s1, const std::string s2, size_t p);
INLINE bool		strStartsWith(const std::string& str, const std::string& start) { if(start.size() > str.size()) return false; return str.substr(0,start.size()) == start; }
INLINE bool		strCaseStartsWith(const std::string& str, const std::string& start) { if(start.size() > str.size()) return false; return subStrCaseEqual(str,start,start.size()); }
size_t			maxStartingEqualStr(const std::list<std::string>& strs);
size_t			maxStartingCaseEqualStr(const std::list<std::string>& strs);
std::vector<std::string> splitstring(const std::string& str, size_t maxlen, size_t maxwidth, class CFont& font);
std::string		splitStringWithNewLine(const std::string& str, size_t maxlen, size_t maxwidth, class CFont& font);
std::string		GetFileExtension(const std::string& filename);
std::string		GetFileExtensionWithDot(const std::string& filename);
std::string		GetBaseFilename(const std::string& filename);
std::string		GetBaseFilenameWithoutExt(const std::string& filename);
std::string		GetFilenameWithoutExt(const std::string& filename);
std::list<std::string> SplitFilename(const std::string& filename, size_t numPartsFromRight = (size_t)-1); // splits fn by PathSep
std::string		GetDirName(const std::string& filename);
bool			FilenameSimplePatternMatch(const std::string& filename, const std::string& pattern);
size_t			stringcasefind(const std::string& text, const std::string& search_for);
size_t			stringcaserfind(const std::string& text, const std::string& search_for);
std::string		StripHtmlTags( const std::string & src );	// Also removes all "\r" and spaces at line beginning
std::string		GetNextWord(std::string::const_iterator it, const std::string& str);
bool 			Compress( const std::string & in, std::string * out, bool noCompression = false );	// Compress given string using zlib, noCompression will just add zlib header and checksum
bool 			Decompress( const std::string & in, std::string * out );	// Decompress, returns false if checksum fails
size_t			StringChecksum( const std::string & data );
bool			FileChecksum( const std::string & path, size_t * _checksum, size_t * _filesize );
std::string		Base64Encode(const std::string &data);
std::string		Base64Decode(const std::string &data);
std::string		UrlEncode(const std::string &data); // Substitute space with + and all non-alphanum symbols with %XX
std::string		AutoDetectLinks(const std::string& text);
std::string		HtmlEntityUnpairedBrackets(const std::string &txt);
size_t			GetPosByTextWidth(const std::string& text, int width, CFont *fnt);
std::string		ColToHex(Color col);
std::string		EscapeHtmlTags( const std::string & src );	// Escape all "<" and ">" and "&"

bool			strSeemsLikeChatCommand(const std::string& str);

INLINE size_t subStrCount(const std::string& str, const std::string& substr) {
	size_t c = 0, p = 0;
	while((p = str.find(substr, p)) != std::string::npos) { c++; p++; }
	return c;
}


// returns true if last char was a newline
bool PrettyPrint(const std::string& prefix, const std::string& buf, const PrintOutFct& printOutFct, bool firstLineWithPrefix = true);

Iterator<char>::Ref HexDump(Iterator<char>::Ref start, const PrintOutFct& printOutFct, const std::set<size_t>& marks = std::set<size_t>(), size_t count = (size_t)-1);




INLINE std::string FixedWidthStr_RightFill(const std::string& str, size_t w, char c) {
	assert(str.size() <= w);
	return str + std::string(str.size() - w, c);
}

INLINE std::string FixedWidthStr_LeftFill(const std::string& str, size_t w, char c) {
	assert(str.size() <= w);
	return std::string(w - str.size(), c) + str;
}

INLINE void StripQuotes(std::string& value) {
	if( value.size() >= 2 )
		if( value[0] == '"' && value[value.size()-1] == '"' )
			value = value.substr( 1, value.size()-2 );	
}

////////////////////
// Read a fixed-length C-string from a file
INLINE std::string freadfixedcstr(FILE *fp, size_t maxlen) {
	std::string fileData;
	freadstr(fileData, maxlen, fp);
	return ReadUntil(fileData, '\0');
}

///////////////////
// Convert a numerical position to iterator
INLINE std::string::iterator PositionToIterator(std::string& str, size_t pos)  {
	std::string::iterator res = str.begin();
	for (size_t i=0; i < pos && res != str.end(); ++i, res++)  {}
	return res;
}





struct stringcaseless {
	bool operator()(const std::string& s1, const std::string& s2) const {
		return stringcasecmp(s1,s2) < 0;
	}
};


struct const_string_iterator {
	const std::string& str;
	size_t pos;

	const_string_iterator(const std::string& s, size_t p = 0) : str(s), pos(p) {}
	const_string_iterator& operator++() { pos++; return *this; }
	const_string_iterator& operator--() { assert(pos > 0); pos--; return *this; }
	
	bool operator==(const const_string_iterator& i) const {
		return &str == &i.str && (pos == i.pos || (pos > str.size() && i.pos > str.size()));
	}
	bool operator!=(const const_string_iterator& i) const { return !(*this == i); }

	char operator*() const { return str[pos]; }
};

#endif
