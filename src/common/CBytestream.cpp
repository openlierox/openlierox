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


// Byte stream class
// Created 13/10/01
// Jason Boettcher


#include <cassert>
#include <stdarg.h>
#include <iomanip>

#include "CBytestream.h"
#include "EndianSwap.h"
#include "StringUtils.h"
#include "MathLib.h"
#include "CScriptableVars.h"
#include "Debug.h"
#include "Iter.h"
#include "Utils.h"
#include "util/CustomVar.h"


void CBytestream::Test()
{
	notes << endl;
	notes << "Running a Bytestream debug test:" << endl;
	notes << "Tested function / Original / Write / Read / Warning" << endl;

	// Byte
	uchar b = 125;
	notes << "Byte: (" << b << ") ";
	writeByte(b);
	ResetPosToBegin();
	notes << "(" << Data << ") ";
	uchar b2 = readByte();
	notes << "(" << b2 << ") ";
	if (b2 != b)
		notes << "NOT SAME!";
	notes <<endl;
	Clear();

	// Bool
	bool boo = true;
	notes << "Bool: (" << boo << ") ";
	writeByte(boo);
	ResetPosToBegin();
	notes << "(" << Data << ") ";
	bool boo2 = readBool();
	notes << "(" << boo2 << ") ";
	if (boo2 != boo)
		notes << "NOT SAME!";
	notes << endl;
	Clear();

	{
		// Integer
		int i = 125;
		notes << "Int: (" << i << ") ";
		writeInt(i, 4);
		ResetPosToBegin();
		notes << "(" << Data << ") ";
		int i2 = readInt(4);
		notes << "(" << itoa(i2) << ") ";
		if (i2 != i)
			notes << "NOT SAME!";
		notes <<endl;
		Clear();
	}

	{
		// Integer
		Sint16 i = -126;
		notes << "Int: (" << i << ") ";
		writeInt(i, 2);
		ResetPosToBegin();
		notes << "(" << Data << ") ";
		Sint16 i2 = readInt(2);
		notes << "(" << itoa(i2) << ") ";
		if (i2 != i)
			notes << "NOT SAME!";
		notes <<endl;
		Clear();
	}

	// Short
	short s = 125;
	notes << "Short: (" << s << ") ";
	writeInt16(s);
	ResetPosToBegin();
	notes << "(" << Data << ") ";
	short s2 = readInt16();
	notes << "(" << s2 << ") ";
	if (s2 != s)
		notes << "NOT SAME!";
	notes <<endl;
	Clear();

	// Float
	float f = 125.125f;
	notes << "Float: (" << f << ") ";
	writeFloat(f);
	ResetPosToBegin();
	notes << "(" << Data << ") ";
	float f2 = readFloat();
	notes << "(" << f2 << ") ";
	if (f2 != f)
		notes << "NOT SAME!";
	notes <<endl;
	Clear();

	// String
	std::string str = "Test";
	notes << "String: (" << str << ") ";
	writeString(str);
	ResetPosToBegin();
	notes << "(" << Data << ") ";
	std::string str2 = readString();
	notes << "(" << str2 << ") ";
	if (str2 != str)
		notes << "NOT SAME!";
	notes <<endl;
	Clear();

	// 2Int12
	short x = 125;
	short y = 521;
	notes << "2Int12: (" << x << "/" << y << ") ";
	write2Int12(x, y);
	ResetPosToBegin();
	notes << "(" << Data << ") ";
	short x2, y2;
	read2Int12(x2, y2);
	notes << "(" << x2 << "/" << y2 << ") ";
	if (x2 != x || y2 != y)
		notes << "NOT SAME!";
	notes <<endl;
	Clear();

	// 2Int4
	short u = 10;
	short v = 12;
	notes << "2Int4: (" << u << "/" << v << ") ";
	write2Int4(u, v);
	ResetPosToBegin();
	notes << "(" << Data << ") ";
	short u2, v2;
	read2Int4(u2, v2);
	notes << "(" << u2 << "/" << v2 << ") ";
	if (u2 != u || v2 != v)
		notes << "NOT SAME!";
	notes <<endl;
	Clear();
	
	// Bits
	writeBit(1); 
	writeBit(1); 
	writeBit(0); 
	writeBit(1);
	writeBit(1);
	writeBit(1);
	writeBit(0);
	writeBit(0);
	writeBit(1);
	notes << "Data.size() = " << Data.size() << " ";
	notes << "Bits: (" << (unsigned)Data[0] << ", " << (unsigned)Data[1] << ") ";
	ResetPosToBegin();
	if(	
		readBit() != 1 ||
		readBit() != 1 ||
		readBit() != 0 ||
		readBit() != 1 ||
		readBit() != 1 ||
		readBit() != 1 ||
		readBit() != 0 ||
		readBit() != 0 ||
		readBit() != 1
		)
		notes << "NOT SAME!";
	notes <<endl;
	Clear();

	std::string str1( "Lala\0_1\0!2345", 12 );
	notes << "Data: " << str1.size() << " ";
	writeData(str1);
	//ResetPosToBegin();
	readByte();
	if( readData(1) != "a" )
		notes << "NOT SAME!";
	str2 = readData();
	if( str2.size() <= 4 || str2[5] != 0 )
		notes << "\"" << str2 << "\" size " << str2.size() << " NOT SAME!";
	notes <<endl;
	Clear();

	// Bit iterator
	char bitData[10];
	bitData[0] = 0;
	bitData[1] = 0;
	CBytestreamBitIterator bitIter(bitData);
	bitIter.setBit(); ++bitIter;
	bitIter.setBit(); ++bitIter;
	++bitIter;
	bitIter.setBit(); ++bitIter;
	bitIter.setBit(); ++bitIter;
	bitIter.setBit(); ++bitIter;
	++bitIter;
	++bitIter;
	bitIter.setBit(); ++bitIter;
	bitIter.resetPos();
	notes << "Bits iterator: (" << (unsigned)bitData[0] << ", " << (unsigned)bitData[1] << ") ";
	if(	
		bitIter.readBit() != 1 ||
		bitIter.readBit() != 1 ||
		bitIter.readBit() != 0 ||
		bitIter.readBit() != 1 ||
		bitIter.readBit() != 1 ||
		bitIter.readBit() != 1 ||
		bitIter.readBit() != 0 ||
		bitIter.readBit() != 0 ||
		bitIter.readBit() != 1
		)
		notes << "NOT SAME!";
	notes <<endl;

}

void CBytestream::Clear() {
	Data = "";
	pos = 0;
	bitPos = 0;
}


///////////////////
// Append another bytestream onto this one
void CBytestream::Append(CBytestream *bs) {
	Data += bs->Data;
}


///////////////////
// Dump the data out
void CBytestream::Dump(const PrintOutFct& printer, const std::set<size_t>& marks, size_t start, size_t count) {
	Iterator<char>::Ref it = GetConstIterator(Data);
	if(start > 0) it->nextn(start);
	HexDump(it, printer, marks, count);
}

void CBytestream::Dump() {
	Dump(PrintOnLogger(notes), Set(pos));
}


// Writes


///////////////////
// Writes a single byte
bool CBytestream::writeByte(uchar byte)
{
	Data += byte;
	return true;
}


///////////////////
// Writes a boolean value to the stream
bool CBytestream::writeBool(bool value)
{
	return writeByte((uchar)value);
}


///////////////////
// Writes an integer to the stream
bool CBytestream::writeInt(int value, uchar numbytes)
{
	// Numbytes cannot be more then 4
	assert(numbytes > 0 && numbytes < 5);

	// HINT: we send always in little endian
	Uint32 val = (Uint32)value;
	EndianSwap(val);

	for(short n = 0; n < numbytes; n++)
		writeByte( ((uchar *)&val)[n] );

	return true;
}

bool CBytestream::writeUInt64(Uint64 val) {
	EndianSwap(val);
	
	for(short n = 0; n < 8; n++)
		writeByte( ((uchar *)&val)[n] );
	
	return true;
}

///////////////////
// Write a short to the stream
bool CBytestream::writeInt16(Sint16 value)
{
	// HINT: this time, the value is stored in big endian
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	ByteSwap5(value);
#endif

	writeByte( ((uchar *)&value)[0]);
	writeByte( ((uchar *)&value)[1]);

	return true;
}


///////////////////
// Writes a float to the stream
bool CBytestream::writeFloat(float value)
{
	union {
		uchar bin[4];
		float val;
	} tmp;
	tmp.val = value;

	// HINT: original LX uses little endian floats over network
	EndianSwap(tmp.bin);

	for(short i = 0; i < 4; i++)
		writeByte(tmp.bin[i]);
			
	return true;
}


bool CBytestream::writeString(const std::string& value) {
	Data += value.c_str(); // convert it to a C-string because we don't want null-bytes in it
	Data += (char)'\0';
	
	return true;
}

// cast 2 int12 to 3 bytes
bool CBytestream::write2Int12(short x, short y) {
	writeByte((ushort)x & 0xff);
	writeByte((((ushort)x & 0xf00) >> 8) + (((ushort)y & 0xf) << 4));
	writeByte(((ushort)y & 0xff0) >> 4);
	return true;
}

// cast 2 int4 to 1 byte
bool CBytestream::write2Int4(short x, short y) {
	return writeByte(((ushort)x & 0xf) + (((ushort)y & 0xf) << 4));
}


bool CBytestream::writeBit(bool bit)
{
	if( bitPos == 0 )
		writeByte( 0 );
	uchar byte = Data[ Data.size() - 1 ];
	byte = byte | ( ( bit ? 1 : 0 ) << bitPos );
	Data[ Data.size() - 1 ] = byte;
	bitPos ++; bitPos %= 8;
	return true;
}

bool CBytestream::writeData(const std::string& value)
{
	Data.append( value );
	return true;
}

bool CBytestream::writeVar(const ScriptVar_t& var, const CustomVar* diffToOld) {
	assert( var.type >= SVT_BOOL && var.type <= SVT_CustomWeakRefToStatic );
	if(var.isCustomType()) {
		assert(var.customVar() != NULL);
		assert(var.customVar()->thisRef.classId != ClassId(-1));
	}
	if(diffToOld) {
		assert(var.isCustomType());
		assert(var.customVar()->thisRef.classId == diffToOld->thisRef.classId);
	}
	if(!writeByte( var.type )) return false;
	switch( var.type ) {
	case SVT_BOOL: return writeBool(var.toBool());
	case SVT_INT32: return writeInt(var.toInt(), 4);
	case SVT_UINT64: return writeUInt64((uint64_t)var);
	case SVT_FLOAT: return writeFloat(var.toFloat());
	case SVT_VEC2: {
		writeFloat(CVec(var).x);
		writeFloat(CVec(var).y);
		return true;
	}
	case SVT_COLOR: {
		writeByte(var.toColor().r);
		writeByte(var.toColor().g);
		writeByte(var.toColor().b);
		writeByte(var.toColor().a);
		return true;
	}
	case SVT_STRING: return writeString(var.toString());
	case SVT_CUSTOM:
	case SVT_CustomWeakRefToStatic:
		return var.customVar()->ToBytestream(this, diffToOld);
	case SVT_CALLBACK:
	case SVT_DYNAMIC:
		assert(false); // should not happen
	}
	assert(false); // should not happen
	return false;
}



// Reads




///////////////////
// Reads a single byte
uchar CBytestream::readByte() {
	if(!isPosAtEnd())
		return Data[pos++];
	else {
#ifndef FUZZY_ERROR_TESTING
		errors <<"reading from stream behind end" << endl;
#endif
		return 0;
	}
}


///////////////////
// Reads a boolean value from the stream
bool CBytestream::readBool()
{
	return readByte() != 0;
}


///////////////////
// Reads an interger value from the stream
int CBytestream::readInt(uchar numbytes)
{
	// Numbytes cannot be more than 4
	if(numbytes <= 0 || numbytes >= 5)
		return 0;

	Uint32 ret = 0;
	for(short n=0; n<numbytes; n++)
		ret += (Uint32)readByte() << (n * 8);
	
	return (unsigned)ret;
}

Uint64 CBytestream::readUInt64()
{
	
	Uint64 ret = 0;
	for(short n=0; n<8; n++)
		ret += (Uint64)readByte() << (n * 8);
	
	return ret;
}




///////////////////
// Read a short from the stream
Sint16 CBytestream::readInt16()
{
	// HINT: this time, the value is stored in big endian
	uchar dat[2];
	dat[1] = readByte();
	dat[0] = readByte();

	Uint16 value;
	value = (Uint16)dat[0];
	value += (Uint16)dat[1] << 8;

	return (Sint16)value;
}



///////////////////
// Read a float value from the stream
float CBytestream::readFloat()
{
	union {
		uchar bin[4];
		float val;
	} tmp;

	tmp.val = 0;
	
	for(short i = 0; i < 4; i++)
		tmp.bin[i] = readByte();

	// HINT: original LX uses little endian floats over network
	EndianSwap(tmp.bin);

	return tmp.val;
}


std::string CBytestream::readString() {
	std::string result = "";
	uchar b;
	while((b = readByte()) != 0) result += (char)b;
	return result;
}

std::string CBytestream::readString(size_t maxlen) {
	std::string result = "";
	size_t i = 0;
	uchar b = 0;
	while(i < maxlen && (b = readByte()) != 0) {
		result += b;
		++i;
	}
	if(b != 0)
		warnings("WARNING: CBytestream: stop reading string at no real ending\n");
	return result;
}

////////////////////
// cast 3 bytes to 2 int12
void CBytestream::read2Int12(short& x, short& y) {
	ushort dat[3];
	dat[0] = readByte();
	dat[1] = readByte();
	dat[2] = readByte();

	x = dat[0] + ((dat[1] & 0xf) << 8);
	y = (short)(((dat[1] & 0xf0) >> 4) + (dat[2] << 4));
}

///////////////////
// cast 1 byte to 2 int4
void CBytestream::read2Int4(short& x, short& y) {
	uchar tmp = readByte();
	x = tmp & 0xf;
	y = (short)((tmp & 0xf0) >> 4);
}


////////////////////
// Read one bit from the bytestream
bool CBytestream::readBit()
{
	if( isPosAtEnd() )
	{
		errors << "reading from stream behind end" << endl;
		return false;
	}
	bool ret = (Data[pos] & ( 1 << bitPos )) != 0;
	bitPos ++;
	if( bitPos >= 8 )
	{
		bitPos = 0;
		pos ++;
	}
	return ret;
}

/////////////////////
// Get data from the bytestream
std::string CBytestream::readData( size_t size )
{
	// Nothing left, and guards against pos being at/past the end:
	// otherwise GetLength() - pos underflows and substr throws.
	if( pos >= GetLength() )
		return "";
	size = MIN( size, GetLength() - pos );
	size_t oldpos = pos;
	pos += size;
	return Data.substr( oldpos, size );
}

bool CBytestream::readVar(ScriptVar_t& var) {
	ScriptVarType_t type = (ScriptVarType_t)readByte();

	switch( type ) {
	case SVT_BOOL: var = ScriptVar_t(readBool()); return true;
	case SVT_INT32: var = ScriptVar_t(readInt(4)); return true;
	case SVT_UINT64: var = ScriptVar_t(readUInt64()); return true;
	case SVT_FLOAT: var = ScriptVar_t(readFloat()); return true;
	case SVT_VEC2: {
		float x = readFloat();
		float y = readFloat();
		var = ScriptVar_t(CVec(x, y));
		return true;
	}
	case SVT_STRING: var = ScriptVar_t(readString()); return true;
	case SVT_COLOR: {
		Color c;
		c.r = readInt(1);
		c.g = readInt(1);
		c.b = readInt(1);
		c.a = readInt(1);
		var = ScriptVar_t(c);
		return true;
	}
	case SVT_CUSTOM:
	case SVT_CustomWeakRefToStatic: {
		if(var.isCustomType()) {
			ClassId classId = readInt16();
			if(classId != var.customVar()->thisRef.classId) {
				errors << "read var: got invalid classId " << classId << " << for static CustomVar with classId " << var.customVar()->thisRef.classId << endl;
				return false;
			}
			var.customVar()->fromBytestream(this, false);
			return true;
		}
		else {
			CustomVar::Ref custom = CustomVar::FromBytestream(this);
			if(!custom) return false;
			var = ScriptVar_t(custom);
			return true;
		}
	}
	case SVT_CALLBACK:
	case SVT_DYNAMIC:
		errors << "read var has not-supported type " << (int)type << endl;
		return false;
	}

	errors << "read var has unknown type " << (int)type << endl;
	return false;
}



/////////////////////
// Read a byte but don't change the position
uchar CBytestream::peekByte() const 
{
	if (!isPosAtEnd())
		return Data[GetPos()];
	errors << "CBytestream::peekByte(): reading from stream beyond end" << endl;
	return 0;
}

///////////////////////
// Peek data from the bytestream
std::string CBytestream::peekData(size_t len) const 
{
	if (GetPos() + len <= GetLength())
		return Data.substr(GetPos(), len);
	return "";
}


// Skips a string, including the terminating character
// Returns true if we're at the end of the stream after the skip
bool CBytestream::SkipString() {
	readString();
	return isPosAtEnd();
}

bool CBytestream::Skip(size_t num) {
	// Clamp to the end so pos never runs past the buffer
	// (readData and others rely on pos <= GetLength()).
	pos = MIN( pos + num, GetLength() );
	return isPosAtEnd();
}

bool CBytestream::SkipVar() {
	ScriptVar_t var;
	readVar(var);
	return isPosAtEnd();
}

////////////////
// Read from network
// WARNING: overrides any previous data
size_t CBytestream::Read(NetworkSocket* sock) {
	Clear();
	char buf[4096];
	int res = sock->Read(buf, sizeof(buf));
	if(res > 0)
		Data.append(buf, res);

#ifdef DEBUG
	// DEBUG: randomly drop packets to test network stability
/*	if (GetRandomInt(128) > 110)  {
		warnings("DEBUG: packet ignored\n");
		Dump();
		warnings("\n");
		Clear();
		return 0;
	}*/
#endif

	return Data.size();
}

bool CBytestream::Send(NetworkSocket* sock) {
	return (size_t)sock->Write(Data) == Data.size();
}

