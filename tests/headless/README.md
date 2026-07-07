# Headless test suite

Runtime tests that drive real `openlierox` processes in dedicated mode (`-dedicated`),
so they need no display or audio,
and run on a bare CI runner.

- `test_smoke.py` — the game boots and hosts a lobby.
- `test_network.py` — networked play between instances.
- `harness.py`, `conftest.py`, `control/` —
  helpers plus the Python 3 control scripts that drive each instance.

## Running

```sh
./tests/headless/run.sh -v
```

Builds the binary if needed
(installing Homebrew deps on macOS;
on Linux install the apt packages from the project [README](../../README.md) first),
then runs pytest. Needs Python 3 + pytest.

## How it works

Each OLX instance runs headless (`-dedicated -script <control script>`)
and is driven over OLX's dedicated-control pipe;
the scripts in `control/` are Python 3 rewrites
of the shipped (Python 2) `dedicated_control_io.py`.
They report progress as stderr markers
(`SERVER_LOBBY`, `CLIENT[c2] PLAYING`, ...) that `harness.py` waits on.
Clients are dedicated instances that `-connect` out,
and each instance gets an isolated `HOME`.

## Notes

- The control scripts are `exec`'d directly,
  so they must stay executable with a `#!/usr/bin/env python3` shebang,
  and the checkout path must have no spaces.
