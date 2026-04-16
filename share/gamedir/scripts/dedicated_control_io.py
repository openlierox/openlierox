#!/usr/bin/env python2
# Dedicated Control handler script for OpenLieroX
# (http://openlierox.sourceforge.net)


# Needed for sleeping/pausing execution
import time
# Needed for directory access
import os
import sys
import traceback


def getRawResponse():
	global EmptySignalsCount
	ret = sys.stdin.readline().strip()
	if ret != "": 
		return ret
	else:
		sys.stderr.write("Dedicated_control: OLX terminated, exiting\n")
		sys.exit(1)

def getResponse():
	ret = []
	resp = getRawResponse()
	while resp != ".":
		if not resp.startswith(':'):
			sys.stderr.write("Dedicated_control: bad OLX dedicated response: " + resp + "\n")
		else:
			ret.append( resp[1:] )
		resp = getRawResponse()
	return ret


def SendCommand(cmd):
	print cmd
	return getResponse()

def getSignal():
	return SendCommand("nextsignal")

## Sending functions ##

# Set a server variable
def setvar(what, data):
	SendCommand( "setvar %s \"%s\"" % (str(what), str(data)) )

def setVar(what, data):
	setvar(what, data)

def setWormWeapons(wormid, weapon1, weapon2, weapon3, weapon4, weapon5):
	SendCommand( "setWormWeapons %d \"%s\" \"%s\" \"%s\" \"%s\" \"%s\"" % (wormid, weapon1, weapon2, weapon3, weapon4, weapon5) )

def listVars(prefix = ""):
	return SendCommand( "listVars %s" % (str(prefix)) )

# Use this to make the server quit
def Quit():
	SendCommand( "quit" )

# Use this to force the server into lobby - it will kick all connected worms and restart the server
def startLobby(port):
	if port:
		SendCommand( "startlobby " + str(port) )
	else:
		SendCommand( "startlobby" )

# start the game (weapon selections screen)
def startGame():
	msgs = SendCommand( "startgame" )
	if len(msgs) > 0:
		for m in msgs:
			messageLog(m, LOG_ERROR)
		return False
	else:
		return True

# Use this to force the server into lobby - it will abort current game but won't kick connected worms
def gotoLobby():
	SendCommand( "gotolobby" )

def addBot(worm = None):
	if worm:
		SendCommand( "addbot \"%s\"" % str(worm) )
	else:
		SendCommand( "addbot" )

def kickBot(msg = None):
	if msg:
		SendCommand( "kickbot \"%s\"" % str(msg) )
	else:
		SendCommand( "kickbot" )

def kickBots():
	SendCommand( "kickbots" )

# Suicides all local bots
def killBots():
	SendCommand( "killbots" )

# Both kick and ban uses the ingame identification code
# for kicking/banning.
def kickWorm(iID, reason = ""):
	if reason != "":
		SendCommand( "kickworm %i \"%s\"" % (int(iID), str(reason)) )
	else:
		SendCommand( "kickworm %i" % int(iID) )

def banWorm(iID, reason = ""):
	if reason != "":
		SendCommand( "banworm %i \"%s\"" % (int(iID), str(reason)) )
	else:
		SendCommand( "banworm %i" % int(iID) )

def muteWorm(iID):
	SendCommand( "muteworm " + str(iID) )

def setWormTeam_io(iID, team):
	SendCommand( "setwormteam " + str(iID) + " " + str(team) )


def setWormTeam(iID, team):
	import dedicated_control_handler as hnd
	if iID in hnd.worms.keys() and hnd.worms[iID].iID != -1:
		hnd.worms[iID].Team = team
		setWormTeam_io(iID, team)
	else:
		messageLog("Worm id %i invalid" % iID ,LOG_ADMIN)

def getWormTeam(iID):
	return int(SendCommand("getwormteam %i" % int(iID))[0])

def getNumberWormsInTeam(team):
	import dedicated_control_handler as hnd
	c = 0
	for w in hnd.worms.values():
		if getWormTeam( w.iID ) == team:
			c = c + 1
	return c

def getWormName(iID):
	return SendCommand("getwormname %i" % int(iID))[0]

def authorizeWorm(iID):
	SendCommand( "authorizeworm " + str(iID) )

def getWormList():
	return [int(w) for w in SendCommand( "getwormlist" )]

# Use this to get the list of all possible bots.
def getComputerWormList():
	return [int(w) for w in SendCommand( "getcomputerwormlist" )]

def getWormIP(iID):
	ret = SendCommand( "getwormip %i" % int(iID) )
	if len(ret) == 0:
		return "0.0.0.0"
	return ret[0]

def getWormLocationInfo(iID):
	ret = SendCommand( "getwormlocationinfo %i" % int(iID) )
	if len(ret) == 0:
		return "Unknown Location"
	return ret[0]

def getWormPing(iID):
	ret = SendCommand( "getwormping %i" % int(iID) )
	if len(ret) == 0:
		return 0
	return int(ret[0])

def getWormSkin(iID):
	ret = SendCommand( "getwormskin %i" % int(iID) )
	return ( int(ret[0]), ret[1].lower() )

def getWormVersion(iID):
	ret = SendCommand( "whoIs %i" % int(iID) )
	if len(ret) == 0:
		return "0.57_Beta1"
	for i in ret:
		if i.startswith("Version: "):
			return i.replace("Version: ", "", 1)
	return "0.57_Beta1"

def getVar(var):
	ret = SendCommand( "getvar %s" % var )
	if len(ret) == 0: # var does not exist
		return "" # TODO: or exception? 
	return ret[0]

def getVarHelp(var):
	ret = SendCommand( "getVarHelp %s" % var )
	if len(ret) == 0: # var does not exist
		return ""
	return ret[0]
	
def getGameType():
	return int(getVar("GameOptions.GameInfo.GameType"))

def isTeamGame():
	gameType = getGameType()
	return (gameType == 1) or (gameType == 4) or (gameType == 5) or (gameType == 7)

def getFullFileName(fn):
	return SendCommand( "getfullfilename \"%s\"" % fn )[0]

def getWriteFullFileName(fn):
	return SendCommand( "getwritefullfilename \"%s\"" % fn )[0]

def listMaps():
	return SendCommand("listmaps")

def listMods():
	return SendCommand("listmods")
	

# Use this to write to stdout (standard output)
def msg(string):
	SendCommand( "msg \"%s\"" % str(string) )

# Send a chat message
def chatMsg(string):
	SendCommand( "chatmsg \"%s\"" % str(string) )

# Send a private chat message
def privateMsg(iID, string):
	SendCommand( "privatemsg %i \"%s\"" % ( int(iID), str(string) ) )


#Log Severity
LOG_CRITICAL = 0 # For things that you REALLY need to exit for.
LOG_ERROR = 1
LOG_WARN = 2
LOG_INFO = 3
# Categories for specific things, perhaps put in a new file?
# These are not in direct relation to the script.
LOG_ADMIN = 4
LOG_USRCMD = 5

def messageLog(message,severity = LOG_INFO):
	# TODO: Allow setting what loglevels you want logged
	outline = time.strftime("%Y-%m-%d %H:%M:%S")
	# Don't clutter the strftime call
	outline += " -- "
	if severity == LOG_CRITICAL:
		outline += "CRITICAL"
	elif severity == LOG_ERROR:
		outline += "ERROR"
	elif severity == LOG_WARN:
		outline += "WARN"
	elif severity == LOG_INFO:
		outline += "INFO"
	elif severity == LOG_ADMIN: #Log to another file?
		outline += "ADMIN"
	elif severity == LOG_USRCMD: #Log to another file?
		outline += "USERCOMMAND"

	outline += " -- "
	outline += str(message) #incase we get anything other than string
	try:
		f = open(cfg.LOG_FILE,"a")
		f.write((outline + "\n"))
		f.close()
	except IOError:
		msg("ERROR: Unable to open logfile.")

	#It's possible that we get a broken pipe here, but we can't exit clearly and also display it,
	# so let python send out the ugly warning.
	msg(outline)

# Stolen from http://www.linuxjournal.com/article/5821
def formatExceptionInfo(maxTBlevel=5):
	cla, exc, trbk = sys.exc_info()
	excName = cla.__name__
	try:
		excArgs = exc.__dict__["args"]
	except KeyError:
		excArgs = "<no args>"
	excTb = traceback.format_tb(trbk, maxTBlevel)
	return (excName, excArgs, excTb)
