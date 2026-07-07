#!/usr/bin/env python3
"""Dedicated-server control script for the headless network test.

Hosts a game,
then starts it once at least one remote worm has joined the lobby.
Progress is reported on stderr via ``SERVER_*`` markers that the harness waits on.

Configuration comes from environment variables
(inherited from OLX, which inherits them from the harness),
so one script serves every scenario:

===========================  =========================================
``OLX_PORT``                 UDP port to host on (default 23400)
``OLX_SERVER_NAME``          advertised server name
``OLX_GAMETYPE``             game mode (default "Death Match")
``OLX_MAP``                  level file (default "Dirt Level.lxl")
``OLX_MOD``                  mod name (default "Classic")
``OLX_WEAPON_SEL_TIME``      seconds before unready clients are kicked
``OLX_RUN_SECONDS``          how long to keep the server loop alive
===========================  =========================================
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _olx_pipe import command, emit, game_state, worm_ids  # noqa: E402


def main():
    port = os.environ.get("OLX_PORT", "23400")
    settings = {
        # Keep the test fully offline: no master-server registration or GeoIP.
        "GameOptions.Network.RegisterServer": 0,
        "GameOptions.Network.UseIpToCountry": 0,
        "GameOptions.Network.ServerName":
            os.environ.get("OLX_SERVER_NAME", "OLX Headless Test"),
        "GameOptions.GameInfo.GameType": os.environ.get("OLX_GAMETYPE", "Death Match"),
        "GameOptions.GameInfo.LevelName": os.environ.get("OLX_MAP", "Dirt Level.lxl"),
        "GameOptions.GameInfo.ModName": os.environ.get("OLX_MOD", "Classic"),
        # Force random weapons + immediate start,
        # so headless clients (with no weapon-selection UI) join with no interaction.
        "GameOptions.GameInfo.ForceRandomWeapons": 1,
        "GameOptions.GameInfo.ImmediateStart": 1,
        "GameOptions.GameInfo.AllowEmptyGames": 1,
        # Required to reproduce #973: allow joining a running game.
        "GameOptions.Server.AllowConnectDuringGame": 1,
        "GameOptions.Server.WeaponSelectionMaxTime":
            int(os.environ.get("OLX_WEAPON_SEL_TIME", "8")),
    }
    for key, value in settings.items():
        command('setvar %s "%s"' % (key, value))

    command("startlobby " + port)
    emit("SERVER_LOBBY")

    started = False
    playing = False
    deadline = time.time() + int(os.environ.get("OLX_RUN_SECONDS", "90"))
    while time.time() < deadline:
        state = game_state()
        worms = worm_ids()
        emit("SERVER_POLL state=%s worms=%s" % (state, ",".join(worms)))
        if state == "Lobby" and len(worms) >= 1 and not started:
            command("startgame")
            emit("SERVER_STARTGAME")
            started = True
        if state == "Playing" and not playing:
            emit("SERVER_PLAYING")
            playing = True
        time.sleep(0.5)
    emit("SERVER_DONE")


main()
