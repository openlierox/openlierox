#!/usr/bin/env python3
"""Dedicated-server control script for the shutdown-hang test (issue #800).

Hosts a lobby with master-server registration enabled,
but routes all HTTP through a proxy that points at a black hole:
a socket that accepts the connection and then never answers.
That is the condition under which a registration request used to hang
inside curl and block OLX's shutdown forever.

After the request has had time to go out and stall,
it issues ``quit`` and lets the harness measure
how long the process then takes to exit.

Environment:
  OLX_PORT           UDP port to host on (default 23400)
  OLX_HTTP_PROXY     proxy address all HTTP is routed through (the black hole)
  OLX_REGISTER_WAIT  seconds to wait after hosting before quitting (default 3)
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _olx_pipe import command, emit  # noqa: E402


def main():
    port = os.environ.get("OLX_PORT", "23400")
    proxy = os.environ.get("OLX_HTTP_PROXY", "")
    command("setvar GameOptions.Network.RegisterServer 1")
    command("setvar GameOptions.Network.UseIpToCountry 0")
    command('setvar GameOptions.Network.HttpProxy "%s"' % proxy)
    command("setvar GameOptions.GameInfo.AllowEmptyGames 1")

    command("startlobby " + port)
    emit("SERVER_LOBBY")

    # Give the registration request time to be sent to (and stall at) the proxy.
    time.sleep(float(os.environ.get("OLX_REGISTER_WAIT", "3")))

    emit("SERVER_QUIT")
    command("quit")  # OLX shuts down and closes the pipe (command then exits)


main()
