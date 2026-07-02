#!/bin/bash
set -euo pipefail

cd $(dirname $0)"/../.."

PACKAGE_NAME="openlierox-$(./get_version.sh)-linux-bundle"
PACKAGE_DIR="distrib/"$PACKAGE_NAME

rm -Rf $PACKAGE_DIR
mkdir -p $PACKAGE_DIR
cp build-output/bin/openlierox $PACKAGE_DIR
cp -av share/gamedir $PACKAGE_DIR/gamedir

# Copy all .so files this binary was built for, to make a self contained bundle.
#
# We must NOT bundle any part of glibc. glibc's sub-libraries (libpthread,
# libm, libdl, librt, libresolv, ...) share internal GLIBC_PRIVATE symbols
# with libc.so.6 and are only ABI-compatible with the exact libc.so.6 they
# shipped with. The final binary always loads the *host's* libc.so.6 and
# dynamic loader (their path is baked into the ELF interpreter / NEEDED and
# can't be overridden by rpath), so bundling e.g. Debian's libpthread.so.0
# next to a host libc.so.6 breaks with
#   undefined symbol: __libc_pthread_init, version GLIBC_PRIVATE
# Because we build against an older glibc than the deploy targets, letting the
# whole glibc family resolve from the host is both correct and forward-safe.
#
# The exclusion list below is the glibc-provided set only; genuinely external
# libraries with confusingly similar names (libcurl, libmikmod, libnsl,
# libtirpc, ...) are still bundled.
GLIBC_EXCLUDE='/(ld-linux[^/]*|libc|libpthread|libm|libmvec|libdl|librt|libresolv|libutil|libanl|libBrokenLocale|libnss_[^/]*|libcrypt)\.so'
mkdir -p $PACKAGE_DIR/lib
ldd $PACKAGE_DIR/openlierox | grep "=> /" | awk '{print $3}' | grep -vE "$GLIBC_EXCLUDE" | xargs -I '{}' cp -v '{}' $PACKAGE_DIR/lib/

cd $PACKAGE_DIR

# Patch openlierox binary to use local lib/ folder
patchelf --set-rpath '$ORIGIN/lib' --force-rpath openlierox

# Patch libraries to use local lib/ folder for transitive dependencies
patchelf --set-rpath '$ORIGIN' --force-rpath ./lib/*.so*
