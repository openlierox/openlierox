#!/bin/sh
# Check if any of exported symbols in application clash with symbol from system libraries
# Internal compiler things like '__aeabi_ddiv' or '___Unwind_Resume'  may be safely ignored, but 'regcomp' will cause your app to fail.
nm -g -p --defined-only project/obj/local/armeabi-v7a/*.so | cut -b 12- | sort | uniq | cat - exports-eclair.txt | sort | uniq -d
