#!/bin/sh
#
# Launch OpenLieroX as a dedicated server (no graphics, no sound).
#
# Usage:
#   ./start_dedicated_server.sh                 # plain dedicated server
#   ./start_dedicated_server.sh -script <path>  # with a dedicated-control script
#   ./start_dedicated_server.sh -exec "<cmd>"   # run a console command on startup
#
# Any extra arguments are forwarded to the OpenLieroX binary.

cd "$(dirname "$0")"
cd share/gamedir

bin="/dev/null"
[ -x ../../$bin ] || bin=build/Xcode/build/Debug/OpenLieroX.app/Contents/MacOS/OpenLieroX
[ -x ../../$bin ] || bin=build/Xcode/build/Release/OpenLieroX.app/Contents/MacOS/OpenLieroX
[ -x ../../$bin ] || bin=bin/openlierox

if [ ! -x "../../$bin" ]; then
	echo "ERROR: OpenLieroX binary not found. Build it first (see README.md)." >&2
	exit 1
fi

exec ../../$bin -dedicated "$@"
