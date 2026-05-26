# CLAUDE.md

Working context for this fork of OpenLieroX (branch `easier-dedicated-server`,
parent `0.59`). Updated as work progresses.

## Project quick facts

- C++ game engine; build with `cmake . && make -j8` from repo root.
  Resulting binary is `bin/openlierox`.
- The same binary runs both the interactive game and a headless dedicated
  server. Dedicated mode is selected by the `-dedicated` CLI flag (see
  [src/main.cpp:476-494](src/main.cpp#L476-L494)).
- In dedicated mode the engine spawns
  [share/gamedir/scripts/dedicated_control](share/gamedir/scripts/dedicated_control)
  as a child process and talks to it line-by-line over stdin/stdout. The
  spawn is direct `exec`, so the script's shebang controls which Python is
  used (see [src/server/DedicatedControl.cpp:138-163](src/server/DedicatedControl.cpp#L138-L163)).
- Build artifacts: `bin/openlierox`, plus object files under `src/` and
  `libs/breakpad/`. Already built once this session (exit 0).

## Goals for this fork

The fork name is `easier-dedicated-server` — the work here is about making
the dedicated-server side simpler to launch, run, and maintain, not about
gameplay changes.

## Gotchas / hard-won knowledge

- **Shebang for the dedicated-control scripts must be `#!/usr/bin/python3 -u`,
  not `#!/usr/bin/env python3 -u`.** The engine `exec`s the script directly
  with no shell, and `env` treats `"python3 -u"` as one program name
  (failing with `'python3 -u': No such file or directory`). The kernel's
  classic shebang format passes everything after the interpreter path as a
  single arg-string to the interpreter itself, which Python then parses. The
  `-u` (unbuffered stdout) is required — without it, `print()` block-buffers
  when stdout is a pipe and the engine never sees responses.
- The OLX engine spawn site for the controller is
  [src/server/DedicatedControl.cpp:138-163](src/server/DedicatedControl.cpp#L138-L163).
- In dedicated mode you'll see `stdin CLI support init failed: stdin is not
  a terminal type device` — this is normal. The interactive console CLI
  can't bind because stdin is the pipe to the controller.

## Verified working

Dedicated server started successfully with Python 3 controller (verified
this session via `./start_dedicated_server.sh`):
- Engine listens on `127.0.1.1:23400` (default).
- Three processes run: parent OLX, worker fork, and
  `/usr/bin/python3 -u .../scripts/dedicated_control cfg/dedicated_config`.
- Engine receives `Dedicated_control started` log message from the
  controller, then `setvar` updates for `GLOBAL_SETTINGS`, then map/mod
  cycling — i.e. the bidirectional stdio protocol works under Python 3.

## Work done this session

### 1. Added [start_dedicated_server.sh](start_dedicated_server.sh)

Repo-root launcher. Locates `bin/openlierox` the same way [start.sh](start.sh)
does, then `exec`s it with `-dedicated`. Extra args pass through, so
`./start_dedicated_server.sh -script scripts/simple_ded_control.sh` works.

### 2. Added [DEDICATED_SERVER.md](DEDICATED_SERVER.md)

Inventory of every dedicated-server resource in the repo (engine entry
point, Python controllers + their config, alternative controllers, OS
packaging tools, official docs).

### 3. Python 2 → Python 3 port

The first-party Python scripts were written for Python 2. Ported to
Python 3 (shebangs to `/usr/bin/env python3`, `print` statements, `except
X, e` → `except X as e`, `raise X, msg` → `raise X(msg)`, `cmp=` sort →
`functools.cmp_to_key`, `xrange/iteritems/itervalues/iterkeys`, `execfile`
→ `exec(compile(...))`, `file()` → `open()`, `os.popen2` → `subprocess`,
`raw_input` → `input`, `cgi.escape` → `html.escape`, `string.strip(s, ch)`
→ `s.strip(ch)`, `__builtins__.x = …` → `import builtins; builtins.x = …`,
`import Image` (old PIL) → `from PIL import Image`, `bytes` indexing
already returns int so `ord(buf[i])` dropped, unbuffered text `open(…, 0)`
→ `buffering=1`, normalized mixed tabs/spaces, `int/int` → `int//int`
where the result was used as a count).

Files ported (all pass `python3 -m py_compile`):

Dedicated-server core (engine-driven):
- [share/gamedir/scripts/dedicated_control](share/gamedir/scripts/dedicated_control)
- [dedicated_control_io.py](share/gamedir/scripts/dedicated_control_io.py)
- [dedicated_control_handler.py](share/gamedir/scripts/dedicated_control_handler.py)
- [dedicated_control_ranking.py](share/gamedir/scripts/dedicated_control_ranking.py) (full rewrite — mixed tabs/spaces)
- [dedicated_control_usercommands.py](share/gamedir/scripts/dedicated_control_usercommands.py)
- [DedServerWatcher.py](share/gamedir/scripts/DedServerWatcher.py)
- [pwn0meter.py](share/gamedir/scripts/pwn0meter.py)
- [simulator.py](share/gamedir/scripts/simulator.py)
- [share/gamedir/cfg/dedicated_config.py](share/gamedir/cfg/dedicated_config.py)
- [share/gamedir/cfg/dedicated_config_lx56.py](share/gamedir/cfg/dedicated_config_lx56.py)

Standalone tools:
- [libs/breakpad_getallsources.py](libs/breakpad_getallsources.py)
- [tools/FixIncompleteCrashReport/fixer.py](tools/FixIncompleteCrashReport/fixer.py)
- [tools/FixIncompleteCrashReport/kmailpipehandler.py](tools/FixIncompleteCrashReport/kmailpipehandler.py)
- [tools/lxl2gusmap/lxl2gusmap.py](tools/lxl2gusmap/lxl2gusmap.py)
- [tools/lxl2gusmap/lx_level.py](tools/lxl2gusmap/lx_level.py) (full rewrite — PIL → Pillow, mixed indentation)
- [tools/OLXLogAnalyser/logins.py](tools/OLXLogAnalyser/logins.py)
- [tests/static__SDL_BlitSurface.py](tests/static__SDL_BlitSurface.py) (no Py2-isms; left as-is)

Bonus fix: vendored [share/gamedir/scripts/portalocker.py](share/gamedir/scripts/portalocker.py)
had Py2 `raise X, msg`, `except X, v`, and a `print` statement — patched
because `dedicated_control_handler.py` imports it at startup.

### Already Python 3 (untouched)
- [team_defenders_beta.py](share/gamedir/scripts/team_defenders_beta.py)
- [scripts/tools/commands.py](share/gamedir/scripts/tools/commands.py)
- [scripts/tools/army_architect.py](share/gamedir/scripts/tools/army_architect.py) and `army_patterns/*.py`

### Vendored Py2 code NOT ported (out of scope)
- `tools/DedicatedServerVideo/atom/` and `tools/DedicatedServerVideo/gdata/`
  — ~150 files of Google's old `gdata` SDK + `tlslite`. Would need to be
  *replaced* (e.g. with the modern `google-api-python-client` /
  `google-auth`), not modernized. [YoutubeUpload.py](tools/DedicatedServerVideo/YoutubeUpload.py)
  depends on these and stays Py2-only until that swap is done.
- `tools/SvnAnalyser/svnstats.py` — obsolete (project moved off SVN).

## Known followups / open questions

- The `getWormSkin` call at [dedicated_control_handler.py:248](share/gamedir/scripts/dedicated_control_handler.py#L248)
  references an undefined name (should be `io.getWormSkin`). Pre-existing
  bug, not introduced by the port; only triggers when
  `cfg.RANKING_AUTHENTICATION` is enabled.
- `lxl2gusmap.py` has `if not level.load(...): exit` — missing parens, so
  it's a no-op. Pre-existing bug, left untouched.
- Modernizing `YoutubeUpload.py` would require swapping out the entire
  vendored `gdata`/`atom` tree for the current Google API libraries.

## Build / run commands

```sh
# Build
cmake . && make -j8

# Run dedicated server (default config)
./start_dedicated_server.sh

# Run dedicated server with a specific control script
./start_dedicated_server.sh -script scripts/simple_ded_control.sh

# Syntax-check a ported file
python3 -m py_compile <path>
```
