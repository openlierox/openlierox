#!/bin/sh

# collection of helper-functions, mainly for compile.sh

# some simple existance-test-function
# searches for a header file in all include paths
#	$1 - header file
#	$INCLUDE_PATH - list of include paths
# returns 1 when not found
test_include_file() {
	for p in $INCLUDE_PATH; do
		[ -e "$p"/"$1" ] && return 0
	done
	return 1
}

# check if executable is there
#	$1 - executable
# return 1 if not available
test_exec() {
	which "$1" 1>/dev/null 2>/dev/null
	return $?
}

# grep specific parameter out of input-stream
#	$1 - parameter-name
# example: stdin=-Iinclude, $1=-I  =>  stdout=include
grep_param() {
	grepexpr=$(echo $1 | sed -e "s/-/[-]/")
	for p in $(xargs); do
		echo "$p" | grep "$grepexpr" | sed -e "s/$1//"
	done
}

# simple own sdl-config
# tries to behave like sdl-config
# replacement for sdl-config if not available
#	$1 = --libs => lib-flags for linker
#	$1 = --cflags => compiler-flags
#	$INCLUDE_PATH - used for include-paths for compiler flags
# prints out the flags on stdout
own_sdl_config() {
	[ "$1" = "--libs" ] && echo "-lSDL"
	if [ "$1" = "--cflags" ]; then
		for d in $INCLUDE_PATH; do
			[ -d "$d/SDL" ] && echo -n "-I$d/SDL "
		done
		echo ""
	fi
}

# simply own xml2-config
# tries to behave like xml2-config
# replacement for xml2-config if not available
#	$1 = --libs => lib-flags for linker
#	$1 = --cflags => compiler-flags
#	$INCLUDE_PATH - used for include-paths for compiler flags
# prints out the flags on stdout
own_xml2_config() {
	[ "$1" = "--libs" ] && echo "-lz -lxml2"
	if [ "$1" = "--cflags" ]; then
		for d in $INCLUDE_PATH; do
			[ -d "$d/libxml2" ] && echo -n "-I$d/libxml2 "
		done
		echo ""
	fi
}

# get somehow a version-string (like "0.57_beta4_r1090")
# prints the OLX version on stdout, in the form
#
#   YYYYMMDD.N+git.HASH      e.g. 20260615.1+git.a6632ac
#
# where
#   YYYYMMDD - date of the HEAD commit
#   N        - number of commits dated on that same day
#   HASH     - 7-char hash of the HEAD commit
#
# Everything is derived from the HEAD commit, so the same commit always
# yields the same version regardless of when or where it is built.
#
# When git is unavailable (e.g. building from a source tarball) it falls
# back to the VERSION file if one is present, otherwise "unknown".
get_olx_version() {
	if command -v git >/dev/null 2>&1 && git rev-parse --git-dir >/dev/null 2>&1; then
		_olx_date="$(git show -s --format=%cd --date=format:%Y%m%d HEAD)"
		_olx_count="$(git log --format=%cd --date=format:%Y%m%d HEAD | grep -c "^${_olx_date}$")"
		_olx_hash="$(git rev-parse --short=7 HEAD)"
		echo "${_olx_date}.${_olx_count}+git.${_olx_hash}"
		return
	fi

	if [ -e VERSION ]; then
		cat VERSION
	else
		echo "unknown"
	fi
}

# builds a parameter string for a list of paths
#	$1 - parameter prefix (like "-I ")
#	$2 - list of paths/files
#	$3 - list of path-suffixes which will be added to each entry of first list
#	"$3" = "" => only $2 is used
# example: $* = -L "lib1 lib2 lib3"  =>  stdout = "-L lib1 -L lib2 -L lib3"
# example: $* = -I "p1 p2" "/SDL /libxml2"  =>  stdout = "-I p1/SDL -I p2/libxml"
# prints out the string on stdout
build_param_str() {
	for p in $2; do
		if [ "$3" = "" ]; then
			echo -n "$1 ${p} "
		else
			for a in $3; do
				echo -n "$1 ${p}${a} "
			done
		fi
	done
	echo ""
}

