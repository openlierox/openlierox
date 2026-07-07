"""Basic smoke tests: the game boots headlessly and can host a game.

No networking between instances here;
just that the binary starts, initialises,
and reaches the lobby as a dedicated server.
"""


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
