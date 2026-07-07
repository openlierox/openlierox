/*
 *  MapLoader_CommanderKeen123.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 11.4.12.
 *  code under LGPL
 *
 */

#include <stdint.h>
#include "MapLoader.h"
#include "MapLoader_common.h"
#include "SafeVector.h"
#include "Color.h"
#include "EndianSwap.h"
#include "FindFile.h"
#include "StringUtils.h"
#include "game/CMap.h"
#include "Cache.h"
#include "ConfigHandler.h"

class ML_CommanderKeen123 : public MapLoad {
public:
	std::string format() { return "Commander Keen (1-3) level"; }
	std::string formatShort() { return "CK"; }
	
	enum {
		MAX_TILES  =  700,
		MAP_MAXWIDTH	=	256,
		MAP_MAXHEIGHT	=	256,
		NUM_OPTIONS     = 21,
		HD_PLANES_START	=		16,
		TILE_W		=	16,
		TILE_H		=	16,
		TILE_S		=	4,
	};
	
	struct stMap
	{
		Uint16 xsize, ysize;            // size of the map
		bool isworldmap;             // this is the world map
		bool ismenumap;				// score+menu map
		unsigned int mapdata[MAP_MAXWIDTH][MAP_MAXHEIGHT];       // the map data
		// in-game, contains monsters and special object tags like for switches 
		// on world map contains level numbers and flags for things like teleporters
		unsigned int objectlayer[MAP_MAXWIDTH][MAP_MAXHEIGHT];
		// player start pos
		int startx, starty;
		// if 1, there is a time limit to finish the level
		bool hastimelimit;			
		int time_m, time_s;		// how much time they have
		// play Tantalus Ray cinematic on time out (ep2)
		// or Game Over on time out (all other episodes)
		bool GameOverOnTimeOut;
		// map forced options (for usermaps)
		int forced_options[NUM_OPTIONS];
	};
	stMap map;
	
	struct stTile
	{
		int solidfall;       // if =1, things can not fall through
		int solidl;          // if =1, things can not walk through left->right
		int solidr;          // if =1, things can not walk through right->left
		int solidceil;       // if =1, things can not go up through
		int goodie;          // if =1, is reported to get_goodie on touch
		int standgoodie;     // if =1, is reported to get_goodie when standing on it
		int lethal;          // if =1 and goodie=1, is deadly to the touch
		int pickupable;      // if =1, will be erased from map when touched
		int points;		  // how many points you get for picking it up
		int priority;        // if =1, will appear in front of objects
		int ice;             // if =1, it's very slippery!
		int semiice;         // if =1, player has no friction but can walk normally
		int masktile;        // if nonzero, specifies a mask for this tile
		int bonklethal;      // if you hit your head on it you die (hanging moss)
		int chgtile;         // tile to change to when level completed (for wm)
		// or tile to change to when picked up (in-level)
		// stuff for animated tiles
		unsigned char isAnimated;  // if =1, tile is animated
		unsigned int animOffset;   // starting offset from the base frame
		unsigned int animlength;   // animation length
	};	
	stTile tiles[MAX_TILES];
	
	// graphics
	unsigned char tiledata[MAX_TILES][16][16];
	
	static int roundup(int index, int nearest)
	{
		if (index % nearest)
		{
			index /= nearest;
			index *= nearest;
			index += nearest;
		}
		return index;
	}
	
	// decompress up to maxlen bytes of data from level file FP,
	// storing it in the buffer 'data'. returns nonzero on error.
	static bool rle_decompress(FILE *fp, SafeVector<Uint16>& data, size_t maxlen, bool debug = false, bool printErrors = false) {
		if(debug) notes << "map_rle_decompress: decompressing " << maxlen << " words." << endl;
		
		size_t index = 0;
		size_t runs = 0;
		while(!feof(fp) && index < maxlen)
		{
			Uint16 ch = 0;
			fread_endian<Uint16>(fp, ch);
			
			if (ch==0xFEFE)
			{
				Sint32 count = 0; fread_endian<Uint16>(fp, count);
				Uint16 what = 0; fread_endian<Uint16>(fp, what);
				while(count-- && index < maxlen) {
					Uint16* p = data[index]; index++;
					if(p) *p = what;
					else errors << "map_rle_decompress: data-index out of range" << endl;
				}
				runs++;
			}
			else
			{
				Uint16* p = data[index]; index++;
				if(p) *p = ch;
				else errors << "map_rle_decompress: data-index out of range" << endl;
			}
		}
		
		if (index < maxlen)
		{
			if(printErrors) errors << "map_rle_decompress (at " << index << "): uh-oh, less data exists than spec'd in header." << endl;
			return false;
		}
		
		if(debug) notes << "map_rle_decompress: decompressed " << index << " words in " << runs << " runs." << endl;
		return true;
	}
	
private:
	struct LatchReader {
		
		ML_CommanderKeen123* parent;
		
		struct EgaHead {
			long LatchPlaneSize;                //Size of one plane of latch data
			long SpritePlaneSize;               //Size of one plane of sprite data
			long OffBitmapTable;                //Offset in EGAHEAD to bitmap table
			long OffSpriteTable;                //Offset in EGAHEAD to sprite table
			short Num8Tiles;                    //Number of 8x8 tiles
			long Off8Tiles;                     //Offset of 8x8 tiles (relative to plane data)
			short Num32Tiles;                   //Number of 32x32 tiles (always 0)
			long Off32Tiles;                    //Offset of 32x32 tiles (relative to plane data)
			short Num16Tiles;                   //Number of 16x16 tiles
			long Off16Tiles;                    //Offset of 16x16 tiles (relative to plane data)
			short NumBitmaps;                   //Number of bitmaps in table
			long OffBitmaps;                    //Offset of bitmaps (relative to plane data)
			short NumSprites;                   //Number of sprites
			long OffSprites;                    //Offset of sprites (relative to plane data)
			short Compressed;                   //(Keen 1 only) Nonzero: LZ compressed data
		};
		
		EgaHead LatchHeader;
		size_t getbit_bytepos[5];
		uint8_t getbit_bitmask[5];
		char* RawData;
		
		LatchReader(ML_CommanderKeen123* p) : parent(p), RawData(NULL) {}
		~LatchReader() {
			if(RawData) delete[] RawData;
			RawData = NULL;
		}
		
		// initilizes the positions getbit will retrieve data from
		void setplanepositions(unsigned long p1, unsigned long p2, unsigned long p3,
							   unsigned long p4, unsigned long p5)
		{
			int i;
			getbit_bytepos[0] = p1;
			getbit_bytepos[1] = p2;
			getbit_bytepos[2] = p3;
			getbit_bytepos[3] = p4;
			getbit_bytepos[4] = p5;
			
			for(i=0;i<=4;i++)
			{
				getbit_bitmask[i] = 128;
			}
		}
		
		// retrieves a bit from plane "plane". the positions of the planes
		// should have been previously initilized with setplanepositions()
		unsigned char getbit(char *buf, unsigned char plane)
		{
			if(plane >= 5) {
				errors << "getbit with plane=" << (int)plane << endl;
				return 0;
			}
			if (!getbit_bitmask[plane])
			{
				getbit_bitmask[plane] = 128;
				getbit_bytepos[plane]++;
			}
			
			int byt = buf[getbit_bytepos[plane]];
			
			int retval = 0;
			if (byt & getbit_bitmask[plane])
				retval = 1;
			
			getbit_bitmask[plane] >>= 1;
			
			return retval;
		}
		
		static Uint32 fgetl(FILE* fp) {
			Uint32 res = 0;
			fread_endian<Uint32>(fp, res);
			return res;
		}
		
		static Uint16 fgeti(FILE* fp) {
			Uint16 res = 0;
			fread_endian<Uint16>(fp, res);
			return res;
		}
		
		// load the EGAHEAD file
		char latch_loadheader(const std::string& dir, bool abs_filename, int episode)
		{
			//unsigned long SpriteTableRAMSize;
			//unsigned long BitmapTableRAMSize;
			//char buf[12];
			//int i,j,k;
			
			std::string fname = dir + "/EGAHEAD.CK" + itoa(episode);
			FILE* headfile = abs_filename ? OpenAbsFile(fname, "rb") : OpenGameFile(fname, "rb");
			if (!headfile)
			{
				errors << "latch_loadheader(): unable to open " << fname << endl;
				return 1;
			}
			
			notes << "latch_loadheader(): reading main header from " << fname << endl;
			
			// read the main header data from EGAHEAD
			LatchHeader.LatchPlaneSize = fgetl(headfile);
			LatchHeader.SpritePlaneSize = fgetl(headfile);
			LatchHeader.OffBitmapTable = fgetl(headfile);
			LatchHeader.OffSpriteTable = fgetl(headfile);
			LatchHeader.Num8Tiles = fgeti(headfile);
			LatchHeader.Off8Tiles = fgetl(headfile);
			LatchHeader.Num32Tiles = fgeti(headfile);
			LatchHeader.Off32Tiles = fgetl(headfile);
			LatchHeader.Num16Tiles = fgeti(headfile);
			LatchHeader.Off16Tiles = fgetl(headfile);
			LatchHeader.NumBitmaps = fgeti(headfile);
			LatchHeader.OffBitmaps = fgetl(headfile);
			LatchHeader.NumSprites = fgeti(headfile);
			LatchHeader.OffSprites = fgetl(headfile);
			LatchHeader.Compressed = fgeti(headfile);
			
			notes << "   LatchPlaneSize = " << LatchHeader.LatchPlaneSize << endl;
			notes << "   SpritePlaneSize = " << LatchHeader.SpritePlaneSize << endl;
			notes << "   OffBitmapTable = " << LatchHeader.OffBitmapTable << endl;
			notes << "   OffSpriteTable = " << LatchHeader.OffSpriteTable << endl;
			notes << "   Num8Tiles = " << LatchHeader.Num8Tiles << endl;
			notes << "   Off8Tiles = " << LatchHeader.Off8Tiles << endl;
			notes << "   Num32Tiles = " << LatchHeader.Num32Tiles << endl;
			notes << "   Off32Tiles = " << LatchHeader.Off32Tiles << endl;
			notes << "   Num16Tiles = " << LatchHeader.Num16Tiles << endl;
			notes << "   Off16Tiles = " << LatchHeader.Off16Tiles << endl;
			notes << "   NumBitmaps = " << LatchHeader.NumBitmaps << endl;
			notes << "   OffBitmaps = " << LatchHeader.OffBitmaps << endl;
			notes << "   NumSprites = " << LatchHeader.NumSprites << endl;
			notes << "   OffSprites = " << LatchHeader.OffSprites << endl;
			notes << "   Compressed = " << LatchHeader.Compressed << endl;
			
			/** read in the sprite table **/
			/*
			 // allocate memory for the sprite table
			 SpriteTableRAMSize = sizeof(SpriteHead) * (LatchHeader.NumSprites + 1);
			 lprintf("latch_loadheader(): Allocating %d bytes for sprite table.\n", SpriteTableRAMSize);
			 
			 SpriteTable = malloc(SpriteTableRAMSize);
			 if (!SpriteTable)
			 {
			 lprintf("latch_loadheader(): Can't allocate sprite table!\n");
			 return 1;
			 }
			 
			 lprintf("latch_loadheader(): Reading sprite table from '%s'...\n", fname);
			 
			 fseek(headfile, LatchHeader.OffSpriteTable, SEEK_SET);
			 for(i=0;i<LatchHeader.NumSprites;i++)
			 {
			 SpriteTable[i].Width = fgeti(headfile) * 8;
			 SpriteTable[i].Height = fgeti(headfile);
			 SpriteTable[i].OffsetDelta = fgeti(headfile);
			 SpriteTable[i].OffsetParas = fgeti(headfile);
			 SpriteTable[i].Rx1 = (fgeti(headfile) >> 8);
			 SpriteTable[i].Ry1 = (fgeti(headfile) >> 8);
			 SpriteTable[i].Rx2 = (fgeti(headfile) >> 8);
			 SpriteTable[i].Ry2 = (fgeti(headfile) >> 8);
			 for(j=0;j<16;j++) SpriteTable[i].Name[j] = fgetc(headfile);
			 // for some reason each sprite occurs 4 times in the table.
			 // we're only interested in the first occurance.
			 for(j=0;j<3;j++)
			 {
			 for(k=0;k<sizeof(SpriteHead);k++) fgetc(headfile);
			 }
			 
			 }
			 */
			/** read in the bitmap table **/
			/*
			 // allocate memory for the bitmap table
			 BitmapTableRAMSize = sizeof(BitmapHead) * (LatchHeader.NumBitmaps + 1);
			 lprintf("latch_loadheader(): Allocating %d bytes for bitmap table.\n", BitmapTableRAMSize);
			 
			 BitmapTable = malloc(BitmapTableRAMSize);
			 if (!BitmapTable)
			 {
			 lprintf("latch_loadheader(): Can't allocate bitmap table!\n");
			 return 1;
			 }
			 
			 lprintf("latch_loadheader(): reading bitmap table from '%s'...\n", fname);
			 
			 fseek(headfile, LatchHeader.OffBitmapTable, SEEK_SET);
			 
			 BitmapBufferRAMSize = 0;
			 for(i=0;i<LatchHeader.NumBitmaps;i++)
			 {
			 BitmapTable[i].Width = fgeti(headfile) * 8;
			 BitmapTable[i].Height = fgeti(headfile);
			 BitmapTable[i].Offset = fgetl(headfile);
			 for(j=0;j<8;j++) BitmapTable[i].Name[j] = fgetc(headfile);
			 
			 // keep a tally of the bitmap sizes so we'll know how much RAM we have
			 // to allocate for all of the bitmaps once they're decoded
			 BitmapBufferRAMSize += (BitmapTable[i].Width * BitmapTable[i].Height);
			 
			 // print the bitmap info to the console for debug
			 for(j=0;j<8;j++) buf[j] = BitmapTable[i].Name[j];
			 buf[j] = 0;
			 lprintf("   Bitmap '%s': %dx%d at offset %04x. RAMAllocSize=0x%04x\n", buf,BitmapTable[i].Width,BitmapTable[i].Height,BitmapTable[i].Offset,BitmapBufferRAMSize);
			 }
			 BitmapBufferRAMSize++;
			 */
			
			fclose(headfile);
			return 0;
		}
		
		char latch_loadlatch(const std::string& dir, bool abs_filename, int episode) {
			
			unsigned long plane1, plane2, plane3, plane4;
			int x,y,t,c=0,p;
			// int b;
			//char *bmdataptr;
			
			std::string fname = dir + "/EGALATCH.CK" + itoa(episode);
			
			notes << "latch_loadlatch(): Opening file " << fname << endl;
			
			FILE* latchfile = abs_filename ? OpenAbsFile(fname, "rb") : OpenGameFile(fname, "rb");
			if (!latchfile)
			{
				errors << "latch_loadlatch(): Unable to open " << fname << endl;
				return 1;
			}
			
			// figure out how much RAM we'll need to read all 4 planes of
			// latch data into memory.
			size_t RawDataSize = (LatchHeader.LatchPlaneSize * 4);
			if(RawData) delete[] RawData;
			RawData = new char[RawDataSize];
			if (!RawData)
			{
				errors << "latch_loadlatch(): Unable to allocate RawData buffer!" << endl;
				fclose(latchfile);
				return 1;
			}
			
			// get the data out of the file into memory, decompressing if necessary.
			if (LatchHeader.Compressed)
			{
				notes << "latch_loadlatch(): Decompressing..." << endl;
				fseek(latchfile, 6, SEEK_SET);
				int ok = lz_decompress(latchfile, (uint8_t*)RawData, (uint8_t*)RawData + RawDataSize);
				if (ok) {
					errors << "lzd returns " << ok << endl;
					fclose(latchfile);
					return 1;
				}
			}
			else
			{
				notes << "latch_loadlatch(): Reading " << RawDataSize << " bytes..." << endl;
				fread(RawData, RawDataSize, 1, latchfile);
			}
			fclose(latchfile);
			
			// these are the offsets of the different video planes as
			// relative to each other--that is if a pixel in plane1
			// is at N, the byte for that same pixel in plane3 will be
			// at (N + plane3).
			plane1 = 0;
			plane2 = (LatchHeader.LatchPlaneSize * 1);
			plane3 = (LatchHeader.LatchPlaneSize * 2);
			plane4 = (LatchHeader.LatchPlaneSize * 3);
			
			// ** read the 8x8 tiles **
			notes << "latch_loadlatch(): Decoding 8x8 tiles..." << endl;
			
			// set up the getbit() function
			setplanepositions(plane1 + LatchHeader.Off8Tiles,
							  plane2 + LatchHeader.Off8Tiles,
							  plane3 + LatchHeader.Off8Tiles,
							  plane4 + LatchHeader.Off8Tiles,
							  0);
			
			for(p=0;p<4;p++)
			{
				for(t=0;t<LatchHeader.Num8Tiles;t++)
				{
					for(y=0;y<8;y++)
					{
						for(x=0;x<8;x++)
						{
							// if we're on the first plane start with black,
							// else merge with the previously accumulated data
							if (p==0)
							{
								c = 0;
							}
							else
							{
								//c = font[t][y][x];
							}
							
							// read a bit out of the current plane, shift it into the
							// correct position and merge it
							c |= (getbit(RawData, p) << p);
							if (p==3 && !c) c=16;
							//font[t][y][x] = c;
						}
					}
				}
			}
			//Make_Font_Clear();
			
			// ** read the 16x16 tiles **
			notes << "latch_loadlatch(): Decoding 16x16 tiles..." << endl;
			
			// set up the getbit() function
			setplanepositions(plane1 + LatchHeader.Off16Tiles,
							  plane2 + LatchHeader.Off16Tiles,
							  plane3 + LatchHeader.Off16Tiles,
							  plane4 + LatchHeader.Off16Tiles,
							  0);
			
			for(p=0;p<4;p++)
			{
				for(t=0;t<LatchHeader.Num16Tiles;t++)
				{
					for(y=0;y<16;y++)
					{
						for(x=0;x<16;x++)
						{
							if (p==0)
							{
								c = 0;
							}
							else
							{
								c = parent->tiledata[t][y][x];
							}
							c |= (getbit(RawData, p) << p);
							parent->tiledata[t][y][x] = c;
						}
					}
				}
			}
			
			// clear all unused tiles
			for(t=LatchHeader.Num16Tiles;t<MAX_TILES;t++)
			{
				for(y=0;y<TILE_H;y++)
					for(x=0;x<TILE_W;x++)
					{
						parent->tiledata[t][y][x] = ((x&1) ^ (y&1)) ? 8:0;
					}
			}
			
			// ** read the bitmaps **
			/*lprintf("latch_loadlatch(): Allocating %d bytes for bitmap data...\n", BitmapBufferRAMSize);
			 BitmapData = malloc(BitmapBufferRAMSize);
			 if (!BitmapData)
			 {
			 lprintf("Cannot allocate memory for bitmaps.\n");
			 return 1;
			 }
			 
			 lprintf("latch_loadlatch(): Decoding bitmaps...\n", fname);
			 
			 // set up the getbit() function
			 setplanepositions(plane1 + LatchHeader.OffBitmaps, \
			 plane2 + LatchHeader.OffBitmaps, \
			 plane3 + LatchHeader.OffBitmaps, \
			 plane4 + LatchHeader.OffBitmaps, \
			 0);
			 
			 // decode bitmaps into the BitmapData structure. The bitmaps are
			 // loaded into one continous stream of image data, with the bitmaps[]
			 // array giving pointers to where each bitmap starts within the stream.
			 
			 for(p=0;p<4;p++)
			 {
			 // this points to the location that we're currently
			 // decoding bitmap data to
			 bmdataptr = &BitmapData[0];
			 
			 for(b=0;b<LatchHeader.NumBitmaps;b++)
			 {
			 bitmaps[b].xsize = BitmapTable[b].Width;
			 bitmaps[b].ysize = BitmapTable[b].Height;
			 bitmaps[b].bmptr = bmdataptr;
			 memcpy(&bitmaps[b].name[0], &BitmapTable[b].Name[0], 8);
			 bitmaps[b].name[8] = 0;  //ensure null-terminated
			 
			 for(y=0;y<bitmaps[b].ysize;y++)
			 {
			 for(x=0;x<bitmaps[b].xsize;x++)
			 {
			 if (p==0)
			 {
			 c = 0;
			 }
			 else
			 {
			 c = *bmdataptr;
			 }
			 c |= (getbit(RawData, p) << p);
			 *bmdataptr = c;
			 bmdataptr++;
			 }
			 }
			 }
			 }*/
			
			delete[] RawData; RawData = NULL;
			return 0;		
		}
		
		
		
		enum {
			LZ_STARTBITS    =    9,
			LZ_MAXBITS     =     12,
			LZ_ERRORCODE   =     256,
			LZ_EOFCODE     =     257,
			LZ_DICTSTARTCODE  =  258,
			LZ_MAXDICTSIZE  =    ((1<<LZ_MAXBITS)+1),
			LZ_MAXSTRINGSIZE =   72
		};
		
		// only temporarly pointing to the current position in buffer when decompressing
		unsigned char *lz_outbuffer;
		unsigned char *lz_outbuffer_end;
		
		struct stLZDictionaryEntry
		{
			int stringlen;
			unsigned char string[LZ_MAXSTRINGSIZE];
		};
		
		stLZDictionaryEntry *lzdict[LZ_MAXDICTSIZE];
		
		// reads a word of length numbits from file lzfile.
		unsigned int lz_readbits(FILE *lzfile, unsigned char numbits, unsigned char reset)
		{
			static int mask, byte;
			unsigned char bitsread;
			unsigned int dat;
			
			if (reset)
			{
				mask = 0;
				byte = 0;
				return 0;
			}
			
			bitsread = 0;
			dat = 0;
			do
			{        
				if (!mask)
				{
					byte = fgetc(lzfile);
					mask = 0x80;
				}
				
				if (byte & mask)
				{
					dat |= 1 << ((numbits - bitsread) - 1);
				}
				
				mask >>= 1;
				bitsread++;
			} while(bitsread<numbits);
			
			return dat;
		}
		
		// writes dictionary entry 'entry' to the output buffer
		void lz_outputdict(int entry)
		{
			int i;
			
			for(i=0;i<lzdict[entry]->stringlen;i++)
			{
				if(lz_outbuffer >= lz_outbuffer_end) {
					errors << "lz_outputdict: out of buffer range" << endl;
					return;
				}
				*lz_outbuffer = lzdict[entry]->string[i];
				lz_outbuffer++;
			}
		}
		
		// decompresses LZ data from open file lzfile into buffer outbuffer
		// returns nonzero if an error occurs
		char lz_decompress(FILE *lzfile, uint8_t *outbuffer, uint8_t* bufend)
		{
			int i;
			int numbits;
			unsigned int dictindex, maxdictindex;
			unsigned int lzcode,lzcode_save,lastcode;
			char addtodict;
			
			/* allocate memory for the LZ dictionary */
			for(i=0;i<LZ_MAXDICTSIZE;i++)
			{
				lzdict[i] = new stLZDictionaryEntry;
				if (!lzdict[i])
				{
					errors << "lz_decompress(): unable to allocate memory for dictionary!" << endl;
					for(i--;i>=0;i--) delete lzdict[i];
					
					return 1;
				}
			}
			
			/* initilize the dictionary */
			
			// entries 0-255 start with a single character corresponding
			// to their entry number
			for(i=0;i<256;i++)
			{
				lzdict[i]->stringlen = 1;
				lzdict[i]->string[0] = i;
			}
			// 256+ start undefined
			for(i=256;i<LZ_MAXDICTSIZE;i++)
			{
				lzdict[i]->stringlen = 0;
			}
			
			// reset readbits
			lz_readbits(NULL, 0, 1);
			
			// set starting # of bits-per-code
			numbits = LZ_STARTBITS;
			maxdictindex = (1 << numbits) - 1;
			
			// point the global pointer to the buffer we were passed
			lz_outbuffer = outbuffer;
			lz_outbuffer_end = bufend;
			
			// setup where to start adding strings to the dictionary
			dictindex = LZ_DICTSTARTCODE;
			addtodict = 1;                    // enable adding to dictionary
			
			// read first code
			lastcode = lz_readbits(lzfile, numbits, 0);
			lz_outputdict(lastcode);
			do
			{
				// read the next code from the compressed data stream
				lzcode = lz_readbits(lzfile, numbits, 0);
				lzcode_save = lzcode;
				
				if (lzcode==LZ_ERRORCODE || lzcode==LZ_EOFCODE)
					break;
				
				// if the code is present in the dictionary,
				// lookup and write the string for that code, then add the
				// last string + the first char of the just-looked-up string
				// to the dictionary at dictindex
				
				// if not in dict, add the last string + the first char of the
				// last string to the dictionary at dictindex (which will be equal
				// to lzcode), then lookup and write string lzcode.
				
				if (lzdict[lzcode]->stringlen==0)
				{  // code is not present in dictionary             
					lzcode = lastcode;
				}
				
				if (addtodict)     // room to add more entries to the dictionary?
				{
					// copies string lastcode to string dictindex, then
					// concatenates the first character of string lzcode.
					for(i=0;i<lzdict[lastcode]->stringlen;i++)
					{
						lzdict[dictindex]->string[i] = lzdict[lastcode]->string[i];
					}
					lzdict[dictindex]->string[i] = lzdict[lzcode]->string[0];
					lzdict[dictindex]->stringlen = (lzdict[lastcode]->stringlen + 1);
					
					// ensure we haven't overflowed the buffer
					if (lzdict[dictindex]->stringlen >= (LZ_MAXSTRINGSIZE-1))
					{
						errors << "lz_decompress(): lzdict[" << dictindex << "]->stringlen is too long...max length is " << (int)LZ_MAXSTRINGSIZE << endl;
						for(i=0;i<LZ_MAXDICTSIZE;i++) delete lzdict[i];
						return 1;
					}
					
					if (++dictindex >= maxdictindex)
					{ // no more entries can be specified with current code bit-width
						if (numbits < LZ_MAXBITS)
						{  // increase width of codes
							numbits++;
							maxdictindex = (1 << numbits) - 1;
						}
						else
						{
							// reached maximum bit width, can't increase.
							// use the final entry (4095) before we shut off
							// adding items to the dictionary.
							if (dictindex>=(LZ_MAXDICTSIZE-1)) addtodict = 0;
						}
					}
				}
				
				// write the string associated with the original code read.
				// if the code wasn't present, it now should have been added.
				lz_outputdict(lzcode_save);
				
				lastcode = lzcode_save;
			} while(1);
			
			/* free the memory used by the LZ dictionary */
			for(i=0;i<LZ_MAXDICTSIZE;i++)
				delete lzdict[i];
			
			return 0;
		}
		
	};
	
	bool loadtileattributes(int episode)
	{
		int t,a,b,c,intendedep,intendedver;
		
		std::string fname = "data/commanderkeen123/ep" + itoa(episode) + "attr.dat";
		
		//  notes << "Loading tile attributes from '" << fname << "'..." << endl;
		
		FILE* fp = OpenGameFile(fname, "rb");
		if (!fp)
		{
			errors << "loadtileattributes(): Cannot open tile attribute file " << fname << endl;
			return false;
		}
		
		/* check the header */
		// header format: 'A', 'T', 'R', episode, version
		a = fgetc(fp);
		b = fgetc(fp);
		c = fgetc(fp);
		if (a != 'A' || b != 'T' || c != 'R')
		{
			errors << "loadtileattributes(): Attribute file corrupt! ('ATR' marker not found)" << endl;
			fclose(fp);
			return false;
		}
		
		intendedep = fgetc(fp);
		if (intendedep != episode)
		{
			errors << "loadtileattributes(): file is intended for episode " << intendedep << " but you're trying to use it with episode " << episode << endl;
			fclose(fp);
			return false;
		}
		
		intendedver = fgetc(fp);
		static const int ATTRFILEVERSION = 2;
		if (intendedver != ATTRFILEVERSION)
		{
			errors << "attr file version " << intendedver << ", I need version " << ATTRFILEVERSION << endl;
			fclose(fp);
			return false;
		}
		
		/* load in the tile attributes */
		
		for(t=0;t<MAX_TILES-1;t++)
		{
			
			tiles[t].solidl = fgetc(fp);
			if (tiles[t].solidl==-1)
			{
				errors << "loadtileattributes(): " << fname << " corrupt! (unexpected EOF)" << endl;
				fclose(fp);
				return false;
			}
			
			tiles[t].solidr = fgetc(fp);
			tiles[t].solidfall = fgetc(fp);
			tiles[t].solidceil = fgetc(fp);
			tiles[t].ice = fgetc(fp);
			tiles[t].semiice = fgetc(fp);
			tiles[t].priority = fgetc(fp);
			if (fgetc(fp)) tiles[t].masktile=t+1; else tiles[t].masktile = 0;
			tiles[t].goodie = fgetc(fp);
			tiles[t].standgoodie = fgetc(fp);
			tiles[t].pickupable = fgetc(fp);
			fread_endian<Uint16>(fp, tiles[t].points);
			tiles[t].lethal = fgetc(fp);
			tiles[t].bonklethal = fgetc(fp);
			fread_endian<Uint16>(fp, tiles[t].chgtile);
			tiles[t].isAnimated = fgetc(fp);
			tiles[t].animOffset = fgetc(fp);
			tiles[t].animlength = fgetc(fp);
			
		}
		
		fclose(fp);
		return true;
	}
	
	
	bool parseTiles(int episode) {
		if(!loadtileattributes(episode)) return false;
		LatchReader latch(this);
		if(latch.latch_loadheader(GetDirName(filename), abs_filename, episode)) return false;
		if(latch.latch_loadlatch(GetDirName(filename), abs_filename, episode)) return false;
		return true;
	}
	
public:
	static const bool withKeenBorders = false;
	
	Result parseHeader(bool printErrors) {
		head.name = GetBaseFilename(filename);
		
		fseek(fp,0,SEEK_SET);
		
		Uint32 maplen = 0;
		fread_endian<Uint32>(fp, maplen);
		if(maplen <= 4) {
			if(printErrors) errors << "CK loader: file size invalid" << endl;
			return false;
		}
		
		SafeVector<Uint16> data(2);
		if(!rle_decompress(fp, data, data.size(), false, printErrors)) {
			if(printErrors) errors << "CK loader: invalid RLE encoding" << endl;
			return false;
		}
		
		Sint32 w = *data[0], h = *data[1];
		if (w >= MAP_MAXWIDTH || h >= MAP_MAXHEIGHT) {
			if(printErrors) errors << "CK loader: map too big" << endl;
			return false;
		}
		
		if(!withKeenBorders) {
			w -= 4; h -= 4;
			if(w < 0 || h < 0) {
				if(printErrors) errors << "CK loader: map too small" << endl;
				return false;
			}
		}
		
		head.width = w * TILE_W;
		head.height = h * TILE_H;
		
		return true;
	}
	
	
	Result parseData(CMap* m) {
		ParseFinalizer finalizer(this, m);
		
		int episode = 0;
		if(filename != "") {
			episode = (int)filename[filename.size() - 1] - (int)'0';
			if(episode < 1 || episode > 3) episode = 0;
		}
		
		if(!parseTiles(episode)) return false;		
		if(!loadPalette(episode)) return false;
		
		fseek(fp,0,SEEK_SET);
		
		map.isworldmap = strStartsWith( stringtolower(GetBaseFilename(filename)), "level80.ck" );
		map.ismenumap = strStartsWith( stringtolower(GetBaseFilename(filename)), "level90.ck" );
		
		Uint32 maplen = 0;
		fread_endian<Uint32>(fp, maplen);
		notes << "Map uncompressed length: " << maplen << " bytes." << endl;
		if (!maplen) {
			errors << "loadmap: This map says it's 0 bytes long... it was probably created by a broken map editor" << endl;
			return false;
		}
		maplen /= 2;
		
		SafeVector<Uint16> data(maplen + 1);
		if (data.size() == 0) {
			errors << "loadmap: unable to allocate " << maplen*2 << " bytes." << endl;
			return false;
		}
		
		if (!rle_decompress(fp, data, maplen, true, true)) {
			errors << "loadmap: RLE decompression error" << endl;
			return false;
		}
		
		map.xsize = *data[0];
		map.ysize = *data[1];
		if (*data[2] != 2) {
			errors << "loadmap(): incorrect number of planes (loader only supports 2)" << endl;
			return false;
		}
		
		notes << "loadmap(): " << filename << " map dimensions " << map.xsize << "x" << map.ysize << endl;
		if (map.xsize >= MAP_MAXWIDTH || map.ysize >= MAP_MAXHEIGHT) {
			errors << "loadmap(): level " << filename << " is too big (max size " << (int)MAP_MAXWIDTH << "x" << (int)MAP_MAXHEIGHT << ")" << endl;
			return false;
		}
		
		Uint16 plane_size = *data[7];
		notes << "plane size " << plane_size << " bytes" << endl;
		if (plane_size & 1) {
			errors << "loadmap(): plane size is not even!" << endl;
			return false;
		}
		
		// copy the tile layer into the map
		size_t index = HD_PLANES_START;
		for(Uint16 y=0;y<map.ysize;y++)
		{
			for(Uint16 x=0;x<map.xsize;x++)
			{
				map.mapdata[x][y] = *data[index++];
			}
		}
		
		// copy the object layer into the map	
		// get index of plane 2, rounding up to the nearest 16 worde boundary (8 words)
		index = roundup((HD_PLANES_START + (plane_size / 2)), 8);
		
		for(Uint16 y=0;y<map.ysize;y++)
		{
			for(Uint16 x=0;x<map.xsize;x++)
			{
				Uint16 t = *data[index++];
				if (t==255)
				{
					//map_setstartpos(x, y);
				}
				else
				{
					map.objectlayer[x][y] = t;
					if (t)
					{
						if (!map.isworldmap)
						{
							// spawn enemies as appropriate
							//AddEnemy(x, y);
						}
						/* else
						 {
						 // spawn Nessie at first occurance of her path
						 if ( episode==3 && t==NESSIE_PATH)
						 {
						 if (!NessieObjectHandle)
						 {
						 NessieObjectHandle = spawn_object(x<<TILE_S<<CSF, y<<TILE_S<<CSF, OBJ_NESSIE);
						 objects[NessieObjectHandle].hasbeenonscreen = 1;
						 }
						 }
						 else
						 {
						 // make completed levels into "done" tiles
						 t &= 0x7fff;
						 if (t < MAX_LEVELS && levelcontrol.levels_completed[t])
						 {
						 map.objectlayer[x][y] = 0;
						 map.mapdata[x][y] = tiles[map.mapdata[x][y]].chgtile;
						 }
						 }
						 } */
					}
				}
			}
		}
		
		// check if the mapfile has a clonekeen-specific special region
		/* if (data[HD_HAS_SPECIAL_1]==SPECIAL_VALUE_1 && \
		 data[HD_HAS_SPECIAL_2]==SPECIAL_VALUE_2)
		 {
		 lprintf("> level created in CloneKeen Editor!\n");
		 unsigned long special_offs;
		 special_offs = data[HD_SPECIAL_OFFS_MSB]; special_offs <<= 16;
		 special_offs |= data[HD_SPECIAL_OFFS_LSB];
		 
		 lprintf("Parsing CloneKeen-specific data at %08x\n", special_offs);
		 if (parse_special_region(&data[special_offs]))
		 {
		 lprintf("loadmap(): error parsing special region\n");
		 return 1;
		 }
		 }
		 else
		 {
		 lprintf("> level NOT created by CloneKeen.\n");
		 } */
		
		map_coat_border(episode);
		//map_calc_max_scroll();
		
		notes << "loadmap(): success!" << endl;
		
		return constructMap(m);
	}
	
private:
	
	void map_coat_border(int episode)
	{		
		enum {
			TILE_FELLOFFMAP		=	582,
			TILE_FELLOFFMAP_EP3	=	0,
			TILE_FUEL			=	245,
			BG_GREY				=	143,
		};
		
		Uint16 border= (episode==3) ? TILE_FUEL : 144;
		for(Uint32 x=0;x<map.xsize;x++)
		{
			map.mapdata[x][0] = border;
			map.mapdata[x][1] = border;
			map.mapdata[x][map.ysize-1] = border;
			map.mapdata[x][map.ysize-2] = border;
		}
		for(Uint32 y=0;y<map.ysize;y++)
		{
			map.mapdata[0][y] = border;
			map.mapdata[1][y] = border;
			map.mapdata[map.xsize-1][y] = border;
			map.mapdata[map.xsize-2][y] = border;
		}
		
		if (episode == 3)
		{
			// coat the top of the map ("oh no!" border) with a non-solid tile
			// so keen can jump partially off the top of the screen
			for(int x=2;x<map.xsize-2;x++)
			{
				map.mapdata[x][1] = BG_GREY;
			}
			
			// make it lethal to fall off the bottom of the map.
			for(int x=2;x<map.xsize-2;x++)
			{
				map.mapdata[x][map.ysize-1] = TILE_FELLOFFMAP_EP3;
			}
		}
		else
		{
			// coat the bottom of the map below the border.
			// since the border has solidceil=1 this provides
			// a platform to catch enemies that fall off the map
			for(int x=2;x<map.xsize-2;x++)
			{
				map.mapdata[x][map.ysize-1] = TILE_FELLOFFMAP;
			}
		}		
	}
	
	
	bool constructMap(CMap* m) {
		m->Name = head.name;
		m->Width = (int)head.width;
		m->Height = (int)head.height;
		m->Type = MPT_IMAGE;
		
		// Allocate the map
	createMap:
		if(!m->Create(m->Width, m->Height, "dirt")) {
			errors << "CMap::Load (" << filename << "): cannot allocate map" << endl;
			if(cCache.GetEntryCount() > 0) {
				hints << "current cache size is " << cCache.GetCacheSize() << ", we are clearing it now" << endl;
				cCache.Clear();
				goto createMap;
			}
			return false;
		}
		m->TileMap();		
		
		LOCK_OR_FAIL(m->bmpBackImageHiRes);
		LOCK_OR_FAIL(m->bmpDrawImage);
		m->lockFlags();
		for(Uint32 x = 0; x < map.xsize; x++)
			for(Uint32 y = 0; y < map.ysize; y++) {
				if(!withKeenBorders) {
					if(x <= 1 || x+2 >= map.xsize) continue;
					if(y <= 1 || y+2 >= map.ysize) continue;
				}
				Uint16 tile = map.mapdata[x][y];
				Uint16 backtile = tile;
				char pixelflag = getPixelFlag(tile);
				if(pixelflag == PX_DIRT) {
					if(tiles[tile].chgtile > 0)
						backtile = tiles[tile].chgtile;
					else
						backtile = searchNextFreeCell(x, y);
				}
				Uint32 realx = x * TILE_W, realy = y * TILE_H;
				if(!withKeenBorders) { realx -= 2*TILE_W; realy -= 2*TILE_H; }				
				fillpixelflags(m->material->line, m->Width, realx, realy, Material::indexFromLxFlag(pixelflag));
				sb_drawtile(m->bmpDrawImage.get(), realx, realy, tile);
				sb_drawtile(m->bmpBackImageHiRes.get(), realx, realy, backtile);
			}
		m->unlockFlags();
		UnlockSurface(m->bmpDrawImage);
		UnlockSurface(m->bmpBackImageHiRes);
		
		m->lxflagsToGusflags();
		return true;
	}
	
	Uint16 searchNextFreeCell(int x, int y) {
#define CHECKNRET(_x,_y) { if(getPixelFlag(map.mapdata[_x][_y]) == PX_EMPTY) return map.mapdata[_x][_y]; }		
		int d = 1;
		while(y+1 < map.ysize) {
			y++;
			CHECKNRET(x,y);
			for(int dx = 1; dx < d; dx++) {
				if(x+dx < map.xsize) CHECKNRET(x+dx,y);
				if(x>=dx) CHECKNRET(x-dx,y);
			}
			for(int dy = 0; dy <= d; dy++) {
				if(x+d < map.xsize) CHECKNRET(x+d,y-dy);
				if(x>d) CHECKNRET(x-d,y-dy);
			}
			d++;
		}
		
		return map.mapdata[x][y];
#undef CHECKNRET		
	}
	
	SafeVector<Color> palette;
	
	bool loadPalette(int episode) {
		static const std::string palettefn = "data/commanderkeen123/palette.ini";
		int palette_ncolors = 17;
		//ReadInteger(palettefn, "", "NCOLORS", &palette_ncolors, 0);
		if(palette_ncolors <= 0) {
			errors << palettefn << " does not contain any colors" << endl;
			return false;
		}
		palette.resize(palette_ncolors);
		for(int i=0;i<palette_ncolors;i++) {
			std::string key = "EP" + itoa(episode) + "_COLOR" + itoa(i);
			std::string col;
			if(ReadString(palettefn, "", key, col, "")) {
				*palette[i] = StrToCol("#" + col);
			}
		}
		return true;
	}
	
	Color getPaletteColor(int c)
	{
		Color* col = palette[c];
		if(col) return *col;
		return Color();
	}
	
	void sb_drawtile(SDL_Surface* dest, Uint32 x, Uint32 y, unsigned int t)
	{
		for(uint8_t dy = 0; dy < TILE_H; dy++)
			for(uint8_t dx = 0; dx < TILE_W; dx++) {
				PutPixel2x2(dest, (x + dx)*2, (y + dy)*2, getPaletteColor(tiledata[t][dy][dx]).get(dest->format));
			}
	}
	
	char getPixelFlag(Uint32 t) {
		char flag = tiles[t].solidceil ? PX_ROCK : (tiles[t].solidfall ? PX_DIRT : PX_EMPTY);
		if(flag == PX_ROCK && (map.isworldmap || map.ismenumap))
			flag = PX_DIRT; // otherwise we would have a lot of not-accessible areas
		return flag;
	}
	
	void fillpixelflags(uint8_t** PixelFlags, Uint32 mapwidth, Uint32 x, Uint32 y, unsigned char flag) {
		for(uint8_t h = 0; h < TILE_H; h++) {
			memset((char*)&PixelFlags[y+h][x], (char)flag, TILE_W);
		}
	}
	
};

MapLoad* createMapLoad_CK123() {
	return new ML_CommanderKeen123();
}
