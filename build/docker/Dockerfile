# OpenLieroX dedicated server — Ubuntu 24.04 "noble"
#
# Why noble: an LTS release with all the build dependencies packaged and in
# standard support until April 2029 (then Ubuntu Pro ESM). The dedicated-server
# automation (share/gamedir/scripts/dedicated_control*) is written in Python 3
# and runs via a "#!/usr/bin/env -S python3 -u" shebang.

# ---------- build stage ----------
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        libsdl1.2-dev \
        libsdl-image1.2-dev \
        libsdl-mixer1.2-dev \
        libxml2-dev \
        libgd-dev \
        zlib1g-dev \
        libzip-dev \
        libx11-dev \
        libcurl4-openssl-dev \
        libboost-dev \
        libopenal-dev \
        libalut-dev \
        libvorbis-dev \
        binutils-dev \
        libiberty-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# DEDICATED_ONLY=Yes -> headless build, no GUI/G15 code paths.
# HAWKNL_BUILTIN=Yes -> use the bundled HawkNL networking lib.
# The in-binary version comes from the committed VERSION file (get_version.sh
# -> functions.sh), so unlike master's git-derived version this build does
# not need git or the .git directory at all.
# The make step leaves one core free during the build (floors at 1).
RUN cmake -DDEDICATED_ONLY=Yes -DHAWKNL_BUILTIN=Yes -DDEBUG=No . \
    && n="$(nproc)" \
    && { [ "$n" -gt 1 ] && n="$((n - 1))" || true; } \
    && make -j"$n"

# ---------- runtime stage ----------
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 \
        libbinutils \
        # Needed for libcurl's HTTPS certificate verification (external-IP
        # lookup and master-server registration both use HTTPS).
        ca-certificates \
        libsdl1.2debian \
        libsdl-image1.2 \
        libsdl-mixer1.2 \
        libxml2 \
        libgd3 \
        zlib1g \
        libzip4t64 \
        libcurl4t64 \
        libopenal1 \
        libalut0 \
        libvorbisfile3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/bin/openlierox /usr/local/bin/openlierox
COPY --from=build /src/share/gamedir /opt/openlierox
COPY build/docker/entrypoint.sh /usr/local/bin/entrypoint.sh

# Run as a non-root user. ~/.OpenLieroX (under HOME) is where OLX writes its
# own configs, logs and rankings — mount a volume there to persist them.
# The directory is pre-created here (rather than left for OLX to create at
# first run) so that when an empty named volume is first mounted over it,
# Docker's volume-populate step copies it up with the right ownership
# instead of the volume defaulting to root:root and OLX failing to write to it.
RUN chmod +x /usr/local/bin/entrypoint.sh \
    && useradd -r -m -d /home/openlierox openlierox \
    && mkdir -p /home/openlierox/.OpenLieroX \
    && chown -R openlierox:openlierox /opt/openlierox /home/openlierox

USER openlierox
ENV HOME=/home/openlierox
WORKDIR /opt/openlierox

# OpenLieroX dedicated servers listen on UDP 23400 by default.
EXPOSE 23400/udp

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
