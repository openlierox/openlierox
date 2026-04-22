# OpenLieroX — notes for Claude

This file is loaded automatically into Claude's context for this repo.
Keep it short; it's a map, not documentation.

## Android port (build-for-android branch)

**Status:** OpenLieroX 0.59 boots to the main menu on Android (SDL2,
x86_64 emulator confirmed). The engine loads Gusanos + Lua 5.1 and
runs the startup Lua script.

**Build:** `./build-android.sh` drives an external commandergenius
checkout at `~/git/commandergenius` (clone of
https://github.com/pelya/commandergenius) as the SDL2/Gradle/NDK
harness. Every run overlays this repo onto the port:

  src/ include/ libs/ VERSION CMakeLists.txt CMakeOlxCommon.cmake
  build/android/{AndroidAppSettings.cfg,AndroidPreBuild.sh,liero.raw,java.patch}
  share/{android-icon.png,tv-banner.png}

then wipes stale application .o files and runs the port's build.sh.
APK lands at [build-output/openlierox-android.apk](build-output/).

`./run-android-emulator.sh` launches the `olx` AVD for testing;
install/launch via `--install build-output/openlierox-android.apk --run`.

`./build-android.sh --sync-data` repacks `share/gamedir/` into
`AndroidData/data.zip.xz` so the APK ships with current game content.

**Port metadata** lives in [build/android/](build/android/). Key
settings in `AndroidAppSettings.cfg`:

- `LibSdlVersion=2` (0.59 uses SDL_CreateWindow etc.)
- `CompiledLibraries`: SDL2_image, SDL2_mixer, xml2, curl, ssl, crypto,
  gd, png, jpeg, vorbis, ogg, lzma, mad, openal
- `AppSubdirsBuild='src/include src/src/* src/libs/hawknl/include
  src/libs/hawknl/src src/libs/libzip src/libs/linenoise src/libs/lua
  src/libs/pstreams src/libs/boost-hdr src/libs/boost_process'` —
  recursive glob under `src/src` picks up gusanos/{gui/detail,…}
- `AppCflags`/`AppCppflags`: `-U_FORTIFY_SOURCE` (NDK default aborts
  Lua 5.1 at `lua_pushcclosure`), C++14, `-DNBREAKPAD -DDISABLE_JOYSTICK
  -DHAVE_LINENOISE`
- `MultiABI='armeabi-v7a arm64-v8a x86 x86_64'`

**Android #ifdef guards in the tree** (minimal set that compiles + boots):

- [src/main.cpp](src/main.cpp) — skip CrashReport / CrashHandler::init/uninit
- [src/common/Networking.cpp](src/common/Networking.cpp) — skip
  `sigignore(SIGPIPE)` (Bionic lacks sigignore)
- [src/common/Debug.cpp](src/common/Debug.cpp) — route log lines through
  `__android_log_print` tagged "OpenLieroX"
- [src/client/Options.cpp](src/client/Options.cpp) — default
  `Video.ColourDepth` to 16
- [include/Unicode.h](include/Unicode.h) — `std::char_traits<Uint16/Uint32>`
  specialisations (NDK libc++ lacks them)
- [src/gusanos/blitters/blitters.h](src/gusanos/blitters/blitters.h) —
  gate `BUILTIN_MMXSSE` off (GAS-style MMX macros reject by LLVM IAS)

**Port patches applied at build time by build-android.sh**:

1. `project/app/src/main/AndroidManifest.xml` — insert INTERNET +
   ACCESS_NETWORK_STATE (SDL2 template doesn't honour
   `AccessInternet=y`; OpenLieroX socket() aborts without it).
2. `project/jni/sdl2_mixer/external/wavpack/Android.mk` — add empty
   `arm64-v8a` branch so `unpack_armv7.S` doesn't leak into the aarch64
   build.

**Boost**: [libs/boost-hdr/boost](libs/boost-hdr/boost) is a symlink into
`/usr/include/boost` (libboost-dev). 0.59 uses only header-only boost
so no static lib is needed.

**What's not yet ported:** touchscreen input (the 0.58 port's
`Touchscreen.{h,cpp}` rely on SDL 1.2's `SDL_android.h` +
`SDL_screenkeyboard.h` which don't exist in SDL2). The APK launches
and runs but has no touch controls — it's playable with a Bluetooth
gamepad or keyboard only. Adding SDL2-native touch handling is a
follow-up.

## Desktop builds

CMake-based; see [CMakeLists.txt](CMakeLists.txt) and
[CMakeOlxCommon.cmake](CMakeOlxCommon.cmake). All Android
modifications in the tree are behind `#ifdef __ANDROID__` so they
don't affect desktop builds.

## Commit style

Plain `git commit`, no Co-Authored-By: Claude trailer.
