#!/usr/bin/env python3
"""Dedicated-client control script for the headless network test.

The instance connects out via the ``-connect`` command-line option;
this script only observes the local game state
and reports it on stderr via ``CLIENT[...]`` markers.
It emits ``CLIENT[<name>] PLAYING`` once the client has joined the running game.

Environment variables:

====================================  =========================================
``OLX_CLIENT_NAME``                   short label used in the emitted markers
``OLX_RUN_SECONDS``                   how long to keep observing
``OLX_LEAVE_SIGNAL_FILE``             once this file exists, disconnect and quit
====================================  =========================================
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _olx_pipe import command, emit, emit_worm_states, game_state, worm_ids  # noqa: E402


def main():
    name = os.environ.get("OLX_CLIENT_NAME", "client")
    leave_signal = os.environ.get("OLX_LEAVE_SIGNAL_FILE")
    emit_state = os.environ.get("OLX_EMIT_STATE")
    reached_playing = False
    combat = False
    prev_worms = set()
    deadline = time.time() + int(os.environ.get("OLX_RUN_SECONDS", "60"))
    while time.time() < deadline:
        if leave_signal and os.path.exists(leave_signal):
            emit("CLIENT[%s] LEAVING" % name)
            command("disconnect")
            break
        state = game_state()
        worms = set(worm_ids())
        emit("CLIENT[%s] state=%s worms=%s" % (name, state, ",".join(sorted(worms))))
        # Emit an event for each worm that appeared or disappeared,
        # so tests can wait on a specific join/leave rather than a whole-list state.
        for gone in sorted(prev_worms - worms):
            emit("CLIENT[%s] WORM_LEFT %s" % (name, gone))
        for joined in sorted(worms - prev_worms):
            emit("CLIENT[%s] WORM_JOINED %s" % (name, joined))
        prev_worms = worms
        if state == "Playing" and not reached_playing:
            emit("CLIENT[%s] PLAYING" % name)
            reached_playing = True
        # Report this client's own view of every worm's state, so a test can
        # check it against the server's view and confirm the game state syncs.
        if reached_playing and emit_state:
            states = emit_worm_states("CLIENT[%s]" % name)
            total_dmg = sum(s["dmg"] for s in states.values())
            total_kills = sum(s["kills"] for s in states.values())
            any_death = any(s["hp"] < 0 for s in states.values())
            if not combat and (total_dmg >= 40 or total_kills > 0 or any_death):
                emit("CLIENT[%s] COMBAT dmg=%g kills=%d death=%d"
                     % (name, total_dmg, total_kills, int(any_death)))
                combat = True
        time.sleep(0.5)
    emit("CLIENT[%s] DONE reached_playing=%s" % (name, reached_playing))


main()
