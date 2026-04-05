#!/bin/bash
#if [ ! -f CMakeCache.txt ]; then
    cmake -DHAWKNL_BUILTIN=Yes .
#fi
make -j8
