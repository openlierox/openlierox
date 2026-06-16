#!/bin/sh

# Prints the full OLX version including the git hash (e.g.
# 20260616.4+git.b547037). For the short version (no hash), used by the
# build/packaging workflows, see get_version.sh.

cd "$(dirname "$0")"

. ./functions.sh

get_olx_version_with_hash
