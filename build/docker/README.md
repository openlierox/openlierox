# Running an OpenLieroX dedicated server with Docker

This directory contains everything needed to run a headless OpenLieroX
**dedicated server** (e.g. on your LAN) inside Docker. The image is built from
this source tree and runs the game in `-dedicated` mode with the Python
automation scripts that cycle games, rotate maps/mods, and handle voting.

> For the full guide — prebuilt image, native build, LAN vs online networking,
> publishing, and troubleshooting — see [`doc/DEDICATED_SERVER.md`](../../doc/DEDICATED_SERVER.md).
> A ready-to-run image is published at
> [`ghcr.io/openlierox/openlierox-server`](https://github.com/orgs/openlierox/packages/container/package/openlierox-server).

## Why Ubuntu 24.04 (noble)?

noble is an LTS release with all the build dependencies packaged and in
standard support until April 2029 (then Ubuntu Pro ESM), unlike Debian 11
"bullseye", which this image used until bullseye's own LTS ended and its
mirrors stopped reliably serving packages. So the image uses `ubuntu:24.04`
as its base.

## Quick start (docker compose)

From this `build/docker/` directory:

```sh
docker compose up -d --build      # build the image and start the server
docker compose logs -f            # watch the server log
docker compose down               # stop and remove the container
```

The server listens on **UDP 23400** (the OLX default) and, by default, is
published on that port on the host — this works on any Docker host, including
Docker Desktop (macOS/Windows). Clients connect to the host machine's IP. If
you want in-game **Local network** auto-discovery instead, uncomment
`network_mode: host` in [`docker-compose.yml`](docker-compose.yml).

## Quick start (plain docker)

From the repository root (two levels up from here):

```sh
docker build -f build/docker/Dockerfile -t openlierox-server .

docker run -d --name openlierox-server \
  -p 23400:23400/udp \
  -v openlierox-data:/home/openlierox/.OpenLieroX \
  openlierox-server
```

For in-game **Local network** auto-discovery instead of a published port, use
host networking (Linux only — not available on Docker Desktop):

```sh
docker run -d --name openlierox-server \
  --network host \
  -v openlierox-data:/home/openlierox/.OpenLieroX \
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
  docker run -d --name openlierox-server \
    -p 23400:23400/udp \
    -v "$PWD/my_dedicated_config.py:/opt/openlierox/cfg/dedicated_config.py:ro" \
    -v openlierox-data:/home/openlierox/.OpenLieroX \
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
  `openlierox-data` volume.
- The container runs as a non-root `openlierox` user.
