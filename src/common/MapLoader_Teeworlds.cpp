/*
 *  MapLoader_Teeworlds.cpp
 *  OpenLieroX
 *
 *  Created by Albert Zeyer on 11.4.12.
 *  code under LGPL
 *
 */

#include "MapLoader.h"
#include "MapLoader_common.h"
#include "StringUtils.h"
#include "FindFile.h"
#include "EndianSwap.h"
#include "util/Result.h"
#include "util/macros.h"
#include "GfxPrimitives.h"
#include "Color.h"
#include "CVec.h"
#include "gusanos/allegro.h"
#include "game/CMap.h"
#include <zlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

/* Teeworlds licence.txt:

Copyright (C) 2007-2012 Magnus Auvinen

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
  claim that you wrote the original software. If you use this software
  in a product, an acknowledgment in the product documentation would be
  appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
  misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

------------------------------------------------------------------------

IMPORTANT NOTE! The source under src/engine/external are stripped
libraries with their own licenses. Mostly BSD or zlib/libpng license but
check the individual libraries.

------------------------------------------------------------------------

With that being said, contact us if there is anything you want to do
that the license does not permit.
*/

/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */

/* from TML:
	:copyright: 2010-2011 by the TML Team, see AUTHORS for more details.
	:license: GNU GPL, see LICENSE for more details.
  */

// references:
// http://sushitee.github.com/tml/mapformat.html
// https://github.com/erdbeere/tml/blob/master/tml/datafile.py
// https://github.com/teeworlds/teeworlds/blob/master/src/engine/shared/datafile.cpp
// https://github.com/teeworlds/teeworlds/blob/master/src/game/mapitems.h
// https://github.com/teeworlds/teeworlds/blob/master/src/game/editor/io.cpp
// https://github.com/teeworlds/teeworlds/blob/master/src/game/client/components/maplayers.cpp
// https://github.com/teeworlds/teeworlds/blob/master/src/game/client/render_map.cpp
// https://github.com/teeworlds/teeworlds/blob/master/datasrc/content.py

struct ML_Teeworlds;

enum ItemType {
	ITEM_VERSION, ITEM_INFO, ITEM_IMAGE, ITEM_ENVELOPE, ITEM_GROUP, ITEM_LAYER,
	ITEM_ENVPOINT
};

enum LayerType {
	LAYERTYPE_INVALID, LAYERTYPE_GAME, LAYERTYPE_TILES, LAYERTYPE_QUADS
};

enum TileFlag {
	TileAir = 0,
	TileSolid,
	TileDeath,
	TileNohook
};

enum {
	ENTITY_NULL=0,
	ENTITY_SPAWN,
	ENTITY_SPAWN_RED,
	ENTITY_SPAWN_BLUE,
	ENTITY_FLAGSTAND_RED,
	ENTITY_FLAGSTAND_BLUE,
	ENTITY_ARMOR_1,
	ENTITY_HEALTH_1,
	ENTITY_WEAPON_SHOTGUN,
	ENTITY_WEAPON_GRENADE,
	ENTITY_POWERUP_NINJA,
	ENTITY_WEAPON_RIFLE,
	NUM_ENTITIES,
	ENTITY_OFFSET=255-16*4
};

enum {
	TILEFLAG_VFLIP=1,
	TILEFLAG_HFLIP=2,
	TILEFLAG_OPAQUE=4,
	TILEFLAG_ROTATE=8,
};

enum {
	TILERENDERFLAG_EXTEND = 1,
	LAYERRENDERFLAG_OPAQUE = 2,
	LAYERRENDERFLAG_TRANSPARENT = 4,
};

struct CDatafileItemType
{
	int32_t m_Type;
	int32_t m_Start;
	int32_t m_Num;

	void read(FILE* f) {
		fread_endian<int32_t>(f, m_Type);
		fread_endian<int32_t>(f, m_Start);
		fread_endian<int32_t>(f, m_Num);
	}
};

struct CDatafileItemOffset {
	int32_t m_Offset;
	void read(FILE* f) { fread_endian<int32_t>(f, m_Offset); }
};

typedef CDatafileItemOffset CDatafileDataOffset;

struct CDatafileItem
{
	int32_t m_TypeAndID;
	int32_t m_Size;

	void read(FILE* f) {
		fread_endian<int32_t>(f, m_TypeAndID);
		fread_endian<int32_t>(f, m_Size);
	}
};

struct CDatafileHeader
{
	char m_aID[4];
	int32_t m_Version;
	int32_t m_Size;
	int32_t m_Swaplen;
	int32_t m_NumItemTypes;
	int32_t m_NumItems;
	int32_t m_NumRawData;
	int32_t m_ItemSize;
	int32_t m_DataSize;

	Result read(FILE* f) {
		fread(m_aID, 1, sizeof(m_aID), f);
		if(memcmp(m_aID, "DATA", sizeof(m_aID)) != 0 && memcmp(m_aID, "ATAD", sizeof(m_aID)) != 0)
			return "signature wrong";
		fread_endian<int32_t>(f, m_Version);
		if(m_Version != 4)
			return "Teeworlds level version " + itoa(m_Version) + " not supported";
		fread_endian<int32_t>(f, m_Size);
		fread_endian<int32_t>(f, m_Swaplen);
		fread_endian<int32_t>(f, m_NumItemTypes);
		fread_endian<int32_t>(f, m_NumItems);
		fread_endian<int32_t>(f, m_NumRawData);
		fread_endian<int32_t>(f, m_ItemSize);
		fread_endian<int32_t>(f, m_DataSize);
		return true;
	}
};

struct CDatafileData
{
	int m_NumItemTypes;
	int m_NumItems;
	int m_NumRawData;
	int m_ItemSize;
	int m_DataSize;
	char m_aStart[4];
};

struct CDatafileInfo
{
	CDatafileItemType *m_pItemTypes;
	int *m_pItemOffsets;
	int *m_pDataOffsets;
	int *m_pDataSizes;

	char *m_pItemStart;
	char *m_pDataStart;
};

struct CDatafile
{
//	IOHANDLE m_File;
	unsigned m_Crc;
	CDatafileInfo m_Info;
	CDatafileHeader m_Header;
	int m_DataStartOffset;
	char **m_ppDataPtrs;
	char *m_pData;
};

struct TWMapInfo {
	std::string author;
	std::string map_version;
	std::string credits;
	std::string license;
	std::string settings;
};

struct TWImage {
	int32_t version;
	int32_t width;
	int32_t height;
	bool external;
	int32_t name_idx;
	int32_t data_idx;
	std::string name;
	SmartPointer<SDL_Surface> image;

	Result read(ML_Teeworlds* l, char* p, char* end);
};

struct TWTile {
	uint8_t index;
	uint8_t flags;
	uint8_t skip;
	uint8_t reserved;
	void read(char*& p, char* end) {
		index = pread_endian<uint8_t>(p, end);
		flags = pread_endian<uint8_t>(p, end);
		skip = pread_endian<uint8_t>(p, end);
		reserved = pread_endian<uint8_t>(p, end);
	}
};

struct TWPoint {
	VectorD2<int32_t> v;
	void read(char*& p, char* end) {
		v.x = pread_endian<int32_t>(p, end);
		v.y = pread_endian<int32_t>(p, end);
	}
};

struct TWColor {
	Color c;
	void read(char*& p, char* end) {
		c.r = pread_endian<int32_t>(p, end);
		c.g = pread_endian<int32_t>(p, end);
		c.b = pread_endian<int32_t>(p, end);
		c.a = pread_endian<int32_t>(p, end);
	}
};

struct TWTileLayer { // CMapItemLayerTilemap
	int32_t version;
	int32_t width;
	int32_t height;
	int32_t flags;
	bool game;
	TWColor color;
	int32_t color_env;
	int32_t color_env_offset;
	int32_t image_id;
	int32_t data_idx;
	std::string name;
	std::vector<TWTile> tiles;

	Result read(ML_Teeworlds* l, char* p, char* end);
};

struct TWQuad {
	TWPoint points[5];
	TWColor colors[4];
	TWPoint texCoords[4];
	int32_t pos_env;
	int32_t pos_env_offset;
	int32_t color_env;
	int32_t color_env_offset;
	void read(char*& p, char* end) {
		for(int i = 0; i < 5; ++i)
			points[i].read(p, end);
		for(int i = 0; i < 4; ++i)
			colors[i].read(p, end);
		for(int i = 0; i < 4; ++i)
			texCoords[i].read(p, end);
		pos_env = pread_endian<int32_t>(p, end);
		pos_env_offset = pread_endian<int32_t>(p, end);
		color_env = pread_endian<int32_t>(p, end);
		color_env_offset = pread_endian<int32_t>(p, end);
	}
};

struct TWQuadLayer { // CMapItemLayerQuads
	int32_t version;
	int32_t num_quads;
	int32_t data_idx;
	int32_t image_id;
	std::string name;
	std::vector<TWQuad> quads;
	Result read(ML_Teeworlds* l, char* p, char* end);
};

struct TWLayer {
	int32_t layer_version;
	LayerType type;
	int32_t flags;
	bool detail;
	TWTileLayer tileLayer;
	TWQuadLayer quadLayer;

	Result read(ML_Teeworlds* l, char* p, char* end);
};

struct TWGroup {
	int32_t version;
	int32_t offset_x;
	int32_t offset_y;
	int32_t parallax_x;
	int32_t parallax_y;
	int32_t start_layer;
	int32_t num_layers;
	bool use_clipping;
	int32_t clip_x;
	int32_t clip_y;
	int32_t clip_w;
	int32_t clip_h;
	std::string name;
	std::vector<TWLayer> layers;

	Result read(ML_Teeworlds* l, char* p, char* end);
	Result readLayers(ML_Teeworlds* l);
};

typedef std::string Raw; // or std::vector. doesn't really matter. std::string is usually copy-on-write which is preferable for us

static std::string rawReadStr(char* start, char* end) {
	size_t s = 0;
	while(start + s < end && start[s] != 0)
		++s;
	return std::string(start, s);
}

struct ML_Teeworlds : MapLoad {
	CMap* map;
	CDatafileHeader teeHeader;
	std::vector<CDatafileItemType> itemTypes;
	std::vector<CDatafileItemOffset> itemOffsets;
	std::vector<CDatafileDataOffset> dataOffsets;
	TWMapInfo info;
	std::vector<TWImage> images;
	std::vector<TWGroup> groups;

	virtual std::string format() { return "Teeworlds"; }
	virtual std::string formatShort() { return "Tee"; }

	virtual Result parseHeader(bool printErrors) {
		head.name = GetBaseFilenameWithoutExt(filename);
		fseek(fp, 0, SEEK_SET);
		if(NegResult r = teeHeader.read(fp)) return r.res;
		return true;
	}

	size_t baseOffset() {
		return
				36 + // CDatafileHeader
				teeHeader.m_NumItemTypes * 12 + // CDatafileItemType
				teeHeader.m_NumItems * 4 + // CDatafileItemOffset
				teeHeader.m_NumRawData * 4 + // CDatafileDataOffset
				teeHeader.m_NumRawData * 4; // uncompressed sizes
	}

	CDatafileItemType* getItemType(ItemType type) {
		foreach(t, itemTypes) {
			if(t->m_Type == type)
				return &*t;
		}
		return NULL;
	}

	size_t getItemSize(uint32_t itemIndex) {
		if(itemIndex >= itemOffsets.size()) return 0;
		if(itemIndex == itemOffsets.size() - 1)
			return teeHeader.m_ItemSize - itemOffsets[itemIndex].m_Offset - 8; // -8 to cut out type_and_id and size
		return itemOffsets[itemIndex+1].m_Offset - itemOffsets[itemIndex].m_Offset - 8;
	}

	Result getItem(uint32_t itemIndex, Raw& data) {
		if(itemIndex >= itemOffsets.size()) return "getItem: index " + itoa(itemIndex) + " out of bounds (size: " + itoa(itemOffsets.size()) + ")";
		size_t off = baseOffset() + itemOffsets[itemIndex].m_Offset + 8; // +8 to cut out type_and_id and size
		size_t size = getItemSize(itemIndex);
		data.resize(size);
		if(fseek(fp, off, SEEK_SET) != 0)
			return "getItem: fseek failed: " + std::string(strerror(errno));
		if(fread(&data[0], 1, size, fp) != size) {
			if(ferror(fp)) return "getItem: file read error";
			if(feof(fp)) return "getItem: eof reached";
			return "getItem: unknown fread error";
		}
		return true;
	}

	Result getItem(ItemType itemType, uint32_t relIndex, Raw& data) {
		CDatafileItemType* t = getItemType(itemType);
		if(!t) return "getItem: item type " + itoa((int)itemType) + " not found";
		if((int)relIndex >= t->m_Num) return "getItem: relIndex " + itoa(relIndex) + " out of bounds (size: " + itoa(t->m_Num) + ")";
		return getItem(t->m_Start + relIndex, data);
	}

	size_t getDataSize(uint32_t index) {
		if(index >= dataOffsets.size()) return 0;
		if(index == dataOffsets.size() - 1)
			return teeHeader.m_DataSize - dataOffsets[index].m_Offset;
		return dataOffsets[index + 1].m_Offset - dataOffsets[index].m_Offset;
	}

	Result getData(uint32_t index, Raw& data) {
		if(index >= dataOffsets.size()) return "getData: index out of bounds";
		size_t off = baseOffset() + teeHeader.m_ItemSize + dataOffsets[index].m_Offset;
		size_t size = getDataSize(index);
		data.resize(size);
		if(fseek(fp, off, SEEK_SET) != 0)
			return "getData: fseek failed: " + std::string(strerror(errno));
		if(fread(&data[0], 1, size, fp) != size) {
			if(ferror(fp)) return "getData: file read error";
			if(feof(fp)) return "getData: eof reached";
			return "getData: unknown fread error";
		}
		return true;
	}

	Result getDecompressedData(uint32_t index, Raw& data) {
		Raw compressedData;
		if(NegResult r = getData(index, compressedData)) return r.res;
		if(!Decompress(compressedData, &data))
			return "decompress failed";
		return true;
	}

	Result getDecompressedDataString(uint32_t index, std::string& str) {
		Raw data;
		if(NegResult r = getDecompressedData(index, data)) return r.res;
		if(data.size() == 0) return "getDecompressedDataString: data is empty";
		if(data[data.size() - 1] != '\0') return "getDecompressedDataString: non-zero at end";
		str = data.substr(0, data.size() - 1);
		return true;
	}

	Result parseVersion() {
		Raw data;
		if(NegResult r = getItem(ITEM_VERSION, 0, data))
			return "parseVersion failed: " + r.res.humanErrorMsg;
		if(data.size() < sizeof(uint32_t))
			return "version item not found / invalid";
		char* p = &data[0];
		uint32_t version = pread_endian<uint32_t>(p, &data[data.size()]);
		if(version != 1)
			return "wrong version";
		return true;
	}

	Result parseInfo() {
		Raw data;
		getItem(ITEM_INFO, 0, data);
		// TODO...
		return true; // not important
	}

	Result parseImages() {
		CDatafileItemType* t = getItemType(ITEM_IMAGE);
		if(!t) return "no images found";

		for(int i = 0; i < t->m_Num; ++i) {
			Raw item;
			if(NegResult r = getItem(t->m_Start + i, item))
				return "parseImages failed: " + r.res.humanErrorMsg;
			images.push_back(TWImage());
			if(NegResult r = images.back().read(this, &item[0], &item[item.size()]))
				return r.res;
		}
		return true;
	}

	Result parseGroups() {
		CDatafileItemType* t = getItemType(ITEM_GROUP);
		if(!t) return "no groups found";

		for(int i = 0; i < t->m_Num; ++i) {
			Raw item;
			if(NegResult r = getItem(t->m_Start + i, item))
				return "parseGroups failed: " + r.res.humanErrorMsg;
			groups.push_back(TWGroup());
			if(NegResult r = groups.back().read(this, &item[0], &item[item.size()]))
				return r.res;
			if(NegResult r = groups.back().readLayers(this))
				return r.res;
		}
		return true;
	}

	void debugPrint(TWLayer& l) {
		notes << "  layer type: " << l.type << endl;
		bool isGameLayer = false;
		if(l.type == LAYERTYPE_TILES && l.tileLayer.game) {
			notes << "  - is game layer" << endl;
			isGameLayer = true;
		}
		if(l.type == LAYERTYPE_TILES) {
			notes << "  - tiles layer, w: " << l.tileLayer.width << ", h: " << l.tileLayer.height << ", image_id: " << l.tileLayer.image_id << endl;
			if(l.tileLayer.image_id == -1)
				notes << "  - no image" << endl;
			if(l.tileLayer.image_id >= 0 && (size_t)l.tileLayer.image_id < images.size()) {
				TWImage& img = images[l.tileLayer.image_id];
				notes << "  - image w: " << img.width << ", h: " << img.height << endl;
			}
			else
				notes << "  - bad image_id" << endl;
		} else if(l.type == LAYERTYPE_QUADS) {
			notes << "  - quad layer, num quads: " << l.quadLayer.num_quads << ", image_id: " << l.quadLayer.image_id << endl;
			if(l.quadLayer.image_id == -1)
				notes << "  - no image" << endl;
			if(l.quadLayer.image_id >= 0 && (size_t)l.quadLayer.image_id < images.size()) {
				TWImage& img = images[l.quadLayer.image_id];
				notes << "  - image w: " << img.width << ", h: " << img.height << endl;
			}
			else
				notes << "  - bad image_id" << endl;
		}

	}

	// 64px is the orig tile pixel size. when we take that as doubleRes,
	// we would get 32px for tile size. This is too big. Thus scale it down.
	static const int TilePixelW = 1024/16, TilePixelH = 1024/16; // = 64
	static const int TargetTilePixelW = 20, TargetTilePixelH = 20;
#define ScaleFactor (float(TilePixelW) / TargetTilePixelW)

	void renderTilemap(int group, int layer, TWLayer& l, int RenderFlags, const SmartPointer<SDL_Surface>& surf) {
		TWTile* tiles = &l.tileLayer.tiles[0];
		const int w = l.tileLayer.width;
		const int h = l.tileLayer.height;
		//const Color color = l.tileLayer.color.c;

		if(l.tileLayer.image_id == -1) {
			warnings << "renderTilemap: no image" << endl;
			debugPrint(l);
			return;
		}
		if(l.tileLayer.image_id < 0 || (size_t)l.tileLayer.image_id >= images.size()) {
			warnings << "renderTilemap: invalid image_id" << endl;
			debugPrint(l);
			return;
		}
		TWImage& img = images[l.tileLayer.image_id];

		const float Scale = 32.f;
		const float ScreenX0 = 0.f, ScreenX1 = 1024.f;
		//const float ScreenY0 = 0.f, ScreenY1 = 768.f;
		const float ScreenW = ScreenX1 - ScreenX0;
		//const float ScreenH = ScreenY1 - ScreenY0;

		// calculate the final pixelsize for the tiles
		const float TilePixelSize = 1024/32.0f;
		const float FinalTileSize = Scale/(ScreenX1-ScreenX0) * ScreenW;
		const float FinalTilesetScale = FinalTileSize/TilePixelSize;

		//float r=1, g=1, b=1, a=1;
		/*if(ColorEnv >= 0)
		{
			float aChannels[4];
			//pfnEval(ColorEnvOffset/1000.0f, ColorEnv, aChannels, pUser);
			r = aChannels[0];
			g = aChannels[1];
			b = aChannels[2];
			a = aChannels[3];
		}*/

		//Graphics()->QuadsBegin();
		//Graphics()->SetColor(Color.r*r, Color.g*g, Color.b*b, Color.a*a);

		const int StartY = 0; //(int)(ScreenY0/Scale)-1;
		const int StartX = 0; //(int)(ScreenX0/Scale)-1;
		const int EndY = h; //(int)(ScreenY1/Scale)+1;
		const int EndX = w; //(int)(ScreenX1/Scale)+1;

		// adjust the texture shift according to mipmap level
		const float TexSize = 1024.0f;
		const float Frac = (1.25f/TexSize) * (1/FinalTilesetScale);
		const float Nudge = (0.5f/TexSize) * (1/FinalTilesetScale);

		for(int y = StartY; y < EndY; y++)
			for(int x = StartX; x < EndX; x++)
			{
				int mx = x;
				int my = y;

				if(RenderFlags&TILERENDERFLAG_EXTEND)
				{
					if(mx<0)
						mx = 0;
					if(mx>=w)
						mx = w-1;
					if(my<0)
						my = 0;
					if(my>=h)
						my = h-1;
				}
				else
				{
					if(mx<0)
						continue; // mx = 0;
					if(mx>=w)
						continue; // mx = w-1;
					if(my<0)
						continue; // my = 0;
					if(my>=h)
						continue; // my = h-1;
				}

				int c = mx + my*w;

				unsigned char Index = tiles[c].index;
				if(Index)
				{
					unsigned char Flags = tiles[c].flags;

					bool Render = false;
					if(Flags&TILEFLAG_OPAQUE)
					{
						if(RenderFlags&LAYERRENDERFLAG_OPAQUE)
							Render = true;
					}
					else
					{
						if(RenderFlags&LAYERRENDERFLAG_TRANSPARENT)
							Render = true;
					}

					if(Render)
					{

						const int tx = Index%16;
						const int ty = Index/16;
						const int Px0 = tx*(1024/16);
						const int Py0 = ty*(1024/16);
						const int Px1 = Px0+(1024/16)-1;
						const int Py1 = Py0+(1024/16)-1;

						float x0 = Nudge + Px0/TexSize+Frac;
						float y0 = Nudge + Py0/TexSize+Frac;
						float x1 = Nudge + Px1/TexSize-Frac;
						float y1 = Nudge + Py0/TexSize+Frac;
						float x2 = Nudge + Px1/TexSize-Frac;
						float y2 = Nudge + Py1/TexSize-Frac;
						float x3 = Nudge + Px0/TexSize+Frac;
						float y3 = Nudge + Py1/TexSize-Frac;

						SmartPointer<SDL_Surface> tileSurf = gfxCreateSurfaceAlpha(TargetTilePixelW, TargetTilePixelW);
						{
							SmartPointer<SDL_Surface> tileSurfOrig = gfxCreateSurfaceAlpha(TilePixelW, TilePixelW);
							CopySurface(tileSurfOrig.get(), img.image.get(), Px0, Py0, 0, 0, TilePixelW, TilePixelH);
							SDL_FillRect(tileSurf.get(), NULL, 0 /*fully transparent*/);
							DrawImageResampledAdv(tileSurf.get(), tileSurfOrig.get(), 0, 0, 0, 0, TilePixelW, TilePixelH, TargetTilePixelW, TargetTilePixelH);
						}

						if(Flags&TILEFLAG_VFLIP)
						{
							tileSurf = GetVMirroredImage(tileSurf);
							x0 = x2;
							x1 = x3;
							x2 = x3;
							x3 = x0;
						}

						if(Flags&TILEFLAG_HFLIP)
						{
							tileSurf = GetHMirroredImage(tileSurf);
							y0 = y3;
							y2 = y1;
							y3 = y1;
							y1 = y0;
						}

						if(Flags&TILEFLAG_ROTATE)
						{
							tileSurf = GetRotatedImage(tileSurf);
							float Tmp = x0;
							x0 = x3;
							x3 = x2;
							x2 = x1;
							x1 = Tmp;
							Tmp = y0;
							y0 = y3;
							y3 = y2;
							y2 = y1;
							y1 = Tmp;
						}

						// TODO: flags
						DrawImageAdv(surf.get(), tileSurf, 0, 0, mx * TargetTilePixelW, my * TargetTilePixelH, TargetTilePixelW, TargetTilePixelH);

						//Graphics()->QuadsSetSubsetFree(x0, y0, x1, y1, x2, y2, x3, y3);
						//IGraphics::CQuadItem QuadItem(x*Scale, y*Scale, Scale, Scale);
						//Graphics()->QuadsDrawTL(&QuadItem, 1);
					}
				}
				x += tiles[c].skip;
			}

		SaveSurface(img.image, "tiletex-" + itoa(group) + "-" + itoa(layer) + ".png", FMT_PNG, "");
	}

	void renderQuads(int group, int layer, TWLayer& l, int RenderFlags) {
		if(l.quadLayer.image_id == -1) {
			warnings << "renderQuads: no image" << endl;
			debugPrint(l);
			return;
		}
		if(l.quadLayer.image_id < 0 || (size_t)l.quadLayer.image_id >= images.size()) {
			warnings << "renderQuads: invalid image_id" << endl;
			debugPrint(l);
			return;
		}
		TWImage& img = images[l.quadLayer.image_id];

		SaveSurface(img.image, "quads-" + itoa(group) + "-" + itoa(layer) + ".png", FMT_PNG, "");
	}

	void renderMap(int renderType, const SmartPointer<SDL_Surface>& surf) {
		bool passedGameLayer = false;

		notes << "found " << groups.size() << " groups" << endl;
		int group = -1;
		foreach(g, groups) {
			++group;
			notes << " group: " << g->layers.size() << " layers" << endl;

			// ignoring clipping scope ...
			// MapScreenToGroup(Center.x, Center.y, pGroup);

			int layer = -1;
			foreach(lp, g->layers) {
				++layer;
				TWLayer& l = *lp;
				bool render = false;
				bool isGameLayer = false;
				if(l.type == LAYERTYPE_TILES && l.tileLayer.game) {
					isGameLayer = true;
					passedGameLayer = true;
					// seems we need to init with sane defaults (tw/game/layers.cpp)
					g->offset_x = g->offset_y = g->parallax_x = g->parallax_y = 0;
				}

				if(renderType == -1)
					render = true;
				else if(renderType == 0) {
					if(passedGameLayer) return;
					render = true;
				}
				else {
					if(passedGameLayer && !isGameLayer)
						render = true;
				}

				if(!render) continue;
				if(isGameLayer) continue;

				if(l.type == LAYERTYPE_TILES) {
					// this seems to happen and doesn't seem to be a problem
					// (except of the background color, maybe that is l.tileLayer.color.c)
					/*if(l.tileLayer.width != surf->w / TargetTilePixelW || l.tileLayer.height != surf->h / TargetTilePixelH) {
						notes << "ignoring layer " << group << "-" << layer << ", size mismatch" << endl;
						debugPrint(l);
						continue;
					}*/
					// blendnone
					renderTilemap(group, layer, l, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_OPAQUE, surf);
					// blendnormal
					renderTilemap(group, layer, l, TILERENDERFLAG_EXTEND|LAYERRENDERFLAG_TRANSPARENT, surf);
				} else if(l.type == LAYERTYPE_QUADS) {
					if(l.quadLayer.image_id == -1)
						continue;
					// blendnone
					//renderQuads(group, layer, l, LAYERRENDERFLAG_OPAQUE);
					// blendnormal
					renderQuads(group, layer, l, LAYERRENDERFLAG_TRANSPARENT);
				}
			}
		}
	}

	bool getGameLayer(TWGroup*& group, TWLayer*& layer) {
		foreach(g, groups) {
			foreach(lp, g->layers) {
				TWLayer& l = *lp;
				if(l.type == LAYERTYPE_TILES && l.tileLayer.game) {
					group = &*g;
					layer = &l;
					return true;
				}
			}
		}
		return false;
	}

	// TargetTilePixel size is doubleRes. This is for material size, i.e. singleRes.
	static const int TileW = TargetTilePixelW/2, TileH = TargetTilePixelH/2;

	void setMaterialIndex(uint32_t x, int32_t y, uint8_t index) {
		for(int ty = 0; ty < TileH; ++ty)
			for(int tx = 0; tx < TileW; ++tx)
				map->material->line[y + ty][x + tx] = index;
	}

	Result buildMaterialMap() {
		TWGroup* gameGroup = NULL;
		TWLayer* gameLayer = NULL;
		if(!getGameLayer(gameGroup, gameLayer))
			return "game layer not found";
		TWTileLayer& l = gameLayer->tileLayer;

		notes << "TW map w: " << l.width << ", h: " << l.height << endl;
		map->Width = l.width * TileW;
		map->Height = l.height * TileH;
		map->material = create_bitmap_ex(8, map->Width, map->Height);

		for(int y = 0; y < l.height; ++y) {
			for(int x = 0; x < l.width; ++x) {
				TWTile& t = l.tiles[y * l.width + x];
				uint8_t matIndex = MATINDEX_BG;
				if(t.index == TileAir)
					matIndex = MATINDEX_BG;
				else if(t.index == TileSolid)
					matIndex = MATINDEX_SOLID;
				else if(t.index == TileDeath)
					matIndex = MATINDEX_DEATH;
				else if(t.index == TileNohook)
					matIndex = MATINDEX_NOHOOK;
				else if(t.index >= ENTITY_OFFSET) {
					Vec pos((float)x * TileW + TileW/2, (float)y * TileH + TileH/2);
					// see CGameMode::TeamName() for references of team-index
					// and see CTF:getTeamBasePos(). that is why we have +1.
					// and also see (level.cpp) canPlayerRespawn().
					switch(t.index - ENTITY_OFFSET) {
					case ENTITY_SPAWN:
						map->config()->spawnPoints.push_back(SpawnPoint(pos, -1));
						break;
					case ENTITY_SPAWN_BLUE:
						map->config()->spawnPoints.push_back(SpawnPoint(pos, 1));
						break;
					case ENTITY_SPAWN_RED:
						map->config()->spawnPoints.push_back(SpawnPoint(pos, 2));
						break;
					case ENTITY_FLAGSTAND_BLUE:
						map->config()->teamBases.push_back(SpawnPoint(pos, 1));
						break;
					case ENTITY_FLAGSTAND_RED:
						map->config()->teamBases.push_back(SpawnPoint(pos, 2));
						break;
					default:; // ignore
					}
				}
				// silently ignore for now. some maps (e.g. TeeCity) have a lot of them...
				//else notes << "unknown game layer tile index " << (int)t.index << endl;
				setMaterialIndex(x * TileW, y * TileH, matIndex);
			}
		}

		return true;
	}

#define ParalaxScaleFactor (1.0f)

	IVec getParalaxSize() {
		IVec size(640,480); // viewport size should be minimum
		foreach(g, groups) {
			foreach(lp, g->layers) {
				TWLayer& l = *lp;
				if(l.type == LAYERTYPE_QUADS && l.tileLayer.game) {
					if(l.quadLayer.image_id == -1)
						continue;
					if(l.quadLayer.image_id < 0 || (size_t)l.quadLayer.image_id >= images.size())
						continue;
					TWImage& img = images[l.quadLayer.image_id];
					IVec imgSize(Vec(img.width / ParalaxScaleFactor, img.height / ParalaxScaleFactor));
					if(imgSize.x > size.x) size.x = imgSize.x;
					if(imgSize.y > size.y) size.y = imgSize.y;
				}
			}
		}
		return size;
	}

	void createParalax() {
		IVec paralaxSize = getParalaxSize();
		map->bmpParallax = gfxCreateSurface(paralaxSize.x, paralaxSize.y);
		DrawRectFill(map->bmpParallax.get(), 0, 0, map->bmpParallax->w, map->bmpParallax->h,
					 // that's the baby-blue color commonly used in Teeworlds :P
					 Color(154, 183, 215));
		foreach(g, groups) {
			foreach(lp, g->layers) {
				TWLayer& l = *lp;
				if(l.type == LAYERTYPE_QUADS && l.tileLayer.game) {
					if(l.quadLayer.image_id == -1)
						continue;
					if(l.quadLayer.image_id < 0 || (size_t)l.quadLayer.image_id >= images.size())
						continue;
					TWImage& img = images[l.quadLayer.image_id];
					IVec imgSize(Vec(img.width / ParalaxScaleFactor, img.height / ParalaxScaleFactor));
					DrawImageResampledAdv(
								map->bmpParallax.get(), img.image,
								0, 0,
								(map->bmpParallax->w - imgSize.x) / 2,
								(map->bmpParallax->h - imgSize.y) / 2,
								img.image->w, img.image->h,
								imgSize.x, imgSize.y
								);
				}
			}
		}
	}

	uint8_t highestAlphaForegroundAirPixel_aroundArea(int x_, int y_) {
		uint8_t alpha = 0;
		static const int AreaTileSize = 3;
		for(int y = std::max(0, y_ - TileH * AreaTileSize); y < std::min(map->material->h, y_ + TileW * AreaTileSize + 1); y += TileH)
		for(int x = std::max(0, x_ - TileW * AreaTileSize); x < std::min(map->material->w, x_ + TileW * AreaTileSize + 1); x += TileW) {
			if(map->material->line[y][x] != MATINDEX_BG) continue;
			for(short dy = 0; dy < 2; ++dy)
			for(short dx = 0; dx < 2; ++dx) {
				uint8_t curAlpha = Color(map->bmpForeground->format, GetPixel(map->bmpForeground.get(), x*2 + dx, y*2 + dy)).a;
				if(curAlpha > alpha)
					alpha = curAlpha;
				if(alpha >= 200)
					// this seems too much. return sth less
					return 200;
			}
		}
		return alpha;
	}

	// we can optimize the pixel flags for rock when there are round corners.
	// just make every rock-pixel where we have alpha=0 a background pixel.
	void optimizeMaterialMap() {
		for(int y = 0; y < map->material->h; ++y)
			for(int x = 0; x < map->material->w; ++x) {
				if(map->material->line[y][x] != MATINDEX_SOLID) continue;

				uint8_t alphaThreshold = highestAlphaForegroundAirPixel_aroundArea(x, y);
				bool haveAlpha = false;
				for(short dy = 0; dy < 2; ++dy)
				for(short dx = 0; dx < 2; ++dx) {
					uint8_t a1 = Color(map->bmpForeground->format, GetPixel(map->bmpForeground.get(), x*2 + dx, y*2 + dy)).a;
					uint8_t a2 = Color(map->bmpDrawImage->format, GetPixel(map->bmpDrawImage.get(), x*2 + dx, y*2 + dy)).a;
					if(a1 <= alphaThreshold && a2 <= alphaThreshold)
						haveAlpha = true;
				}
				if(haveAlpha)
					map->material->line[y][x] = MATINDEX_BG;
			}
	}

	virtual Result parseData(CMap* m) {
		ParseFinalizer finalizer(this, m);
		map = m;

		if(NegResult r = parseHeader(false)) return r.res; // just do it again to seek to right pos

		m->Name = head.name;
		m->Type = MPT_IMAGE;
		m->m_config = LevelConfig();

		if(teeHeader.m_NumItemTypes < 0)
			return "invalid numItemTypes";
		itemTypes.resize(teeHeader.m_NumItemTypes);
		for(int i = 0; i < teeHeader.m_NumItemTypes; ++i)
			itemTypes[i].read(fp);

		if(teeHeader.m_NumItems < 0)
			return "invalid numItems";
		itemOffsets.resize(teeHeader.m_NumItems);
		for(int i = 0; i < teeHeader.m_NumItems; ++i)
			itemOffsets[i].read(fp);

		if(teeHeader.m_NumRawData < 0)
			return "invalid numRawData";
		dataOffsets.resize(teeHeader.m_NumRawData);
		for(int i = 0; i < teeHeader.m_NumRawData; ++i)
			dataOffsets[i].read(fp);

		if(NegResult r = parseVersion()) return r.res;
		if(NegResult r = parseImages()) return r.res;
		if(NegResult r = parseGroups()) return r.res;

		// envpoints, envelopes not needed (?)

		if(NegResult r = buildMaterialMap()) return r.res;

		map->bmpDrawImage = map->bmpBackImageHiRes = gfxCreateSurfaceAlpha(map->Width / TileW * TargetTilePixelW, map->Height / TileH * TargetTilePixelH);
		SDL_FillRect(map->bmpDrawImage.get(), NULL, SDL_MapRGBA(map->bmpDrawImage.get()->format, 0, 0, 0, 0)); // alpha everywhere
		map->bmpForeground = gfxCreateSurfaceAlpha(map->Width / TileW * TargetTilePixelW, map->Height / TileH * TargetTilePixelH);
		SDL_FillRect(map->bmpForeground.get(), NULL, SDL_MapRGBA(map->bmpForeground.get()->format, 0, 0, 0, 0)); // alpha everywhere

		renderMap(0, map->bmpBackImageHiRes);
		renderMap(1, map->bmpForeground);
		createParalax();

		optimizeMaterialMap();

		if(!m->MiniNew(m->material->w, m->material->h)) {
			errors << "Teeworlds lvl loader (" << filename << "): cannot create minimap" << endl;
			return "cannot create minimap";
		}

		map->lxflagsToGusflags();
		return true;
	}

};

Result TWImage::read(ML_Teeworlds *l, char *p, char *end) {
	version = pread_endian<int32_t>(p, end);
	width = pread_endian<int32_t>(p, end);
	height = pread_endian<int32_t>(p, end);
	external = (bool) pread_endian<int32_t>(p, end);
	name_idx = pread_endian<int32_t>(p, end);
	data_idx = pread_endian<int32_t>(p, end);
	if(p > end) return "image item data is invalid, read behind end";

	if(width <= 0)
		return "image width invalid: " + itoa(width);
	if(height <= 0)
		return "image height invalid: " + itoa(width);

	if(NegResult r = l->getDecompressedDataString(name_idx, name)) return r.res;
	if(external) {
		std::string filename = "data/teeworlds/mapres/" + name + ".png";
		image = LoadGameImage(filename, true);
		if(!image.get()) {
			warnings << "failed to load external image " << name << endl;
			image = gfxCreateSurfaceAlpha(1, 1); // create dummy surface
			SDL_FillRect(image.get(), NULL, 0); // alpha everywhere
		}
		else if(image->w != width || image->h != height)
			warnings << "size of mapres " << name << " does not match."
			<< " expected: " << IVec(width,height) << ", got: " << IVec(image->w,image->h)
			<< endl;
	}
	else { // non external
		Raw data;
		if(NegResult r = l->getDecompressedData(data_idx, data))
			return r.res;
		if(data.size() != (size_t) width * height * 4)
			return "raw image data size invalid. expected: " + itoa(width*height*4) + ", got: " + itoa(data.size());
		image = SDL_CreateRGBSurface(
					0,
					width, height,
					32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		LockSurface(image);
		for(int y = 0; y < height; ++y)
			memcpy((uint8_t*)image->pixels + y * image->pitch, &data[y * width * 4], width * 4);
		UnlockSurface(image);
	}

	return true;
}

Result TWGroup::read(ML_Teeworlds* l, char* p, char* end) {
	version = pread_endian<int32_t>(p, end);
	offset_x = pread_endian<int32_t>(p, end);
	offset_y = pread_endian<int32_t>(p, end);
	parallax_x = pread_endian<int32_t>(p, end);
	parallax_y = pread_endian<int32_t>(p, end);
	start_layer = pread_endian<int32_t>(p, end);
	num_layers = pread_endian<int32_t>(p, end);
	if(version >= 2) {
		use_clipping = (bool) pread_endian<int32_t>(p, end);
		clip_x = pread_endian<int32_t>(p, end);
		clip_y = pread_endian<int32_t>(p, end);
		clip_w = pread_endian<int32_t>(p, end);
		clip_h = pread_endian<int32_t>(p, end);
	}
	else {
		use_clipping = false;
		clip_x = clip_y = clip_w = clip_h = 0;
	}
	if(version >= 3 && end - p >= 3*4)
		name = rawReadStr(p, p + 3*4);
	else
		name = "";

	// ptrdiff_t is 64-bit on LLP64 Windows, where neither itoa(long)
	// nor itoa(int) is preferred — cast explicitly to long.
	if(p > end) return "group item data is invalid, read behind end by " + itoa((long)(p - end)) + " bytes";
	return true;
}

Result TWGroup::readLayers(ML_Teeworlds* l) {
	CDatafileItemType* t = l->getItemType(ITEM_LAYER);
	if(!t) return "item layer type not found";
	if(start_layer + num_layers > t->m_Num)
		return "start_layer + num_layers are going out of bounds of item layers";
	layers.resize(num_layers);
	for(int j = 0; j < num_layers; ++j) {
		Raw data;
		if(NegResult r = l->getItem(t->m_Start + start_layer + j, data))
			return "readLayers: " + r.res.humanErrorMsg;
		if(NegResult r = layers[j].read(l, &data[0], &data[data.size()]))
			return "readLayer: " + r.res.humanErrorMsg;
	}
	return true;
}

Result TWLayer::read(ML_Teeworlds* l, char* p, char* end) {
	layer_version = pread_endian<int32_t>(p, end);
	type = (LayerType) pread_endian<int32_t>(p, end);
	flags = pread_endian<int32_t>(p, end);
	detail = (flags & 1) != 0;

	if(type == LAYERTYPE_TILES) {
		return tileLayer.read(l, p, end);
	} else if(type == LAYERTYPE_QUADS) {
		return quadLayer.read(l, p, end);
	} else {
		// ignore other types for now...
		if(p > end) return "layer itemdata is invalid, read behind end";
		return true;
	}
	assert(false); return false;
}

Result TWTileLayer::read(ML_Teeworlds* l, char* p, char* end) {
	version = pread_endian<int32_t>(p, end);
	width = pread_endian<int32_t>(p, end);
	height = pread_endian<int32_t>(p, end);
	flags = pread_endian<int32_t>(p, end);
	game = (flags & 1) != 0;
	color.read(p, end);
	color_env = pread_endian<int32_t>(p, end);
	color_env_offset = pread_endian<int32_t>(p, end);
	image_id = pread_endian<int32_t>(p, end);
	data_idx = pread_endian<int32_t>(p, end);
	name = "";
	if(version >= 3 && end - p >= 3*4)
		name = rawReadStr(p, p + 3*4);

	if(width <= 0)
		return "tile layer: got invalid width " + itoa(width);
	if(height <= 0)
		return "tile layer: got invalid height " + itoa(height);

	{
		Raw data;
		if(NegResult r = l->getDecompressedData(data_idx, data))
			return "tile data: " + r.res.humanErrorMsg;
		if(data.size() % 4)
			return "tile data size is not a multiple of 4";
		tiles.resize(data.size() / 4);
		char *tp = &data[0], *tend = &data[data.size()];
		for(size_t i = 0; i < tiles.size(); ++i)
			tiles[i].read(tp, tend);

		if(tiles.size() != (size_t) width * height)
			return "tile count invalid, expected: " + itoa(width*height) + ", got: " + itoa(tiles.size());
	}

	// ignore tele_list, speedup_list for now. only used for TeeWorlds race mod

	if(p > end) return "tilelayer itemdata is invalid, read behind end";
	return true;
}

Result TWQuadLayer::read(ML_Teeworlds* l, char* p, char* end) {
	version = pread_endian<int32_t>(p, end);
	num_quads = pread_endian<int32_t>(p, end);
	data_idx = pread_endian<int32_t>(p, end);
	image_id = pread_endian<int32_t>(p, end);
	name = "";
	if(version >= 2 && end - p >= 3*4)
		name = rawReadStr(p, p + 3*4);

	{
		Raw data;
		if(NegResult r = l->getDecompressedData(data_idx, data))
			return "quad data: " + r.res.humanErrorMsg;
		if(data.size() % 152)
			return "quad data size is not a multiple of 152";
		quads.resize(data.size() / 152);
		char *tp = &data[0], *tend = &data[data.size()];
		for(size_t i = 0; i < quads.size(); ++i)
			quads[i].read(tp, tend);

		if(num_quads != (int)quads.size())
			return "quads num expected: " + itoa(num_quads) + ", got: " + itoa(quads.size());
	}

	if(p > end) return "quadlayer itemdata is invalid, read behind end";
	return true;
}

MapLoad* createMapLoad_Teeworlds() {
	return new ML_Teeworlds();
}
