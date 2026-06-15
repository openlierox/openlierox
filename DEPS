Gentoo: gcc
Debian: build-essential

-- libsdl
Gentoo: media-libs/libsdl
Debian: libsdl2-dev
tested versions: 1.2.11, 1.2.6

-- SDL_image
Debian: libsdl2-image-dev
Gentoo: media-libs/sdl-image
tested versions: 1.2.5

-- hawknl
* builtin version possible
Gentoo: dev-games/hawknl
Debian: no deb-package found, you have to install it manually
tested versions: 1.68

-- gd
Gentoo: media-libs/gd
Debian: libgd2-noxpm-dev
tested versions: 2.0.33

-- zlib
Gentoo: sys-libs/zlib
Debian: zlib1g-dev
tested versions: 1.2.3

-- libzip
* builtin version possible
Gentoo: dev-libs/libzip
Debian: libzip-dev
tested version: 0.8

-- libxml2
Gentoo: dev-libs/libxml2
Debian: libxml2-dev
tested versions: 2.6.26

-- libcurl
Gentoo: net-misc/curl
Debian: libcurl4-dev
tested versions: 7.19.6

-- libboost-signals
Debian: libboost-signals-dev
tested versions: 1.38

-- libopenal
Debian: libopenal-dev
tested versions: 1.8.466-2

-- libalut
Debian: libalut-dev
tested versions: 1.1.0

-- libvorbis
Debian: libvorbis-dev
tested versions: 1.2.0

-- libbfd
Debian: binutils-dev

-- libyaml-cpp
Debian: libyaml-cpp-dev

----

Quick command for Debian/Ubuntu:
sudo apt-get install build-essential git cmake libsdl2-dev libsdl2-mixer-dev libsdl2-image-dev libgd2-noxpm-dev zlib1g-dev libzip-dev libxml2-dev libx11-dev libcurl4-gnutls-dev libboost-signals-dev libboost-system-dev libopenal-dev libalut-dev libvorbis-dev
cmake -D HAWKNL_BUILTIN=1 -D DEBUG=0 -D X11=1 .

Quick command for OpenBSD:
pkg_add subversion cmake sdl sdl-image sdl-mixer libxml gd

Quick command for FreeBSD/PC-BSD:
pkg_add -r subversion cmake sdl sdl_image sdl_mixer libxml2 gd
cmake -DHAWKNL_BUILTIN=1 -DLIBZIP_BUILTIN=1 -D DEBUG=0 .

You can cross-compile Windows .EXE using Mingw from Linux -
install "mingw32" Debian package, run "mingw_cross_compile.sh" and then "make".
Note that you'll need 64-bit CPU, because linker chews up 1.6 Gb of RAM, and that number will grow.
