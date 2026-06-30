# Running an OpenLieroX dedicated server with Docker

This directory contains everything needed to run a headless OpenLieroX
**dedicated server** (e.g. on your LAN) inside Docker. The image is built from
this source tree and runs the game in `-dedicated` mode with the Python
automation scripts that cycle games, rotate maps/mods, and handle voting.

## Why Debian 11 (bullseye)?

The dedicated-server control scripts (`share/gamedir/scripts/dedicated_control*`)
are written in **Python 2**. Debian 11 "bullseye" is the last Debian release that
still ships `python2.7` in its main repository — Debian 12 and Ubuntu 24.04
removed it. So the image uses `debian:bullseye-slim` as its base.

## Quick start (docker compose)

From this `docker/` directory:

```sh
docker compose up -d --build      # build the image and start the server
docker compose logs -f            # watch the server log
docker compose down               # stop and remove the container
```

The server listens on **UDP 23400** (the OLX default). With the default
`network_mode: host`, it is reachable on your LAN immediately and shows up in the
in-game **Local network** server list. Other players can also connect directly
to the host machine's IP.

## Quick start (plain docker)

From the repository root (one level up from here):

```sh
docker build -f docker/Dockerfile -t openlierox-server .

docker run -d --name olx-server \
  --network host \
  -v olx-data:/home/olx/.OpenLieroX \
  openlierox-server
```

If you can't use host networking (e.g. on Docker Desktop / macOS / Windows), map
the port instead — but note LAN broadcast discovery may not work through the
bridge, so clients would connect to the host IP directly:

```sh
docker run -d --name olx-server \
  -p 23400:23400/udp \
  -v olx-data:/home/olx/.OpenLieroX \
  openlierox-server
```

## Configuring the server

Server name, port, level/mod rotation, voting, team rules, etc. live in the
control config `share/gamedir/cfg/dedicated_config.py` (baked into the image at
`/opt/openlierox/cfg/dedicated_config.py`). Two ways to customize:

- **Edit + rebuild:** change `dedicated_config.py` in the source tree and rebuild.
- **Mount an override (no rebuild):** put your edited copy on the host and mount
  it over the one in the image:

  ```sh
  docker run -d --name olx-server --network host \
    -v "$PWD/my_dedicated_config.py:/opt/openlierox/cfg/dedicated_config.py:ro" \
    -v olx-data:/home/olx/.OpenLieroX \
    openlierox-server
  ```

To load an entirely different config file, set `OLX_DED_CONFIG` (a gamedir-relative
path, **without** the `.py` suffix), e.g. `-e OLX_DED_CONFIG=cfg/dedicated_config_lx56`.

Key settings in `dedicated_config.py`:

| Setting | Meaning |
| --- | --- |
| `SERVER_PORT` | UDP port (default `23400`). If you change it, update the published port too when using bridge networking. |
| `GLOBAL_SETTINGS["GameOptions.Network.ServerName"]` | Name shown in the server browser. |
| `LEVELS` / `SMALL_LEVELS` | Map rotation. |
| `PRESETS` / `MODS` | Mod/preset rotation. |
| `MIN_PLAYERS` | Players required before a round starts. |

## Notes

- The build uses `-DDEDICATED_ONLY=Yes`, so no graphics/sound subsystems are
  initialized — it runs fine without a display.
- Persistent data (rankings, logs, generated config) is written to
  `~/.OpenLieroX` inside the container; the compose file maps that to the
  `olx-data` volume.
- The container runs as a non-root `olx` user.
