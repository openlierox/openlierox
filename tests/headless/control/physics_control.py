#!/usr/bin/env python3
"""Control script for the deterministic physics tests.

It sets up a single controlled worm,
advances the simulation by an exact number of fixed frames,
then reports the worm's state.
The simulation uses a fixed timestep,
and here it is driven frame by frame (``simStep``)
with a fixed seed (``simSetSeed``)
and forced input (``setWormInput``, so no non-deterministic bot AI runs),
so the outcome is reproducible and a test can compare it against a golden value.

Environment variables:

====================  =========================================================
``OLX_PHYS_SEED``     RNG seed (default 12345)
``OLX_PHYS_SPAWN``    "x,y" to drop the worm at, in the air (default "456,212")
``OLX_PHYS_INPUT``    forced input "move shoot jump carve" (default all false)
``OLX_PHYS_STEPS``    number of fixed frames to advance (default 50)
====================  =========================================================
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _olx_pipe import command, emit, game_state, worm_ids  # noqa: E402


def main():
    seed = os.environ.get("OLX_PHYS_SEED", "12345")
    spawn = os.environ.get("OLX_PHYS_SPAWN", "456,212")
    forced = os.environ.get("OLX_PHYS_INPUT", "false false false false")
    steps = int(os.environ.get("OLX_PHYS_STEPS", "50"))

    settings = {
        "GameOptions.Network.RegisterServer": 0,
        "GameOptions.Network.UseIpToCountry": 0,
        "GameOptions.GameInfo.GameType": "Death Match",
        "GameOptions.GameInfo.LevelName": "Dirt Level.lxl",
        "GameOptions.GameInfo.ModName": "Classic",
        "GameOptions.GameInfo.ForceRandomWeapons": 1,
        "GameOptions.GameInfo.ImmediateStart": 1,
        "GameOptions.GameInfo.AllowEmptyGames": 1,
    }
    for key, value in settings.items():
        command('setvar %s "%s"' % (key, value))
    command("startlobby 23400")

    # A bot, purely as a worm object to control:
    # a dedicated server cannot add a human worm (addHuman is refused in
    # dedicated mode), and setWormInput below bypasses the bot AI entirely,
    # so the worm never acts on its own.
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
    if not worm_ids():
        emit("PHYS_ERROR no worm")
        return
    wid = worm_ids()[0]

    # Deterministic setup: fixed seed, freeze the sim (manual stepping only),
    # force the worm's input, and drop it at a known point in the air.
    command("simSetSeed " + seed)
    command("simStep 0")
    command("setWormInput %s %s" % (wid, forced))
    command('spawnWorm %s "%s"' % (wid, spawn))

    def frame():
        return int(command("getFrame")[0])

    def report(label):
        pos = command("getWormPos %s" % wid)
        vel = command("getWormVelocity %s" % wid)
        emit("%s x=%s y=%s vx=%s vy=%s frame=%d"
             % (label, pos[0], pos[1], vel[0], vel[1], frame()))

    report("PHYS_INIT")
    target = frame() + steps
    command("simStep %d" % steps)
    while frame() < target:  # wait until all steps are done
        time.sleep(0.02)
    report("PHYS_RESULT")


main()
