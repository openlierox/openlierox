# Running an OpenLieroX dedicated server

OpenLieroX has a built-in **dedicated server** mode: a headless instance with no
graphics or sound that hosts a game others connect to. A set of Python control
scripts drive it automatically — cycling lobby → game, rotating maps and mods,
handling voting and admin/user chat commands.

This document covers two ways to run it:

- **[With Docker](#running-with-docker)** (recommended) — one self-contained image,
  nothing to compile or install on the host.
- **[Natively from source](#running-natively-from-source)** — a headless build
  you run directly.

It also covers [networking](#networking-lan-vs-online),
[configuration](#configuring-the-server),
[persistent data](#persistent-data), and
[publishing the image to Docker Hub](#publishing-the-image-to-docker-hub).

The Docker files live in [`docker/`](../docker); a condensed quick-start is in
[`docker/README.md`](../docker/README.md).

---

## How the dedicated server works

The server is the normal `openlierox` binary started in dedicated mode:

```sh
openlierox -dedicated -exec "script dedicated_control cfg/dedicated_config"
```

- `-dedicated` runs headless (no graphics, no sound), so no display is required.
- `-exec "script dedicated_control …"` loads the **Python 2** automation script
  ([`share/gamedir/scripts/dedicated_control`](../share/gamedir/scripts)), which
  reads its settings from a config such as
  [`share/gamedir/cfg/dedicated_config.py`](../share/gamedir/cfg/dedicated_config.py).

Without the control script the binary still runs, but it just sits in the lobby
and has to be driven manually over stdin. The script is what makes it a
"set-and-forget" server.

The server listens on **UDP port 23400** by default (`LX_PORT`, see
`SERVER_PORT` in the config).

---

## Running with Docker

### Use the prebuilt image (no build needed)

A ready-to-run image is published on Docker Hub at
[`klirktag/openlierox-server`](https://hub.docker.com/r/klirktag/openlierox-server).
If you just want to host a server and don't need to modify the source, pull it
instead of building:

```sh
docker run -d --name olx-server \
  --network host \
  -v olx-data:/home/olx/.OpenLieroX \
  klirktag/openlierox-server:20260630.2
```

Use `:latest` to always track the newest published build, or pin a dated tag such
as `:20260630.2` for reproducibility. To run it with docker compose, set
`image: klirktag/openlierox-server:20260630.2` and drop the `build:` section in
[`docker/docker-compose.yml`](../docker/docker-compose.yml).

The rest of this section describes building the image yourself from this source
tree (needed if you change the code or config defaults).

### Why Debian 11 (bullseye)?

The control scripts are written in **Python 2**. Debian 11 "bullseye" is the last
Debian release that still ships `python2.7` in its main repository — Debian 12 and
Ubuntu 24.04 removed it (and Ubuntu 24.04's `t64` library rename breaks several of
the runtime package names too). So the image is based on `debian:bullseye-slim`.
The image is built in two stages: a build stage compiles the `-DDEDICATED_ONLY`
binary, and a slim runtime stage ships only the binary, the game data, the shared
libraries, and `python2.7`.

### Quick start (docker compose)

From the [`docker/`](../docker) directory:

```sh
docker compose up -d --build      # build the image and start the server
docker compose logs -f            # watch the server log
docker compose down               # stop and remove the container
```

The compose file uses `network_mode: host`, so the server is reachable on your LAN
immediately and shows up in the in-game **Local network** server list. See
[Networking](#networking-lan-vs-online) for why.

### Quick start (plain docker)

From the repository root:

```sh
docker build -f docker/Dockerfile -t openlierox-server .

docker run -d --name olx-server \
  --network host \
  -v olx-data:/home/olx/.OpenLieroX \
  openlierox-server
```

The image runs as a non-root `olx` user.

---

## Running natively from source

If you would rather not use Docker, build a headless binary directly. Install the
build dependencies (see the main [`README.md`](../README.md)), plus `python2` for
the control scripts, then:

```sh
cmake -DDEDICATED_ONLY=Yes -DHAWKNL_BUILTIN=Yes .
make -j"$(nproc)"
```

`-DDEDICATED_ONLY=Yes` produces a binary with no graphics/sound code paths. Start
the server from the game data directory:

```sh
cd share/gamedir
../../bin/openlierox -dedicated -exec "script dedicated_control cfg/dedicated_config"
```

For running one or more instances as a managed system service on Debian, see the
init-script daemon in [`tools/DedicatedDaemonDebian`](../tools/DedicatedDaemonDebian),
which starts one `openlierox -dedicated` per config file found in a config
directory.

---

## Networking: LAN vs online

The same image works for both LAN and internet play — networking is a **runtime**
choice (`network_mode` / `--network`), not baked into the image.

### Host networking (default, best for LAN)

The in-game **Local network** browser discovers servers by sending a UDP
**broadcast** ping to `255.255.255.255:23400`; the server replies and is added to
the list (see `SendBroadcastPing` / `pingLAN` in
[`src/game/ServerList.cpp`](../src/game/ServerList.cpp)).

Docker's **bridge** network (NAT) does **not** relay subnet broadcasts between the
host LAN and the container, so a bridged server will not appear in the LAN list and
its replies would carry the bridge IP instead of the host's. `network_mode: host`
binds the server directly on the host's network stack, so broadcast discovery just
works. This is why the compose file defaults to host mode.

### Bridge networking with a published port (works for online)

UDP itself works fine through the bridge — only *broadcast discovery* doesn't. For
internet play, broadcast is irrelevant anyway (online clients use the master-server
list or connect by IP), so a port mapping is all you need:

```sh
docker run -d --name olx-server \
  -p 23400:23400/udp \
  -v olx-data:/home/olx/.OpenLieroX \
  openlierox-server
```

Use this on Docker Desktop (macOS/Windows), where host networking isn't available,
or anywhere you want port isolation. Clients then connect to the host's IP directly.

### Hosting online

For internet play you need **UDP 23400 reachable from outside** (open the firewall
and/or port-forward it) and a stable public IP. The server registers itself with
the OLX master server on startup (you'll see it resolve `server.az2000.de` and
report `Our external IP address is …` in the log), after which it appears in the
public **Internet** server browser.

| Where you host | Host mode available? | Notes |
| --- | --- | --- |
| Your LAN machine | Yes | Use host mode for LAN auto-discovery. |
| A VPS / VM you control (EC2, DigitalOcean, Hetzner, …) | Yes | Either host mode or bridge + `-p 23400:23400/udp`. |
| Managed/serverless (Cloud Run, Fargate, Render, …) | No | They block host mode and often don't expose arbitrary **UDP** — generally unsuitable for a game server. |

---

## Configuring the server

Server name, port, map/mod rotation, voting, and team rules live in the control
config [`share/gamedir/cfg/dedicated_config.py`](../share/gamedir/cfg/dedicated_config.py)
(baked into the image at `/opt/openlierox/cfg/dedicated_config.py`).

| Setting | Meaning |
| --- | --- |
| `SERVER_PORT` | UDP port (default `23400`). With bridge networking, update the published port to match. |
| `GLOBAL_SETTINGS["GameOptions.Network.ServerName"]` | Name shown in the server browser. |
| `LEVELS` / `SMALL_LEVELS` | Map rotation. |
| `PRESETS` / `MODS` | Mod/preset rotation. |
| `MIN_PLAYERS` | Players required before a round starts. |
| `VOTING` / `VOTING_PERCENT` | In-game voting for map/mod/kick. |

Two ways to customize without rebuilding:

- **Mount an override file** over the one in the image:

  ```sh
  docker run -d --name olx-server --network host \
    -v "$PWD/my_dedicated_config.py:/opt/openlierox/cfg/dedicated_config.py:ro" \
    -v olx-data:/home/olx/.OpenLieroX \
    openlierox-server
  ```

- **Point at a different config file** with the `OLX_DED_CONFIG` environment
  variable — a gamedir-relative path **without** the `.py` suffix, e.g.
  `-e OLX_DED_CONFIG=cfg/dedicated_config_lx56`.

---

## Persistent data

OpenLieroX writes its runtime state (rankings, logs, generated config) to
`~/.OpenLieroX`, which is `/home/olx/.OpenLieroX` inside the container. The compose
file maps it to the `olx-data` volume so it survives container recreation; with
`docker run`, add `-v olx-data:/home/olx/.OpenLieroX`.

---

## Publishing the image to Docker Hub

The canonical published image is
[`klirktag/openlierox-server`](https://hub.docker.com/r/klirktag/openlierox-server)
(see [Use the prebuilt image](#use-the-prebuilt-image-no-build-needed) to just
consume it). The steps below are for publishing your own build — the image is
networking-agnostic, so a single published image serves both LAN and online use.
Replace `YOURNAME` with your Docker Hub username (the maintainer uses `klirktag`).

Docker tags can't contain `+`, so use the **short** version string
(`./get_version.sh`, e.g. `20260630.2`) rather than the full
`YYYYMMDD.N+git.HASH`.

```sh
docker login

docker tag openlierox-server YOURNAME/openlierox-server:$(./get_version.sh)
docker tag openlierox-server YOURNAME/openlierox-server:latest

docker push YOURNAME/openlierox-server:$(./get_version.sh)
docker push YOURNAME/openlierox-server:latest
```

To also publish ARM builds (Raspberry Pi, Graviton, Apple Silicon), build and push
multi-arch in one step (the arm64 variant compiles under emulation and is slow):

```sh
docker buildx build --platform linux/amd64,linux/arm64 \
  -f docker/Dockerfile \
  -t YOURNAME/openlierox-server:latest \
  --push .
```

---

## Troubleshooting

- **Server doesn't appear in the in-game LAN list.** You're almost certainly on
  bridge networking — broadcast discovery needs `--network host`. Otherwise add the
  server by the host's IP manually.
- **Players can't connect over the internet.** UDP 23400 isn't reachable: open the
  firewall / port-forward it, and confirm the log shows the external IP being
  resolved.
- **`error while loading shared libraries`.** The runtime image is missing a
  library the binary links; it must be added to the runtime stage of the Dockerfile
  (this is why `libbinutils`, which provides `libbfd`, is installed there).
- **View the log:** `docker compose logs -f` (or `docker logs olx-server`).
