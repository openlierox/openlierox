"""Basic smoke tests: the game boots headlessly and can host a game.

No networking between instances here;
just that the binary starts, initialises,
and reaches the lobby as a dedicated server.
"""

import signal
import subprocess


def test_dedicated_server_reaches_lobby(network_game):
    """A dedicated server starts up and reaches the hosting lobby."""
    server = network_game.start_server()
    assert server.wait_for("SERVER_LOBBY", timeout=30), (
        "dedicated server did not reach the lobby in time:\n" + server.read_log()
    )


def test_startup_reports_version(network_game):
    """OLX prints its version banner on startup."""
    server = network_game.start_server()
    assert server.wait_for("SERVER_LOBBY", timeout=30), server.read_log()
    # e.g. "H: OpenLieroX/20260704.3+git.0899403 is starting ..."
    assert "OpenLieroX/" in server.read_log()
    assert "is starting" in server.read_log()


def test_loads_configured_mod_and_map(network_game):
    """The server loads without complaining about the mod/map we configured."""
    server = network_game.start_server()
    assert server.wait_for("SERVER_POLL state=Lobby", timeout=30), server.read_log()
    log = server.read_log()
    assert "invalid mod name" not in log, log
    assert "could not load level" not in log.lower(), log


def test_graceful_shutdown_on_quit_signal(network_game):
    """A quit signal to a running server shuts it down cleanly, without crashing.

    Regression test for the quit-signal crash:
    once the gameloop thread is running,
    the SIGINT/SIGTERM handler used to assign the game.state Attr directly
    from signal context, off the gameloop thread,
    which tripped the attribute-update machinery's thread assertion and aborted.
    The handler must instead only push an SDL_QUIT event,
    which the gameloop dispatches to set the state on the right thread.
    """
    server = network_game.start_server()
    # Once the lobby is up, the gameloop thread is running,
    # which is the precondition for the crash.
    assert server.wait_for("SERVER_LOBBY", timeout=30), server.read_log()

    server.proc.send_signal(signal.SIGINT)

    try:
        rc = server.proc.wait(timeout=30)
    except subprocess.TimeoutExpired:
        raise AssertionError(
            "server did not exit after SIGINT:\n" + server.read_log())

    # A crash (abort/segfault) shows up as termination by signal,
    # i.e. a negative return code (e.g. -6 for SIGABRT).
    # A clean shutdown returns 0.
    assert rc == 0, (
        "server did not shut down cleanly (returncode=%d):\n" % rc
    ) + server.read_log()
