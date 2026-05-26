#!/usr/bin/env python3 -u

import sys, os, re
import subprocess

if len(sys.argv) < 2:
	print("usage:", sys.argv[0], "<script>")
	exit(1)

script = sys.argv[1]
proc = subprocess.Popen(
	script,
	shell=True,
	stdin=subprocess.PIPE,
	stdout=subprocess.PIPE,
	universal_newlines=True,
	bufsize=1,
)
sin, sout = proc.stdin, proc.stdout


def olxdir():
	try:
		from win32com.shell import shellcon, shell
		homedir = shell.SHGetFolderPath(0, shellcon.CSIDL_MYDOCUMENTS, 0, 0)

	except ImportError:
		homedir = os.path.expanduser("~")

	if os.name == "nt":
		p = "OpenLieroX"
	elif os.name == "mac":
		p = "Library/Application Support/OpenLieroX"
	else:
		p = ".OpenLieroX"

	return homedir + "/" + p


def getwritefullfilename(fn):
	return olxdir() + "/" + fn


def getvar(var):
	var = var.lower()

	if var == "gameoptions.network.forceminversion":
		return "OpenLieroX/0.58_rc1"

	# no special handling yet; assume its a string and return empty
	return ""


def handle(cmd, params):
	if cmd == "getwritefullfilename" or cmd == "getfullfilename":
		return getwritefullfilename(params[0])

	if cmd == "listmaps":
		# some scripts need some output here
		return ["CastleStrike.lxl"]

	if cmd == "getvar":
		return [ getvar(params[0]) ]

	if cmd == "nextsignal":
		while True:
			ret = re.findall(r"[^ \t\"]+", input("Enter signal: ").strip())
			if len(ret) > 0: break
		return ret

	# unknown/not handled, just return empty list
	return []


while True:
	l = sout.readline().strip()
	print("Script:", l)
	cmd = re.findall(r"[^ \t\"]+", l)

	if len(cmd) > 0:
		ret = []
		try:
			ret = handle(cmd[0].lower(), cmd[1:])
		except Exception:
			print("Error while handling", cmd)
			print(sys.exc_info())

		for rl in ret:
			sin.write(":" + str(rl) + "\n")
		sin.write(".\n")
		sin.flush()

	else:
		break
