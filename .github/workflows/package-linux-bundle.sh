#!/bin/bash
set -euo pipefail

cd $(dirname $0)"/../.."

PACKAGE_NAME="openlierox-$(./get_version.sh)-linux-bundle"
PACKAGE_DIR="distrib/"$PACKAGE_NAME

rm -Rf $PACKAGE_DIR
mkdir -p $PACKAGE_DIR
cp build-output/bin/openlierox $PACKAGE_DIR
cp -av share/gamedir $PACKAGE_DIR/gamedir

# Copy all .so files this binary was built for, to make a self contained bundle
mkdir -p $PACKAGE_DIR/lib
ldd $PACKAGE_DIR/openlierox | grep -v "libc.so*" | grep "=> /" | awk '{print $3}' | xargs -I '{}' cp -v '{}' $PACKAGE_DIR/lib/

cd $PACKAGE_DIR

# Patch openlierox binary to use local lib/ folder
patchelf --set-rpath '$ORIGIN/lib' --force-rpath openlierox

# Patch libraries to use local lib/ folder for transitive dependencies
patchelf --set-rpath '$ORIGIN' --force-rpath ./lib/*.so*
