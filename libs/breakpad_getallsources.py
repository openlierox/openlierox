#!/usr/bin/env python3

import sys, os
from glob import glob
from pprint import pprint

mydir = os.path.dirname(__file__) or "."
os.chdir(mydir + "/breakpad")

# In Python 3, sys.platform on Linux is "linux" (no "2" suffix).
_platMap = {
	"darwin": "mac",
	"linux": "linux",
	"linux2": "linux",
	"win32": "windows",
}
p = _platMap[sys.platform]

dirs = [
	"third_party/libdisasm",
	"client",
	"client/" + p,
	"client/" + p + "/handler",
	"client/" + p + "/crash_generation",
	"common",
	"common/" + p,
	"common/dwarf",
	"processor"
	]

files = []

for d in dirs:
	for ext in ["c","cc","m","mm"]:
		files += glob("src/" + d + "/*." + ext)

def has_main_func(fn):
	return open(fn).read().find("int main") >= 0

files = [fn for fn in files if not has_main_func(fn)]
files = [fn for fn in files if fn.find("_unittest.") < 0]
files = [fn for fn in files if fn.find("HTTPMultipartUpload.m") < 0]
files = [fn for fn in files if fn.find("crash_generation/ConfigFile.mm") < 0]
files = [fn for fn in files if fn.find("crash_generation/Inspector.mm") < 0]

if sys.argv[1:] == ["-debug"]:
	pprint(files)

else:
	for f in files:
		sys.stdout.write("libs/breakpad/" + f + ";")
	sys.stdout.write("\n")
