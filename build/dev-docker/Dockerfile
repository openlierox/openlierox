# OpenLieroX development / build base image.
#
# Purpose: a single image that carries every build dependency needed to
# compile OpenLieroX from *any* of the currently-maintained branches:
#
#   * 0.58   -> SDL 1.2 stack (libsdl1.2 / SDL_mixer 1.2 / SDL_image 1.2)
#   * 0.59   -> SDL2 stack + boost + OpenAL/ALUT + vorbis + yaml-cpp + libbfd
#   * master -> same as 0.59
#
# Because we occasionally backport fixes to the older lines, this image
# deliberately installs BOTH the SDL 1.2 and the SDL2 dev stacks so the same
# container can build every branch without swapping images.
#
# Debian 11 (bullseye) is the base on purpose: it is the last Debian release
# that still ships python2 (2.7.18) in its archive. OpenLieroX's dedicated
# server control scripts (dedicated_control, cfg/*.py) require Python 2, and
# bullseye lets us "apt-get install python2" instead of building CPython 2.7
# from source (which is what the Ubuntu 24.04 runtime image has to do).
#
# This is a BASE image, not a build of OLX itself. It intentionally contains
# no source. Mount your checkout at /src and run cmake+make inside it:
#
#   docker build -t openlierox-dev:bullseye build/dev-docker
#   docker run --rm -it -v "$PWD":/src openlierox-dev:bullseye
#   # then, inside the container, for a 0.59 / master build:
#   cmake -DHAWKNL_BUILTIN=Yes -DDEBUG=No -DCMAKE_BUILD_TYPE=Release .
#   make -j"$(nproc)"
#
#   # for a 0.58 build, the SDL 1.2 stack above is already present.
#
# To match host file ownership on bind mounts, pass your UID/GID at build:
#   docker build --build-arg UID=$(id -u) --build-arg GID=$(id -g) ...

FROM debian:11

ARG DEBIAN_FRONTEND=noninteractive
ENV DEBIAN_FRONTEND=noninteractive

# --- toolchain + all OLX build dependencies (0.58, 0.59, master) -----------
#
# Notes on a few choices:
#  * libgd-dev            -- modern name for the old libgd2-noxpm-dev in DEPS.
#  * NO libboost-signals-dev -- Boost.Signals v1 was removed upstream (>=1.69)
#    and is absent from bullseye. OLX only needs the header-only Boost.Signals2
#    plus a few utility headers, all provided by libboost-dev.
#  * binutils-dev + libiberty-dev -- provide libbfd for the crash/backtrace
#    handler (0.59 / master).
#  * both SDL 1.2 and SDL2 dev stacks are installed side by side.
RUN apt-get update && apt-get install -y --no-install-recommends \
        # --- core toolchain ---
        build-essential \
        cmake \
        make \
        pkg-config \
        ca-certificates \
        git \
        wget \
        curl \
        ccache \
        gdb \
        # patchelf: rewrites rpaths when assembling the self-contained
        # Linux bundle (.github/workflows/package-linux-bundle.sh).
        patchelf \
        zip \
        # --- Python 2 (dedicated_control + cfg/*.py, build-time detection) ---
        python2 \
        python2-dev \
        python2.7 \
        python2.7-dev \
        python-is-python2 \
        # --- SDL 1.2 stack (branch 0.58) ---
        libsdl1.2-dev \
        libsdl-mixer1.2-dev \
        libsdl-image1.2-dev \
        # --- SDL2 stack (branch 0.59 / master) ---
        libsdl2-dev \
        libsdl2-mixer-dev \
        libsdl2-image-dev \
        # --- shared graphics / xml / net / archive deps (all branches) ---
        libxml2-dev \
        libgd-dev \
        zlib1g-dev \
        libzip-dev \
        libx11-dev \
        libcurl4-openssl-dev \
        libboost-dev \
        libboost-system-dev \
        # --- audio (0.59 / master) ---
        libopenal-dev \
        libalut-dev \
        libvorbis-dev \
        # --- misc (0.59 / master) ---
        libyaml-cpp-dev \
        binutils-dev \
        libiberty-dev \
    && rm -rf /var/lib/apt/lists/*

# ccache on PATH so repeated dev builds are fast.
ENV PATH=/usr/lib/ccache:${PATH}
ENV CCACHE_DIR=/ccache
RUN mkdir -p /ccache && chmod 777 /ccache

# --- non-root dev user, UID/GID overridable to match the host -------------
# Keeps bind-mounted source files owned by your host user instead of root.
ARG UID=1000
ARG GID=1000
RUN groupadd --gid "${GID}" dev 2>/dev/null || true \
    && useradd --create-home --uid "${UID}" --gid "${GID}" --shell /bin/bash dev 2>/dev/null || true \
    && mkdir -p /src \
    && chown "${UID}:${GID}" /src /ccache

USER ${UID}:${GID}
WORKDIR /src

CMD ["/bin/bash"]
