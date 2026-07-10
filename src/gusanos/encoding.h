#ifndef VERMES_ENCODING_H
#define VERMES_ENCODING_H

#include <utility>
#include "gui/omfggui.h" // For Rect
#include "netstream.h"
#include "util/Bitstream.h"
#include <iostream>
#include <stdexcept>
#include "Debug.h"
#include "CodeAttributes.h"

using std::cerr;
using std::endl;

namespace Encoding
{
	
INLINE unsigned int bitsOf(unsigned long n)
{
	unsigned int bits = 0;
	for(; n; n >>= 1)
		bits++;
		
	return bits;
}

INLINE void encode(BitStream& stream, int i, int count)
{
	if(count <= 0) {
		errors << "encode: count =" << count << endl;
		return;
	}
	if(i < 0 || i >= count) {
		errors << "encode: i = " << i << ", count = " << count << endl;
		return;
	}
	stream.addInt(i, bitsOf(count - 1));
}

INLINE int decode(BitStream& stream, int count)
{
	if(count <= 0) {
		errors << "decode: count = " << count << endl;
		return 0;
	}
	return stream.getInt(bitsOf(count - 1));
}

INLINE unsigned int signedToUnsigned(int n)
{
	if(n < 0)
		return ((-n) << 1) | 1;
	else
		return n << 1;
}

INLINE int unsignedToSigned(unsigned int n)
{
	if(n & 1)
		return -(int)(n >> 1);
	else
		return (n >> 1);
}

INLINE void encodeBit(BitStream& stream, int bit)
{
	stream.addInt(bit, 1);
}

INLINE int decodeBit(BitStream& stream)
{
	return stream.getInt(1);
}

INLINE void encodeEliasGamma(BitStream& stream, unsigned int n)
{
	if(n < 1)
		throw std::runtime_error("encodeEliasGamma can't encode 0");
		
	int prefix = bitsOf(n);

	for(int i = 0; i < prefix - 1; ++i)
		encodeBit(stream, 0);

	encodeBit(stream, 1);
	stream.addInt(n, prefix - 1);
}

INLINE unsigned int decodeEliasGamma(BitStream& stream)
{
	int prefix = 0;
	for(; decodeBit(stream) == 0 && stream.bitPos() < stream.bitSize(); )
		++prefix;
	
	if(stream.bitPos() >= stream.bitSize())
		// error - reached end		
		return 0;
	
	// prefix = number of prefixed zeroes
	
	// prefix comes from the (untrusted) stream and can exceed 31,
	// which would make 1 << prefix undefined; a bit that high does not fit
	// the unsigned-int result anyway, so drop it.
	return stream.getInt(prefix) | (prefix < 32 ? ((uint32_t)1 << prefix) : 0u);
}

INLINE void encodeEliasDelta(BitStream& stream, unsigned int n)
{
	if(n < 1) {
		errors << "encodeEliasDelta: n = " << n << endl;
		return;
	}
	int prefix = bitsOf(n);
	encodeEliasGamma(stream, prefix);
	stream.addInt(n, prefix - 1);
}

INLINE unsigned int decodeEliasDelta(BitStream& stream)
{
	int prefix = decodeEliasGamma(stream) - 1;
	if(prefix < 0)
		// error - probably end reached
		return 0;
	
	// prefix comes from the (untrusted) stream and can exceed 31,
	// which would make 1 << prefix undefined; a bit that high does not fit
	// the unsigned-int result anyway, so drop it.
	return stream.getInt(prefix) | (prefix < 32 ? ((uint32_t)1 << prefix) : 0u);
}

struct VectorEncoding
{
	VectorEncoding();
	
	VectorEncoding(Rect area_, int subPixelAcc_ = 1);
		
	template<class T>
	std::pair<long, long> quantize(T const& v)
	{
		long y = static_cast<long>((v.y - area.y1) * subPixelAcc + 0.5);
		if(y < 0)
			y = 0;
		else if(y > height)
			y = height - 1;
			
		long x = static_cast<long>((v.x - area.x1) * subPixelAcc + 0.5);
		if(x < 0)
			x = 0;
		else if(x > width)
			x = width - 1;
			
		return std::make_pair(x, y);
	}
	
	template<class T>
	void encode(BitStream& stream, T const& v)
	{
		long y = static_cast<long>((v.y - area.y1) * subPixelAcc + 0.5);
		if(y < 0)
			y = 0;
		else if(y > height)
			y = height - 1;
			
		long x = static_cast<long>((v.x - area.x1) * subPixelAcc + 0.5);
		if(x < 0)
			x = 0;
		else if(x > width)
			x = width - 1;
		
		stream.addInt(x, bitsX);
		stream.addInt(y, bitsY);
	}
	
	template<class T>
	T decode(BitStream& stream)
	{
		typedef typename T::manip_t manip_t;
		
		long x = stream.getInt(bitsX);
		long y = stream.getInt(bitsY);
		
		return T(manip_t(x) / subPixelAcc + area.x1, manip_t(y) / subPixelAcc + area.y1);
	}
	
	long totalBits()
	{
		return bitsX + bitsY;
	}
	
	Rect area;
	long total;
	long width;
	long height;
	long bitsX;
	long bitsY;
	
	int subPixelAcc;
	double isubPixelAcc;
};

struct DiffVectorEncoding
{
	DiffVectorEncoding(int subPixelAcc_ = 1);
	
	template<class T>
	std::pair<long, long> quantize(T const& v)
	{
		long y = static_cast<long>(v.y * subPixelAcc + 0.5);	
		long x = static_cast<long>(v.x * subPixelAcc + 0.5);

		return std::make_pair(x, y);
	}
	
	template<class T>
	void encode(BitStream& stream, T const& v)
	{
		long y = static_cast<long>(v.y * subPixelAcc + 0.5);	
		long x = static_cast<long>(v.x * subPixelAcc + 0.5);
		
		encodeEliasDelta(stream, signedToUnsigned(x) + 1);
		encodeEliasDelta(stream, signedToUnsigned(y) + 1);
	}
	
	template<class T>
	T decode(BitStream& stream)
	{
		typedef typename T::manip_t manip_t;
		
		long x = unsignedToSigned(decodeEliasDelta(stream) - 1);
		long y = unsignedToSigned(decodeEliasDelta(stream) - 1);
		
		return T(manip_t(x) / subPixelAcc, manip_t(y) / subPixelAcc);
	}

	int subPixelAcc;
};

}

#endif //VERMES_ENCODING_H
