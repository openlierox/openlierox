/*
 *  MapLoader_LieroX.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 11.4.12.
 *  code under LGPL
 *
 */

#include <stdint.h>
#include "MapLoader.h"
#include "MapLoader_common.h"
#include "PreInitVar.h"
#include "FileUtils.h"
#include "EndianSwap.h"
#include "StringUtils.h"
#include "game/CMap.h"
#include "FileUtils.h"
#include "Color.h"
#include "PixelFunctors.h"
#include "Cache.h"
#include <zlib.h>
#ifndef DEDICATED_ONLY
#include <gd.h>
#endif

class ML_LieroX : public MapLoad {
	
	std::string id;
	PIVar(int,0) Type;
	std::string Theme_Name;
	PIVar(int,0) numobj;
	PIVar(bool,false) ctf;
	std::string format() { return id; }
	std::string formatShort() { return "LX"; }
	
	Result parseHeader(bool printErrors) {
		// Header
		id = freadfixedcstr(fp, 32);
		int	version = 0;
		fread_endian<Sint32>(fp, version);
		
		// Check to make sure it's a valid level file
		if((id != "LieroX Level" && id != "LieroX CTF Level") || version != MAP_VERSION) {
			if(printErrors) errors << "CMap::Load: " << filename << " is not a valid level file (" << id << ") or wrong version (" << version << ")" << endl;
			return false;
		}
		
		// CTF map?
		ctf = (id == "LieroX CTF Level"); // TODO: there's no CTF maps around, and it was a hack, remove it
		
		head.name = freadfixedcstr(fp, 64);		
		fread_endian<Sint32>(fp, head.width);
		fread_endian<Sint32>(fp, head.height);
		fread_endian<Sint32>(fp, (int&)Type);
		Theme_Name = freadfixedcstr(fp, 32);
		fread_endian<Sint32>(fp, (int&)numobj);
		
		return true;
	}
	
	
	///////////////////
	// Load the image format
	bool LoadImageFormat(CMap* m)
	{
		// Load the details
		Uint32 size = 0, destsize = 0;
		
		fread_compat(size, sizeof(Uint32), 1, fp);
		EndianSwap(size);
		fread_compat(destsize, sizeof(Uint32), 1, fp);
		EndianSwap(destsize);
		
		// Allocate the memory
		uint8_t *pSource = new uint8_t[size];
		uint8_t *pDest = new uint8_t[destsize];
		
		if(!pSource || !pDest) {
			errors << "CMap::LoadImageFormat: not enough memory" << endl;
			return false;
		}
		
		if(fread(pSource, sizeof(uint8_t), size, fp) < size) {
			errors << "CMap::LoadImageFormat: cannot read data" << endl;
			return false;
		}
		
		ulong lng_dsize = destsize;
		if( uncompress( pDest, &lng_dsize, pSource, size ) != Z_OK ) {
			errors("Failed decompression\n");
			delete[] pSource;
			delete[] pDest;
			return false;
		}
		destsize = (uint32_t) lng_dsize; // can only get smaller
		if( destsize < Uint32(head.width * head.height * 3 * 2) )
		{
			errors("CMap::LoadImageFormat(): image too small for Width*Height");
			delete[] pSource;
			delete[] pDest;
			return false;
		}
		
		delete[] pSource;  // not needed anymore
		
		//
		// Translate the data
		//
		
		// Lock surfaces
		LOCK_OR_FAIL(m->bmpBackImageHiRes);
		LOCK_OR_FAIL(m->bmpDrawImage);
		
		Uint64 p=0;
		
		// Load the back image
		for (Sint64 y = 0; y < head.height; y++)  {
			for (Sint64 x = 0; x < head.width; x++)  {
				Color curcolor = Color(pDest[p], pDest[p+1], pDest[p+2]);
				p += 3;
				PutPixel2x2(m->bmpBackImageHiRes.get(), (int)x*2, (int)y*2, curcolor.get(m->bmpBackImageHiRes->format));
			}
		}
		
		// Load the front image
		for (Sint64 y = 0; y < head.height; y++)  {
			for (Sint64 x = 0;x < head.width; x++)  {
				Color curcolor = Color(pDest[p], pDest[p+1], pDest[p+2]);
				p += 3;
				PutPixel2x2(m->bmpDrawImage.get(), (int)x*2, (int)y*2, curcolor.get(m->bmpDrawImage->format));
			}
		}
		
		
		// Load the pixel flags and calculate dirt count
		m->nTotalDirtCount = 0;
		
		m->lockFlags();
		
		for(Sint64 y=0; y< head.height; y++) {
			for(Sint64 x=0; x<head.width; x++) {
				uint8_t lxflag = pDest[p++];
				m->material->line[y][x] = Material::indexFromLxFlag(lxflag);
				if(lxflag & PX_EMPTY)
					CopyPixel2x2_SameFormat(m->bmpDrawImage.get(), m->bmpBackImageHiRes.get(), (int)x*2, (int)y*2);
				if(lxflag & PX_DIRT)
					m->nTotalDirtCount++;
			}
		}
		m->unlockFlags();
		
		// Unlock the surfaces
		UnlockSurface(m->bmpBackImageHiRes);
		UnlockSurface(m->bmpDrawImage);
		
		// Load the CTF gametype variables
		if (ctf)  {
			warnings << "CMap::LoadImageFormat(): trying to load old-format CTF map, we do not support this anymore" << endl;
			Uint16 dummy;
			fread_endian<Uint16>(fp, dummy);
			fread_endian<Uint16>(fp, dummy);
			fread_endian<Uint16>(fp, dummy);
			fread_endian<Uint16>(fp, dummy);
			fread_endian<Uint16>(fp, dummy);
			fread_endian<Uint16>(fp, dummy);
		}
		
		//SDL_SaveBMP(pxf, "mat.bmp");
		//SDL_SaveBMP(m->bmpImage.get(), GetWriteFullFileName("debug-front.bmp",true).c_str());
		//SDL_SaveBMP(m->bmpBackImage.get(), GetWriteFullFileName("debug-back.bmp",true).c_str());
		
		// Delete the data
		delete[] pDest;
		
		// Try to load additional data (like hi-res images)
		LoadAdditionalLevelData(m);
		
		m->lxflagsToGusflags();
		return true;
	}
	
	///////////////////
	// Load the high-resolution images
	void LoadAdditionalLevelData(CMap* m)
	{
		// Check that we are not at the end of the file
		// HINT: this needs to be done because until we actually read the EOF, feof returns false
		int c = fgetc(fp);
		if (c == EOF)
			return;
		ungetc(c, fp);
		
		while( !feof(fp) && !ferror(fp) )
		{
			std::string chunkName;
			if(!fread_fixedwidthstr<16>(chunkName, fp)) {
				errors << "CMap::LoadAdditionalLevelData: error while reading chunk name" << endl;
				break;
			}
			Uint32 size = 0;
			if(fread_endian<Uint32>(fp, size) == 0) {
				errors << "CMap::LoadAdditionalLevelData: error while reading size (attribute " << chunkName << ")" << endl;
				break;
			}
			uint8_t *pSource = new uint8_t[size];
			if(pSource == NULL) {
				errors << "CMap::LoadAdditionalLevelData: not enough memory" << endl;
				break;
			}
			if(fread(pSource, sizeof(uint8_t), size, fp) < size) {
				delete[] pSource;
				errors << "CMap::LoadAdditionalLevelData: error while reading data (attribute " << chunkName << ")" << endl;
				break;
			}
			
			if( stringcaseequal( chunkName, "OLX hi-res data") )
				LoadLevelImageHiRes( m, pSource, size );
			else
				if( stringcaseequal( chunkName, "OLX level config") )
					LoadLevelConfig( m, pSource, size );
				else
					warnings << "Unknown additional data found in level file: " << chunkName << ", size " << size << endl;
			
			delete [] pSource;
		}
		
	}
	
	
	///////////////////
	// Load level config, such as CTF base spawnpoints
	void LoadLevelConfig(CMap* m, uint8_t *pSource, Uint32 size)
	{
		warnings << "CMap::LoadLevelConfig(): level config is not used yet in this version of OLX" << endl;
		return;
		
		// TODO: test if this code works
		
		Uint32 destsize = *(Uint32 *)(pSource);
		EndianSwap(destsize);
		
		// Allocate the memory
		uint8_t *pDest = new uint8_t[destsize];
		
		ulong lng_dsize = destsize;
		int ret = uncompress( pDest, &lng_dsize, pSource + sizeof(Uint32), size - sizeof(Uint32) );
		if( ret != Z_OK ) 
		{
			warnings << "CMap::LoadLevelConfig(): failed to load hi-res image, using low-res image" << endl;
			lng_dsize = 0;
		}
		destsize = (uint32_t) lng_dsize; // can only get smaller
		
		// Fill up additional data
		m->AdditionalData.clear();
		Uint32 pos = 0;
		Uint32 AdditionalDataSize = *(Uint32 *)(pDest + pos);
		EndianSwap(AdditionalDataSize);
		pos += 4;
		if( AdditionalDataSize + 4 > destsize )
		{
			warnings << "CMap::LoadLevelConfig(): wrong additional data size " << AdditionalDataSize << endl;
		}
		else
		{
			bool nameChunk = true;
			std::string nameChunkData;
			while( AdditionalDataSize + 4 > pos ) // 4 bytes of whole data size
			{
				Uint32 chunkSize = *(Uint32 *)(pDest + pos);
				EndianSwap(chunkSize);
				if( chunkSize + pos > AdditionalDataSize )
				{
					warnings << "CMap::LoadLevelConfig(): wrong additional data chunk size " << chunkSize << endl;
					break;
				}
				pos += 4;
				
				if( nameChunk )
					nameChunkData = std::string( (char *)(pDest + pos), chunkSize );
				else
					m->AdditionalData[nameChunkData] = std::string( (char *)(pDest + pos), chunkSize );
				
				nameChunk = !nameChunk;
				pos += chunkSize;
			}
		}
		delete[] pDest;
	}
	
	///////////////////
	// Load the high-resolution images
	void LoadLevelImageHiRes(CMap* m, uint8_t *pSource, Uint32 size)
	{
#ifndef DEDICATED_ONLY // Hi-res images are not needed for dedicated server, it uses only material image which is in low-res data
		
		if(bDedicated) return;
		
		gdImagePtr gdImage = gdImageCreateFromPngPtr( size, pSource );
		
		if( !gdImage || gdImageSX(gdImage) != (int)head.width * 2 || gdImageSY(gdImage) != (int)head.height * 4 )
		{
			warnings << "CMap: hi-res image loading failed" << endl;
			if( gdImage )
				gdImageDestroy(gdImage);
			return;
		}
		
		m->bmpBackImageHiRes = gfxCreateSurface((int)head.width*2, (int)head.height*2);
		if(m->bmpBackImageHiRes.get() == NULL) 
		{
			warnings << "CMap::LoadImageFormatHiRes(): bmpBackImageHiRes creation failed, using low-res image" << endl;
			gdImageDestroy(gdImage);
			return;
		}
		
		hints << "CMap: Loading high-res level images" << endl;
		// Lock surfaces
		LOCK_OR_QUIT(m->bmpDrawImage);
		LOCK_OR_QUIT(m->bmpBackImageHiRes);
		
		Uint32 curcolor=0;
		Uint8* curpixel = (Uint8*)m->bmpDrawImage.get()->pixels;
		Uint8* PixelRow = curpixel;
		Uint8 bpp = m->bmpDrawImage.get()->format->BytesPerPixel;
		
		// Load the front image
		for (Sint64 y = 0; y < head.height*2; y++, PixelRow += m->bmpDrawImage.get()->pitch)  {
			curpixel = PixelRow;
			for (Sint64 x = 0; x < head.width*2; x++, curpixel += bpp)  {
				curcolor = gdImageGetTrueColorPixel( gdImage, (int)x, (int)y ); // Maybe we can make direct memory access, but PNG may be palette-based, and I'm too lazy
				curcolor = Pack(Color(gdTrueColorGetRed(curcolor), gdTrueColorGetGreen(curcolor), gdTrueColorGetBlue(curcolor)), m->bmpDrawImage->format);
				PutPixelToAddr(curpixel, curcolor, bpp);
			}
		}
		
		curpixel = (Uint8*)m->bmpBackImageHiRes.get()->pixels;
		PixelRow = curpixel;
		Sint64 HeightX2 = head.height*2;
		// Load the back image
		for (Sint64 y = 0; y < head.height*2; y++, PixelRow += m->bmpBackImageHiRes.get()->pitch)  {
			curpixel = PixelRow;
			for (Sint64 x = 0; x < head.width*2; x++, curpixel += bpp)  {
				curcolor = gdImageGetTrueColorPixel( gdImage, (int)x, (int)y + (int)HeightX2 ); // Maybe we can make direct memory access, but PNG may be palette-based, and I'm too lazy
				curcolor = Pack(Color(gdTrueColorGetRed(curcolor), gdTrueColorGetGreen(curcolor), gdTrueColorGetBlue(curcolor)), m->bmpBackImageHiRes->format);
				PutPixelToAddr(curpixel, curcolor, bpp);
			}
		}
		
		// Update image according to the pixel flags

		curpixel = (Uint8 *)m->bmpDrawImage.get()->pixels;
		PixelRow = curpixel;
		Uint8 *backpixel = (Uint8 *)m->bmpBackImageHiRes.get()->pixels;
		Uint8 *BackPixelRow = backpixel;
		
		m->lockFlags();
		Uint8 bppX2 = bpp*2;
		int pitch = m->bmpDrawImage.get()->pitch;
		for(Sint64 y=0; y<head.height; y++, PixelRow+=pitch*2, BackPixelRow+=pitch*2 ) 
		{
			curpixel = PixelRow;
			backpixel = BackPixelRow;
			for(Sint64 x=0; x<head.width; x++, curpixel+=bppX2, backpixel+=bppX2)
			{
				if(m->unsafeGetPixelFlag((long)x, (long)y) & PX_EMPTY)
				{
					memcpy(curpixel, backpixel, bppX2);
					memcpy(curpixel+pitch, backpixel+pitch, bppX2);
				}
			}
		}
		m->unlockFlags();
		UnlockSurface(m->bmpBackImageHiRes);
		UnlockSurface(m->bmpDrawImage);
		
		gdImageDestroy(gdImage);
		
#endif // DEDICATED_ONLY
		
	}
	
	
	Result parseData(CMap* m) {
		ParseFinalizer finalizer(this, m);
		
		m->Name = head.name;
		m->Width = (int)head.width;
		m->Height = (int)head.height;
		m->Type = Type;
		const int Width = (int)head.width, Height = (int)head.height;
		
		
		/*
		 notes("Level info:\n");
		 notes("  id = %s\n", id);
		 notes("  version = %i\n", version);
		 notes("  Name = %s\n", Name);
		 notes("  Width = %i\n", Width);
		 notes("  Height = %i\n", Height);
		 notes("  Type = %i\n", Type);
		 notes("  Theme_Name = %s\n", Theme_Name);
		 notes("  numobj = %i\n", numobj);
		 */
		
		// Load the images if in an image format
		if(Type == MPT_IMAGE)
		{
			// Allocate the map
		createMap:
			if(!m->Create(Width, Height, Theme_Name, m->MinimapWidth, m->MinimapHeight)) {
				errors << "CMap::Load (" << filename << "): cannot allocate map" << endl;
#ifdef MEMSTATS
				printMemStats();
#endif
				if(cCache.GetEntryCount() > 0) {
					hints << "current cache size is " << cCache.GetCacheSize() << ", we are clearing it now" << endl;
					cCache.Clear();
					goto createMap;
				}
				return false;
			}
			
			// Load the image format
			notes << "CMap::Load: level " << filename << " is in image format" << endl;
			return LoadImageFormat(m);
		} else if (ctf)  {
			errors("pixmap format is not supported for CTF levels\n");
			return false;
		}
		
		
		
		// Create a blank map
		if(!m->Create(Width, Height, Theme_Name, m->MinimapWidth, m->MinimapHeight)) {
			errors << "CMap::Load (" << filename << "): cannot create map" << endl;
			return false;
		}
		m->TileMap();
		
		SmartPointer<SDL_Surface> bmpImage = gfxCreateSurface(Width, Height);
		SmartPointer<SDL_Surface> bmpBackImage = gfxCreateSurface(Width, Height);
		
		// Lock the surfaces
		LOCK_OR_FAIL(bmpImage);
		LOCK_OR_FAIL(bmpBackImage);
		
		// Dirt map
		Uint8 *p1 = (Uint8 *)bmpImage.get()->pixels;
		Uint8 *p2 = (Uint8 *)bmpBackImage.get()->pixels;
		Uint8 *dstrow = p1;
		Uint8 *srcrow = p2;
		
		// Load the bitmask, 1 bit == 1 pixel with a yes/no dirt flag
		uint size = Width*Height/8;
		uint8_t *bitmask = new uint8_t[size];
		if (!bitmask)  {
			errors << "CMap::Load: Could not create bit mask" << endl;
			return false;
		}
		if(fread(bitmask,sizeof(uint8_t),size,fp) < size) {
			errors << "CMap::Load: could not read bitmask" << endl;
			return false;
		}
		
		static const unsigned char mask[] = {1,2,4,8,16,32,64,128};
		
		m->nTotalDirtCount = Width*Height;  // Calculate the dirt count
		
		m->lockFlags();
		
		for(size_t n = 0, i = 0, x = 0; i < size; i++, x += 8) {
			if (x >= (size_t)Width)  {
				srcrow += bmpBackImage.get()->pitch;
				dstrow += bmpImage.get()->pitch;
				p1 = dstrow;
				p2 = srcrow;
				x = 0;
			}
			
			// 1 bit == 1 pixel with a yes/no dirt flag
			for(size_t j = 0; j < 8;
				j++,
				n++,
				p1 += bmpImage.get()->format->BytesPerPixel,
				p2 += bmpBackImage.get()->format->BytesPerPixel) {
				
				if(bitmask[i] & mask[j])  {
					m->unsafeSetPixelFlag(n % Width, n / Width, PX_EMPTY);
					m->nTotalDirtCount--;
					memcpy(p1, p2, bmpImage.get()->format->BytesPerPixel);
				}
			}
		}
		
		m->unlockFlags();
		
		delete[] bitmask;
		
		// Unlock the surfaces
		UnlockSurface(bmpImage);
		UnlockSurface(bmpBackImage);
		
		m->bmpDrawImage = GetCopiedStretched2Image(bmpImage);
		bmpImage = NULL;
		m->bmpBackImageHiRes = GetCopiedStretched2Image(bmpBackImage);
		bmpBackImage = NULL;
		
		// Objects
		object_t o;
		m->NumObjects = 0;
		for(int i = 0; i < numobj; i++) {
			fread_compat(o.Type,	sizeof(Sint32),	1,	fp);
			EndianSwap(o.Type);
			fread_compat(o.Size,	sizeof(Sint32),	1,	fp);
			EndianSwap(o.Size);
			fread_compat(o.X,	    sizeof(Sint32),	1,	fp);
			EndianSwap(o.X);
			fread_compat(o.Y,	    sizeof(Sint32),	1,	fp);
			EndianSwap(o.Y);
			
			// Place the object
			if(o.Type == OBJ_STONE)
				m->PlaceStone(o.Size, CVec((float)o.X, (float)o.Y));
			else if(o.Type == OBJ_MISC)
				m->PlaceMisc(o.Size, CVec((float)o.X, (float)o.Y));
		}
		
		m->lxflagsToGusflags();
		return true;
	}
};

MapLoad* createMapLoad_LieroX() {
	return new ML_LieroX();
}
