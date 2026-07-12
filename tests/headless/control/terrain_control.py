#!/usr/bin/env python3
"""Control script for the terrain-carve test (see tests/headless/test_terrain.py).

It starts a game, then freezes the simulation (``simStep`` manual stepping)
so nothing but an explicit carve can change the map.
It reports the whole-map checksum and two region checksums
before and after the carve,
so the test can confirm the carve changed the map,
changed the carved region,
and left an untouched region alone.

Environment variables:

====================  =========================================================
``OLX_MAP``           level file (default "Dirt Level.lxl")
``OLX_CARVE_AT``      "x,y" to carve at (default "200,150", on the dirt)
``OLX_TARGET_REGION`` "col,row" region the carve falls in (default "3,2")
``OLX_CONTROL_REGION``"col,row" region far from the carve (default "0,0")
====================  =========================================================
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _olx_pipe import command, emit, game_state, worm_ids  # noqa: E402


def _one(ret):
    return ret[0] if ret else "?"


def whole_chk():
    return _one(command("getMapMaterialChecksum"))


def region_chk(reg):
    col, row = reg.split(",")
    return _one(command("getMapRegionChecksum %s %s" % (col, row)))


def main():
    carve_at = os.environ.get("OLX_CARVE_AT", "200,150")
    target = os.environ.get("OLX_TARGET_REGION", "3,2")
    control = os.environ.get("OLX_CONTROL_REGION", "0,0")

    settings = {
        "GameOptions.Network.RegisterServer": 0,
        "GameOptions.Network.UseIpToCountry": 0,
        "GameOptions.GameInfo.GameType": "Death Match",
        "GameOptions.GameInfo.LevelName": os.environ.get("OLX_MAP", "Dirt Level.lxl"),
        "GameOptions.GameInfo.ModName": "Classic",
        "GameOptions.GameInfo.ForceRandomWeapons": 1,
        "GameOptions.GameInfo.ImmediateStart": 1,
        "GameOptions.GameInfo.AllowEmptyGames": 1,
    }
    for key, value in settings.items():
        command('setvar %s "%s"' % (key, value))
    command("startlobby 23400")

    # A bot just so the game can start; we freeze and force no input,
    # so it never acts and never touches the map itself.
    added = set()
    deadline = time.time() + 15
    while len(added) < 1 and time.time() < deadline:
        if game_state() == "Lobby":
            added.update(command("addBots 1"))
        if len(added) < 1:
            time.sleep(0.3)
    command("startgame")
    for _ in range(60):
        if game_state() == "Playing":
            break
        time.sleep(0.2)
    if game_state() != "Playing":
        emit("TERRAIN_ERROR not playing (state=%s)" % game_state())
        return

    # Freeze: from now on the map only changes when we carve it.
    command("simSetSeed 12345")
    command("simStep 0")
    for wid in worm_ids():
        command("setWormInput %s false false false false" % wid)

    emit("TERRAIN_PRE whole=%s target=%s control=%s"
         % (whole_chk(), region_chk(target), region_chk(control)))

    x, y = carve_at.split(",")
    carved = command("carveMap %s %s" % (x, y))
    emit("TERRAIN_CARVED count=%s" % _one(carved))

    emit("TERRAIN_POST whole=%s target=%s control=%s"
         % (whole_chk(), region_chk(target), region_chk(control)))
    emit("TERRAIN_DONE")


main()
