# OpenLieroX WebAssembly port

A self-contained build of OpenLieroX 0.59 for the browser, living
entirely under [build/wasm/](.) and structured to mirror the Android
port at [build/android/](../android/) — a single driver script that
bootstraps every prerequisite, fetches third-party sources, and
produces deployable artefacts under `output/`.

The engine boots in any modern browser, renders the full menu system,
runs local single-player matches with software 2D rendering and
loopback networking, plays audio through the built-in OpenAL JS
backend, and respects mouse/keyboard input through SDL2. The source
diff against mainline OLX is small and lives behind `__EMSCRIPTEN__`
guards.

## Status

| Area | State |
|---|---|
| Build pipeline | Working; idempotent re-runs |
| Boot to main menu | Clean (single intentional warning about UDP) |
| Menu navigation (Local Play / Profiles / Options / Editor) | Working |
| Local single-player match | Working — gameplay, kills, dirt, respawn |
| Audio | Working (OpenAL JS port) |
| LAN / internet networking | Stubbed; UDP unavailable in browsers |
| Fullscreen | Disabled by master switch (see below) |
| Persistent user data | In-memory only (MEMFS, wiped on reload) |

## Quick start

```bash
cd build/wasm
./build-wasm.sh           # first run installs emsdk + clones deps
python3 serve.py          # http://localhost:8000/
```

`build-wasm.sh` is idempotent. Useful flags:

- `--clean` — wipe the CMake build dir
- `--release` / `--debug` (default debug)
- `--skip-emsdk` / `--skip-fetch` / `--skip-data` — skip the matching
  bootstrap stage on a re-run

The Emscripten SDK is installed outside the repo so the working
tree stays small and the ~1.6 GB toolchain can be shared across
projects. Default location: `$HOME/wasm-tools/emsdk`. Override with
`WASM_TOOLS_DIR=/some/path ./build-wasm.sh`.

For a redistributable bundle (release build, plus header config and
the COI service-worker shim) staged under
`distrib/openlierox-wasm/`:

```bash
./build.sh
```

## Layout

```
build/wasm/
├── build-wasm.sh           # main driver (emsdk → deps → preload → cmake → emcc)
├── build.sh                # release wrapper that stages distrib/openlierox-wasm/
├── fetch-emsdk.sh          # installs emsdk locally
├── fetch-dependencies.sh   # clones libxml2 / libgd / curl
├── CMakeLists.txt          # emcmake-driven build
├── cmake/                  # Find{ZLIB,PNG,JPEG}.cmake stubs that
│                           # redirect subprojects to emcc ports
├── jni/                    # alut shim (libfreealut replacement)
├── shell/
│   ├── shell.html          # HTML template emcc fills in
│   ├── coi-serviceworker.js  # vendored COI shim (MIT, gzuidhof)
│   └── coi-serviceworker.LICENSE
├── serve.py                # static dev server with COOP/COEP headers
├── run-headless.py         # CDP-driven Chrome harness (scripted)
├── attach-debug.py         # CDP-driven visible-Chrome debug log
├── deps/                   # cloned third-party sources (gitignored)
└── output/                 # generated artefacts (gitignored)
                            # (Emscripten SDK lives under
                            #  $WASM_TOOLS_DIR/emsdk, default
                            #  $HOME/wasm-tools/emsdk — out of repo)
    ├── preload/gamedir/    # staged subset of share/gamedir
    ├── build/              # CMake build dir
    ├── openlierox.html / .js / .wasm / .data
    └── index.html          # alias of openlierox.html
```

## Toolchain and dependency strategy

The build leans on Emscripten's official ports for libraries that
have a reliable upstream port, and builds from source the ones it
doesn't:

**From emcc ports** (linked via `-sUSE_*` and `-l*`):

```
-sUSE_SDL=2  -sUSE_SDL_IMAGE=2
-sUSE_LIBPNG=1  -sUSE_LIBJPEG=1
-sUSE_OGG=1  -sUSE_VORBIS=1  -sUSE_ZLIB=1
-lopenal
```

SDL2_mixer is intentionally skipped — OLX talks to OpenAL directly,
and the bundled SDL2_mixer port lacks a pthread variant in the emcc
release we use, which would break `--shared-memory` linking.

**From source** (cloned into `deps/` by [fetch-dependencies.sh](fetch-dependencies.sh)):

- libxml2 v2.13.5
- libgd gd-2.3.3 (software map blitting helpers)
- libcurl curl-8_10_1 (HTTP-only)
- boost-hdr — symlink to `/usr/include/boost`

**Vendored in-tree**:

- HawkNL, libzip, Lua 5.1 — already under `libs/` upstream, compiled
  in directly.
- `jni/alut_stub.c` — five entry points OLX calls (alutInit / alutExit
  / alutGetError + helpers); replaces freealut.

The `Find{ZLIB,PNG,JPEG}.cmake` stubs in [cmake/](cmake/) are
load-bearing: subprojects call `find_package(ZLIB|PNG|JPEG)`, which
on Emscripten would otherwise fail (no `libz.a` to locate on disk).
The stubs declare imported targets and set `*_LIBRARIES=""` so libgd
doesn't append `-lz` / `-lpng` / `-ljpeg` — emcc would then pull the
non-pthread sysroot variants and break the link.

## Threading model and cross-origin isolation

Built with `-pthread -sPROXY_TO_PTHREAD=1 -sPTHREAD_POOL_SIZE=12`.

- `main()` runs on a Web Worker, not the browser's main thread. Every
  blocking call OLX makes (`SDL_WaitEvent`, `SDL_Delay`,
  `pthread_cond_wait`, `ThreadPool::wait`) just blocks the worker —
  the browser's main thread keeps painting and dispatching input,
  which it proxies back to the engine worker.
- Existing OLX code that calls `SDL_CreateThread` or stands up its
  own pools works unchanged.
- The 40-thread pool the engine reports at boot is OLX's own
  `threadpool` — a `PTHREAD_POOL_SIZE=12` pre-allocation just covers
  the steady-state long-lived threads (network, audio, asset I/O)
  without paying worker-creation latency mid-game.

Threads need `SharedArrayBuffer`, which browsers only expose to
**cross-origin isolated** pages. Three response headers are required:

```
Cross-Origin-Opener-Policy:   same-origin
Cross-Origin-Embedder-Policy: require-corp
Cross-Origin-Resource-Policy: same-origin
```

[serve.py](serve.py) sets all three for local development.
[build.sh](build.sh) ships the same configuration as a `_headers`
file (Netlify / Cloudflare Pages) and `.htaccess` (Apache) inside
the distrib bundle. For hosts that can't set custom headers
(GitHub Pages), the bundle also includes
[coi-serviceworker.js](shell/coi-serviceworker.js) — a small MIT
service-worker shim that intercepts fetches and re-injects the
headers client-side. `build.sh` injects its `<script>` tag into
`index.html` automatically.

## Renderer

Forced to SDL2's **software** renderer on Emscripten:
[src/client/AuxLib.cpp:521](../../src/client/AuxLib.cpp#L521).

The emcc SDL2 GLES2 backend has to proxy every GL call to the
browser's main thread (WebGL contexts are main-thread only). On the
`SDL_CreateTexture → glTexImage2D` path the proxy mailbox struct can
end up unaligned, and the wasm threads spec traps unaligned atomic
accesses — different Chrome builds surface this as either "memory
access out of bounds" or "operation does not support unaligned
accesses". OLX renders a single 640×480 backbuffer into an
SDL_Surface and only needs the renderer to blit one texture per
frame, so software rendering costs nothing and removes the trap.

Initial window creation is also short-circuited if a video surface
already exists ([src/client/AuxLib.cpp:331](../../src/client/AuxLib.cpp#L331)).
Tearing down the SDL window/renderer mid-session destroys the canvas
while the browser keeps dispatching mouse events into it, which
reliably corrupts dlmalloc.

### Fullscreen master switch

`kWasmAllowFullscreen` in [src/client/AuxLib.cpp](../../src/client/AuxLib.cpp)
is hard-set to `false`. Browser fullscreen interacts badly with OLX
(canvas resize mid-game, mouse coordinate drift, unclean state on
exit). When `false`:

- `tLXOptions->bFullscreen` is forced off the first time
  `initWindow` runs, regardless of `Options.cfg` or the menu.
- The runtime `SwitchMode` key (default Alt+Enter) is consumed in
  [src/game/Game.cpp:761](../../src/game/Game.cpp#L761) — no
  video-mode reset happens.

Both call sites are guarded by `#ifdef __EMSCRIPTEN__`; desktop
builds are unaffected. Flip to `true` to test if the underlying
Chrome/SDL fullscreen interaction ever gets fixed.

## Networking

Browsers cannot open raw UDP sockets, but local single-player needs
the OLX server and client (running in the same wasm module) to talk.
Solution: select HawkNL's **NL_LOOP_BACK** driver on Emscripten in
[src/common/Networking.cpp:195](../../src/common/Networking.cpp#L195).
`NL_LOOP_BACK` routes packets between sockets opened in the same
process — exactly what local play needs — and never touches the
network stack.

Other networking adjustments:

- [src/client/DeprecatedGUI/MenuSystem.cpp:83](../../src/client/DeprecatedGUI/MenuSystem.cpp#L83)
  — `Menu_InitSockets()` demoted from `SystemError` (which aborts the
  engine) to a warning when UDP socket open fails. Single-player
  paths don't touch the lobby sockets, so the menu can continue.
- [src/common/Networking.cpp:978](../../src/common/Networking.cpp#L978)
  — `NetworkSocket::Read()` returns `NL_INVALID` silently when the
  socket is closed. Background readers call this every frame and
  would otherwise log a red error line each time.
- [src/server/CServer.cpp:272](../../src/server/CServer.cpp#L272)
  — server-side socket-open failure is a warning, not a fatal.

## Source-level changes

All `__EMSCRIPTEN__` guards in the tree, by purpose:

**Stubs for unavailable platform features**

- [src/main.cpp:210,265,416](../../src/main.cpp#L210) — skip
  `CrashHandler::init/uninit` and `DoCrashReport` (no Mach exceptions
  / signal stacks in browser).
- [src/common/Debug_DumpCallstack.cpp:18](../../src/common/Debug_DumpCallstack.cpp#L18),
  [src/common/Debug_GetCallstack.cpp:55](../../src/common/Debug_GetCallstack.cpp#L55)
  — `HAVE_EXECINFO 0`. Emscripten's musl has no `<execinfo.h>`.
- [src/common/Networking.cpp:218](../../src/common/Networking.cpp#L218)
  — skip `sigignore(SIGPIPE)`.
- [src/common/TeeStdoutHandler.cpp:16](../../src/common/TeeStdoutHandler.cpp#L16)
  — stub stdout-tee (the POSIX implementation uses `open("/dev/zero")
  + mmap(MAP_SHARED) + pipe() + fork()`, none of which works under
  MEMFS; stdout already lands in the browser console).

**Compatibility fixes**

- [include/Unicode.h:18](../../include/Unicode.h#L18) — provide
  `char_traits<unsigned short|int>` shim that libc++ otherwise lacks.
- [include/StringUtils.h:79](../../include/StringUtils.h#L79) —
  Emscripten's `<compat/string.h>` defines `strlwr` returning
  `char*`; defer to it instead of redefining locally.
- [src/gusanos/blitters/blitters.h:18](../../src/gusanos/blitters/blitters.h#L18)
  — skip MMX/SSE blitter path; emcc has no x86 intrinsics.
- [libs/hawknl/src/errorstr.c:93,120,125,130](../../libs/hawknl/src/errorstr.c#L93)
  — exclude `TRY_AGAIN` / `NO_RECOVERY` / `HOST_NOT_FOUND` /
  `NO_DATA` cases. WASI errno values overlap with the netdb.h
  values of these constants, causing duplicate-case errors.

**Path / filesystem**

- [src/common/FindFile.cpp:490](../../src/common/FindFile.cpp#L490) —
  search paths set to `/gamedir` (preloaded) and
  `${HOME}/.OpenLieroX`.

**Game state**

- [src/game/GameState.cpp:370](../../src/game/GameState.cpp#L370) —
  removed the `assert(!haveObject(o))` in `addObject`. After the
  first match the engine stays alive (browsers don't exit between
  matches), and `GameState::reset()` re-registers the `&game` /
  `&gameSettings` singletons that are still in `objCreations`,
  re-firing the assert. The actual mutation
  (`std::map::operator[] =`) is already insert-or-replace and is
  safe; only the strict assert was fragile.

## Memory and runtime tuning

Set on the link line in [CMakeLists.txt](CMakeLists.txt):

| Flag | Why |
|---|---|
| `-sINITIAL_MEMORY=536870912` | 512 MB up front. OLX easily allocates that loading mods; growing memory mid-game stalls WebGL and the worker. |
| `-sALLOW_MEMORY_GROWTH=1` | Still allow growth as a fallback. |
| `-sSTACK_SIZE=8388608` | 8 MB main stack. OLX's call chains (Gusanos blitters, ConfigHandler, ScriptableVars setup) blow past the 64 KB default. |
| `-sDEFAULT_PTHREAD_STACK_SIZE=4194304` | 4 MB per pthread, mirroring the desktop default. Removes a class of "stack overflow → memory access out of bounds" crashes. |
| `-sSTACK_OVERFLOW_CHECK=2` | Real diagnostic on overflow instead of a silent OOB. |
| `-sASSERTIONS=1` | Keep Emscripten's runtime sanity checks (kept on in release too). |
| `-sEXIT_RUNTIME=0` | Engine stays alive across matches. |
| `-sFORCE_FILESYSTEM=1` | The preload archive and `FS` API are always available even if no obvious filesystem call is linked. |
| `-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap','FS','callMain']` | Used by the shell + harnesses. |

## Game-data preload

The full `share/gamedir` is ~8000 files / ~136 MB. Each file becomes
a separate Emscripten "preload dependency" registered serially before
`main()` runs, which would take minutes. [build-wasm.sh](build-wasm.sh)
stages a curated subset under `output/preload/gamedir/`:

```
cfg/ data/ scripts/ gui_skins/ themes/ skins/ games/
Classic/ Gusanos/  [MW 1.0/ promode/ if present]
GeoIP.dat  startup.lua
levels/{747, Base Fight, BetaBoxDE, Castle Strike, Dirt Level}.lxl
*.gamesettings
```

About 1300 files / 12 MB — the menu, profiles, options, level editor,
Introduction game, and Classic/Gusanos mods all work; mod-dropdown
references resolve. The empty `levels/` directory is stage-required:
`SinglePlayer` paths are `levels/../games/foo/bar`, and POSIX path
normalisation needs every component to exist before resolving `..`.

CMake doesn't track files inside the preload dir as link deps, so
`build-wasm.sh` deletes the link artefacts (`openlierox.{html,js,
wasm,data}` under `build/libgd/Bin/`) before each build to force emcc
to relink whenever the staged data changes.

## Distribution

[build.sh](build.sh) wraps `build-wasm.sh --release` and stages
`distrib/openlierox-wasm/`:

```
distrib/openlierox-wasm/
├── index.html              # shell + injected COI <script>
├── openlierox.js           # emscripten loader
├── openlierox.wasm         # compiled module
├── openlierox.data         # preload archive
├── coi-serviceworker.js    # vendored COI shim
├── coi-serviceworker.LICENSE
├── manifest.webmanifest    # PWA manifest — installable web app
├── icon-256.png            # app icons (manifest + shell <link>)
├── icon-512.png
├── build-info.json         # version / commit / file list (provenance)
├── _headers                # Netlify / CF Pages headers
└── .htaccess               # Apache headers + MIME types
```

The bundle works on:

- **Hosts that set headers natively** (Netlify, Cloudflare Pages,
  Apache via `.htaccess`, nginx/S3/CloudFront with manual config) —
  the COI shim notices `crossOriginIsolated === true` on first load
  and exits without registering.
- **Hosts that can't** (GitHub Pages) — the shim registers as a
  service worker on first load, triggers one `location.reload()`,
  and intercepts every subsequent fetch to add COOP/COEP/CORP
  headers. After the reload the engine boots normally.

Caveats: the shim doesn't work in Safari Private Browsing or inside
iOS in-app WebViews (service workers are disabled there). For those
audiences, a header-capable host is the only option.

### Installable web app

The bundle is a PWA: `index.html` links `manifest.webmanifest` and
ships `icon-256.png` / `icon-512.png`, so the shell's **Install web
app** button gets a real install prompt on Chromium and **Add to Home
Screen** works on iOS — both launch the game full-screen with no
browser chrome (the shell's `body.standalone` CSS fills the screen).
The manifest's `start_url`/`scope` are relative (`./`), so the bundle
installs correctly from whatever path it's served at — no per-host
edits. (Without a manifest the Install button can't fire
`beforeinstallprompt`; this is why it ships in the bundle.)

### build-info.json (provenance / scripted updates)

`build.sh` writes `build-info.json` into the bundle so a deployer never
has to eyeball the release title to know what a bundle contains:

```json
{
  "name": "openlierox-wasm",
  "version": "0.59_beta10",
  "commit": "<full sha>",
  "commitShort": "<short sha>",
  "commitDate": "<ISO 8601>",
  "entry": "index.html",
  "engineFiles": ["openlierox.js", "openlierox.wasm", "openlierox.data"],
  "files": ["index.html", "openlierox.js", ...]
}
```

`engineFiles` are the three artefacts that actually change every build
(the heavy ones worth cache-busting); `files` is the full publish set.

### Updating a deployed copy

The bundle is self-contained — publishing an update is "replace the
files". For caches, the recommended pattern (used by
openlierox.github.io) is to drop each build into a **versioned/dated
folder** and point a stable entry page at it, so a) old clients keep
working off the old folder while the CDN catches up, and b) you only
edit one path per update. A deploy script can stay version-agnostic by
reading `build-info.json` instead of hard-coding names:

```sh
unzip -q openlierox-*-wasm.zip -d /tmp/olx
dir="/tmp/olx/$(ls /tmp/olx)"
ver="$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["version"])' "$dir/build-info.json")"
dest="web-demo/$ver"            # or a date — whatever your cache-busting scheme uses
mkdir -p "$dest" && cp "$dir"/* "$dest"/
# then update the single path reference your stable entry page uses
```

If you keep a stable install entry (so installed web apps survive
updates), point its `Module.locateFile` + engine `<script src>` at the
new `engineFiles` folder and leave its URL/manifest unchanged.

## Debug harness

Two CDP-driven helpers under [build/wasm/](.) that drive a real
Chrome and stream `Runtime.consoleAPICalled` /
`Runtime.exceptionThrown` / `Log.entryAdded` events:

- [run-headless.py](run-headless.py) — headless, scriptable.
  ```
  python3 build/wasm/run-headless.py [duration_s] [shot_every_s] \
      "at:8:CLICK:160,160" "at:12:KEY:Return" "at:14:SHOT:foo"
  ```
  Click coordinates are SDL canvas-internal; the harness translates
  them to viewport coords through the live `getBoundingClientRect`.
  Mouse presses use `Input.synthesizeTapGesture` (a real platform
  input event) instead of `Input.dispatchMouseEvent` (a synthetic JS
  MouseEvent some emcc-SDL2 builds discard). Screenshots land under
  `/tmp/olx-shots/`.
- [attach-debug.py](attach-debug.py) — visible Chrome window the
  user drives interactively. Streams console + exception events to
  `/tmp/olx-debug.log`.

Both need `--remote-allow-origins=*` on the launched Chrome (already
set by the harnesses) so the WS handshake from the local CDP client
isn't rejected as forbidden cross-origin.

## Known limitations

- **No real networking.** UDP isn't available; LAN / internet play
  requires WebSockets or WebRTC, which HawkNL doesn't speak. The
  loopback driver covers single-player only.
- **No persistent user data.** Settings, downloaded mods, and
  player profiles live in MEMFS and are wiped on tab reload. IDBFS
  mounting under `~/.OpenLieroX` is the obvious follow-up but isn't
  wired up.
- **Audio gating.** Chrome's autoplay policy blocks the first
  AudioContext until a user gesture; the OpenAL JS backend handles
  the unblock once the user clicks the canvas.
- **Fullscreen disabled** (see master switch above).
- **Initial download is large.** ~90 MB `.wasm` + ~12 MB `.data`.
  An emcc release build with `--release` is meaningfully smaller; a
  brotli/gzip pre-compression pass on the host helps further.
- **Safari Private Browsing / iOS in-app WebViews** can't run from
  GitHub Pages because the COI shim depends on service workers,
  which are disabled in those contexts. A header-capable host works
  fine.

## Working journal

Iteration-by-iteration debugging notes — the actual sequence of
crashes, traces, and fixes — live in the project's
[CLAUDE.md](../../CLAUDE.md). This document is the polished
reference; the journal there is the trail of breadcrumbs that got us
here.
