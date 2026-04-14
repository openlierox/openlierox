#!/bin/bash
set -euo pipefail

cd $(dirname $0)"/../.."
cmake -S . -B build-output -DCMAKE_BUILD_TYPE=Release -DHAWKNL_BUILTIN=Yes
mkdir -p build-output/bin
cmake --build build-output -j$(nproc)