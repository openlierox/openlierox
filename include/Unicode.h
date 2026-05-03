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
#include "CodeAttributes.h"

#ifdef __ANDROID__
// Android's libc++ only provides std::char_traits specialisations for
// char, wchar_t, char8_t, char16_t, char32_t. OpenLieroX instantiates
// std::basic_string with Uint16/Uint32 (unsigned short / unsigned int);
// add minimal specialisations for those here so the typedefs below
// compile. We only use these strings as buffers, so locale-aware bits
// are intentionally not implemented.
#include <cstring>
namespace std {
template<typename T>
struct __olx_int_char_traits {
	typedef T char_type;
	typedef int int_type;
	typedef streamoff off_type;
	typedef streampos pos_type;
	typedef mbstate_t state_type;
	static void assign(char_type& c1, const char_type& c2) noexcept { c1 = c2; }
	static constexpr bool eq(char_type a, char_type b) noexcept { return a == b; }
	static constexpr bool lt(char_type a, char_type b) noexcept { return a <  b; }
	static int compare(const char_type* s1, const char_type* s2, size_t n) {
		for (size_t i = 0; i < n; ++i) { if (s1[i] < s2[i]) return -1; if (s2[i] < s1[i]) return 1; } return 0;
	}
	static size_t length(const char_type* s) { size_t i = 0; while (s[i] != char_type()) ++i; return i; }
	static const char_type* find(const char_type* s, size_t n, const char_type& a) {
		for (size_t i = 0; i < n; ++i) if (s[i] == a) return s + i; return nullptr;
	}
	static char_type* move(char_type* s1, const char_type* s2, size_t n) {
		return static_cast<char_type*>(memmove(s1, s2, n * sizeof(char_type)));
	}
	static char_type* copy(char_type* s1, const char_type* s2, size_t n) {
		return static_cast<char_type*>(memcpy(s1, s2, n * sizeof(char_type)));
	}
	static char_type* assign(char_type* s, size_t n, char_type a) {
		for (size_t i = 0; i < n; ++i) s[i] = a; return s;
	}
	static constexpr int_type  not_eof(int_type c) noexcept  { return c == eof() ? 0 : c; }
	static constexpr char_type to_char_type(int_type c) noexcept { return char_type(c); }
	static constexpr int_type  to_int_type(char_type c) noexcept { return int_type(c); }
	static constexpr bool      eq_int_type(int_type a, int_type b) noexcept { return a == b; }
	static constexpr int_type  eof() noexcept { return -1; }
};
template<> struct char_traits<unsigned short> : __olx_int_char_traits<unsigned short> {};
template<> struct char_traits<unsigned int>   : __olx_int_char_traits<unsigned int>   {};
}
#endif // __ANDROID__

typedef Uint32 UnicodeChar;
typedef std::basic_string<UnicodeChar> Unicode32String;
#ifdef WIN32
typedef wchar_t Utf16Char;
typedef std::wstring Utf16String;
#else
typedef Uint16 Utf16Char;
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
INLINE size_t IncUtf8StringIterator(_Iterator1& it, const _Iterator2& last) {
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
INLINE size_t MultIncUtf8StringIterator(_Iterator& it, const _Iterator& last, size_t count) {
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
INLINE size_t DecUtf8StringIterator(_Iterator1& it, const _Iterator2& first) {
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
INLINE _Iterator GetMultIncUtf8StringIterator(_Iterator it, const _Iterator& last, size_t count) {
	MultIncUtf8StringIterator(it, last, count);
	return it;
}

INLINE std::string::const_iterator Utf8PositionToIterator(const std::string& str, size_t pos) {
	std::string::const_iterator res = str.begin();
	MultIncUtf8StringIterator(res, str.end(), pos);
	return res;
}

INLINE std::string::iterator Utf8PositionToIterator(std::string& str, size_t pos) {
	std::string::iterator res = str.begin();
	MultIncUtf8StringIterator(res, str.end(), pos);
	return res;
}



////////////////////////
// Reads next unicode character from a UTF8 encoded string
// the iterator shows at the next character after this operation

////////////////////
// Convert UTF8 to unicode (takes iterator pointing to the first UTF8-encoded character)
template<typename It1, typename It2>
inline UnicodeChar GetNextUnicodeFromUtf8(It1 &it, const It2& last, size_t& num_skipped) {
	num_skipped = 0;
	if(it == last) return 0;
	
	unsigned char ch = *it;
	UnicodeChar res = ch;
	if ( ch >= 0xFC ) {
		res  =  (ch&0x01) << 30; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 24; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 18; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 12; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 6; it++;  num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F);
	} else
	if ( ch >= 0xF8 ) {
		res  =  (ch&0x03) << 24; num_skipped++; it++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 18; num_skipped++; it++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 12; num_skipped++; it++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 6;  num_skipped++; it++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F);
	} else
	if ( ch >= 0xF0 ) {
		res  =  (ch&0x07) << 18; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 12; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 6; it++;  num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F);
	} else
	if ( ch >= 0xE0 ) {
		res  =  (ch&0x0F) << 12; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F) << 6; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F);
	} else
	if ( ch >= 0xC0 ) {
		res  =  (ch&0x1F) << 6; it++; num_skipped++; if(it == last) return 0; ch = *it;
		res |=  (ch&0x3F);
	}

	it++; num_skipped++;
	return res;
}

template<typename It1, typename It2>
inline UnicodeChar GetNextUnicodeFromUtf8(It1& it, const It2& last) {
	size_t tmp;
	return GetNextUnicodeFromUtf8(it, last, tmp);
}

INLINE UnicodeChar GetUnicodeFromUtf8(const std::string& str, size_t pos) {
	std::string::const_iterator it = Utf8PositionToIterator(str, pos);
	return GetNextUnicodeFromUtf8(it, str.end());
}

////////////////////
// Gets the UTF8 representation of the unicode character (can be more bytes)
std::string GetUtf8FromUnicode(UnicodeChar ch);



INLINE size_t Utf8StringSize(const std::string& str)  {
	size_t res = 0;
	std::string::const_iterator it = str.begin();
	for(; it != str.end(); IncUtf8StringIterator(it, str.end()))
		res++;

	return res;
}

INLINE std::string Utf8SubStr(const std::string& str, size_t start, size_t n = (size_t)-1) {
	if (n == (size_t)-1)
		return std::string(Utf8PositionToIterator(str, start), str.end());
	else
		return std::string(
			Utf8PositionToIterator(str, start),
			Utf8PositionToIterator(str, start + n));
}

INLINE void Utf8Erase(std::string& str, size_t start, size_t n = (size_t)-1) {
	std::string::iterator it = Utf8PositionToIterator(str, start);
	str.erase(it, GetMultIncUtf8StringIterator(it, str.end(), n));	
}

INLINE void Utf8Insert(std::string& str, size_t start, const std::string& s) {
	str.insert(Utf8PositionToIterator(str, start), s.begin(), s.end());
}

INLINE void InsertUnicodeChar(std::string& str, size_t pos, UnicodeChar ch) {
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
INLINE std::string Utf8ToSystemNative(const std::string& utf8str) { return utf8str; }
INLINE std::string SystemNativeToUtf8(const std::string& natstr) { return natstr; }
#endif

size_t TransformRawToUtf8Pos(const std::string& text, size_t pos);
size_t TransformUtf8PosToRaw(const std::string& text, size_t pos);
INLINE size_t TransformRawToUtf8ToRaw(const std::string& src, size_t srcpos, const std::string& dest) {
	return TransformUtf8PosToRaw(dest, TransformRawToUtf8Pos(src, srcpos));
}

#endif
