#!/usr/bin/python3

import sys, os
from glob import glob
from pprint import pprint

mydir = os.path.dirname(__file__) or "."
os.chdir(mydir + "/breakpad")

p = {
	"darwin": "mac",
	"linux": "linux",
	"win32": "windows",
	}[sys.platform]

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

files = list(filter(lambda fn: not has_main_func(fn), files))
files = list(filter(lambda fn: fn.find("_unittest.") < 0, files))
files = list(filter(lambda fn: fn.find("HTTPMultipartUpload.m") < 0, files))
files = list(filter(lambda fn: fn.find("crash_generation/ConfigFile.mm") < 0, files))
files = list(filter(lambda fn: fn.find("crash_generation/Inspector.mm") < 0, files))

if sys.argv[1:] == ["-debug"]:
	pprint(files)
	
else:
	for f in files:
		sys.stdout.write("libs/breakpad/" + f + ";")
	sys.stdout.write("\n")
