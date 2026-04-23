// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Audio_Scope.h"

#include <assert.h>
#include <stdlib.h>
#include <sstream>
#include <stdexcept>

/* Copyright (C) 2005-2006 by Shay Green. Permission is hereby granted, free of
charge, to any person obtaining a copy of this software module and associated
documentation files (the "Software"), to deal in the Software without
restriction, including without limitation the rights to use, copy, modify,
merge, publish, distribute, sublicense, and/or sell copies of the Software, and
to permit persons to whom the Software is furnished to do so, subject to the
following conditions: The above copyright notice and this permission notice
shall be included in all copies or substantial portions of the Software. THE
SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

/* Copyright (c) 2019 by Michael Pyne, under the same terms as above. */

// Returns largest power of 2 that is <= the given value.
// From Henry Warren's book "Hacker's Delight"
static unsigned largest_power_of_2_within(unsigned x)
{
	static_assert(sizeof(x) <= 4, "This code doesn't work on 64-bit int");

	// Set all bits less significant than most significant bit that's set
	// e.g. 0b00100111  ->  0b00111111
	x = x | (x >>  1);
	x = x | (x >>  2);
	x = x | (x >>  4);
	x = x | (x >>  8);
	x = x | (x >> 16);

	// Clear all set bits besides the highest one.
	return x - (x >> 1);
}

// =============
// Error helpers
// =============

// If the given SDL return code is an error and if so returns a string
// with an explanation based on the provided explanation and SDL library
std::string check_sdl( int ret_code, const char *explanation )
{
	static std::string empty;
	if ( ret_code >= 0 )
		return empty;

	std::stringstream outstream;
	outstream << explanation << " " << SDL_GetError();
	return outstream.str();
}

// Overload of above
std::string check_sdl( const void *ptr, const char *explanation )
{
	return check_sdl( ptr ? 0 : -1, explanation );
}

#define RETURN_SDL_ERR(res,msg) do {            \
	auto check_res = check_sdl( (res), (msg) ); \
	if( !check_res.empty() ) {                  \
		return check_res;                       \
	}                                           \
} while (0)

// ===========
// Audio_Scope
// ===========

Audio_Scope::Audio_Scope()
{
}

Audio_Scope::~Audio_Scope()
{
	free( scope_lines );
	
	if ( window_renderer )
		SDL_DestroyRenderer( window_renderer );
	if ( window )
		SDL_DestroyWindow( window );
}

std::string Audio_Scope::init( int width, int height )
{
	assert( height <= 16384 );
	assert( !scope_lines ); // can only call init() once
	
	scope_height = height;
	scope_lines = reinterpret_cast<SDL_Point *>( calloc( width, sizeof( SDL_Point ) ) );
	if ( !scope_lines )
		return "Out of memory";
	
	buf_size = width;
	
	for ( sample_shift = 1; sample_shift < 14; )
		if ( ((0x7FFFL * 2) >> sample_shift++) < height )
			break;
	
	v_offset = (height - largest_power_of_2_within(height)) / 2;
	
	// What the user will see
	window = SDL_CreateWindow( "libgme sample player",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			width, height,
			0 /* no flags */ );
	RETURN_SDL_ERR( window, "Couldn't create output window" );

	// Render object used to update window (perhaps in video or GPU ram)
	window_renderer = SDL_CreateRenderer( window, -1, 0 /* no flags */ );
	RETURN_SDL_ERR( window_renderer, "Couldn't create renderer for output window" );
	
	return std::string(); // success
}

const char* Audio_Scope::draw( const short* in, long count, int step )
{
	if ( count >= buf_size )
	{
		count = buf_size;
	}
	
	SDL_SetRenderDrawColor( window_renderer, 0, 0, 0, 255 );
	SDL_RenderClear( window_renderer );

	render( in, count, step );
	SDL_SetRenderDrawColor( window_renderer, 0, 255, 0, 255 );
	SDL_RenderDrawLines( window_renderer, scope_lines, count );

	SDL_RenderPresent( window_renderer );

	return 0; // success
}

void Audio_Scope::render( short const* in, long count, int step )
{
	for( long i = 0; i < count; i++ )
	{
		// Average left, right channels
		int sample = (0x7FFF * 2 - in [i * step] - in [i * step + 1]) >> sample_shift;
		scope_lines [i].x = i;
		scope_lines [i].y = sample + v_offset;
	}
}

void Audio_Scope::set_caption( const char* caption )
{
    SDL_SetWindowTitle( window, caption );
}
