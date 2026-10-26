#!/usr/bin/python3 -u
#  _______                     _____        __               _               
# |__   __|                   |  __ \      / _|             | |              
#    | | ___  __ _ _ __ ___   | |  | | ___| |_ ___ _ __   __| | ___ _ __ ___ 
#    | |/ _ \/ _` | '_ ` _ \  | |  | |/ _ \  _/ _ \ '_ \ / _` |/ _ \ '__/ __|
#    | |  __/ (_| | | | | | | | |__| |  __/ ||  __/ | | | (_| |  __/ |  \__ \
#    |_|\___|\__,_|_| |_| |_| |_____/ \___|_| \___|_| |_|\__,_|\___|_|  |___/
#                                                                                                                                                        
# This is a dedicated server script that uses the Army Factory tool.
# The human players fight in team against teams of bots.
#========= TODOS ==========================================
# * There is a division by zero bug in most of the army_patterns.

#========= Imports ========================================
import sys
import os
sys.path.append( os.path.dirname(sys.argv[0]) ) # Add the the folder this file is in to the system path.
import time
import random
import re
import traceback
import tools.commands as cmd
import tools.army_architect as army_architect

#========= Configuration ==================================

GAME_OPTIONS = {	
	"GameOptions.GameInfo.GameType":                "Team Death Match",
	"GameOptions.GameInfo.FullAimAngle":            1,
	"GameOptions.GameInfo.LoadingTime":             10,
	"GameOptions.GameInfo.Lives":                   -1,
	"GameOptions.GameInfo.KillLimit":               -1,
	"GameOptions.GameInfo.ModName":                 "Liero v2.0",
	"GameOptions.Network.ServerName":               "Team Defenders Beta",
	"GameOptions.Network.WelcomeMessage":           "Welcome <player>! Your team's mission is to defend this land from the invading enemies. USE LASER!! It is the only weapon working properly due to a bug.",
	"GameOptions.Server.MaxPlayers":                32,
	"GameOptions.Server.AllowConnectDuringGame":    1,
	"GameOptions.GameInfo.AllowEmptyGames":         1,
	"GameOptions.GameInfo.ImmediateStart":          1,
	"GameOptions.GameInfo.TimeLimit":               5,
	"GameOptions.GameInfo.SelfHit":                 0,
	"GameOptions.GameInfo.SelfInjure":              0,
	"GameOptions.GameInfo.TeamHit":                 0,
	"GameOptions.GameInfo.TeamInjure":              0,
	"GameOptions.Server.WeaponSelectionMaxTime":    60,
	"GameOptions.GameInfo.CTF_AllowRopeForCarrier": 0,
	"GameOptions.GameInfo.RelativeAirJump":         0, # SorZ said it's lame
	"GameOptions.GameInfo.InfiniteMap":             0, # Infinite map is still broken and the rope is warping
	# Various options that should be set, you don't need to touch them in most cases
	"GameOptions.Server.ServerSideHealth":          0, # Turn this on if ppl hack and don't die on your server
	"GameOptions.Server.AllowNickChange":           1,
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

DEFENDER_TEAM = 0 # Blue
ENEMY_TEAM = 1 # Red
LOBBY_COUNTDOWN_SECONDS = 15
MIN_PLAYERS_TO_START_GAME = 1
lobby_countdown_started = False
lobby_countdown_left = LOBBY_COUNTDOWN_SECONDS

#========= Initializations ================================

# Find a list of all available maps.
all_available_maps = []
while len(all_available_maps) == 0:
	# We repeat this because right at OLX startup, it may return an empty list.
	all_available_maps = cmd.listMaps()
	time.sleep(1)

if float(re.match(r"([A-Za-z]+/)?(?P<num>[0-9]+\.[0-9]+).*", cmd.getVar("GameOptions.Network.ForceMinVersion")[0]).group("num")) + 0.001 < 0.59:
	cmd.msg("LX levels only")
	all_available_maps = [ x for x in all_available_maps if x.endswith(".lxl") ]
else:
	cmd.msg("all levels")


	
#========= Definitions ====================================
def init_server():
	"""This function sets the the default game options and starts the server."""
	global GAME_OPTIONS;
	
	#Set the game options
	for key, value in GAME_OPTIONS.items():
		cmd.setVar( key, value )
	
	cmd.startLobby(0) # This will start the server. 0 Means default port.

def try_start_lobby_countdown():
	global lobby_countdown_started, lobby_countdown_left
	if lobby_countdown_started == False and cmd.getGameState()[0] == "S_SVRLOBBY" and len(cmd.getWormList()) >= MIN_PLAYERS_TO_START_GAME:
		lobby_countdown_started = True
		lobby_countdown_left = LOBBY_COUNTDOWN_SECONDS
		cmd.chatMsg("The game will start in %i seconds" % LOBBY_COUNTDOWN_SECONDS)
		return True
	return False

def signal_handler(signal):
	global lobby_countdown_started, lobby_countdown_left, invading_army
	
	if signal[0] == "newworm":
		if not worm_is_local(signal[1]): # This player is from outside the the dedicated server
			cmd.setWormTeam(signal[1], DEFENDER_TEAM) # Put it in the defender team.
		try_start_lobby_countdown() # Perhaps we are enough players to start the game now!

	if signal[0] == "wormdied":
		pass
		# If the worm is a local enemy worm and it is out of lives, kick it.
		#if worm_is_local(signal[1]) and cmd.getWormLives(signal[1])[0] == "-1":
		#	cmd.kickWorm(signal[1])
		# Now the game could be over. If it is; we would like to view the scoreboard...
		
	if signal[0] == "quit":
		exit()
	
	if signal[0] == "timer":
		if lobby_countdown_started:
			lobby_countdown_left -= 1
			
			if lobby_countdown_left < 1:
				lobby_countdown_started = False
				#Start the new game!
				start_game_ret = cmd.startGame()
				if len(start_game_ret) > 0: # this means an error
					cmd.chatMsg(start_game_ret[0])
				# Add new bots:
				invading_army.produce()
				cmd.setVar("GameOptions.GameInfo.AllowEmptyGames", False) # Set the allow empty games flag
	
	if signal[0] in ["backtolobby", "lobbystarted"]:
		# We have entered the lobby. prepare for next battle...
		
		cmd.setVar("GameOptions.GameInfo.AllowEmptyGames", True) # Set the allow empty games flag
		
		# Kick all bots in case there are any. However there should not be. They should be kicked ingame when they are out of lives
		cmd.kickBots() 
		
		# select next map, next mod and decide which the next enemies will be!
		cmd.map(random.choice(all_available_maps))
		invading_army = army_architect.generate_blueprint(get_num_foreign_worms()+0.5, None, 0, ENEMY_TEAM) # DARN! We do not want to decide challenge level yet! What ever! It will adapt afterwards...
		
		# Give army information
		cmd.chatMsg("<b>Next Invading Army: "+get_command_safe_string(invading_army.name)+"</b>")
		cmd.chatMsg(get_command_safe_string(invading_army.description))
		cmd.chatMsg(" ")
		
		try_start_lobby_countdown() # Perhaps we are enough players to start the game now!
		
			
def get_command_safe_string(string):
	return string.replace('"', "''")

def worm_is_local(id):
	"""Uses whois to find out if the worm is a local one or not"""
	cmd_resp = parse_olx_dict_format(cmd.whoIs(id))
	if len(cmd_resp) == 0:
		return False #Ugly error handling
	if cmd_resp["NetSpeed"] == "local":
		return True
	return False

def get_num_foreign_worms():
	ret = 0
	ids = cmd.getWormList()
	if len(ids) < 1:
		return 0 #Ugly error handling
	for id in ids:
		if not worm_is_local(id):
			ret += 1	
	return ret
	
def parse_olx_dict_format(list):
	return dict([ re.match("([^:]+): (.*)", item).groups() for item in list ])
	

#========= Action! ========================================
init_server()
while True:
	try:
		signal_handler(cmd.nextSignal())
	except SystemExit:
		break
	except:
		cmd.msg( "Unexpected error in signal handler main loop:" )
		for line in traceback.format_exc().splitlines():
			cmd.msg( get_command_safe_string(line) )	