/*
 *  Bitstream.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 15.02.10.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */

#include "Bitstream.h"
#include "Debug.h"
#include "EndianSwap.h"


BitStream::BitStream(const std::string& raw) {
	m_readPos = 0;
	m_data.reserve( raw.size() * 8 );
	for(size_t i = 0; i < raw.size(); ++i)
		addInt((unsigned char) raw[i], 8);
}

// Grows the bit stream if the number of bits that are going to be added exceeds the buffer size
void BitStream::growIfNeeded(size_t addBits)
{
	m_data.reserve(m_data.size() + addBits);
}

void BitStream::writeBits(const std::vector<bool>& bits)
{
	growIfNeeded(bits.size());
	for(std::vector<bool>::const_iterator i = bits.begin(); i != bits.end(); ++i)
		m_data.push_back(*i);
}

std::vector<bool> BitStream::readBits(size_t bitCount)
{
	std::vector<bool> ret;
	ret.reserve(bitCount);
	for(size_t i = 0; i < bitCount && m_readPos < m_data.size(); ++i, ++m_readPos)
		ret.push_back(m_data[m_readPos]);
	if(ret.size() < bitCount)
		errors << "BitStream::readBits: reading from behind end" << endl;
	return ret;
}

void BitStream::addBool(bool b) {
	m_data.push_back(b);
}

void BitStream::addInt(uint32_t n, int bits) {
	growIfNeeded(bits);
	for(int i = 0; i < bits; ++i)
		m_data.push_back( ((n >> i) & 1) != 0 );
}

void BitStream::addSignedInt(int32_t n, int bits) {
	growIfNeeded(bits);
	addBool( n < 0 );
	if( n >= 0)
		addInt(n, bits - 1);
	else
		addInt(((uint32_t)1 << (bits - 1)) - n, bits - 1);
}

void BitStream::addFloat(float f, int bits) {
	// TODO: check bits
	union  {
		char bytes[4];
		float f;
	} data;
	data.f = f;
	BEndianSwap(data.f);
	addBitStream( BitStream( std::string(data.bytes, 4) ) );
}

void BitStream::addBitStream(const BitStream& str) {
	writeBits(str.m_data);
}

void BitStream::addString(const std::string& str) {
	std::string::const_iterator end = str.end();
	for(std::string::const_iterator i = str.begin(); i != end; ++i)
		if(*i == 0) {
			warnings << "BitStream::addString: strings contains NULL-char: " << str << endl;
			end = i;
		}
	
	std::string raw(str.begin(), end);
	raw += '\0';
	addBitStream(BitStream(raw));
}

bool BitStream::getBool() {
	if(m_readPos < m_data.size()) {
		bool ret = m_data[m_readPos];
		m_readPos++;
		return ret;
	}
	
	errors << "BitStream::getBool: reading from behind end" << endl;
	return false;
}

uint32_t BitStream::getInt(int bits) {
	uint32_t ret = 0;
	for(int i = 0; i < bits; ++i) {
		bool b = getBool();
		// Bits beyond 32 cannot fit the result; still consume them from the
		// stream, but keep the shift below the width to avoid the undefined
		// behaviour of shifting a 32-bit value by >= 32 (bits is caller- or,
		// via elias-gamma, stream-controlled).
		if(b && i < 32) ret |= (uint32_t)1 << i;
	}
	return ret;
}

int32_t BitStream::getSignedInt(int bits) {
	bool sign = getBool();
	
	if( !sign /*n >= 0*/ )
		return getInt(bits - 1);
	else
		return ((uint32_t)1 << (bits - 1)) - getInt(bits - 1);
}

float BitStream::getFloat(int bits) {
	// TODO: see addFloat, check bits
	union  {
		char bytes[4];
		float f;
	} data;
	
	for(int i = 0; i < 4; ++i)
		data.bytes[i] = getCharFromBits(*this);
	BEndianSwap(data.f);
	
	return data.f;
}

std::string BitStream::getString() {
	std::string ret;
	while(char c = getCharFromBits(*this))
		ret += c;
	return ret;
}

BitStream* BitStream::Duplicate() { 
	return new BitStream(*this);
}

void BitStream::reset()
{
	m_readPos = 0;
	m_data.clear();
}

//
// BitStream Tests
//
bool BitStream::testBool()
{
	reset();
	addBool(true);
	addBool(false);
	bool r1 = getBool();
	bool r2 = getBool();
	return r1 && !r2;
}

bool BitStream::testInt()
{
	reset();
	for (int i = 0; i < 32; i++)
		addInt((uint32_t)1 << i, i + 1);
	for (int i = 0; i < 32; i++)
		if (getInt(i + 1) != (uint32_t(1) << i))
			return false;
	return true;
}

bool BitStream::testFloat()
{
	reset();
	union T  {
		int n;
		float f;
	};
	
	for (int i = 0; i < 32; i++)  {
		T t; t.n = 1 << i;
		addFloat(t.f, i + 1);
	}
	for (int i = 0; i < 32; i++)  {
		T t; t.f = getFloat(i + 1);
		if (t.n != (1 << i))
			return false;
	}
	return true;
}

bool BitStream::testStream()
{
	reset();
	BitStream s2;
	s2.addBool(true);
	s2.addBool(false);
	s2.addInt(50, 32);
	s2.addInt(60, 16);
	s2.addInt(30, 5);
	addInt(14, 4);
	addBool(true);
	addBitStream(s2);
	if (getInt(4) != 14)
		return false;
	if (getBool() != true)
		return false;
	if (getBool() != true)
		return false;
	if (getBool() != false)
		return false;
	if (getInt(32) != 50)
		return false;
	if (getInt(16) != 60)
		return false;
	if (getInt(5) != 30)
		return false;
	return true;
}

bool BitStream::testSafety()
{
	try {
		reset();
		getBool();
		addInt(5, 5);
		getInt(6);
		addFloat(0.0f, 32);
		getFloat(31);
		getInt(5);
	} catch (...)  {
		return false;
	}
	return true;
}

bool BitStream::testString()
{
	addBool(true);
	addString("some short text to test bitstream");
	if (getBool() != true)
		return false;
	if (std::string(getString()) != "some short text to test bitstream")
		return false;
	return true;
}

bool BitStream::runTests()
{
	bool res = true;
	if (!testBool())  {
		printf("Boolean test failed!\n");
		res = false;
	}
	if (!testInt())  {
		printf("Integer test failed\n");
		res = false;
	}
	if (!testFloat())  {
		printf("Float test failed\n");
		res = false;
	}
	if (!testStream())  {
		printf("Stream test failed\n");
		res = false;
	}
	if (!testString())  {
		printf("String test failed\n");
		res = false;
	}
	if (!testSafety())  {
		printf("Safety test failed\n");
		res = false;
	}
	return res;
}
