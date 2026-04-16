#!/usr/bin/env -S python2 -u
# Dedicated Control handler script for OpenLieroX
# (http://openlierox.sourceforge.net)

# TODO: move admin/user commands to different file

# Needed for sleeping/pausing execution
import time
# Needed for directory access
import os
import sys
import threading
import traceback
import random
import portalocker
import subprocess, signal, os.path
import distutils.version

import dedicated_control_io as io
setvar = io.setvar
formatExceptionInfo = io.formatExceptionInfo

import dedicated_control_ranking as ranking

import dedicated_control_usercommands as cmds


## Global vars
# Preset stuffies
availablePresets = []

worms = {} # List of all worms on the server
# Bots don't need to be itterated with the other ones.
bots = {}  # Dictionary of all possible bots

presetData = {}

# Function that controls ded server behavior
controlHandler = None

gameStartedHandler = None
newWormHandler = None
wormDiedHandler = None
wormSpawnedHandler = None

scriptPaused = False


# General options that should be set before server starts
GLOBAL_SETTINGS = {	

	# Various options that should be set, you don't need to touch them in most cases

	"GameOptions.GameInfo.ServerSideHealth":        0, # Turn this on if ppl hack and don't die on your server
	"GameOptions.GameInfo.AllowNickChange":         1,
	"GameOptions.GameInfo.AllowStrafing":           1,
	"GameOptions.Network.AllowRemoteBots":          1,
	"GameOptions.Network.AllowWantsJoinMsg":        1,
	"GameOptions.Network.WantsToJoinFromBanned":    0,
	"GameOptions.Network.UseIpToCountry":           1,
	"GameOptions.Network.RegisterServer":           1,
	"GameOptions.Network.Speed":                    2, # 2 = LAN, do not change
	"GameOptions.Advanced.MaxFPS":                  95, # Higher values will decrease netlag, also needed if ServerSideHealth = 1, 
	"GameOptions.Game.AntilagMovementPrediction":   1, # If ServerSideHealth = 1 this influences gameplay
	"GameOptions.Misc.LogConversations":            0,
	"GameOptions.Advanced.MatchLogging":            0, # Do not save game results screenshot
	"GameOptions.Misc.ScreenshotFormat":            1, # 0 = JPG, 1 = PNG
	"GameOptions.Network.EnableChat":               0, # No IRC chat needed for ded server
	"GameOptions.Network.AutoSetupHttpProxy":       0,
	"GameOptions.Network.HttpProxy":                "",
	"GameOptions.Advanced.MaxCachedEntries":        0, # Disable cache for ded server
}


# Uncomment following 3 lines to get lots of debug spam into dedicated_control_errors.log file
#def HeavyDebugTrace(frame,event,arg):
#	sys.stderr.write( 'Trace: ' + str(event) + ': ' + str(arg) + ' at ' + str(frame.f_code) + "\n" )
#sys.settrace(HeavyDebugTrace)

## Local vars
presetDir = os.path.join(os.path.dirname(sys.argv[0]),"presets")

# Game states
GAME_READY = -1
GAME_QUIT = 0
GAME_LOBBY = 1
GAME_WEAPONS = 2
GAME_PLAYING = 3

gameState = GAME_READY

sentStartGame = False

videoRecorder = None
videoRecorderSignalTime = 0

def DoNothing(): pass


# TODO: Expand this class, use it.
class Worm:
	def __init__(self):
		self.Name = ""
		self.Continent = ""
		self.CountryShortcut = ""
		self.Country = ""
		self.Ip = ""
		self.iID = -1
		self.isAdmin = False
		self.isDedAdmin = False
		self.Ping = [] # Contains 25 ping values, real ping = average
		self.Lives = -1 # -1 means Out
		self.Team = 0
		self.Alive = False
		self.Voted = 0
		self.FailedVoteTime = 0

	def clear(self):
		self.__init__()

def init():
	initPresets()

	io.startLobby(cfg.SERVER_PORT)

	# if we load this script with already some worms on board, we have to update our worm list now
	for w in io.getWormList():
		parseNewWorm( w, io.getWormName(w) )

	global GLOBAL_SETTINGS;
	for f in GLOBAL_SETTINGS.keys():
		io.setvar( f, GLOBAL_SETTINGS[f] )
	for f in cfg.GLOBAL_SETTINGS.keys():
		io.setvar( f, cfg.GLOBAL_SETTINGS[f] )
	
	if io.getVar("GameOptions.GameInfo.AllowEmptyGames") == "false" and cfg.MIN_PLAYERS < 2:
		io.messageLog("GameOptions.GameInfo.AllowEmptyGames is false - setting cfg.MIN_PLAYERS to 2", io.LOG_WARN)
		cfg.MIN_PLAYERS = 2
		


## High-level processing ##

# Parses all signals that are not 2 way (like getip info -> olx returns info)
# Returns False if there's nothing to read
def signalHandler(sig):
	global gameState, oldGameState, scriptPaused, sentStartGame, worms

	oldGameState = gameState

	if len(sig) == 0:
		return False #Didn't get anything
		
	header = sig[0]
	
	try:
		if header == "newworm":
			parseNewWorm(int(sig[1]), sig[2])
		elif header == "wormleft":
			parseWormLeft(sig)
		elif header == "privatemessage":
			parsePrivateMessage(sig)
		elif header == "chatmessage":
			parseChatMessage(sig)
		elif header == "wormdied":
			parseWormDied(sig)
		elif header == "wormspawned":
			parseWormSpawned(sig)
		elif header == "wormauthorized":
			parseWormAuthorized(sig)
		elif header == "wormgotadmin":
			worms[int(sig[1])].isDedAdmin = True
			# no other difference to wormauthorized yet - and we also get wormauthorized, so nothing to do anymore
			pass
			
		## Check GameState ##
		elif header == "quit":
			gameState = GAME_QUIT
			exit()
	
		elif header == "backtolobby" or header == "lobbystarted":
			if cfg.RANKING:
				ranking.refreshRank()
			gameState = GAME_LOBBY
			sentStartGame = False
			controlHandler()
	
		elif header == "weaponselections":
			gameState = GAME_WEAPONS
			controlHandler()
		elif header == "gamestarted":
			gameState = GAME_PLAYING
			sentStartGame = False
			controlHandler()
			if gameStartedHandler != None:
				gameStartedHandler()
		#TODO: gamestarted && gameloopstart are pretty much duplicates
		# Or are they? Check.
		# Same thing for gameloopend and backtolobby
		elif header == "gameloopstart": #Sent when game starts
			pass
		elif header == "gameloopend": #Sent at game end
			pass
		elif header == "gameloopend": #Sent when OLX starts
			pass
		elif header == "timer": # Sent once per second
			controlHandler()
			
		elif header == "custom":
			parseCustom(sig)
			
		else:
			io.messageLog(("I don't understand %s." % (sig)),io.LOG_ERROR)
	
	except Exception:
		traceback.print_exc(None, sys.stderr)

	return True

def parseNewWorm(wormID, name):
	global worms, newWormHandler

	name = name.replace("\t", " ").strip() # Do not allow tab in names, it will screw up our ranking tab-separated text-file database
	exists = False
	try:
		worm = worms[wormID]
		exists = True
	except KeyError: #Worm doesn't exist.
		worm = Worm()
	worm.Name = name
	worm.iID = wormID
	worm.Ping = []

	worms[wormID] = worm

	if io.getGameType() == 4: # Hide and Seek
		minSeekers = 1
		if len(worms.values()) >= 4: minSeekers = 2
		if io.getNumberWormsInTeam(1) < minSeekers:
			io.setWormTeam(wormID, 1) # Seeker
		else:
			io.setWormTeam(wormID, 0) # Hider		
	else:
		# Balance teams
		teams = [0,0,0,0]
		for w in worms.keys():
			teams[worms[w].Team] += 1
		minTeam = 0
		minTeamCount = teams[0]
		for f in range(cfg.MAX_TEAMS):
			if minTeamCount > teams[f]:
				minTeamCount = teams[f]
				minTeam = f

		io.setWormTeam(wormID, minTeam)

	if cfg.RANKING_AUTHENTICATION:
		if not name in ranking.auth:
			ranking.auth[name] = getWormSkin(wormID)
			try:
				f = open(io.getFullFileName("pwn0meter_auth.txt"),"r")
				try:
					portalocker.lock(f, portalocker.LOCK_EX)
				except:
					pass
				f.write( name + "\t" + str(ranking.auth[name][0]) + " " + ranking.auth[name][1] + "\n" )
				f.close()
			except IOError:
				msg("ERROR: Unable to open pwn0meter_auth.txt")
		else:
			if ranking.auth[name] != getWormSkin(wormID):
				io.kickWorm(wormID, "Player with name %s already registered" % name)

	wormIP = io.getWormIP(wormID).split(":")[0]
	# io.messageLog("Curtime " + str(time.time()) + " IP " + str(wormIP) + " Kicked worms: " + str(cmds.kickedUsers), io.LOG_INFO)
	if wormIP in cmds.kickedUsers and cmds.kickedUsers[ wormIP ] > time.time():
			io.kickWorm( wormID, "You can join in " + str(int(cmds.kickedUsers[ wormIP ] - time.time())/60 + 1) + " minutes" )
			return
	cmds.recheckVote()

	ver = io.getWormVersion(wormID)
	if distutils.version.LooseVersion(ver.lower()) < distutils.version.LooseVersion("OpenLieroX/0.58_rc5".lower()):
		io.privateMsg(wormID, "Download OpenLieroX 0.58 rc5 at http://openlierox.net/")

	if newWormHandler != None:
		newWormHandler(wormID, name)


def parseWormLeft(sig):
	global worms, scriptPaused

	wormID = int(sig[1])
	name = sig[2:]

	try:
		if worms[wormID].isAdmin:
			io.messageLog(("Worm %i (%s) removed from admins" % (wormID,name)),io.LOG_ADMIN)
	except KeyError:
		io.messageLog("AdminRemove: Our local copy of wormses doesn't match the real list.",io.LOG_ERROR)

	# Call last, that way we still have the data active.
	worms.pop(wormID)

	cmds.recheckVote()

	# If all admins left unpause ded server (or it will be unusable)
	isAdmins = False
	for w in worms.keys():
		if worms[w].isAdmin:
			isAdmins = True
	if not isAdmins:
		scriptPaused = False


def parsePrivateMessage(sig):
	pass
	
def parseWormAuthorized(sig):	
	global worms

	wormID = int(sig[1])
	try:
		if not worms[wormID].isAdmin:
			worms[wormID].isAdmin = True
			io.messageLog(("Worm %i (%s) added to admins" % (wormID,worms[wormID].Name)),io.LOG_ADMIN)
			# TODO: Send the last part in a PM to the admin. (Needs new backend for private messaging. Add teamchat too!)
			io.authorizeWorm(wormID)
			io.privateMsg(wormID, "%s authenticated for admin! Type %shelp for command info" % (worms[wormID].Name,cfg.ADMIN_PREFIX))
	except KeyError:
		io.messageLog("AdminAdd: Our local copy of wormses doesn't match the real list.",io.LOG_ERROR)

def parseChatMessage(sig):
	global worms

	wormID = int(sig[1])
	message = sig[2]
	#io.msg("Chat msg from worm %i: %s" % (wormID, message))
	if worms[wormID].isAdmin:
		if not cmds.parseAdminCommand(wormID,message):
			cmds.parseUserCommand(wormID,message)
	else:
		cmds.parseUserCommand(wormID,message)

def parseWormDied(sig):
	global worms, wormDiedHandler

	deaderID = int(sig[1])
	killerID = int(sig[2])
	worms[deaderID].Lives -= 1
	worms[deaderID].Alive = False

	if wormDiedHandler != None:
		wormDiedHandler(deaderID, killerID)

	if not cfg.RANKING:
		return

	try:
		f = open(io.getWriteFullFileName("pwn0meter.txt"),"a")
		if not killerID in io.getComputerWormList():
			try:
				portalocker.lock(f, portalocker.LOCK_EX)
			except:
				pass
			f.write( time.strftime("%Y-%m-%d %H:%M:%S") + "\t" + worms[deaderID].Name + "\t" + worms[killerID].Name + "\n" )
		f.close()
	except IOError:
		io.msg("ERROR: Unable to open pwn0meter.txt")

	if not killerID in io.getComputerWormList():
		if deaderID == killerID:
			try:
				ranking.rank[worms[killerID].Name][2] += 1
			except KeyError:
				ranking.rank[worms[killerID].Name] = [0,0,1,len(ranking.rank)+1]
		else:
			try:
				ranking.rank[worms[killerID].Name][0] += 1
			except KeyError:
				ranking.rank[worms[killerID].Name] = [1,0,0,len(ranking.rank)+1]
	if not deaderID in io.getComputerWormList():
		try:
			ranking.rank[worms[deaderID].Name][1] += 1
		except KeyError:
			ranking.rank[worms[deaderID].Name] = [0,1,0,len(ranking.rank)+1]

def parseWormSpawned(sig):
	global worms, wormSpawnedHandler

	wormID = int(sig[1])
	worms[wormID].Alive = True

	if wormSpawnedHandler != None:
		wormSpawnedHandler(wormID)

def parseCustom(sig):
	if not cmds.parseAdminCommand(-1, "%s%s" % (cfg.ADMIN_PREFIX, str(sig[1:]))):
		cmds.parseUserCommand(-1, "%s%s" % (cfg.USER_PREFIX, str(sig[1:])))




## Preset loading functions ##
def initPresets():
	global availablePresets

	# Reset - incase we get called a second time
	availablePresets = []

	for f in os.listdir(presetDir):
		if f.lower() != "defaults" and f.lower() != ".svn":
			availablePresets.append(f)

	#for p in cfg.PRESETS:
	#	if availablePresets.count(p) == 0:
	#		availablePresets.append(p)

	availablePresets.sort()

	if (len(availablePresets) == 0):
		io.messageLog("There are no presets available - nothing to do. Exiting.",io.LOG_CRITICAL)
		exit()


## Control functions

def average(a):
	r = 0
	for i in a:
		r += i
	return r / len(a)

def checkMaxPing():
	
	global worms
	for f in worms.keys():
		if worms[f].iID == -1 or not worms[f].Alive:
			continue
		ping = int(io.getWormPing(worms[f].iID))
		if ping > 0:
			worms[f].Ping.insert( 0, ping )
			if len(worms[f].Ping) > 25:
				worms[f].Ping.pop()
				if average(worms[f].Ping) > cfg.MAX_PING:
					io.kickWorm( worms[f].iID, "Your ping is " + str(average(worms[f].Ping)) + " allowed is " + str(cfg.MAX_PING) )



class StandardCiclerBase: #TODO: it's cycler not cicler lol
	def __init__(self):
		self.list = []
		self.preSelectedList = []
		self.curIndex = 0
		self.curSelection = None

	def cicle(self, remove = True):
		#io.messageLog(("%s.cicle(): preSelectedList %s" % (str(self.__class__), str(self.preSelectedList))),io.LOG_WARN)
		if len(self.preSelectedList) > 0:
			self.curSelection = self.preSelectedList[0]
			if remove:
				self.preSelectedList.pop(0)
			self.apply()
			return

		if len(self.list) == 0: return
		self.curSelection = self.list[self.curIndex]
		self.apply()
		if remove:
			self.curIndex = self.curIndex + 1
			if self.curIndex >= len(self.list):
				self.curIndex = 0
				random.shuffle(self.list)

	def apply(self):
		if not self.curSelection: return
		#io.messageLog(("%s.apply(): curSelection %s" % (str(self.__class__), str(self.curSelection))),io.LOG_WARN)
	
	def pushSelection(self, selection):
		self.preSelectedList.insert(0, selection)
		while len(self.preSelectedList) > cfg.VOTING_QUEUE_SIZE:
			self.preSelectedList.pop(len(self.preSelectedList) - 1)
		global gameState
		if gameState == GAME_LOBBY: # Game not started yet - force selection immediately
			self.cicle(remove = False)
		#io.messageLog(("%s.pushSelection(%s): preSelectedList %s" % (str(self.__class__), str(selection), str(self.preSelectedList))),io.LOG_WARN)

	def describe(self, idx):
		#if idx < 0:
		#	return self.curSelection if self.curSelection else ""
		if idx >= 0 and idx < len(self.preSelectedList):
			return self.preSelectedList[idx]
		return ""


class StandardCiclerGameVar(StandardCiclerBase):
	def __init__(self):
		StandardCiclerBase.__init__(self)
		self.gameVar = ""
	
	def apply(self):
		if not self.curSelection: return
		StandardCiclerBase.apply(self)
		io.setvar(self.gameVar, self.curSelection)

class MapCicler(StandardCiclerGameVar):
	def __init__(self):
		StandardCiclerGameVar.__init__(self)
		self.gameVar = "GameOptions.GameInfo.LevelName"
	
	def cicle(self, remove = True):
		global worms
		oldlist = self.list

		#self.list = io.listMaps()

		if len(worms) <= cfg.MAX_PLAYERS_SMALL_LEVELS:
			self.list = cfg.SMALL_LEVELS
		else:
			self.list = cfg.LEVELS
		

		if self.list != oldlist:
			self.curIndex = 0
			random.shuffle(self.list)
		
		StandardCiclerGameVar.cicle(self, remove)

	def describe(self, idx):
		r = StandardCiclerGameVar.describe(self, idx)
		return r.replace(".lxl", "") if r != "" else "random map"

mapCicler = MapCicler()
mapCicler.cicle()

def SetWeaponBans(name = "Standard 100lt"):
	modName = io.getVar("GameOptions.GameInfo.ModName")
	io.setvar( "GameServer.WeaponRestrictionsFile", "cfg/presets/" + modName + "/" + name + ".wps" )

class ModCicler(StandardCiclerGameVar):
	def __init__(self):
		StandardCiclerGameVar.__init__(self)
		self.gameVar = "GameOptions.GameInfo.ModName"
	
	def apply(self):
		global availablePresets, presetDir
		if not self.curSelection: return

		sDefaults = os.path.join(presetDir,"Defaults")
		try:
			execfile(sDefaults)
		except:
			io.messageLog("Error in preset: " + str(formatExceptionInfo()),io.LOG_ERROR)

		sFile = os.path.join(presetDir,self.curSelection)

		if os.path.exists(sFile):
			StandardCiclerBase.apply(self)
			try:
				fPreset = file(sFile,"r")
				line = fPreset.readline()
				if line.find("python") != -1:
					fPreset.close()
					execfile(sFile)
				else:
					print line.strip().replace('"','')
					for line in fPreset.readlines():
						print line.strip().replace('"','')
					fPreset.close()
			except IOError:
				# File does not exist, perhaps it was removed.
				io.messageLog(("Unable to load %s, forcing rehash of all presets" % sFile),io.LOG_WARN)
				initPresets()
			except:
				io.messageLog("Error in preset: " + str(formatExceptionInfo()),io.LOG_ERROR)
		else:
			StandardCiclerGameVar.apply(self)
			SetWeaponBans()

	def describe(self, idx):
		r = StandardCiclerGameVar.describe(self, idx)
		return r if r != "" else "Random"

modCicler = ModCicler()
modCicler.list = cfg.PRESETS
if len(modCicler.list) == 0:
	modCicler.list = availablePresets
if len(modCicler.list) == 0:
	io.messageLog("Waiting for mod list ...")
	while len(modCicler.list) == 0:
		modCicler.list = io.listMods()


class GameVarCicler(StandardCiclerBase):
	def __init__(self):
		StandardCiclerBase.__init__(self)
		self.gameCount = -1
		self.savedList = []
		self.restoreDefaults = []

	def cicle(self, remove = True):
		if self.gameCount >= 0:
			if remove:
				self.gameCount -= 1
			if self.gameCount < 0:
				self.preSelectedList = self.restoreDefaults
				self.restoreDefaults = []
				self.savedList = []

		while True:
			StandardCiclerBase.cicle(self)
			if len(self.preSelectedList) <= 0:
				self.preSelectedList = list(self.savedList)
				return

	def apply(self):
		if not self.curSelection: return
		io.setvar(self.curSelection[0], self.curSelection[1])

	def pushSelection(self, selection, gameCount = cfg.VOTING_QUEUE_SIZE):
		defaultValue = io.getVar(selection[0])
		defaultSet = False
		for (v, d) in self.restoreDefaults:
			if v == selection[0]:
				defaultSet = True
		if not defaultSet:
			self.restoreDefaults.insert(len(self.restoreDefaults), (selection[0], defaultValue))
		else:
			for i in range(len(self.preSelectedList)):
				if selection[0] == self.preSelectedList[i][0]:
					self.preSelectedList.pop(i)
					break

		self.preSelectedList.append(selection)
		self.savedList = list(self.preSelectedList)
		self.gameCount = gameCount
		global gameState
		if gameState == GAME_LOBBY: # Game not started yet - force selection immediately
			self.cicle(remove = False)

	def describe(self, idx):
		if idx >= self.gameCount or idx < 0:
			return ""
		s = ""
		for r in self.preSelectedList:
			if s != "":
				s += " "
			if str(r[0]) == "GameOptions.GameInfo.GameType":
				gametypes = ["DM", "TeamDM", "Tag", "Demolition", "Hide&Seek", "CTF", "Race", "TeamRace"]
				try:
					s += "GameType=" + gametypes[int(r[1])]
				except:
					pass
			else:
				s += str(r[0]).replace("GameOptions.GameInfo.", "") + "=" + str(r[1])
		return s

gameVarCicler = GameVarCicler()

def randomizeTeams():
	global worms
	for w in worms.values():
		io.setWormTeam( w.iID, 0 )
	for f in range(int(len(worms)/2)):
		red = random.choice(worms.values()).iID
		if io.getWormTeam( red ) == 1: # Not "while" to avoid infinite loop
			red = random.choice(worms.values()).iID
		io.setWormTeam( red, 1 )

def describeScheduledPresets():
	global modCicler, mapCicler, gameVarCicler
	s = ""
	for i in range(0, cfg.VOTING_QUEUE_SIZE):
		d = modCicler.describe(i) + " on " + mapCicler.describe(i)
		v = gameVarCicler.describe(i)
		if v != "":
			d += " with " + v
		if d == "Random on random map":
			break
		if s != "":
			s += ", "
		s += d
	return s

def selectPreset( Mod = None, Level = None, VarName = None, VarValue = None, VarGameCount = cfg.VOTING_QUEUE_SIZE ):
	global modCicler, mapCicler, gameVarCicler

	#io.messageLog(("selectPreset(): Preset %s Level %s Mod %s" % (str(Preset), str(Level), str(Mod))),io.LOG_WARN)

	if Mod:
		modCicler.pushSelection(Mod)
	if Level:
		mapCicler.pushSelection(Level)
	if VarName and VarValue and VarGameCount:
		gameVarCicler.pushSelection((VarName, VarValue), gameCount = VarGameCount)

	io.chatMsg( "Presets: " + describeScheduledPresets() )

lobbyWaitBeforeGame = time.time() + cfg.WAIT_BEFORE_GAME
lobbyWaitAfterGame = time.time()
lobbyWaitGeneral = time.time() + cfg.WAIT_BEFORE_SPAMMING_TOO_FEW_PLAYERS_MESSAGE
lobbyEnoughPlayers = False
oldGameState = GAME_LOBBY
printVoteDuringGameTime = 0

def controlHandlerDefault():

	global worms, gameState, lobbyChangePresetTimeout, lobbyWaitBeforeGame, lobbyWaitAfterGame
	global lobbyWaitGeneral, lobbyEnoughPlayers, oldGameState, scriptPaused, sentStartGame, gameStartedHandler
	global modCicler, mapCicler, gameVarCicler, printVoteDuringGameTime
	global videoRecorder, videoRecorderSignalTime
	
	if scriptPaused:
		return

	curTime = time.time()
	cmds.recheckVote(False)
	
	if gameState == GAME_LOBBY:

		# Do not check ping in lobby - it's wrong

		if oldGameState != GAME_LOBBY:
			modCicler.cicle(remove = False)
			mapCicler.cicle(remove = False)
			gameVarCicler.cicle(remove = False)
			presetsMsg = describeScheduledPresets()
			if presetsMsg != "":
				io.chatMsg("Presets: " + presetsMsg)
			lobbyEnoughPlayers = False # reset the state
			lobbyWaitGeneral = curTime + cfg.WAIT_BEFORE_SPAMMING_TOO_FEW_PLAYERS_MESSAGE
			lobbyWaitAfterGame = curTime
			if oldGameState == GAME_PLAYING:
				lobbyWaitAfterGame = curTime + cfg.WAIT_AFTER_GAME
			cmds.increaseVoteTime()
			cmds.recheckVote()
			if videoRecorder:
				os.kill(videoRecorder.pid, signal.SIGINT)  # videoRecorder.send_signal(signal.SIGINT) # This is available only on Python 2.6
				videoRecorderSignalTime = time.time()
				io.chatMsg("Waiting for video recorder to finish")

		canStart = True
		if videoRecorder and videoRecorder.returncode == None:
			canStart = False
			videoRecorder.poll()
			if time.time() - videoRecorderSignalTime > cfg.TIME_TO_KILL_VIDEORECORDER:
				io.chatMsg("Video recorder stalled, killing")
				os.kill(videoRecorder.pid, signal.SIGKILL)
				videoRecorder.poll()
			if videoRecorder.returncode != None:
				io.chatMsg("Video recorder encoding took " + str(int(time.time() - videoRecorderSignalTime)) + " secs")
				canStart = True
				videoRecorder = None

		if lobbyWaitAfterGame <= curTime:

			if len(worms) < cfg.MIN_PLAYERS and lobbyWaitGeneral <= curTime:
				lobbyWaitGeneral = curTime + cfg.WAIT_BEFORE_SPAMMING_TOO_FEW_PLAYERS_MESSAGE
				io.chatMsg(cfg.TOO_FEW_PLAYERS_MESSAGE)

			if not lobbyEnoughPlayers and len(worms) >= cfg.MIN_PLAYERS: # Enough players already - start game
				lobbyEnoughPlayers = True
				io.chatMsg(cfg.WAIT_BEFORE_GAME_MESSAGE)
				lobbyWaitBeforeGame = curTime + cfg.WAIT_BEFORE_GAME

			if lobbyEnoughPlayers and len(worms) < cfg.MIN_PLAYERS: # Some players left when game not yet started
				lobbyEnoughPlayers = False
				io.chatMsg(cfg.TOO_FEW_PLAYERS_MESSAGE)

			if lobbyEnoughPlayers and not sentStartGame:

				if lobbyWaitBeforeGame <= curTime and canStart: # Start the game

					modCicler.cicle(remove = True)
					mapCicler.cicle(remove = True)
					gameVarCicler.cicle(remove = True)

					if io.isTeamGame():
						if not cfg.ALLOW_TEAM_CHANGE:
							counter = 0
							for w in worms.values():
								if w.iID != -1:
									io.setWormTeam( w.iID, counter % cfg.MAX_TEAMS )
									counter += 1

					if io.startGame():
						if cfg.ALLOW_TEAM_CHANGE and io.isTeamGame():
							io.chatMsg(cfg.TEAM_CHANGE_MESSAGE)
						sentStartGame = True
						if cfg.RECORD_VIDEO:
							try:
								#io.messageLog("Running dedicated-video-record.sh, curdir " + os.path.abspath(os.path.curdir) ,io.LOG_INFO)
								videoRecorder = subprocess.Popen( ["./dedicated-video-record.sh", "./dedicated-video-record.sh"],
												stdin=open("/dev/null","r"), stdout=open("../../../dedicatedVideo.log","w"),
												stderr=subprocess.STDOUT, cwd=".." )
							except:
								io.messageLog(formatExceptionInfo(),io.LOG_ERROR)
					else:
						io.chatMsg("Game could not be started")
						oldGameState == GAME_PLAYING # hack that it resets at next control handler call
					
	if gameState == GAME_WEAPONS:

		#checkMaxPing()

		# if we allow empty games, ignore this check
		if len(worms) < cfg.MIN_PLAYERS and not io.getVar("GameOptions.GameInfo.AllowEmptyGames"): # Some players left when game not yet started
			io.chatMsg("Too less players -> back to lobby")
			io.gotoLobby()
			sentStartGame = False

		if oldGameState != gameState:
			cmds.increaseVoteTime()

	if gameState == GAME_PLAYING:

		checkMaxPing()

		if oldGameState != gameState:
			cmds.increaseVoteTime()

		if printVoteDuringGameTime < time.time():
			printVoteDuringGameTime = time.time() + 7
			cmds.recheckVote()

	oldGameState = gameState

controlHandler = controlHandlerDefault

