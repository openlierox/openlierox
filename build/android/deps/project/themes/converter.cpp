#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>
#include <SDL_image.h>
#include <vector>
#include <algorithm>
#include <string>
#include <netinet/in.h>


int
main(int argc, char *argv[])
{
	if(argc < 3)
	{
		printf("Usage: converter source.png target.raw [target bpp = 16 or 32]\n");
		return 1;
	}
	SDL_Surface * src = IMG_Load(argv[1]);
	if(!src)
		return 1;
	bool perPixeAlpha = true;
	bool target32bpp = false;
	if( argc >= 3 && strcmp(argv[3], "32") == 0 )
		target32bpp = true;
	/*
	if( src->format->BitsPerPixel == 32 )
	{
		for( int i = 0; i < src->h; i++ )
		{
			for( int ii = 0; ii < src->w; ii++ )
			{
				Uint32 alpha = (* (Uint32 *) ((Uint8 *)src->pixels + i*src->pitch + ii*4)) & src->format->Amask;
				if( src->format->Amask >= 0x000000ff )
					alpha /= 0x1000000;
				if( alpha > 15 && alpha < 240 )
				{
					perPixeAlpha = true;
					break;
				}
			}
		}
	}
	*/
	printf("Converter: %s BPP %d %dx%d perPixeAlpha %d target %d bpp\n", argv[1], src->format->BitsPerPixel, src->w, src->h, (int)perPixeAlpha, target32bpp ? 32 : 16);
	SDL_Surface * format1 = SDL_CreateRGBSurface( SDL_SWSURFACE|SDL_SRCALPHA, 1, 1, 16, 0xF800, 0x7C0, 0x3E, 0x1 );
	if( perPixeAlpha )
		format1 = SDL_CreateRGBSurface( SDL_SWSURFACE|SDL_SRCALPHA, 1, 1, 16, 0xF000, 0xF00, 0xF0, 0xF );
	if( target32bpp )
		format1 = SDL_CreateRGBSurface( SDL_SWSURFACE|SDL_SRCALPHA, 1, 1, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000 );
	if(!format1)
		return 1;
	SDL_Surface * dst = SDL_ConvertSurface(src, format1->format, SDL_SWSURFACE|SDL_SRCALPHA);
	if(!dst)
		return 1;
	FILE * ff = fopen(argv[2], "wb");
	if(!ff)
		return 1;
	int w = htonl(dst->w);
	fwrite( &w, 1, 4, ff );
	int h = htonl(dst->h);
	fwrite( &h, 1, 4, ff );
	int format = htonl(target32bpp ? 2 : (perPixeAlpha ? 1 : 0));
	fwrite( &format, 1, 4, ff );
	for( int i = 0; i < dst->h; i++ )
	{
		for( int ii = 0; ii < dst->w; ii++ )
		{
			if( (!target32bpp) && (
				* (Uint16 *) ((Uint8 *)dst->pixels + i*dst->pitch + ii*2) & 0x1 == 0 && ! perPixeAlpha) )
				* (Uint16 *) ((Uint8 *)dst->pixels + i*dst->pitch + ii*2) = 0;
			fwrite( (Uint8 *)dst->pixels + i*dst->pitch + ii*(target32bpp?4:2), 1, (target32bpp?4:2), ff );
		}
	}
	fclose(ff);
	return 0;
}

#ifdef WIN32
#include <windows.h>
int CALLBACK WinMain(
  HINSTANCE hInstance,
  HINSTANCE hPrevInstance,
  LPSTR lpCmdLine,
  int nCmdShow
)
{
	return main(0, NULL);
};

#endif
