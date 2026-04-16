#!/usr/bin/env -S python2 -u

# TODO: what is this script for? why not using cfg/options.cfg ?
# TODO: This script should only contain special settings which are not covered by options.cfg

# TODO: register commands in ChatCommand system
ADMIN_PREFIX = "!" # What kind of prefix you want for admin commands. Example: !?-.@$ A.K.A you can use prettymuch everything.
SERVER_PORT = 23400 # What port to start server on, 23400 is the default

# Where to log what is happening
LOG_FILE = "dedicated_control.log"

MIN_PLAYERS = 2
MAX_TEAMS = 2 # Only blue and red teams
TOO_FEW_PLAYERS_MESSAGE = "Waiting for %i players." % (MIN_PLAYERS)
WAIT_BEFORE_SPAMMING_TOO_FEW_PLAYERS_MESSAGE = 30 # Seconds to wait before another "Game will start with %i players" message
FILL_WITH_BOTS_TO = 0 # Fill server with bots if noone playing, set to 2 to get 1 bot with a single human player

WAIT_AFTER_GAME = 30 # Seconds to wait in lobby after round finished
WAIT_BEFORE_GAME = 5 # Seconds to wait in lobby before next round, will give some message
WAIT_BEFORE_GAME_MESSAGE = "Game will start in %i seconds" % WAIT_BEFORE_GAME

import dedicated_control_io as io # control handler

GAME_LIVES = -2
GAME_MAX_KILLS = 15
GAME_MAX_TIME = 15

# Note: This is unfair and I don't thing it is such a good idea. (At least for the average player, only 
# pro-gamers perhaps want that.)
# A user with a high ping doesn't give any disadvantages to other players (or at least that should not be the case and I wonder if it is).
MAX_PING = 2000 # Max ping to auto-kick player

RECORD_VIDEO = 0 # If we should record video on our ded server. Warning: it eats CPU!
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

VOTING = 1 # Should we allow voting for preset/map/mod/lt/kick/mute
VOTING_PERCENT = 51 # How much users in percent should vote yes for vote to pass
VOTING_TIME = 30 # For how much time we should wait votes from users
VOTING_COUNT_NEGATIVE = 1 # If we should count negative votes, so users can fail unpopular vote before timeout
VOTING_KICK_TIME = 5 # Time in minutes when user kicked by voting cannot re-join server (it auto-kicks user again with message)
VOTING_AUTO_ACCEPT = 0 # If we should accept the vote after timeout, if too little users voted no
VOTING_QUEUE_SIZE = 3 # How many maps/mods users can push into a queue by voting

# List of levels - preset chooses a random level from those
LEVELS = [	
			"Alien Hood.lxl",
			"Blat Arena.lxl",
			"CastleStrike.lxl",
			"Complex.lxl",
			"destrdome.lxl",
			"FossilFacility.lxl",
			"GammaComplex.lxl",
			"HW-house.lxl",
			"LieroFactory(Revisited).lxl",
			"LieroFactory_Maintenance.lxl",
			"JailBreak.lxl",
			"JukkeDome.lxl",
			"Kirby_ice_cream_island.lxl",
			"KitchenKombat.lxl",
			"Labrinth.lxl",
			"Lake Liero.lxl",
			"Lamda_bunker_(Razvisited).lxl",
			"MsPaintPower.lxl",
			"Ore Quarry.lxl",
			"RIP_Home.lxl",
			"Snus Industry Ltd.lxl",
			"Sunrise_Mansion.lxl",
			"Temple Hood.lxl",
			"Tetrisv2.lxl",
			"Treasurev2.lxl",
			"tombofwormses.lxl",
			"wormmountain.lxl",
		]

MAX_PLAYERS_SMALL_LEVELS = 3 # If 3 or less players, select only small levels
SMALL_LEVELS = [
			"BetaBoxDE.lxl",
			"BoN.lxl",
			"black cave.lxl",
			"Blat Arena.lxl",
			"Duel.lxl",
			"HW Arena.lxl",
			"Poo Arena.lxl",
			"Utopia.lxl",
			"wormmountain.lxl",
			"X Arena.lxl",
]

# List of presets to cycle on server - you may specify some preset multiple times, then it will have higher chances of appearing
# If this list is empty all presets are used
PRESETS = [ "Mortars", "Random" ]

GLOBAL_SETTINGS = {
	"GameOptions.Network.ServerName":               "Games nonstop + voting",
	"GameOptions.Network.WelcomeMessage":           "Welcome, <player>. Type !help for voting commands.",
	"GameOptions.GameInfo.AllowConnectDuringGame":  1,
	"GameOptions.GameInfo.AllowEmptyGames":         0,
	"GameOptions.GameInfo.ImmediateStart":          1,
	"GameOptions.GameInfo.SelfHit":                 1,
	"GameOptions.GameInfo.SelfInjure":              1,
	"GameOptions.GameInfo.TeamHit":                 1,
	"GameOptions.GameInfo.TeamInjure":              1,
	"GameOptions.GameInfo.SuicideDecreasesScore":   1,
	"GameOptions.GameInfo.WeaponSelectionMaxTime":  60,
	"GameOptions.GameInfo.CTF_AllowRopeForCarrier": 0,
	"GameOptions.GameInfo.RelativeAirJump":         0, # SorZ said it's lame
	"GameOptions.GameInfo.InfiniteMap":             0, # Infinite map is still broken and the rope is warping
}

