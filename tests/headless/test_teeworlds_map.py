"""A shipped Teeworlds (.map) level loads and a round starts on it.

Regression guard for the Teeworlds map-loader bounds checks
(the header/offset validation around #1050 / #1051):
a real, valid Teeworlds map must still load, not be rejected.
"""


def test_teeworlds_map_loads_and_starts(network_game):
    server = network_game.start_server(
        OLX_MAP="ctf1.map",      # a small shipped Teeworlds map
        OLX_GAMETYPE="Death Match",
        OLX_BOTS=2,              # enough to start a round, which forces the load
        OLX_RUN_SECONDS=30,
    )
    assert server.wait_for("SERVER_PLAYING", timeout=60), (
        "server did not start a round on the Teeworlds map:\n" + server.read_log()
    )
    log = server.read_log()
    assert "could not load level" not in log.lower(), log
