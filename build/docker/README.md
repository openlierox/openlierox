# Running an OpenLieroX 0.58 dedicated server with Docker

This directory contains everything needed to run a headless OpenLieroX
**dedicated server** (e.g. on your LAN) inside Docker. The image is built from
this source tree and runs the game in `-dedicated` mode with the Python
automation scripts that cycle games, rotate maps/mods, and handle voting.

> For the full guide — prebuilt image, native build, LAN vs online networking,
> publishing, and troubleshooting — see [`doc/DEDICATED_SERVER.md`](../../doc/DEDICATED_SERVER.md).
> A ready-to-run image is published at
> [`ghcr.io/openlierox/openlierox-server`](https://github.com/orgs/openlierox/packages/container/package/openlierox-server),
> the same image master publishes — see [Image tags](#image-tags) for which
> tag to use.

## Image tags

`ghcr.io/openlierox/openlierox-server` is shared with master, which publishes
its own builds there under `:latest` and its own version tags. Images built
from this branch are pushed under **this branch's version tag only** (e.g.
`0.58_rc5`, from `./get_version.sh`) — **never `:latest`**, since 0.58 is the
old, legacy version of the game and must never be what `:latest` resolves to.
Always pin the version tag when running a 0.58 image; this is why
[`docker-compose.yml`](docker-compose.yml) does too.

## Why Ubuntu 24.04 (noble)?

noble is an LTS release with all the build dependencies packaged and in
standard support until April 2029 (then Ubuntu Pro ESM). The dedicated-server
control scripts (`share/gamedir/scripts/dedicated_control*`) are written in
**Python 3**, which the image installs from the distribution (`python3`). The
image uses `ubuntu:24.04` as its base for both the build and runtime stages.

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
| `PRESETS` | Mod/preset rotation. |
| `MIN_PLAYERS` | Players required before a round starts. |
| `FILL_WITH_BOTS_TO` | Fill the server with bots up to this many players. |

## Notes

- The build uses `-DDEDICATED_ONLY=Yes`, so no graphics/sound subsystems are
  initialized — it runs fine without a display.
- Persistent data (options, logs, rankings) is written to `~/.OpenLieroX`
  inside the container; the compose file maps that to the `openlierox-data`
  volume.
- The container runs as a non-root `openlierox` user.
