"""Client for OpenLieroX's dedicated-control pipe protocol.

An OLX instance started with ``-dedicated -script <path>``
runs that script as a subprocess
and talks to it over a line-based pipe:

* the script writes a console command to stdout; OLX runs it
* OLX replies with zero or more ``:<arg>`` lines, ending with a lone ``.``
* signals are pulled the same way, via the ``nextsignal`` command

Only the script's stdout/stdin are wired to OLX.
Whatever it writes to stderr goes to the console,
so the harness reads progress markers from there.

This is the Python 3 counterpart of the shipped ``dedicated_control_io.py``,
which is Python 2 and no longer runs on current systems.
"""

import sys


def emit(marker):
    """Write a progress *marker* to stderr for the harness to observe."""
    sys.stderr.write(marker + "\n")
    sys.stderr.flush()


def command(cmd):
    """Send one console command to OLX and return its list of return args.

    Raises SystemExit when OLX closes the pipe (it is shutting down),
    which cleanly ends the control script.
    """
    sys.stdout.write(cmd + "\n")
    sys.stdout.flush()
    args = []
    while True:
        line = sys.stdin.readline()
        if line == "":  # OLX closed the pipe -> stop
            raise SystemExit(0)
        line = line.rstrip("\n")
        if line == ".":
            break
        if line.startswith(":"):
            args.append(line[1:])
    return args


def game_state():
    """Return the current high-level game state.

    One of Inactive, Lobby, Preparing or Playing
    (or ``?`` if OLX returned nothing).
    """
    res = command("getGameState")
    return res[0] if res else "?"


def worm_ids():
    """Return the worm ids currently known to this instance (as strings)."""
    return command("getwormlist")


def worm_state(wid):
    """Return a worm's replicated state as a dict, or None if unavailable.

    ``getWormScore`` returns ``[lives, kills, damage, health]``
    (health is -1 when the worm is dead), and ``getWormPos`` returns ``[x, y]``.
    Both are read-only and work on a client as well as the server,
    so the same view can be compared on both sides.
    """
    score = command("getWormScore " + str(wid))
    if len(score) < 4:
        return None
    st = {"kills": int(score[1]), "dmg": float(score[2]), "hp": float(score[3])}
    pos = command("getWormPos " + str(wid))
    if len(pos) >= 2:
        st["x"], st["y"] = float(pos[0]), float(pos[1])
    return st


def emit_worm_states(prefix):
    """Emit one ``<prefix> STATE ...`` marker per worm; return ``{id: state}``.

    The caller uses the returned states to detect combat, deaths and respawns.
    """
    states = {}
    for wid in worm_ids():
        st = worm_state(wid)
        if st is None:
            continue
        emit("%s STATE id=%s hp=%g kills=%d dmg=%g"
             % (prefix, wid, st["hp"], st["kills"], st["dmg"]))
        states[wid] = st
    return states
