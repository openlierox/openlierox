#!/usr/bin/env python3

# Simple script that will restard ded server if it hung
# You do not need that script on Windows - we have ded server installed as system service there

import time
import os
import sys
import signal

import subprocess

if sys.platform == "win32":
	import ctypes


olxPath = "openlierox.exe" # Modify here the path to OLX binary
TimeToSleep=60 # OLX and ded server will write something in logs once per 40 seconds

def startProcess():
	print("Starting " + olxPath)
	if sys.platform == "win32":
		proc = subprocess.Popen( [olxPath, olxPath, "-dedicated"] )
	else:
		proc = os.spawnvp( os.P_NOWAIT, "sh", ["sh", "-c", "./" + olxPath + " -dedicated > stdout.txt"] )
	return proc

def killProcess(proc):
	if sys.platform == "win32":
		# Doesn't work anyway
		ctypes.windll.kernel32.TerminateProcess( proc.pid, 1 )
	else:
		os.kill( proc, signal.SIGKILL )

proc = startProcess()
uptime = time.time()

def signalHandler(signum, frame):
	print("Signal caught - exiting")
	print("Killing process, uptime was %f hours" % ( ( time.time() - uptime ) / 3600 ))
	killProcess(proc)
	sys.exit()

signal.signal(signal.SIGTERM, signalHandler)
signal.signal(signal.SIGABRT, signalHandler)
signal.signal(signal.SIGINT, signalHandler)
if sys.platform != "win32":
	signal.signal(signal.SIGQUIT, signalHandler)

time.sleep(3)

fileSize1 = os.stat("stdout.txt").st_size
fileSize2 = os.stat("dedicated_control.log").st_size
time.sleep(TimeToSleep)

while True:

	#print("stdout.txt size %i was %i, dedicated_control.log size %i was %i" % \
	#	( os.stat("stdout.txt").st_size, fileSize1, os.stat("dedicated_control.log").st_size, fileSize2 ))
	if fileSize1 == os.stat("stdout.txt").st_size or fileSize2 == os.stat("dedicated_control.log").st_size:
		print("Killing process, uptime was %f hours" % ( ( time.time() - uptime ) / 3600 ))
		killProcess(proc)
		proc = startProcess()
		uptime = time.time()

	fileSize1 = os.stat("stdout.txt").st_size
	fileSize2 = os.stat("dedicated_control.log").st_size

	time.sleep(TimeToSleep)
