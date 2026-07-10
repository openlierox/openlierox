#!/usr/bin/env python3
"""Control script that dumps the scanned mod list, for the mod-list test.

The file-list cache is built asynchronously at startup,
so poll ``listMods`` until the seeded positive-control mod appears
(or give up), then emit one ``MOD:<path>`` marker per mod
and a final ``MODLIST_DONE``.
The test seeds directories in a search path
and checks which of them the scan reports as mods.
"""

import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _olx_pipe import command, emit  # noqa: E402


def main():
    expect = os.environ.get("OLX_EXPECT_MOD", "")
    mods = []
    deadline = time.time() + 20
    while time.time() < deadline:
        mods = command("listMods")
        if not expect or expect in mods:
            break
        time.sleep(0.3)

    for path in mods:
        emit("MOD:" + path)
    emit("MODLIST_DONE")


main()
