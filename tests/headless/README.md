# Headless test suite

Runtime tests that drive real `openlierox` processes with no display or audio device,
so they run on a bare CI runner.
Everything runs in **dedicated mode** (`-dedicated`),
which skips SDL video/audio entirely.

They are deliberately small:
the intent is a foundation to extend, not full coverage.

## What is covered

- **`test_smoke.py`** –
  the binary boots, prints its version,
  and reaches the hosting lobby as a dedicated server.
- **`test_network.py`** –
  one dedicated server and two connecting clients:
  - `test_client_can_join_in_lobby` –
    a client that joins while the server is still in the lobby can play.
    This is the control case: it works today.
  - `test_client_can_join_running_game` –
    a client that joins an **already-running** game can play.
    This reproduces [issue #973](https://github.com/openlierox/openlierox/issues/973)
    and **fails until the bug is fixed** —
    #973 is critical (it makes builds unusable for hosting),
    so the suite fails loudly rather than tolerating it.
    It is fix-agnostic —
    it passes with any correct fix,
    not only the [PR #966](https://github.com/openlierox/openlierox/pull/966) protocol revert.

## Running locally

Easiest — build (if needed) and run in one step;
extra args go to pytest:

```sh
./tests/headless/run.sh -v
```

On macOS `run.sh` installs the Homebrew build deps for you (via [`build.sh`](build.sh));
on Linux install the apt dev packages listed in the project [README](../../README.md) first.

Or do it by hand
(the suite auto-discovers `build-output/bin/openlierox`, `build/bin/openlierox`
or `bin/openlierox`, or set `OLX_BINARY`):

```sh
./tests/headless/build.sh          # or your own cmake build into build-output/
cd tests/headless && python3 -m pytest -v   # needs Python 3 + pytest
```

If the binary is not found the whole suite is skipped rather than failing.

## How it works

An OLX instance started with `-dedicated -script <path>`
runs that script as a subprocess
and talks to it over a line-based pipe
(the same mechanism the shipped `dedicated_control` uses).
The script sends console commands and reads back results;
see [`control/_olx_pipe.py`](control/_olx_pipe.py).

The scripts in [`control/`](control/) are Python 3 rewrites
of the relevant bits of the shipped (Python 2) `dedicated_control_io.py`:

- [`server_control.py`](control/server_control.py)
  hosts a game and starts it once a client has joined the lobby.
- [`client_control.py`](control/client_control.py)
  observes a connecting client's game state.

Each script reports progress as markers on **stderr**
(e.g. `SERVER_LOBBY`, `CLIENT[c2] PLAYING`).
Since OLX owns the script pipe,
the harness does not drive instances live;
instead each script is self-contained
and [`harness.py`](harness.py) coordinates the scenario
by waiting for those markers in each instance's combined output log.
Configuration is passed to the scripts through environment variables
(see each script's docstring).

Each instance gets an isolated `HOME`,
so runs do not touch the real user profile or interfere with one another.

## Notes / limitations

- OLX `exec`s the control script directly,
  so `server_control.py` and `client_control.py`
  must keep their executable bit and `#!/usr/bin/env python3` shebang.
- OLX splits the `-script` path on spaces,
  so the checkout path must not contain spaces.
- The default `-dedicated` control script shipped in the game
  (`share/gamedir/scripts/dedicated_control`) is Python 2
  and no longer runs on current systems;
  this suite ships its own Python 3 control scripts instead.
