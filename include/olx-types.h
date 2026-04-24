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


// Type definitions
// Created on 29/11/01
// By Jason Boettcher

// changed by US


#ifndef __OLX_TYPES_H__
#define __OLX_TYPES_H__

#include <SDL.h>
#include <cassert>
#include <cstdint>

struct TimeDiff {
	Uint64 timeDiff;
	// Explicit constructors needed 'cause TimeDiff(float) != TimeDiff(int), and we will have hard-to-find errors in expressions
	explicit TimeDiff(Uint64 td = 0) : timeDiff(td) {}
	explicit TimeDiff(float seconds) : timeDiff((Uint64)(seconds * 1000.0f)) { assert(seconds >= 0.0f); }
	explicit TimeDiff(int td) : timeDiff(td) { assert(td >= 0); }
	explicit TimeDiff(Uint64 start, Uint64 end) : timeDiff(end - start) { assert(end >= start); }

	const TimeDiff & operator=(float seconds) { *this = TimeDiff(seconds); return *this; };
	// No operator=(int) here, 'cause it will be in milliseconds - inconsistent, and hard-to-find errors occur
	
	float seconds() const { return timeDiff * 0.001f; }
	Uint64 milliseconds() const { return timeDiff; }
	
	TimeDiff operator+(const TimeDiff& td) const { return TimeDiff(timeDiff + td.timeDiff); }
	TimeDiff operator-(const TimeDiff& td) const { assert(timeDiff >= td.timeDiff); return TimeDiff(timeDiff - td.timeDiff); }
	float operator/(const TimeDiff& td) const { return (float)timeDiff / (float)td.timeDiff; }
	TimeDiff operator*(float f) const { return TimeDiff((Uint64)(timeDiff * f)); }
	
	TimeDiff& operator+=(const TimeDiff& td) { timeDiff += td.timeDiff; return *this; }
	TimeDiff& operator-=(const TimeDiff& td) { assert(timeDiff >= td.timeDiff); timeDiff -= td.timeDiff; return *this; }
	TimeDiff& operator*=(float f) { *this = (*this) * f; return *this; }
	
	bool operator<(const TimeDiff& td) const { return timeDiff < td.timeDiff; }
	bool operator>(const TimeDiff& td) const { return timeDiff > td.timeDiff; }
	bool operator<=(const TimeDiff& td) const { return timeDiff <= td.timeDiff; }
	bool operator>=(const TimeDiff& td) const { return timeDiff >= td.timeDiff; }
	bool operator==(const TimeDiff& td) const { return timeDiff == td.timeDiff; }
	bool operator!=(const TimeDiff& td) const { return timeDiff != td.timeDiff; }	

	bool operator<(float td) const { return *this < TimeDiff(td); }
	bool operator>(float td) const { return *this > TimeDiff(td); }
	bool operator<=(float td) const { return *this <= TimeDiff(td); }
	bool operator>=(float td) const { return *this >= TimeDiff(td); }

};

struct AbsTime {
	Uint64 time;
	explicit AbsTime(Uint64 t = 0) : time(t) {}
	explicit AbsTime(int t) : time(t) {}
	explicit AbsTime(float seconds) : time( (Uint64)(seconds * 1000.0f) ) {}
	static AbsTime Max() { return AbsTime((Uint64)-1); }

	const AbsTime & operator=(float seconds) { *this = AbsTime(seconds); return *this; };
	// No operator=(int) here, 'cause it will be in milliseconds - inconsistent, and hard-to-find errors occur

	float seconds() const { return time * 0.001f; }
	Uint64 milliseconds() const { return time; }
	
	TimeDiff operator-(const AbsTime& t) const { return TimeDiff(t.time, time); }
	AbsTime operator+(const TimeDiff& td) const { return AbsTime(time + td.timeDiff); }
	AbsTime operator-(const TimeDiff& td) const { return AbsTime(time - td.timeDiff); }
	AbsTime operator+(float td) const { return AbsTime(time + (Uint64)(td * 1000.0f)); }
	// Remake all your expressions so they will not include AbsTime minus something - it wraps around easily
	//AbsTime operator-(float td) const { return AbsTime(time - (Uint64)(td * 1000.0f)); }
	
	AbsTime& operator+=(const TimeDiff& td) { time += td.timeDiff; return *this; }
	
	bool operator<(const AbsTime& t) const { return time < t.time; }
	bool operator>(const AbsTime& t) const { return time > t.time; }
	bool operator<=(const AbsTime& t) const { return time <= t.time; }
	bool operator>=(const AbsTime& t) const { return time >= t.time; }
	bool operator==(const AbsTime& t) const { return time == t.time; }	
	bool operator!=(const AbsTime& t) const { return time != t.time; }	
};

typedef unsigned int	uint;
typedef unsigned char	uchar;
typedef unsigned long	ulong;
typedef unsigned char	uint24[3];

typedef uchar byte;
typedef unsigned short ushort;

// Class for having a null point, to allow overwriting a function which takes null.
// HINT: Don't ever use this class for something different. Every use of this function
// interprets it to have this parameter null.
class Null {}; extern Null null;

#endif  // __TYPES_H__

