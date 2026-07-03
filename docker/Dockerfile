# OpenLieroX 0.58 dedicated server — Debian 11 "bullseye"
#
# This is the 0.58 backport of the dedicated-server Docker support. Two reasons
# the base is debian:bullseye-slim:
#   * The dedicated-server control scripts (share/gamedir/scripts/
#     dedicated_control*) are written in Python 2; bullseye is the last Debian
#     release that still ships python2.7.
#   * OpenLieroX 0.58 is built against SDL 1.2 (not SDL2 — that's the master
#     branch). bullseye still provides the SDL 1.2 dev/runtime packages.
#
# Built in two stages: a build stage compiles a -DDEDICATED_ONLY binary, and a
# slim runtime stage ships only the binary, game data, shared libraries and
# python2.7.

# ---------- build stage ----------
FROM debian:bullseye-slim AS build

ENV DEBIAN_FRONTEND=noninteractive
# Dependencies for a DEDICATED_ONLY build of 0.58 (derived from debian/control,
# minus SDL_mixer/gd which the dedicated build does not link). HawkNL and LibZIP
# are built from the bundled libs/ sources, so they need no -dev packages.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libsdl1.2-dev \
        libsdl-image1.2-dev \
        libxml2-dev \
        zlib1g-dev \
        libcurl4-openssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# DEDICATED_ONLY=Yes  -> headless build (no gfx/sound), drops SDL_mixer/gd/X11.
# HAWKNL_BUILTIN=Yes   -> use the bundled HawkNL networking lib.
# LIBZIP_BUILTIN=Yes   -> use the bundled LibZIP (libs/libzip). On Linux this
#                         option defaults Off (links system libzip), but 0.58
#                         ships no libzip-dev dependency, so the official deb
#                         build (debian/rules) enables the builtin — we match it.
# DEBUG=No             -> 0.58's DEBUG option defaults to Yes and overrides
#                         CMAKE_BUILD_TYPE, so it must be turned off explicitly
#                         to get a Release build.
# Out-of-source build into build-output/ (mirrors .github/workflows/
# build-linux.sh); the target's OUTPUT_NAME is "bin/openlierox", so the bin/
# subdir must exist first. Leaves one core free during the build (floors at 1).
RUN cmake -S . -B build-output -DDEBUG=No -DHAWKNL_BUILTIN=Yes -DLIBZIP_BUILTIN=Yes -DDEDICATED_ONLY=Yes \
    && mkdir -p build-output/bin \
    && n="$(nproc)" \
    && { [ "$n" -gt 1 ] && n="$((n - 1))" || true; } \
    && cmake --build build-output -j"$n"

# ---------- runtime stage ----------
FROM debian:bullseye-slim

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        python2.7 \
        libsdl1.2debian \
        libsdl-image1.2 \
        libxml2 \
        zlib1g \
        libcurl4 \
    && ln -sf /usr/bin/python2.7 /usr/bin/python \
    && ln -sf /usr/bin/python2.7 /usr/bin/python2 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build-output/bin/openlierox /usr/local/bin/openlierox
COPY --from=build /src/share/gamedir /opt/openlierox
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh

# Run as a non-root user. ~/.OpenLieroX (under HOME) is where OLX writes its
# own configs, logs and rankings — mount a volume there to persist them.
RUN chmod +x /usr/local/bin/entrypoint.sh \
    && useradd -r -m -d /home/olx olx \
    && chown -R olx:olx /opt/openlierox

USER olx
ENV HOME=/home/olx
WORKDIR /opt/openlierox

# OpenLieroX dedicated servers listen on UDP 23400 by default.
EXPOSE 23400/udp

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
