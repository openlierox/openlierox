#!/usr/bin/env python3 -u

# TODO: what is this script for? why not using cfg/options.cfg ?
# TODO: This script should only contain special settings which are not covered by options.cfg

# TODO: register commands in ChatCommand system
ADMIN_PREFIX = "!" # What kind of prefix you want for admin commands. Example: !?-.@$ A.K.A you can use prettymuch everything.
SERVER_PORT = 23402 # What port to start server on, 23400 is the default

# Where to log what is happening
#NOTE: Do not specify path here, only file name. Path will be searched automatically based on Openlierox settings
LOG_FILE = "dedicated_control.log"

MIN_PLAYERS = 1
MIN_PLAYERS_TEAMS = 6 # Players will be split in two teams automatically if there is enough players
MAX_TEAMS = 2 # Only blue and red teams
TOO_FEW_PLAYERS_MESSAGE = "Game will start with minimum %i players. Team Deathmatch if there's %i or more players" % (MIN_PLAYERS, MIN_PLAYERS_TEAMS)
WAIT_BEFORE_SPAMMING_TOO_FEW_PLAYERS_MESSAGE = 30 # Seconds to wait before another "Game will start with %i players" message
FILL_WITH_BOTS_TO = 0 # Fill server with bots if noone playing, set to 2 to get 1 bot with a single human player

WAIT_AFTER_GAME = 10 # Seconds to wait in lobby after round finished
WAIT_BEFORE_GAME = 30 # Seconds to wait in lobby before next round, will give some message
WAIT_BEFORE_GAME_MESSAGE = "Game will start in %i seconds" % WAIT_BEFORE_GAME

import dedicated_control_io as io # control handler

GAME_LIVES = int(io.getVar("GameOptions.GameInfo.Lives"))
GAME_MAX_KILLS = int(io.getVar("GameOptions.GameInfo.KillLimit"))
GAME_MAX_TIME = float(io.getVar("GameOptions.GameInfo.TimeLimit"))
WEAPON_SELECTION_TIME = int(io.getVar("GameOptions.GameInfo.WeaponSelectionMaxTime"))

# Note: This is unfair and I don't thing it is such a good idea. (At least for the average player, only 
# pro-gamers perhaps want that.)
# A user with a high ping doesn't give any disadvantages to other players (or at least that should not be the case and I wonder if it is).
MAX_PING = 2000 # Max ping to auto-kick player

RECORD_VIDEO = 1 # If we should record video on our ded server. Warning: it eats CPU!
TIME_TO_KILL_VIDEORECORDER = 60 # Wait one minute before killing recorder - it may encode rather slowly!

# TODO: We should use the OLX chatcommand system.
# TODO: Register dedscript commands in OLX chatcommand system.
# Users can enter some commands too
USER_PREFIX = ADMIN_PREFIX # Change to have custom user command prefix instead of "!"
ALLOW_TEAM_CHANGE = True # Player should type "!b", "!r", "!g", or "!y" to set it's own team
TEAM_CHANGE_MESSAGE = "Set your team with %steam b/r" % USER_PREFIX
if MAX_TEAMS >= 3:
	TEAM_CHANGE_MESSAGE += "/g"
if MAX_TEAMS >= 4:
	TEAM_CHANGE_MESSAGE += "/y"


RANKING = 1 # Should we allow !rank user command
RANKING_AUTHENTICATION = 0 # Should we authenticate worm by it's skin color (pretty weak, but !password cmd is kinda ugly)

VOTING = 0 # Should we allow voting for preset/map/mod/lt/kick/mute
VOTING_PERCENT = 51 # How much users in percent should vote yes for vote to pass
VOTING_TIME = 40 # For how much time we should wait votes from users
VOTING_COUNT_NEGATIVE = 1 # If we should count negative votes, so users can fail unpopular vote before timeout
VOTING_KICK_TIME = 5 # Time in minutes when user kicked by voting cannot re-join server (it auto-kicks user again with message)
VOTING_AUTO_ACCEPT = 1 # If we should accept the vote after timeout, if too little users voted no

# List of levels - preset chooses a random level from those
LEVELS = [	
			"Blat Arena.lxl",
			"CastleStrike.lxl",
			"castle_wars.lxl",
			"Complex.lxl",
			"destrdome.lxl",
			"FossilFacility.lxl",
			"GammaComplex.lxl",
			"HW-house.lxl",
			"LieroFactory(Revisited).lxl",
			"JailBreak.lxl",
			"JukkeDome.lxl",
			"Ore Quarry.lxl",
			"Tetrisv2.lxl",
			"tombofwormses.lxl",
			"Utopia.lxl",
			"wormmountain.lxl",
		]

MAX_PLAYERS_SMALL_LEVELS = 3 # If 3 or less players, select only small levels
SMALL_LEVELS = [
			"BetaBoxDE.lxl",
			"Blat Arena.lxl",
			"Duel.lxl",
			"Poo Arena.lxl",
			"Utopia.lxl",
			"wormmountain.lxl",
			"X Arena.lxl",
]

MODS = [ 
	"8-Bit Warfare v1.2", 
	"Classic", 
	"FoodFight v0.6", 
	"MSF II", 
	"MW 1.0", 
	"NarutoNT-1.26", 
	"Shock v1.40 Air",
	"Trick or Treat v0.4",
	"WH40K 0.13",
	"Zelda v0.8",
]

# List of presets to cycle on server - you may specify some preset multiple times, then it will have higher chances of appearing
# If this list is empty all presets are used
PRESETS = [ "Random" ]

GLOBAL_SETTINGS = {
	"GameOptions.Network.ServerName":                "LX56 compatible",
	"GameOptions.GameInfo.WeaponSelectionMaxTime":   60,
}
