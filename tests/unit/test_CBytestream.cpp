/*
 *  Unit tests for CBytestream, the network byte-buffer reader/writer.
 */

#include "unittest.h"
#include "CBytestream.h"

void test_CBytestream() {
	// Regression for the Skip/readData underflow (#1000, fixed in #1002):
	// skipping past the end must keep the read position in bounds, so
	// readData clamps instead of underflowing GetLength()-pos and throwing.
	{
		CBytestream bs;
		bs.writeByte(1);
		bs.writeByte(2);
		bs.Skip(1000);                          // far past the two written bytes
		std::string data = "unset";
		CHECK_NOTHROW(data = bs.readData(10));  // must not throw
		CHECK(data == "");                      // and returns empty
		CHECK(bs.readByte() == 0);              // reading past the end yields 0
	}

	// Truncated reads: past the end, primitive reads return zero.
	{
		CBytestream bs;
		bs.writeByte(42);
		bs.ResetPosToBegin();
		CHECK(bs.readByte() == 42);
		CHECK(bs.readByte() == 0);      // nothing left
		CHECK(bs.readInt(4) == 0);
	}

	// Basic write/read round-trip.
	{
		CBytestream bs;
		bs.writeInt(0x12345678, 4);
		bs.writeInt16(-1234);
		bs.writeString("hello");
		bs.ResetPosToBegin();
		CHECK(bs.readInt(4) == 0x12345678);
		CHECK(bs.readInt16() == -1234);
		CHECK(bs.readString() == "hello");
	}
}
