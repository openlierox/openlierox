# OpenLieroX dedicated server — Ubuntu 24.04 "noble"
#
# Why noble: an LTS release with all the build dependencies packaged and in
# standard support until April 2029 (then Ubuntu Pro ESM), unlike Debian 11
# "bullseye", which we moved off of after its LTS ended and its mirrors
# stopped reliably serving packages.

# ---------- build stage ----------
FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        libsdl2-dev \
        libsdl2-image-dev \
        libsdl2-mixer-dev \
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
        libyaml-cpp-dev \
        binutils-dev \
        libiberty-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# DEDICATED_ONLY=Yes      -> headless build, no GUI/G15 code paths.
# HAWKNL_BUILTIN=Yes      -> use the bundled HawkNL networking lib.
# OLX_DOWNLOAD_...=OFF     -> use the committed gamecontrollerdb.txt instead of
#                            fetching it at configure time (no network needed,
#                            and irrelevant for a dedicated server anyway).
# The in-binary version is derived from the git history (CMakeOlxVersion.cmake
# -> get_version_full.sh), which is why git and the copied .git dir are needed.
# The make step leaves one core free during the build (floors at 1).
RUN cmake -DDEDICATED_ONLY=Yes -DHAWKNL_BUILTIN=Yes -DOLX_DOWNLOAD_GAMECONTROLLERDB=OFF -DDEBUG=No . \
    && n="$(nproc)" \
    && { [ "$n" -gt 1 ] && n="$((n - 1))" || true; } \
    && make -j"$n"

# ---------- runtime stage ----------
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3 \
        libbinutils \
        libsdl2-2.0-0 \
        libsdl2-image-2.0-0 \
        libsdl2-mixer-2.0-0 \
        libxml2 \
        libgd3 \
        zlib1g \
        libzip4t64 \
        libcurl4t64 \
        libopenal1 \
        libalut0 \
        libvorbisfile3 \
        libyaml-cpp0.8 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/bin/openlierox /usr/local/bin/openlierox
COPY --from=build /src/share/gamedir /opt/openlierox
COPY build/docker/entrypoint.sh /usr/local/bin/entrypoint.sh

# Run as a non-root user. ~/.OpenLieroX (under HOME) is where OLX writes its
# own configs, logs and rankings — mount a volume there to persist them.
RUN chmod +x /usr/local/bin/entrypoint.sh \
    && useradd -r -m -d /home/openlierox openlierox \
    && chown -R openlierox:openlierox /opt/openlierox

USER openlierox
ENV HOME=/home/openlierox
WORKDIR /opt/openlierox

# OpenLieroX dedicated servers listen on UDP 23400 by default.
EXPOSE 23400/udp

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
