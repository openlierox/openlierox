# OpenLieroX Android port

This directory is a self-contained Gradle/CMake project that produces an
APK for Android phones, tablets, and Chromebooks. It builds OpenLieroX
0.59 from the same `src/` / `include/` / `libs/` tree the desktop builds
use, plus a small amount of Android-specific glue (a custom
`SDLActivity` subclass and an asset-extraction step on first launch).

## Scope

- Single APK that runs on Android 5.0 (API 21) and up.
- ABIs: `arm64-v8a` and `x86_64` only — see [app/build.gradle:48](app/build.gradle#L48).
- Starts in normal client mode (singleplayer / join / host through the
  in-game UI).
- No external assets — `share/gamedir` is bundled inside the APK and
  unpacked into app-private storage on first run.

## Directory layout

```
build/android/
├── ANDROID-PORT.md          ← this file
├── build-android.sh         ← top-level build driver
├── fetch-dependencies.sh    ← clones third-party native deps
├── run-android-emulator.sh  ← boot AVD, install, smoke-test
├── build.gradle             ← root Gradle project
├── settings.gradle          ← Gradle project settings
├── gradle.properties        ← Gradle JVM args / AndroidX flags
├── gradlew, gradlew.bat     ← Gradle wrapper
├── gradle/                  ← Gradle wrapper jar
├── app/                     ← the only Gradle module
│   ├── build.gradle         ← Android plugin config (ABIs, SDKs, NDK)
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── res/             ← mipmap launcher icons, strings.xml
│       ├── java/
│       │   ├── net/openlierox/OpenLieroXActivity.java
│       │   └── org/libsdl/app/        ← vendored SDL2 Java glue
│       └── jni/
│           ├── CMakeLists.txt         ← native build
│           ├── alut_stub.c            ← in-tree freealut replacement
│           ├── COPYING                ← stub for libgd's CPack
│           ├── cmake-modules/         ← FindPNG/FindJPEG overrides
│           └── include/AL/alut.h      ← header for alut_stub
├── deps/                    ← third-party native sources (gitignored)
└── output/                  ← all build artefacts (gitignored)
    ├── assets/              ← staged game data (APK input)
    ├── build/               ← Gradle build dir
    ├── cxx/                 ← CMake build dir
    ├── gradle-cache/        ← Gradle project cache
    └── openlierox-*.apk     ← final APK
```

`deps/` and `output/` are ignored by [.gitignore](../../.gitignore).

## Build pipeline

There are three layered build steps, each driven by the layer above:

1. **[build-android.sh](build-android.sh)** — orchestration. Sets
   `ANDROID_SDK_ROOT` / `ANDROID_NDK_HOME`, calls
   `fetch-dependencies.sh`, stages game data, then invokes the Gradle
   wrapper. Output goes to [output/](output/).
2. **Gradle (Android Gradle Plugin 8.7.3)** —
   [app/build.gradle](app/build.gradle). Compiles Java glue, packages
   assets, calls CMake via the AGP `externalNativeBuild` block, and
   produces `app-debug.apk` / `app-release.apk`.
3. **CMake** — [app/src/main/jni/CMakeLists.txt](app/src/main/jni/CMakeLists.txt).
   Builds every native dep as a CMake subproject plus
   `libmain.so` (OpenLieroX itself).

### Driver: [build-android.sh](build-android.sh)

```
./build/android/build-android.sh [--release] [--clean] [--install] [--run]
                                 [--skip-data] [--skip-fetch]
```

- `--release` — `assembleRelease` instead of `assembleDebug`. Release
  is signed with the debug key (no Play Store config yet).
- `--clean` — `gradle clean` before building.
- `--install` — `adb install -r` after a successful build.
- `--run` — implies `--install`, then `adb shell am start` the
  activity.
- `--skip-data` — don't restage `share/gamedir` into
  `output/assets/gamedir`.
- `--skip-fetch` — don't run `fetch-dependencies.sh`.

The script auto-discovers `ANDROID_SDK_ROOT` from `$ANDROID_HOME`,
`~/android-sdk`, or `~/Android/Sdk`, and `ANDROID_NDK_HOME` from
`$ANDROID_SDK_ROOT/ndk/<latest>` if unset.

### Native deps: [fetch-dependencies.sh](fetch-dependencies.sh)

Clones each dep at a pinned tag into [deps/](deps/) using
`git clone --depth 1 --branch <tag> --recurse-submodules`:

| Dep | Tag | Purpose |
|---|---|---|
| `SDL` | `release-2.32.2` | Window / input / audio |
| `SDL_image` | `release-2.8.4` | PNG/JPG/BMP/GIF/TGA loading |
| `SDL_mixer` | `release-2.8.0` | OGG/Vorbis playback |
| `libxml2` | `v2.13.5` | Config / XML parsing |
| `libgd` | `gd-2.3.3` | Image manipulation (used by OLX renderer) |
| `libpng` | `v1.6.43` | PNG (also pulled in by SDL_image) |
| `libjpeg-turbo` | `3.0.4` | JPEG (libgd dependency) |
| `libogg` | `v1.3.5` | Ogg container |
| `libvorbis` | `v1.3.7` | Vorbis decoder |
| `openal-soft` | `1.23.1` | OpenAL via OpenSL ES backend |
| `freealut` | `freealut_1_1_0` | Cloned but not built (see below) |
| `curl` | `curl-8_10_1` | HTTP only, no TLS |

The script also:

- **Patches `libjpeg-turbo`** — wraps every `install(...)` call in
  `if(FALSE)` and downgrades its "cannot be added with
  add_subdirectory" error to a warning. Upstream forbids embedding;
  we accept the consequences.
- **Patches `libgd/src/gdkanji.c`** — guards the in-source `iconv_t`
  typedef and `<iconv.h>` include with `!defined(__ANDROID__)` so
  Bionic's iconv doesn't conflict.
- **Symlinks `boost-hdr/boost → /usr/include/boost`** — OpenLieroX
  needs Boost headers (header-only). The Linux/CI host must have
  `libboost-dev` installed; the Android NDK does not ship Boost.
- **`freealut` is cloned but unused.** Its CMake build doesn't survive
  `add_subdirectory()`, and OLX only needs `alutInit`, `alutExit`, and
  the two error helpers, so we ship them in
  [app/src/main/jni/alut_stub.c](app/src/main/jni/alut_stub.c) against
  a stub header at
  [app/src/main/jni/include/AL/alut.h](app/src/main/jni/include/AL/alut.h).

### Game-data staging

`build-android.sh` copies `share/gamedir` into
`output/assets/gamedir` and writes `output/assets/gamedir.version`
from [get_version.sh](../../get_version.sh) (the `VERSION` file if
present, otherwise the `LX_VERSION` define in
`src/common/Version.cpp`). Gradle picks this up via
[app/build.gradle:88-92](app/build.gradle#L88-L92):

```groovy
sourceSets { main { assets.srcDirs += [new File(outputRoot, 'assets')] } }
```

The result: every APK contains the current `share/gamedir/` tree under
`assets/gamedir/` plus a `gamedir.version` text file.

### Gradle build

Single-module project (`:app`). Key knobs in
[app/build.gradle](app/build.gradle):

- `compileSdk 36`, `targetSdk 34`, `minSdk 21`.
- `ndkVersion '29.0.14206865'` — pinned so behaviour is
  reproducible.
- `externalNativeBuild.cmake.path 'src/main/jni/CMakeLists.txt'`.
- `OLX_ROOT` is passed to CMake as the absolute path to the repo root,
  so the CMakeLists can `file(GLOB ${OLX_ROOT}/src/*.cpp)` etc.
- `android.useLegacyPackaging true` for JNI libs — needed because some
  of the vendored deps don't mark their `.so`s as `extractNativeLibs`-
  safe.
- `outputRoot = build/android/output`. Gradle's build dir, the AGP CMake
  staging dir (`output/cxx`), and the project cache
  (`output/gradle-cache`, set by `--project-cache-dir` in
  `build-android.sh`) all funnel under there. The repo only has one
  ignored sub-tree to manage.

### Native build

[app/src/main/jni/CMakeLists.txt](app/src/main/jni/CMakeLists.txt)
adds every dep with `add_subdirectory()` against the cloned
[deps/](deps/) tree. It only links targets — `install()` targets are
suppressed where possible. Highlights:

- **zlib**: NDK ships `libz.so` since API 21. Stock `FindZLIB` doesn't
  find it; we wire up a `ZLIB::ZLIB` IMPORTED target manually so
  libpng / libxml2 / libcurl find it.
- **SDL_image vendored deps**: `libpng`, `libjpeg`, `libogg`, `libvorbis`
  come along transitively. Top-level CMakeLists guards against
  duplicate `add_subdirectory()`s by checking for existing targets
  (`png_static`, `jpeg-static`, `ogg`, `vorbis`).
- **libpng include fix**: SDL_image's vendored libpng exposes
  `${CMAKE_CURRENT_SOURCE_DIR}` as a public include but not the build
  dir, where `pnglibconf.h` is generated. We forward the build dir as
  a `PUBLIC` include on `png_static` so consumers (incl.
  `IMG_png.c`) compile.
- **OpenAL Soft** — built `STATIC` with the OpenSL ES backend. (Oboe
  backend is disabled.)
- **libcurl** — `HTTP_ONLY=ON`. No TLS, no SSH, no LDAP, no SMTP/IMAP/
  POP3/etc. Only what HawkNL / OLX actually use.
- **libgd** ships a stub `COPYING` because libgd's CPack
  unconditionally references `${CMAKE_SOURCE_DIR}/COPYING`.
- **`FindPNG.cmake` / `FindJPEG.cmake` overrides** in
  [app/src/main/jni/cmake-modules/](app/src/main/jni/cmake-modules/)
  point libgd at the in-tree `png_static` / `jpeg-static` targets.
  They're also copied into `deps/libgd/cmake/modules/` because libgd
  resets `CMAKE_MODULE_PATH` inside its own CMakeLists.

OpenLieroX itself is `add_library(main SHARED ...)` — i.e.
`libmain.so`. The activity loads it via
`android.app.lib_name=main` in
[AndroidManifest.xml](app/src/main/AndroidManifest.xml).
Sources are `file(GLOB_RECURSE ${OLX_ROOT}/src/*.cpp ...)` minus
`/breakpad/`, `/MacMain`, and `/MacHelpers`. Vendored libs HawkNL,
libzip, and Lua are compiled in.

Compile defines: `NBREAKPAD`, `HAVE_BOOST`,
`SYSTEM_DATA_DIR="/data/data/net.openlierox/files"`.

## Java side

### [OpenLieroXActivity](app/src/main/java/net/openlierox/OpenLieroXActivity.java)

Subclass of `org.libsdl.app.SDLActivity`:

- `getLibraries()` returns
  `["SDL2", "SDL2_image", "SDL2_mixer", "main"]` — the order matters,
  SDL must load before `libmain.so`.
- `getMainFunction()` returns `"SDL_main"`.
- `onCreate()` calls `extractGamedirIfNeeded()` **before**
  `super.onCreate()` so the native side sees a populated
  `${SYSTEM_DATA_DIR}/OpenLieroX/` directory.

### Asset extraction

On launch the activity:

1. Reads `assets/gamedir.version` from the APK.
2. Reads `getFilesDir()/OpenLieroX/.gamedir.version`.
3. If they don't match (or the file doesn't exist), copies the entire
   `assets/gamedir/` tree into `getFilesDir()/OpenLieroX/` and writes
   the new version marker.

`gamedir.version` is `"<olx-version> data:<sha1>"`, where the sha1 is a content
hash of the fully-staged `gamedir` tree (computed in `build-android.sh`). It is
deliberately **not** just the git version: editing bundled data without bumping
the git version (e.g. uncommitted changes, or rebuilding the same commit) would
otherwise leave the marker unchanged, so the device would keep serving a stale
copy and the new data would never be extracted. Hashing the staged tree means
*any* change to bundled data changes the marker and forces a re-extract, while
identical data is left untouched. 

The native build sees `getFilesDir() == /data/data/net.openlierox/files`,
which is what `SYSTEM_DATA_DIR` resolves to.

This trades some startup latency (a few hundred ms on the first
launch) for ergonomic on-device file access — OLX expects to be able
to `fopen()` data files, which it can't do directly against an APK
asset.

### Vendored SDL2 Java
[app/src/main/java/org/libsdl/app/](app/src/main/java/org/libsdl/app/)
is a verbatim copy of SDL2's `android-project/app/src/main/java/org/
libsdl/app/`, vendored so we don't have to add an external SDL2
checkout to the Java source set. Update by copying from the same SDL2
tag listed in `fetch-dependencies.sh`.

## App identity

- Package: `net.openlierox`.
- Activity: `.OpenLieroXActivity`, `singleInstance`,
  `screenOrientation=landscape`,
  `theme=@android:style/Theme.NoTitleBar.Fullscreen`.
- Permissions: `INTERNET`, `ACCESS_NETWORK_STATE`, `WAKE_LOCK`,
  `VIBRATE`.
- Features: declares OpenGL ES 2.0, gamepad, and Leanback (Android TV)
  — all `android:required="false"`.
- Launcher icon: [share/android-icon.png](../../share/android-icon.png)
  (512x512), resampled into per-density PNGs at
  [app/src/main/res/mipmap-{m,h,xh,xxh,xxxh}dpi/ic_launcher.png](app/src/main/res/).

## Building locally

Prerequisites:

- JDK 17 (Temurin or system).
- Android SDK with `platforms;android-36`, `build-tools;36.0.0`,
  `platform-tools`, and `ndk;29.0.14206865`.
- `libboost-dev` (for the `boost-hdr` symlink target).
- `git`, `xz`, `zip` for asset / dep handling.

Quick start:

```sh
# Set ANDROID_SDK_ROOT or ANDROID_HOME if it's not at ~/android-sdk
./build/android/build-android.sh        # debug APK
./build/android/build-android.sh -r     # build, install, launch
```

Outputs:

- `build/android/output/openlierox-debug.apk`
- `build/android/output/openlierox-release.apk` (with `--release`)

First build takes ~10 min (clones deps, compiles SDL2/SDL2_image/
SDL2_mixer/libxml2/libgd/libpng/libjpeg-turbo/libogg/libvorbis/
openal-soft/libcurl + OLX). Incremental rebuilds are seconds.

## Smoke-testing in an emulator

[run-android-emulator.sh](run-android-emulator.sh) boots a headless AVD
named `olx`, installs the APK, launches the activity, and verifies the
process stays alive for N seconds (default 12). Saves
`output/logcat-<timestamp>.log` for diagnostics.

```sh
./build/android/run-android-emulator.sh
./build/android/run-android-emulator.sh build/android/output/openlierox-debug.apk 30
```

The AVD must already exist:

```sh
sdkmanager 'system-images;android-34;default;x86_64'
avdmanager create avd -n olx -k 'system-images;android-34;default;x86_64'
```

## CI

[.github/workflows/package-android.yml](../../.github/workflows/package-android.yml)
runs the same build-android.sh on `ubuntu-24.04`:

1. Checkout with submodules.
2. `apt-get install libboost-dev xz-utils zip`.
3. JDK 17 via `actions/setup-java@v4`.
4. `android-actions/setup-android@v3`, then `sdkmanager` installs the
   pinned platforms/build-tools/NDK.
5. Pre-creates `~/.android/debug.keystore` (CI runners don't have one
   so apksigner aborts otherwise).
6. `./build/android/build-android.sh`.
7. Renames the APK to `openlierox-${VERSION}-android.apk` using
   [get_version.sh](../../get_version.sh) (matches the Windows / Linux /
   macOS workflows).
8. Uploads the APK as a GitHub Actions artifact.

## Limitations / to be implemented

- **No on-screen controls.** Touch is delivered to SDL but OLX still
  treats input like a desktop. Practical play needs a Bluetooth
  gamepad or a hardware-keyboard Chromebook for now. The
  build-for-android branch's overlay-control framework is **not**
  ported here.
- **Release APK is debug-signed.** Add a release keystore + signing
  config before publishing to Play.
- **Two ABIs only** (`arm64-v8a`, `x86_64`). 32-bit ARM and x86 were
  dropped because device share is negligible and they double APK size.
- **Asset extraction is full-tree.** A future optimisation would be to
  diff against the previous `gamedir.version` and only overwrite
  changed files.