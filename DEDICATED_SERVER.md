# Dedicated Server Resources

Map of everything in this repository related to running OpenLieroX as a
headless dedicated server. Use this as an index when you need to launch,
script, configure, package, or extend a dedicated server.

## Quick start

```sh
./start_dedicated_server.sh                          # plain dedicated server
./start_dedicated_server.sh -script scripts/simple_ded_control.sh
./start_dedicated_server.sh -exec "setvar GameOptions.Network.ServerName \"my server\""
```

`start_dedicated_server.sh` is a thin wrapper around the local `bin/openlierox`
binary that passes `-dedicated` (which also implies `-nosound`). Any extra
arguments are forwarded to the engine.

## Engine entry point

The dedicated mode is built into the main binary and selected at startup:

- [src/main.cpp:476-494](src/main.cpp#L476-L494) — handles the `-dedicated` and
  `-script <path>` CLI flags. `-dedicated` disables graphics and sound;
  `-script` additionally points the dedicated controller at a custom script
  (sets `tLXOptions->sDedicatedScript`).
- [doc/openlierox.6:50-53](doc/openlierox.6#L50-L53) — man-page description.
  When `-dedicated` is given, OLX auto-runs `scripts/dedicated_control` as a
  CGI-style child process and talks to it over stdin/stdout.

Search path order for the control script: `~/.OpenLieroX/scripts/`, `./`,
`/usr/share/games/OpenLieroX/scripts/`.

## Default control script (Python)

The engine launches this Python program in dedicated mode and drives it via a
line-based protocol on stdio:

- [share/gamedir/scripts/dedicated_control](share/gamedir/scripts/dedicated_control)
  — main loop / entry point.
- [share/gamedir/scripts/dedicated_control_handler.py](share/gamedir/scripts/dedicated_control_handler.py)
  — event and command dispatcher.
- [share/gamedir/scripts/dedicated_control_io.py](share/gamedir/scripts/dedicated_control_io.py)
  — engine I/O protocol implementation.
- [share/gamedir/scripts/dedicated_control_ranking.py](share/gamedir/scripts/dedicated_control_ranking.py)
  — player ranking.
- [share/gamedir/scripts/dedicated_control_usercommands.py](share/gamedir/scripts/dedicated_control_usercommands.py)
  — chat-command implementations (admin/user commands).
- [share/gamedir/scripts/portalocker.py](share/gamedir/scripts/portalocker.py)
  — cross-platform file locking helper used by the above.

### Configuration for the default controller

- [share/gamedir/cfg/dedicated_config.py](share/gamedir/cfg/dedicated_config.py)
  — admin prefix, server port, min players, log file, etc.
- [share/gamedir/cfg/dedicated_config_lx56.py](share/gamedir/cfg/dedicated_config_lx56.py)
  — LX 0.56-compatible variant.
- [share/gamedir/Gusanos/config-ded.cfg](share/gamedir/Gusanos/config-ded.cfg)
  — config specific to Gusanos game mode running dedicated.

General gameplay/network settings live in `share/gamedir/cfg/options.cfg`
(loaded by every OLX process, not dedicated-specific).

## Alternative control scripts

These are example controllers you can plug in via `-script <path>`. They are
useful as references for the engine ↔ controller protocol or as ready-made
game modes:

- [share/gamedir/scripts/simple_ded_control.sh](share/gamedir/scripts/simple_ded_control.sh)
  — minimal bash controller; the clearest reference for the protocol.
- [share/gamedir/scripts/simple_ded_control.php](share/gamedir/scripts/simple_ded_control.php)
  — PHP equivalent.
- [share/gamedir/scripts/dedicated_control.sh](share/gamedir/scripts/dedicated_control.sh)
  — wrapper shell around the default Python controller.
- [share/gamedir/scripts/ded_fixed_gamemode](share/gamedir/scripts/ded_fixed_gamemode)
  — server stuck on one fixed mode.
- [share/gamedir/scripts/simple_map_cycling](share/gamedir/scripts/simple_map_cycling)
  — straightforward map rotation.
- [share/gamedir/scripts/map_cycle_and_ranking](share/gamedir/scripts/map_cycle_and_ranking)
  — map rotation with player ranking.
- [share/gamedir/scripts/team_defenders_beta.py](share/gamedir/scripts/team_defenders_beta.py)
  — custom team-based game mode.
- [share/gamedir/scripts/DedServerWatcher.py](share/gamedir/scripts/DedServerWatcher.py)
  — external watcher that restarts crashed servers.
- [share/gamedir/scripts/pwn0meter.py](share/gamedir/scripts/pwn0meter.py),
  [share/gamedir/scripts/simulator.py](share/gamedir/scripts/simulator.py)
  — utilities used alongside dedicated mode.

### Console commands

A dedicated server is driven by the same console commands as the interactive
game; controllers issue them on stdout. The list is in:

- [share/gamedir/scripts/tools/available_commands.txt](share/gamedir/scripts/tools/available_commands.txt)
- [share/gamedir/scripts/tools/commands.py](share/gamedir/scripts/tools/commands.py)

You can also fire individual commands at startup with `-exec "<command>"`.

## Launcher / packaging tooling

OS-level packaging that turns OLX into a managed service:

- [tools/DedicatedDaemonDebian/](tools/DedicatedDaemonDebian/) — Debian
  package. Installs `openlierox-dedicated` as a system service that launches
  one OLX instance per `.py` config in `/etc/openlierox-dedicated/`.
  Key file: [bin/openlierox-dedicated](tools/DedicatedDaemonDebian/bin/openlierox-dedicated).
- [tools/OLXDedServerUnixDaemon/](tools/OLXDedServerUnixDaemon/) — generic
  Unix install/uninstall scripts plus a sysv-style init script in
  [src/openlierox](tools/OLXDedServerUnixDaemon/src/openlierox).
- [tools/OLXDedServerWindowsService/](tools/OLXDedServerWindowsService/) —
  Windows service wrapper (C++; see [src/](tools/OLXDedServerWindowsService/src/)).
- [tools/DedicatedServerVideo/](tools/DedicatedServerVideo/) — runs a
  dedicated server inside `Xvfb`, screen-records it, and uploads to YouTube.
  Entry points:
  [start_dedicated.sh](tools/DedicatedServerVideo/start_dedicated.sh),
  [dedicated-video-record.sh](tools/DedicatedServerVideo/dedicated-video-record.sh),
  [install.sh](tools/DedicatedServerVideo/install.sh),
  [YoutubeUpload.py](tools/DedicatedServerVideo/YoutubeUpload.py).

## Documentation

- [doc/openlierox.6](doc/openlierox.6) — man page; the authoritative short
  reference for CLI flags and dedicated mode.
- [doc/ChangeLog](doc/ChangeLog) — historical notes on dedicated-mode
  features.
- [README.md](README.md) — build instructions; dedicated mode is built into
  the same binary, no separate target required.

## Local repo additions

- [start_dedicated_server.sh](start_dedicated_server.sh) — repo-root launcher
  that runs `bin/openlierox -dedicated "$@"` from the proper working
  directory (`share/gamedir`).
