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
[publishing the image to GHCR](#publishing-the-image-to-ghcr).

The Docker files live in [`build/docker/`](../build/docker); a condensed quick-start is in
[`build/docker/README.md`](../build/docker/README.md).

---

## How the dedicated server works

The server is the normal `openlierox` binary started in dedicated mode:

```sh
openlierox -dedicated -exec "script dedicated_control cfg/dedicated_config"
```

- `-dedicated` runs headless (no graphics, no sound), so no display is required.
- `-exec "script dedicated_control …"` loads the **Python 3** automation script
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

A ready-to-run image is published on GitHub Container Registry at
[`ghcr.io/openlierox/openlierox-server`](https://github.com/orgs/openlierox/packages/container/package/openlierox-server).
If you just want to host a server and don't need to modify the source, pull it
instead of building:

```sh
docker run -d --name openlierox-server \
  -p 23400:23400/udp \
  -v openlierox-data:/home/openlierox/.OpenLieroX \
  ghcr.io/openlierox/openlierox-server:latest
```

A published port works on any Docker host, including Docker Desktop
(macOS/Windows); see [Networking](#networking-lan-vs-online) for the
`--network host` alternative, which trades that compatibility for in-game LAN
auto-discovery.

Use `:latest` to always track the newest published build, or pin a specific
version tag (CI publishes one per merge to master, named by `./get_version.sh`)
for reproducibility. [`build/docker/docker-compose.yml`](../build/docker/docker-compose.yml)
already points at `ghcr.io/openlierox/openlierox-server:latest`, so
`docker compose up -d` pulls it with no further changes.

The rest of this section describes building the image yourself from this source
tree (needed if you change the code or config defaults).

### Why Ubuntu 24.04 (noble)?

noble is an LTS release with all the build dependencies packaged, and in
standard support until April 2029 (then Ubuntu Pro ESM covers it further),
unlike Debian 11 "bullseye", which this image used until bullseye's own LTS
ended and its mirrors stopped reliably serving packages. So the image is
based on `ubuntu:24.04`. The image is built in two stages: a build stage
compiles the `-DDEDICATED_ONLY` binary, and a slim runtime stage ships only
the binary, the game data, the shared libraries, and `python3`.

### Quick start (docker compose)

From the [`build/docker/`](../build/docker) directory:

```sh
docker compose up -d --build      # build the image and start the server
docker compose logs -f            # watch the server log
docker compose down               # stop and remove the container
```

The compose file publishes **UDP 23400** by default, which works on any Docker
host. If you want in-game **Local network** auto-discovery instead, uncomment
`network_mode: host` in
[`build/docker/docker-compose.yml`](../build/docker/docker-compose.yml) — see
[Networking](#networking-lan-vs-online) for why that trade-off exists.

### Quick start (plain docker)

From the repository root:

```sh
docker build -f build/docker/Dockerfile -t openlierox-server .

docker run -d --name openlierox-server \
  -p 23400:23400/udp \
  -v openlierox-data:/home/openlierox/.OpenLieroX \
  openlierox-server
```

The image runs as a non-root `openlierox` user.

---

## Running natively from source

If you would rather not use Docker, build a headless binary directly. Install the
build dependencies (see the main [`README.md`](../README.md)), plus `python3` for
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

### Bridge networking with a published port (default, works everywhere)

UDP itself works fine through Docker's bridge network, so a port mapping is all
you need, and it works on every Docker host, including Docker Desktop
(macOS/Windows), where host networking isn't available. This is why the compose
file defaults to it:

```sh
docker run -d --name openlierox-server \
  -p 23400:23400/udp \
  -v openlierox-data:/home/openlierox/.OpenLieroX \
  openlierox-server
```

For internet play this is all you need — online clients use the master-server
list or connect by IP, so LAN broadcast discovery is irrelevant. For LAN play,
clients connect to the host's IP directly (see below for auto-discovery instead).

### Host networking (opt-in, LAN auto-discovery)

The in-game **Local network** browser discovers servers by sending a UDP
**broadcast** ping to `255.255.255.255:23400`; the server replies and is added to
the list (see `SendBroadcastPing` / `pingLAN` in
[`src/game/ServerList.cpp`](../src/game/ServerList.cpp)).

Docker's **bridge** network (NAT) does **not** relay subnet broadcasts between the
host LAN and the container, so a bridged server will not appear in the LAN list and
its replies would carry the bridge IP instead of the host's. `network_mode: host`
binds the server directly on the host's network stack, so broadcast discovery just
works — but it only exists on Linux, which is why it isn't the default. Uncomment
`network_mode: host` in
[`build/docker/docker-compose.yml`](../build/docker/docker-compose.yml), or with plain
`docker run`, pass `--network host` instead of `-p 23400:23400/udp`:

```sh
docker run -d --name openlierox-server \
  --network host \
  -v openlierox-data:/home/openlierox/.OpenLieroX \
  openlierox-server
```

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
  docker run -d --name openlierox-server \
    -p 23400:23400/udp \
    -v "$PWD/my_dedicated_config.py:/opt/openlierox/cfg/dedicated_config.py:ro" \
    -v openlierox-data:/home/openlierox/.OpenLieroX \
    openlierox-server
  ```

- **Point at a different config file** with the `OLX_DED_CONFIG` environment
  variable — a gamedir-relative path **without** the `.py` suffix, e.g.
  `-e OLX_DED_CONFIG=cfg/dedicated_config_lx56`.

---

## Persistent data

OpenLieroX writes its runtime state (rankings, logs, generated config) to
`~/.OpenLieroX`, which is `/home/openlierox/.OpenLieroX` inside the container. The
compose file maps it to the `openlierox-data` volume so it survives container
recreation; with `docker run`, add
`-v openlierox-data:/home/openlierox/.OpenLieroX`.

---

## Publishing the image to GHCR

The canonical published image is
[`ghcr.io/openlierox/openlierox-server`](https://github.com/orgs/openlierox/packages/container/package/openlierox-server)
(see [Use the prebuilt image](#use-the-prebuilt-image-no-build-needed) to just
consume it). It publishes automatically: every merge to `master` builds the image
and pushes both `:latest` and the short version tag, gated on the headless tests
passing (see
[`.github/workflows/package-docker.yml`](../.github/workflows/package-docker.yml)).
The steps below are only for testing a build against your own registry namespace
before it lands on master — the image is networking-agnostic, so a single
published image serves both LAN and online use.

Docker tags can't contain `+`, so use the **short** version string
(`./get_version.sh`, e.g. `20260630.2`) rather than the full
`YYYYMMDD.N+git.HASH`.

```sh
gh auth refresh --scopes write:packages
gh auth token | docker login ghcr.io -u YOURNAME --password-stdin

docker tag openlierox-server ghcr.io/YOURNAME/openlierox-server:$(./get_version.sh)
docker tag openlierox-server ghcr.io/YOURNAME/openlierox-server:latest

docker push ghcr.io/YOURNAME/openlierox-server:$(./get_version.sh)
docker push ghcr.io/YOURNAME/openlierox-server:latest
```

Replace `YOURNAME` with your GitHub username; publishing under the `openlierox`
org instead requires package-write access on that org.

To also publish ARM builds (Raspberry Pi, Graviton, Apple Silicon), build and push
multi-arch in one step (the arm64 variant compiles under emulation and is slow):

```sh
docker buildx build --platform linux/amd64,linux/arm64 \
  -f build/docker/Dockerfile \
  -t ghcr.io/YOURNAME/openlierox-server:latest \
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
- **View the log:** `docker compose logs -f` (or `docker logs openlierox-server`).
