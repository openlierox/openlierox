#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys

if len(sys.argv) != 2:
	print("usage:", sys.argv[0], " <mail>")
	exit(1)

mail = open(sys.argv[1])
if not mail:
	print("cannot open", sys.argv[1])
	exit(1)

import os


symsdir = ""

import re

def findSymfile(module):
	fn = symsdir + "/" + module + ".x86.sym"
	if os.path.exists(fn):
		return fn
	return None

def findFunction(searchaddr, symfile):
	f = open(symfile)
	curfunc = None
	files = {}
	for line in f.readlines():
		line = line.strip("\n")

		file = re.match("FILE (?P<nr>[0-9]+) (?P<file>.*)", line)
		if file:
			files[int(file.group("nr"))] = os.path.basename(file.group("file").replace("\\", "/"))
			continue

		func = re.match("FUNC (?P<addr>[0-9a-f]+) (?P<size>[0-9a-f]+) [0-9a-f]+ (?P<func>.*)", line)
		if func:
			# if there was an earlier curfunc, we didn't find the exact line
			if curfunc: break

			addr = int(func.group("addr"), 16)
			size = int(func.group("size"), 16)

			# checks if we are already ahead
			if addr > searchaddr: break

			# check if out-of-range
			if addr + size <= searchaddr: continue

			# this is the right function!
			curfunc = func.group("func")
			continue

		if not curfunc: continue

		line = re.match("(?P<addr>[0-9a-f]+) (?P<size>[0-9a-f]+) (?P<line>[0-9]+) (?P<filenum>[0-9]+)", line)
		if not line: break

		addr = int(line.group("addr"), 16)
		size = int(line.group("size"), 16)

		# checks if we are already ahead
		if addr > searchaddr: break

		# check if out-of-range
		if addr + size < searchaddr: continue

		# this is the right line!
		return curfunc + " (%s:%s)" % (files[int(line.group("filenum"))], line.group("line"))

	return curfunc




inthread = False
for line in mail.readlines():
	line = line.strip("\n")
	if not inthread:
		print(line)
		subj = re.match(r"^Subject: (\[.*\])? OpenLieroX (?P<ver>.*) crash report", line)
		if subj:
			symsdir = subj.group("ver").replace(" ", "-")
			continue
		opsys = re.match(r"Operating system: (?P<os>\S+)", line)
		if opsys:
			symsdir = symsdir + "-" + opsys.group("os") + "-syms"
			if not os.path.exists(symsdir):
				print("cannot find symsdir", symsdir)
				quit(1)
		inthread = re.match(r"^Thread [0-9]+.*$", line)
	else:
		m = re.match(r"\s*(?P<tid>[0-9]+)\s+(?P<mod>\S+)\s+([0-9.]+\s*)?0x[0-9a-f]+\s*\(0x(?P<reladdr>[0-9a-f]+)\)\s*(?P<funcname>\S+)", line)
		if not m:
			print(line)
			inthread = re.match(r"\s*(?P<tid>[0-9]+)", line)
		else:
			fallbackstr =  "%s %s   %s %s" % ( m.group("tid").rjust(2), m.group("mod").ljust(20), ("(0x" + m.group("reladdr") + ")").rjust(8), "??")

			if m.group("funcname") == "??":
				symfile = findSymfile(m.group("mod"))
				if not symfile: print(fallbackstr); continue
				func = findFunction(int(m.group("reladdr"),16), symfile)
				if not func: print(fallbackstr); continue
				print("%s %s   %s %s" % ( m.group("tid").rjust(2), m.group("mod").ljust(20), ("(0x" + m.group("reladdr") + ")").rjust(8), func))
			else:
				print(fallbackstr)
