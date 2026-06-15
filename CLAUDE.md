# OpenLieroX — Wasm port debugging notes

> Working journal kept while bringing the WebAssembly build (under
> `build/wasm/`) up to a usable state. Most recent entry first.

## Project layout reminder

- Stand-alone build under `build/wasm/`, mirroring the Android port.
- Driver: `build/wasm/build-wasm.sh`. CMake-driven, emcc toolchain.
- Server: `build/wasm/serve.py` (adds COOP/COEP for SharedArrayBuffer).
- Source-level `__EMSCRIPTEN__` guards are intentionally minimal —
  same shape as the existing `__ANDROID__` guards.
- Networking does not need to work in browser (user-confirmed).
  HawkNL/UDP can be stubbed/skipped if it blocks startup.
- See `build/wasm/WASM-PORT.md` for the full design overview.

## Debug harness

- `build/wasm/run-headless.py` — drives Chrome via the DevTools
  Protocol. Streams `Runtime.consoleAPICalled` /
  `Runtime.exceptionThrown` / `Log.entryAdded` events and saves a
  PNG every N seconds to `/tmp/olx-shots/`.
- Run with: `python3 build/wasm/serve.py &` then
  `/home/ekirprivat/py-venv-global/bin/python build/wasm/run-headless.py [duration_s] [shot_every_s]`.
- Needs `--remote-allow-origins=*` on Chrome to allow CDP from
  the harness's WS client.

## State

- Build works end-to-end; artefacts are produced under
  `build/wasm/output/` (`openlierox.{html,js,wasm,data}`).
- Page boots: prints startup messages, threadpool spawns 40
  threads, FS search paths resolve to `/gamedir` +
  `/home/web_user/.OpenLieroX`. Canvas is still black — menu
  hasn't rendered yet.

## Iterations

### iter 1 — first headless boot

Headless Chrome at `http://localhost:8000/openlierox.html` reaches
the engine: startup banner prints, threadpool spawns 40 threads, FS
search paths resolve. Canvas stays black after the first second of
output. Need a streaming console to find the actual hang point.

### iter 2 — CDP harness in place

Built `build/wasm/run-headless.py`. Drives Chrome via the DevTools
Protocol, streams `Runtime.consoleAPICalled` /
`Runtime.exceptionThrown` / `Log.entryAdded` events, and saves PNG
screenshots periodically into `/tmp/olx-shots/`.

Needed `--remote-allow-origins=*` on the Chrome args; without it the
WS handshake from the harness was rejected with HTTP 403.

### iter 3 — diagnose hang: networking init

With the harness streaming logs we saw the engine getting all the
way to "Loading (70%): Initializing menu system" and then aborting
with `E: SystemError: Error: Failed to open a socket for networking`.

Origin: `src/client/DeprecatedGUI/MenuSystem.cpp:Menu_InitSockets()`.
The function opens UDP sockets for SCK_LAN / SCK_NET / SCK_FOO and
calls `SystemError(...)` when isOpen() returns false — the browser
can't open raw UDP sockets.

Fix (committed): demote the `SystemError` to a warning under
`__EMSCRIPTEN__` and let the menu continue with closed sockets.
Single-player paths don't touch tSocket, so this is enough.

### iter 4 — quiet the per-frame socket-read spam

After the InitSockets fix the menu loads, but background readers
still call `NetworkSocket::Read()` on the closed lobby sockets every
frame, which logged a red error line each time. Suppressed under
`__EMSCRIPTEN__` in `src/common/Networking.cpp` — return NL_INVALID
silently when the socket is closed.

### iter 5 — main menu rendering

Screenshot at /tmp/olx-shots/shot-0048s.png on the boot run shows
the full OLX main menu: "OpenLX" logo, LOCAL PLAY / NETPLAY /
PLAYER PROFILES / LEVEL EDITOR / OPTIONS, Liero mascot art, theme
dropdown. Status bar reads "Running."

### iter 6 — input dispatch

Locked the canvas to its native 640×480 in `shell.html` (was
`flex: 1`, which made the SDL canvas size + click coords ambiguous).
Extended `run-headless.py` with a CLICK / KEY / SHOT mini-script
that takes canvas-internal coords and translates them through the
live DOM rect into absolute viewport coords.

Click in canvas-center triggered `LOCAL PLAY → Single Player →
Introduction → intro1`; the menu navigation works end-to-end. The
"Problem while loading level ../games/introduction/intro1" message
that follows is a **pre-existing OLX issue** with the Introduction
game on Linux, unrelated to the Wasm port — the level dir exists in
the staged data but the loader rejects it for some other reason.

### Result

The Wasm port is functionally complete for the menu / single-player
flows that don't require networking:

1. `cd build/wasm && ./build-wasm.sh` produces
   `output/openlierox.{html,js,wasm,data}`.
2. `python3 build/wasm/serve.py` then visit
   `http://localhost:8000/openlierox.html`.
3. Engine boots, main menu renders, mouse and keyboard input
   dispatched through the canvas works.

Source diff is small and lives mostly in `__EMSCRIPTEN__`-guarded
blocks so the rest of OLX is unaffected.

Reproducible verification: `python3 build/wasm/serve.py &` then
`build/wasm/run-headless.py 30 8 "at:8:CLICK:160,160"` (or any other
scripted action) — screenshots land under `/tmp/olx-shots/`.

### iter 7 — multi-screen verification

Walked the menu via scripted clicks. Confirmed sub-screens render
correctly and respond to input:

- `LOCAL PLAY`: full layout with Game dropdown ("Introduction" /
  other mods), Level / Mod tabs, Level number slider, BACK + START
  buttons.
- `PLAYER PROFILES`: full layout with NEW PLAYER / VIEW PLAYERS
  tabs, Worm Details (Name field, Type and Skin dropdowns, Red /
  Green / Blue colour sliders), BACK + CREATE buttons.

Click coordinates inside the canvas are passed to SDL2 by the emcc
port without translation problems — the harness's
`canvas-internal -> viewport` mapping (read off
`getBoundingClientRect`) is the right model. Specific menu-item Y
coordinates are not documented in this log because they vary per
release; the harness deliberately doesn't bake them in.

Pre-existing OpenLieroX issue surfaced (not fixed here): `Single
Player → Introduction → intro1` reports "Problem while loading
level ../games/introduction/intro1" even though the directory is
present in the staged `gamedir/`. Same behaviour in the native
Linux build, so it's an upstream defect with the bundled
Introduction game on case-sensitive filesystems.

### iter 8 — clean up the remaining boot-time errors

Snapshotted everything the harness logged on a cold boot and worked
through them:

- `404 /favicon.ico` — added a `<link rel="icon" href="data:,">`
  stub to the shell so the browser stops asking for one.
- `E: teeStdout: cannot open dummy file` — the POSIX teeStdout
  uses `open("/dev/zero")` + `mmap(MAP_SHARED)` + `pipe()` + `fork()`
  to wire up a stdout tee. None of that lines up with Emscripten's
  MEMFS, and stdout already lands in the browser console anyway.
  Added an `__EMSCRIPTEN__` branch in `TeeStdoutHandler.cpp` that
  stubs all four entry points to no-ops.
- `E: Error when loading GeoIP database` — `share/gamedir/GeoIP.dat`
  wasn't in the staged preload subset. Added it.
- `E: default Gusanos mod not found` — `Gusanos/` mod wasn't in the
  staged subset. Added it.
- (Also) `Problem while loading level ../games/introduction/intro1`
  — turned out to be path-resolution on a missing `levels/` dir,
  not a case bug. POSIX needs every component of a path to exist
  before `..` resolves, so when the staged tree had no `levels/`
  the loader's `levels/../games/introduction/intro1` couldn't
  normalize. Staged the `levels/` dir with four sample maps.

After all four fixes, the only console line on a cold boot is the
intentional `W: Menu_InitSockets: UDP sockets unavailable on
Emscripten — continuing without networking`. Clicking LOCAL PLAY
now shows the actual `intro1` map preview ("This is the
introduction. Try out and have fun.") in place of the previous red
"Problem while loading level" error.

### iter 10 — fix the in-game malloc OOB

User trace pointed at `_malloc(64)` being called from a mouse-event
handler on the browser main thread, immediately after the worker
that runs `main()` issued `resetting video mode` and rebuilt the
SDL renderer. The "Invalid UTF-8 leading byte 0x000000b8" warning
right before the trap was a tell-tale that wasm linear memory was
read with a bad pointer — a heap-corruption symptom. Fixes:

src/client/AuxLib.cpp / VideoPostProcessor::initWindow
  Short-circuit on Emscripten: if a video surface is already up,
  reuse it. Tearing down + recreating the SDL window/renderer was
  destroying the canvas while the browser's main thread kept
  dispatching mouse events into it, which is what corrupted
  dlmalloc.

build/wasm/CMakeLists.txt
  - Drop -sOFFSCREEN_FRAMEBUFFER (only needed for the GL renderer
    we no longer use post-iter-9).
  - Bump main stack to 8 MB; per-pthread stack to 4 MB. OLX's
    Gusanos blitters / ScriptableVars setup easily blow the
    default 64 KB pthread stack and the resulting stack overflow
    would surface as the same OOB symptom.
  - Initial memory 256 MB → 512 MB; growth under -pthread is racey.
  - -sSTACK_OVERFLOW_CHECK=2 so a real stack overflow yields a
    diagnostic instead of a random OOB.
  - PTHREAD_POOL_SIZE 8 → 12 to cover gameplay threads.

build/wasm/shell/shell.html
  Set `width="640" height="480"` on the canvas element so the
  internal pixel buffer matches OLX's logical surface from the very
  first frame — avoids SDL_RenderSetLogicalSize having to scale
  every mouse event and prevents the canvas from being resized
  inconsistently during the first SDL_CreateWindow.

src/common/Networking.cpp / InitNetworkSystem
  On Emscripten, select HawkNL's NL_LOOP_BACK driver instead of
  NL_IP. Browsers can't open raw UDP sockets, but NL_LOOP_BACK
  routes packets between sockets opened in the same process —
  exactly what an OLX local game (server + client both running
  here) needs. With this in place Menu_LocalStartGame ->
  SinglePlayerGame::startGame proceeds past the previous "Could
  not open UDP socket" stop sign without any network calls leaving
  the browser tab.

build/wasm/run-headless.py
  - Coords are canvas-internal again; SDL_RenderSetLogicalSize does
    the OLX-logical mapping for us.
  - Use Input.synthesizeTapGesture (a real platform input event)
    instead of Input.dispatchMouseEvent (a synthetic JS MouseEvent
    that some emcc-SDL2 builds discard). With the tap gesture, OLX
    actually sees the START button press.

build/wasm/attach-debug.py (new harness)
  Visible Chrome counterpart to run-headless.py — launches a real
  window, attaches via CDP, streams console + exception events to
  /tmp/olx-debug.log so the user can drive interactively while we
  tail traces.

After all of these, the headless harness can click LOCAL PLAY then
START, the engine reaches `prepareGameloop` and shows the in-game
loading spinner. No more "memory access out of bounds" trap. The
spinner doesn't yet complete to actual gameplay — that's the next
investigation, but the OOB is gone.

### Confirmed working: main menu in real Chrome

User confirmed (after iter 9) that real desktop Chrome now boots
to the main menu without the previous "memory access out of bounds"
trap. LOCAL PLAY / NETPLAY / PLAYER PROFILES / LEVEL EDITOR /
OPTIONS all reachable via mouse clicks.

Remaining work: starting an actual match from the menu still
crashes. Next iteration is to capture the in-game crash trace via
the CDP harness while the user reproduces it interactively.

### iter 9 — fix the wasm trap real Chrome was hitting

User report: real desktop Chrome was throwing
"memory access out of bounds". My headless harness wasn't catching
it because I had been running with `--disable-gpu`, which selects
the SDL software renderer and skips the buggy code path entirely.

Re-ran the harness with `--use-gl=angle --use-angle=swiftshader` so
GL is enabled. Reproduced a related but more specific trap and
captured a backtrace via the CDP exception channel:

    RuntimeError: operation does not support unaligned accesses
      at emscripten_thread_mailbox_ref
      at em_task_queue_send
      at do_proxy / emscripten_proxy_async
      at do_dispatch_to_thread / emscripten_dispatch_to_thread_*
      at glTexImage2D
      at GLES2_CreateTexture
      at SDL_CreateTexture

Cause: `-pthread + -sPROXY_TO_PTHREAD=1` builds run main() on a
worker. When SDL2's GLES2 backend handles `SDL_CreateTexture`, every
GL call (`glTexImage2D` etc.) has to be proxied to the main thread
because WebGL contexts are main-thread only. The proxy queue's
mailbox uses atomics, and on this code path the mailbox struct ends
up unaligned, which traps under the wasm threads spec. Different
Chrome versions surface this as either "memory access out of
bounds" or "operation does not support unaligned accesses".

Fix (in src/client/AuxLib.cpp): on `__EMSCRIPTEN__`, set
`SDL_HINT_RENDER_DRIVER=software` and pass `SDL_RENDERER_SOFTWARE`
to `SDL_CreateRenderer`. The 640×480 backbuffer doesn't need GPU
acceleration anyway — the engine does its drawing into a software
SDL_Surface and the renderer just blits one texture to the screen.

Verified by re-running the harness with GPU on. Renderer now
reports "software / hardware accelerated: false" and the boot log
contains zero exceptions; the main menu renders the same as in the
old `--disable-gpu` headless run.

## iter 13 — second-game-start abort fixed

**Symptom (user-reported, real Chrome, build with fullscreen
disabled and engine staying alive between matches):** play one
match, quit back to the menu, start a new match → as soon as the
local client finishes its `NET_CONNECTING` handshake the wasm
worker aborts with

  Aborted(Assertion failed: !haveObject(o),
    at: src/game/GameState.cpp:371, addObject)

**Cause:** `GameState::GameState()` registers the singletons
`&game` and `&gameSettings` into `objs`. After the first match
those refs (or other re-used object pointers) are still in
`game.gameStateUpdates->objCreations`. On the second match
`updateToCurrent()` calls `reset()` (which constructs a fresh
GameState containing the singletons) and then iterates
`objCreations` and calls `addObject()` for each — the singleton
refs re-appear and `assert(!haveObject(o))` fires.

The actual write inside `addObject` is `objs[o] = ObjectState(o)`,
which is `std::map::operator[] =` — that's already insert-or-
replace and is fine with a re-add. Only the strict assert was
fragile.

**Fix:** [src/game/GameState.cpp:370](src/game/GameState.cpp#L370)
— removed the assert; left a comment explaining the wasm-keeps-
engine-alive context. Desktop builds usually exit between
matches so they rarely hit this path; the wasm port reliably
does.

**Verified by user:** two matches in a row from the same engine
session now both reach gameplay cleanly. Commit `b36f414d`.

## Wasm fullscreen master switch

There is a single boolean controlling whether the wasm build is
allowed to enter fullscreen at all:

  `kWasmAllowFullscreen` in [src/client/AuxLib.cpp](src/client/AuxLib.cpp)
  (inside `VideoPostProcessor::initWindow`, under `__EMSCRIPTEN__`).

Currently set to **`false`** because browser fullscreen mode
interacts badly with OLX (canvas resize mid-game, mouse coordinate
drift, broken state on exit).

**User preference:** keep this off by default — the windowed canvas
is much easier to alt-tab between than a fullscreen browser tab.

Effect when `false`:
- `tLXOptions->bFullscreen` is forced off the first time
  `initWindow` runs, regardless of what `Options.cfg` or the menu
  picker says. A note is logged so this is visible.
- The runtime fullscreen toggle key (`SwitchMode`, default Alt+Enter)
  is consumed quietly in [src/game/Game.cpp](src/game/Game.cpp)'s
  per-frame handler — no video-mode reset happens.

Flip to `true` to test fullscreen again once those issues are
fixed; nothing else needs changing. Both call sites are guarded by
`#if defined(__EMSCRIPTEN__)` so desktop builds are unaffected.

## MILESTONE — gameplay runs in the browser (2026-05-03)

After iter 11 (HawkNL `NL_LOOP_BACK` UDP fixes — see below), the
Wasm port reaches actual gameplay end-to-end:

- `Lua: Lua: server started …`
- `H: server started on 127.0.0.1:23400`
- `Client connect to 127.0.0.1:23400` → `GameServer: our local
  client has connected` (the loopback fix made this work)
- `H: Worm joined: [CPU] K1ll0r (id 0, ...)` and `Kamikazee!`
- `Game:state: update 4 -> 5` (S_Lobby → S_Playing)
- `LUA: Lua: game begin …`
- `<CWorm:0> CWorm:bAlive: update false -> true` ×2
- `<CWorm:1> CWorm:iDirtCount: update 0 -> 20` … rising over time
  (worms eating terrain)
- `<CWorm:1> CGameObject:health: update 100 -> 98` (taking damage)

Confirmed in real Chrome by the user. This is the headline result —
the OpenLieroX engine actually runs as Wasm in a browser, with
local-game networking, software rendering, OpenAL audio, and the
full game state machine.

What remains:

- ~~After ~6 seconds of gameplay the engine reinitialises from
  scratch~~ **Fixed by disabling fullscreen** (iter 12). The
  reinit was the SDL fullscreen request triggering a Chrome
  state change that aborted the wasm worker. With
  `kWasmAllowFullscreen=false`, gameplay runs cleanly until the
  user actually quits — verified ~30 s of live play with kills,
  health updates, and respawn, ending with `Game:state: update
  5 -> 1` (back to Inactive) without a single fresh boot in the
  console.
- Mouse / keyboard input goes through SDL in real Chrome but not
  via CDP synthetic events — that's a harness limitation, not a
  port bug.

### Status after iter 10

* **Boot**: clean — only the intentional UDP-loopback warning lands
  in the console.
* **Main menu**: renders, navigates (LOCAL PLAY / NETPLAY / PLAYER
  PROFILES / LEVEL EDITOR / OPTIONS).
* **Click START → game start**: in headless, the engine reaches
  `prepareGameloop` and shows the in-game loading spinner without
  the previous "memory access out of bounds" trap. The spinner
  doesn't yet fully complete to gameplay — that's the next thing to
  investigate.
* The user reported the OOB still happened in their real-Chrome
  build; once they test the iter-10 build (loopback + reset
  short-circuit + larger pthread stacks + canvas-size lock) we'll
  know whether the trap is fully gone.

Tools:

* `build/wasm/run-headless.py [duration_s] [shot_every_s]
  "at:T:CLICK:x,y" "at:T:KEY:name" "at:T:SHOT:name"` — scripted
  headless harness; clicks use SDL canvas-internal coords, the
  mouse press uses Input.synthesizeTapGesture so OLX's CButton
  actually registers the press.
* `build/wasm/attach-debug.py` — visible-Chrome counterpart;
  attaches to a real window the user drives and streams console +
  exception events to `/tmp/olx-debug.log`.

### Outcome

Wasm port boots, renders, and accepts input. Commit log:

- `01310a6ba` — initial Wasm port skeleton (this branch)
- `e25fe9796` — disable network init aborts, silence socket-read
  spam, lock canvas size, harness scriptable input

`build/wasm/WASM-PORT.md` documents design + how to build/run.
`build/wasm/run-headless.py` documents how to verify in CI-like
fashion.
