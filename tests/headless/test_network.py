"""Headless network-play tests: a dedicated server plus real network clients.

The scenario is issue #973:
a client that joins a game that is *already running* must be able to play.
Two clients tell the cases apart:

* ``c1`` joins while the server is still in the *lobby* -- the control case,
  which works today; its join is what starts the game.
* ``c2`` joins *after* the game has started -- the mid-game join
  that is broken on current master:
  the late joiner never learns the mod,
  gets "invalid mod name (none)" and is kicked.

A client counts as joined once it reaches the ``Playing`` state.
"""


def _host_running_game(network_game):
    """Start the server, join client ``c1`` in the lobby, and start the game.

    Returns once the server reports ``SERVER_PLAYING`` and ``c1`` is playing.
    """
    server = network_game.start_server()
    assert server.wait_for("SERVER_LOBBY", timeout=30), server.read_log()

    c1 = network_game.add_client("c1")
    # c1 joining the lobby is what makes the server start the game.
    assert server.wait_for("SERVER_PLAYING", timeout=40), (
        "server never started the game after c1 joined:\n" + server.read_log()
    )
    assert c1.wait_for("CLIENT[c1] PLAYING", timeout=30), (
        "control client c1 failed to join the lobby game:\n" + c1.read_log()
    )
    return server, c1


def test_client_can_join_in_lobby(network_game):
    """A client that joins in the lobby can play (the control case)."""
    _host_running_game(network_game)


def test_client_can_join_running_game(network_game):
    """A client joining an already-running game can play (issue #973).

    This currently FAILS: it reproduces the critical #973 bug,
    where a mid-game joiner never learns the mod and is kicked.
    #973 is severe enough that the suite should fail loudly rather than
    tolerate it, so this is a hard failure until the bug is fixed
    (by any correct fix, not only the PR #966 protocol revert).
    """
    server, c1 = _host_running_game(network_game)

    c2 = network_game.add_client("c2")
    joined = c2.wait_for("CLIENT[c2] PLAYING", timeout=25)

    # Diagnostic: on the broken protocol the late joiner logs this.
    assert joined, (
        "mid-game joiner c2 never reached Playing.\n"
        "invalid-mod-name seen: %s\n%s"
        % ("invalid mod name" in c2.read_log(), c2.read_log())
    )
