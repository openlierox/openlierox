# OpenLieroX

This is the game OpenLieroX! Homepage: <http://openlierox.net>

Content of this file:

* Description
* About
* Compilation
* Game search paths
* Development
* Report bugs / feature requests
* Thank you

## Description

It's some type of a real-time, excessive clone of Worms. Or like 2D Quake with worms and ninja ropes.

## About

OpenLieroX is based on Jason Boettcher's famous Liero Xtreme.

Jason B. has released his work in 2006 under the zlib-licence and after some months of work, we ported and enhanced his work and got OpenLieroX.

## Compilation

For more details, read here: <http://www.openlierox.net/wiki/index.php/Compile_OpenLieroX>

### Linux/Unix

Install development dependencies, for Ubuntu and Debian it can be done with:

```
sudo apt install \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-mixer-dev \
    libxml2-dev \
    libgd-dev \
    zlib1g-dev \
    libzip-dev \
    libx11-dev \
    libcurl4-openssl-dev \
    libboost-dev \
    libopenal-dev \
    libalut-dev \
    libvorbis-dev \
    libyaml-cpp-dev \
    binutils-dev \
    libiberty-dev
```

To build use `CMake`, e.g.:

```
cmake .
```

or:

```
cmake -DHAWKNL_BUILTIN=Yes .
```

followed by:

```
make -j8
```

Then for starting the game:

```
./start.sh
```

### Mac OS X

Use the Xcode project under `build/Xcode`.

### Windows

Use the MSVC project under `build/msvc 2010`.

## Game search paths

The game uses case insensitive filenames (it will use the first found on case sensitive filesystems).
The game searches the paths `~/.OpenLieroX`, `./` and `/usr/share/games/OpenLieroX` for game-data (all path are relative to this bases) (in this order) by default.
You can also add more searchpathes and change this in `cfg/options.cfg`.
Own modified configs, screenshots and other stuff always will be stored in `~/.OpenLieroX`.

More details: <http://www.openlierox.net/wiki/index.php/Virtual_File_System>

## Development

If you are interested in the development, either in how we work, the work / source code itself or if you want to support us in any way, read here: <http://www.openlierox.net/wiki/index.php/Development>

## Report bugs / feature requests

If you find a bug in OpenLieroX, please fill in a bug report! If you have a nice idea about a feature or if you just miss something, please fill in a feature request. We have a tracker for this: <https://github.com/albertz/openlierox/issues>

When filling in a bug report, please be precise! Say exactly, what version you are using, what operating system (Windows, MacOSX, Linux) and what version of that you are using. If you are not using the most recent version of OpenLieroX, please try with the newest version if the problem is already fixed there. If you are using a version from Git, please say exactly what revision that is.

Beside that, for the bug itself, the console output is needed in almost all cases. Just post it or attach it to the bug report. Depending on the operating system, the console output of OpenLieroX is a bit hidden from you.

For Linux/Unix, just call the game via console. You will get all the output there.

For MacOSX, there are multiple ways.

One way of getting the output (also from already running OLX or already exited OLX) is the tool `/Applications/Utilities/Console`. MacOSX saves the console output of every application and this tool can show them. Specify the filter "openlierox" and you will see all the output.

Another way is similar to Unix, just call it via console (e.g. with the `/Applications/Utilities/Terminal` application). E.g., if you installed OpenLieroX to `/Applications`, the full path to call it via console would be:

    /Applications/OpenLieroX.app/Contents/MacOS/OpenLieroX

For Windows, there should be a file `stdout.txt` in the OpenLieroX directory containing all the output.

## Thanks for all the fish

So, well, I think that was all the important stuff.
Look at the Homepage for further details.

* Official homepage of OpenLieroX: <http://openlierox.net>
* Wiki containing many further details: <http://openlierox.net/wiki>
* Forum around the game: <http://openlierox.net/forum>
* GitHub project page: <https://github.com/albertz/openlierox>
* SourceForge project page: <http://sourceforge.net/projects/openlierox/>
* Homepage of Albert Zeyer: <http://www.az2000.de/>

Thank you for enjoying it!

-- The team: Dark Charlie, Albert Zeyer and the [RIP] clan
