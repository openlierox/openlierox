# OpenLieroX dev base image

A Docker image that carries **every build dependency needed to compile
OpenLieroX from any of the maintained branches** — `0.58`, `0.59`, and
`master` — in a single environment. It is meant for development and for
building/backporting across versions, not for shipping a server.

## Why this exists

We sometimes have to backport fixes to older release lines, and those lines
don't all build against the same libraries:

| Branch   | SDL stack | Extra deps                                          |
|----------|-----------|-----------------------------------------------------|
| `0.58`   | SDL **1.2** (`libsdl1.2`, `SDL_mixer 1.2`, `SDL_image 1.2`) | — |
| `0.59`   | **SDL2**  | boost, OpenAL/ALUT, vorbis, yaml-cpp, libbfd (binutils) |
| `master` | **SDL2**  | same as `0.59`                                      |

Rather than juggle one image per branch, this image installs **both** the
SDL 1.2 and the SDL2 dev stacks (plus everything the newer branches need)
side by side, so the same container builds all three.

## Why Debian 11 (bullseye)

OpenLieroX's dedicated-server control scripts (`share/gamedir/dedicated_control`
and `share/gamedir/cfg/*.py`) are **Python 2**. Bullseye is the last Debian
release that still ships `python2` (2.7.18) in its archive, so we can just
`apt-get install python2` instead of compiling CPython 2.7 from source (which
is what the Ubuntu 24.04 dedicated-server runtime image has to do — see the
`dockerize-dedicated-server` branch). `python-is-python2` provides
`/usr/bin/python`, which the `#!/usr/bin/python -u` shebang in the control
scripts relies on.

## What's inside

- **Toolchain:** `build-essential`, `cmake`, `make`, `pkg-config`, `git`,
  `ccache` (on `PATH` at `/usr/lib/ccache`), `gdb`.
- **Python 2:** `python2`, `python2-dev`, `python2.7`, `python2.7-dev`,
  `python-is-python2`.
- **SDL 1.2 stack** (for `0.58`) and **SDL2 stack** (for `0.59`/`master`).
- **Shared libs:** libxml2, libgd, zlib, libzip, libX11, libcurl, boost
  (headers + system), OpenAL, ALUT, vorbis, yaml-cpp, binutils/libbfd.
- A non-root **`dev`** user (UID/GID overridable) so bind-mounted source
  keeps host ownership. Default working directory is `/src`.

### Two intentional substitutions vs. the `DEPS` file

- `DEPS` lists `libgd2-noxpm-dev` → we install **`libgd-dev`** (the old
  name no longer exists in Debian).
- `DEPS` lists `libboost-signals-dev` → **omitted**. Boost.Signals v1 was
  removed upstream (Boost ≥ 1.69) and isn't in bullseye. OLX only needs the
  header-only Boost.Signals2, which comes with `libboost-dev`.

## Build the image

```sh
# from the repo root
docker build -t openlierox-dev:bullseye build/dev-docker

# match your host user so mounted files aren't owned by root:
docker build \
  --build-arg UID=$(id -u) --build-arg GID=$(id -g) \
  -t openlierox-dev:bullseye build/dev-docker
```

## Use it to build OLX

Mount your checkout at `/src` and run the build inside the container:

```sh
docker run --rm -it -v "$PWD":/src openlierox-dev:bullseye
```

Then, inside the container:

```sh
# 0.59 / master (SDL2)
cmake -DHAWKNL_BUILTIN=Yes -DDEBUG=No -DCMAKE_BUILD_TYPE=Release .
make -j"$(nproc)"

# 0.58 (SDL 1.2) — the SDL 1.2 stack is already present, same commands
cmake -DHAWKNL_BUILTIN=Yes -DDEBUG=No .
make -j"$(nproc)"
```

`-DDEBUG=No` matters: the project's CMake defines `OPTION(DEBUG ... Yes)` and
overrides `CMAKE_BUILD_TYPE`, so without it you get a debug build with
`assert()` active. `-DHAWKNL_BUILTIN=Yes` uses the bundled HawkNL instead of a
system package.

### Persisting the ccache across runs

`ccache` speeds up repeated builds. Mount a volume at `/ccache` to keep the
cache between container runs:

```sh
docker run --rm -it \
  -v "$PWD":/src \
  -v olx-ccache:/ccache \
  openlierox-dev:bullseye
```

## Notes

- This image contains **no OpenLieroX source** — mount it. That keeps the
  image branch-agnostic and small to rebuild.
- The container runs as a non-root user by default. If you need to install
  extra packages ad hoc, run with `--user root` (or add them to the
  Dockerfile).
- This host has known Docker DNS quirks; if `apt-get` fails during the build,
  build with `--network=host`.
