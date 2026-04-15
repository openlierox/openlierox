/*
	OpenLieroX

	UTF8/Unicode conversions
	
	code under LGPL
	created 01-05-2007
	by Albert Zeyer and Dark Charlie
*/

#ifndef __UNICODE_H__
#define __UNICODE_H__

#include <SDL.h> // for Uint32
#include <string>

typedef char32_t UnicodeChar;
typedef std::basic_string<UnicodeChar> Unicode32String;
#ifdef WIN32
typedef wchar_t Utf16Char;
typedef std::wstring Utf16String;
#else
typedef char16_t Utf16Char;
typedef std::basic_string<Utf16Char> Utf16String;
#endif


struct ConversionItem {
	UnicodeChar Unicode;
	unsigned char Utf8[4];
	char Ascii;
};

#define UNKNOWN_CHARACTER ' '  // Characters not in conversion table
extern ConversionItem tConversionTable[];


///////////////////////
// Moves the iterator to next unicode character in the string, returns number of bytes skipped
template<typename _Iterator1, typename _Iterator2>
inline size_t IncUtf8StringIterator(_Iterator1& it, const _Iterator2& last) {
	if(it == last) return 0;
	unsigned char c;
	size_t res = 1;
	for(++it; last != it; ++it, ++res) {
		c = *it;
		if(!(c&0x80) || ((c&0xC0) == 0xC0)) break;
	}

	return res;
}

template<typename _Iterator>
inline size_t MultIncUtf8StringIterator(_Iterator& it, const _Iterator& last, size_t count) {
	size_t res = 0;
	for(size_t i = 0; i < count; i++) {
		if(it == last) break;
		res += IncUtf8StringIterator(it, last);
	}

	return res;
}

///////////////////
// The iterator points at first byte of the UTF8 encoded character, returns number of bytes skipped
template<typename _Iterator1, typename _Iterator2>
inline size_t DecUtf8StringIterator(_Iterator1& it, const _Iterator2& first) {
	if(it == first) return 0;
	size_t res = 1;
	unsigned char c;
	--it;
	for(; first != it; --it, ++res) {
		c = *it;
		if(!(c&0x80) || ((c&0xC0) == 0xC0)) break;
	}

	return res;
}

template<typename _Iterator>
inline _Iterator GetMultIncUtf8StringIterator(_Iterator it, const _Iterator& last, size_t count) {
	MultIncUtf8StringIterator(it, last, count);
	return it;
}

inline std::string::const_iterator Utf8PositionToIterator(const std::string& str, size_t pos) {
	std::string::const_iterator res = str.begin();
	MultIncUtf8StringIterator(res, str.end(), pos);
	return res;
}

inline std::string::iterator Utf8PositionToIterator(std::string& str, size_t pos) {
	std::string::iterator res = str.begin();
	MultIncUtf8StringIterator(res, str.end(), pos);
	return res;
}



////////////////////////
// Reads next unicode character from a UTF8 encoded string
// the iterator shows at the next character after this operation
UnicodeChar GetNextUnicodeFromUtf8(std::string::const_iterator &it, const std::string::const_iterator& last, size_t& num_skipped);
inline UnicodeChar GetNextUnicodeFromUtf8(std::string::const_iterator& it, const std::string::const_iterator& last)  {
	size_t tmp;	return GetNextUnicodeFromUtf8(it, last, tmp); }

inline UnicodeChar GetUnicodeFromUtf8(const std::string& str, size_t pos) {
	std::string::const_iterator it = Utf8PositionToIterator(str, pos);
	return GetNextUnicodeFromUtf8(it, str.end());
}

////////////////////
// Gets the UTF8 representation of the unicode character (can be more bytes)
std::string GetUtf8FromUnicode(UnicodeChar ch);



inline size_t Utf8StringSize(const std::string& str)  {
	size_t res = 0;
	std::string::const_iterator it = str.begin();
	for(; it != str.end(); IncUtf8StringIterator(it, str.end()))
		res++;

	return res;
}

inline std::string Utf8SubStr(const std::string& str, size_t start, size_t n = (size_t)-1) {
	if (n == (size_t)-1)
		return std::string(Utf8PositionToIterator(str, start), str.end());
	else
		return std::string(
			Utf8PositionToIterator(str, start),
			Utf8PositionToIterator(str, start + n));
}

inline void Utf8Erase(std::string& str, size_t start, size_t n = (size_t)-1) {
	std::string::iterator it = Utf8PositionToIterator(str, start);
	str.erase(it, GetMultIncUtf8StringIterator(it, str.end(), n));	
}

inline void Utf8Insert(std::string& str, size_t start, const std::string& s) {
	str.insert(Utf8PositionToIterator(str, start), s.begin(), s.end());
}

inline void InsertUnicodeChar(std::string& str, size_t pos, UnicodeChar ch) {
	std::string tmp = GetUtf8FromUnicode(ch);
	Utf8Insert(str, pos, tmp);
}

// Uppercase/lowercase handling
UnicodeChar	UnicodeToLower(UnicodeChar c);
UnicodeChar	UnicodeToUpper(UnicodeChar c);

// Conversion functions

int FindTableIndex(UnicodeChar c);
char UnicodeCharToAsciiChar(UnicodeChar c);
std::string RemoveSpecialChars(const std::string &Utf8String);
std::string Utf16ToUtf8(const Utf16String& str);
Utf16String Utf8ToUtf16(const std::string& str);
std::string UnicodeToUtf8(const Unicode32String& str);
Unicode32String Utf8ToUnicode(const std::string& str);
std::string UnicodeToAscii(const std::string& utf8str);
std::string ISO88591ToUtf8(const std::string& isostr);
#ifdef WIN32
std::string Utf8ToSystemNative(const std::string& utf8str);
std::string SystemNativeToUtf8(const std::string& natstr);
#else // Other platforms use natively utf8 (at least we suppose so)
inline std::string Utf8ToSystemNative(const std::string& utf8str) { return utf8str; }
inline std::string SystemNativeToUtf8(const std::string& natstr) { return natstr; }
#endif

size_t TransformRawToUtf8Pos(const std::string& text, size_t pos);
size_t TransformUtf8PosToRaw(const std::string& text, size_t pos);
inline size_t TransformRawToUtf8ToRaw(const std::string& src, size_t srcpos, const std::string& dest) {
	return TransformUtf8PosToRaw(dest, TransformRawToUtf8Pos(src, srcpos));
}

#endif
