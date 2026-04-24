#ifndef OLX_STRINGCONV_H
#define OLX_STRINGCONV_H

#include <string>
#include <sstream>
#include "CodeAttributes.h"


/////////////
// C-string itoa for non-windows compilers (on Windows it's defined in windows.h)
#ifndef WIN32
// TODOL remove this
INLINE char* itoa(int val, char* buf, int base) {
	int i = 29; // TODO: bad style
	buf[i+1] = '\0';

	do {
		buf = "0123456789abcdefghijklmnopqrstuvwxyz"[val % base] + buf;
		--i, val /= base;
	} while(val && i);

	return &buf[i+1];
}

// Cross-compiler compatibility
#	define		stricmp		strcasecmp
#endif


template<typename T>
T from_string(const std::string& s, std::ios_base& (*f)(std::ios_base&), bool& failed) {
	std::istringstream iss(s); T t = T();
	failed = (iss >> f >> t).fail();
	return t;
}

template<typename T>
T from_string(const std::string& s, std::ios_base& (*f)(std::ios_base&)) {
	std::istringstream iss(s); T t = T();
	iss >> f >> t;
	return t;
}

template<typename T>
T from_string(const std::string& s, bool& failed) {
	std::istringstream iss(s); T t = T();
	failed = (iss >> t).fail();
	return t;
}


// Conversion functions from numbers to string

template<typename T>
std::string to_string(T val) {
	std::ostringstream oss;
	oss << val;
	return oss.str();
}

template<>
INLINE std::string to_string<bool>(bool val) {
	if(val) return "true"; else return "false";
}

template<>
INLINE std::string to_string<const char*>(const char* val) {
	if(val) return val; else return "";
}

template<> bool from_string<bool>(const std::string& s, bool& fail);


template<typename T>
T from_string(const std::string& s) {
	bool fail; return from_string<T>(s, fail);
}

INLINE int atoi(const std::string& str)  { return from_string<int>(str);  }
INLINE float atof(const std::string& str) { return from_string<float>(str);  }


INLINE std::string ftoa(float val, int precision = -1)
{
	std::string res = to_string<float>(val);
	if (precision != -1)  {
		size_t dotpos = res.find_last_of('.');
		if (dotpos == std::string::npos)  {
			res += '.';
			for (int i = 0; i < precision; i++)
				res += '0';
		} else {
			res = res.substr(0, dotpos + precision);
		}
	}

	return res;
}

INLINE std::string itoa(unsigned long num, short base=10)  {
	std::string buf;

	do {
		buf = "0123456789abcdefghijklmnopqrstuvwxyz"[num % base] + buf;
		num /= base;
	} while(num);

	return buf;
}

// std::string itoa
INLINE std::string itoa(long num, short base=10)  {
	if(num >= 0)
		return itoa((unsigned long)num, base);
	else
		return "-" + itoa((unsigned long)-num, base);
}

INLINE std::string itoa(int num, short base=10)  { return itoa((long)num,base); }
INLINE std::string itoa(unsigned int num, short base=10)  { return itoa((unsigned long)num,base); }

// If 64-bit long available?
#ifdef ULLONG_MAX
INLINE std::string itoa(unsigned long long num, short base=10)  {
	std::string buf;

	do {
		buf = "0123456789abcdefghijklmnopqrstuvwxyz"[num % base] + buf;
		num /= base;
	} while(num);

	return buf;
}
#endif

template<typename _T> std::string hex(_T num) { return itoa(num,16); }

#endif // STRINGCONV_H
