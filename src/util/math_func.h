#ifndef VERMES_MATH_H
#define VERMES_MATH_H

#include <cmath>
#include <boost/random.hpp>
#include "CodeAttributes.h"

/**
 * Magic number goodness!
 */

extern boost::mt19937 rndgen;
extern boost::uniform_01<boost::mt19937> rnd;
extern boost::variate_generator<boost::mt19937, boost::uniform_real<> > midrnd;

float const Pi = 3.14159265358979323846f;

INLINE float deg2rad( float Degrees )
{
	return (Degrees * Pi) / 180;
}

INLINE float rad2deg( float Radians )
{
	return (Radians * 180) / Pi;
}

INLINE unsigned long rndInt(unsigned long max)
{
	return static_cast<unsigned long>(
		(static_cast<unsigned long long>(rndgen()) * max) >> (CHAR_BIT * sizeof(boost::mt19937::result_type))
	);
}

template<unsigned int B, unsigned int E>
//struct ctpow { static unsigned int const value = ctpow<B*B, E>>1>::value * ((E & 1) ? B : 1); };
struct ctpow { static unsigned int const value = ctpow<B*B, E>::value * ((E & 1) ? B : 1); };

template<unsigned int B>
struct ctpow<B, 0> { static unsigned int const value = 1; };

// MSVC 2013+ ships lrintf as an intrinsic in <cmath>, so defining
// another one trips C2169 ("intrinsic function, cannot be defined").
// Keep the inline replacement only for older toolchains that still
// need it.
#if defined(_MSC_VER) && _MSC_VER < 1800
INLINE long lrintf(float f){

#ifdef _M_X64
	//x64 fix
	//http://groups.google.com/group/openjpeg/browse_thread/thread/64b89cdb1a44bb3b
	return (long)((f > 0.0f) ? (f + 0.5f) : (f - 0.5f));
#else

	int i;
	_asm{
		fld f
		fistp i
	};
	return i;

#endif

}
#endif

INLINE long roundAny(float v)
{
	return lrintf(v);
}

#endif //VERMES_MATH_H
