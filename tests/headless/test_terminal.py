"""Terminal-restore test for the interactive stdin CLI.

Unlike the rest of the suite,
this runs a dedicated server with the stdin CLI active (no ``-disablestdincli``)
on a real pty, then quits it and inspects the raw terminal bytes.

Regression test for the shutdown staircase:
the stdin CLI puts the terminal in linenoise raw mode (ONLCR off),
and the tee-stdout handler thread re-enabled raw mode around each write,
so the final shutdown messages used to print while the terminal was still raw.
A bare "\n" is then not turned into "\r\n",
so each message staircases down the screen instead of starting at column 0.
"""

import os
import pty
import select
import signal
import time

import pytest

from harness import GAMEDIR, SERVER_CONTROL, find_binary

MARKER = b"Standard Out/Err recovered"
PROMPT = b"OLX> "


def _run_dedicated_on_pty(binary, home, port):
    """Run a dedicated server with the stdin CLI active on a pty until it quits.

    Hosts a lobby and stays there,
    sends SIGINT once it is up,
    and returns the raw terminal bytes collected until it exits.
    """
    env = dict(os.environ)
    env["HOME"] = home
    env["TERM"] = "xterm"  # make sure linenoise treats it as a supported terminal
    env["OLX_PORT"] = str(port)
    env["OLX_RUN_SECONDS"] = "120"
    env["OLX_START_WHEN_WORMS"] = "99"  # stay in the lobby, never auto-start

    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(GAMEDIR)
        os.environ.clear()
        os.environ.update(env)
        os.execv(binary, [binary, "-dedicated", "-script", SERVER_CONTROL])
        os._exit(127)

    buf = bytearray()
    start = time.time()
    deadline = start + 60
    lobby_at = None
    sent = False
    quit_at = None
    eof = False
    while time.time() < deadline:
        r, _, _ = select.select([fd], [], [], 0.5)
        if r:
            try:
                data = os.read(fd, 4096)
            except OSError:
                eof = True
                break
            if not data:
                eof = True
                break
            buf += data
            if lobby_at is None and b"state=Lobby" in buf:
                lobby_at = time.time()
        # Once the lobby is up and the prompt is drawn, ask it to quit.
        if lobby_at is not None and not sent and time.time() - lobby_at > 2.0:
            os.kill(pid, signal.SIGINT)
            sent = True
            quit_at = time.time()
        if quit_at is not None and time.time() - quit_at > 25.0:
            break

    if not eof:
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass
    try:
        os.waitpid(pid, 0)
    except OSError:
        pass
    return bytes(buf)


def test_shutdown_messages_not_staircased(tmp_path):
    """The final shutdown message prints at column 0, not staircased.

    Runs on a pty with a supported TERM,
    so the stdin CLI must activate and its prompt must appear;
    if it does not, that is a real failure, not a skip.
    """
    binary = find_binary()
    if binary is None:
        pytest.fail("openlierox binary not found; build it first or set OLX_BINARY",
                    pytrace=False)

    raw = _run_dedicated_on_pty(binary, str(tmp_path), 23470)

    assert PROMPT in raw, (
        "stdin CLI never showed its prompt, so the raw-mode path was not exercised:\n"
        + repr(raw[-300:])
    )

    idx = raw.find(MARKER)
    assert idx >= 0, "shutdown recovery message never printed:\n" + repr(raw[-400:])

    # Cooked shutdown ends the line at column 0:
    # a CRLF (ONLCR on), or an explicit column-0 reset (ESC [ 0 G).
    # A bare "\n" means raw mode: staircased.
    nl = raw.find(b"\n", idx + len(MARKER))
    assert nl >= 0, "no newline after the recovery message:\n" + repr(raw[idx:])
    segment = raw[idx + len(MARKER):nl]
    cooked = raw[nl - 1:nl] == b"\r" or b"\x1b[0G" in segment
    assert cooked, (
        "shutdown message printed in raw mode (staircased); "
        "bytes around it: " + repr(raw[max(0, idx - 20):nl + 1])
    )
