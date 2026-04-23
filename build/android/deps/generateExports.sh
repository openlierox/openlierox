#!/bin/sh
# Run this from directory <Eclair repo>/out/target/product/generic/obj/SHARED_LIBRARIES
for LIB in c m dl log stdc++ dvm ssl z binder EGL GLES_android GLESv1_CM ETC1 ui utils cutils hardware hardware_legacy pixelflinger; do
nm -g -p --defined-only lib${LIB}_intermediates/LINKED/*.so
done | cut -b 12- | sort | uniq > exports-eclair.txt
