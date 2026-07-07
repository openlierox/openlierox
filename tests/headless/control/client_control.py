#!/usr/bin/env python3
"""Dedicated-client control script for the headless network test.

The instance connects out via the ``-connect`` command-line option;
this script only observes the local game state
and reports it on stderr via ``CLIENT[...]`` markers.
It emits ``CLIENT[<name>] PLAYING`` once the client has joined the running game.

Environment variables:

===========================  =========================================
``OLX_CLIENT_NAME``          short label used in the emitted markers
``OLX_RUN_SECONDS``          how long to keep observing
===========================  =========================================
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _olx_pipe import emit, game_state, worm_ids  # noqa: E402


def main():
    name = os.environ.get("OLX_CLIENT_NAME", "client")
    reached_playing = False
    deadline = time.time() + int(os.environ.get("OLX_RUN_SECONDS", "60"))
    while time.time() < deadline:
        state = game_state()
        worms = worm_ids()
        emit("CLIENT[%s] state=%s worms=%s" % (name, state, ",".join(worms)))
        if state == "Playing" and not reached_playing:
            emit("CLIENT[%s] PLAYING" % name)
            reached_playing = True
        time.sleep(0.5)
    emit("CLIENT[%s] DONE reached_playing=%s" % (name, reached_playing))


main()
