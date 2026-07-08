"""Headless gameplay test: bots play a real round, and the state syncs to a client.

The other tests only cover the lobby and join/leave. This one plays an actual
round with no human input: the server adds bots that move, shoot, damage each
other, die and respawn. A separate client connects and reports its own view of
every worm, which must match what the server produced. That exercises in-game
state replication (health, kills, damage), which a real multiplayer session
depends on, rather than just the lobby handshake.

The simulation is not deterministic (bot AI and frame timing vary), so the
assertions check properties and a tolerance, not exact values: combat happened,
a worm died and respawned, the client saw the same combat and a death, and its
damage numbers track the server's.
"""

import re


def _states(log, prefix):
    """Yield (worm_id, hp, kills, dmg) for each ``<prefix> STATE`` marker in *log*."""
    pat = re.compile(
        re.escape(prefix) + r" STATE id=(\d+) hp=(\S+) kills=(\d+) dmg=(\S+)")
    for line in log.splitlines():
        m = pat.search(line)
        if m:
            yield m.group(1), float(m.group(2)), int(m.group(3)), float(m.group(4))


def _max_damage(log, prefix):
    out = {}
    for wid, _hp, _kills, dmg in _states(log, prefix):
        out[wid] = max(out.get(wid, 0.0), dmg)
    return out


def _deaths(log, prefix):
    """The set of worm ids ever seen dead (hp < 0) in *log*."""
    return {wid for wid, hp, _k, _d in _states(log, prefix) if hp < 0}


def test_bots_play_a_round_and_state_syncs_to_a_client(network_game):
    server = network_game.start_server(
        OLX_BOTS=3,              # three bots so combat is lively and reliable
        OLX_EMIT_STATE=1,        # report every worm's state each poll
        OLX_START_WHEN_WORMS=4,  # start once the three bots and the client are in
        # Cluster worms at one spot so they fight at once;
        # random spawns can delay the first kill past the timeout, causing flakes.
        OLX_SPAWN_CLOSE=1,
        OLX_RUN_SECONDS=90,
    )
    assert server.wait_for("SERVER_LOBBY", timeout=30), server.read_log()

    client = network_game.add_client(
        "c1", env={"OLX_EMIT_STATE": "1", "OLX_RUN_SECONDS": "90"})

    # The round starts with the client in it.
    assert server.wait_for("SERVER_PLAYING", timeout=45), (
        "server never started the round:\n" + server.read_log())
    assert client.wait_for("CLIENT[c1] PLAYING", timeout=30), (
        "client never joined the round:\n" + client.read_log())

    # A real round plays out on the server: the bots fight, and a worm dies and
    # respawns (so the whole life cycle, not just a first hit, is exercised).
    assert server.wait_for("SERVER_COMBAT", timeout=60), (
        "no combat happened on the server:\n" + server.read_log())
    assert server.wait_for("SERVER_RESPAWN", timeout=60), (
        "no worm died and respawned:\n" + server.read_log())

    # The client's own replicated view shows the same combat: state syncs.
    assert client.wait_for("CLIENT[c1] COMBAT", timeout=30), (
        "client never observed the combat the server produced:\n" + client.read_log())

    srv_log, cli_log = server.read_log(), client.read_log()

    # A death is a discrete, unambiguous event that must replicate:
    # at least one worm the server saw die was also seen dead by the client.
    srv_deaths = _deaths(srv_log, "SERVER")
    cli_deaths = _deaths(cli_log, "CLIENT[c1]")
    assert srv_deaths, "no worm died on the server:\n" + srv_log
    assert srv_deaths & cli_deaths, (
        "client did not observe any of the server's deaths: server=%s client=%s"
        % (sorted(srv_deaths), sorted(cli_deaths)))

    # And the numbers agree: for each worm the server saw take real damage, the
    # client independently saw it take a comparable amount (allowing for a little
    # replication lag), rather than a stale or empty view.
    srv_dmg = _max_damage(srv_log, "SERVER")
    cli_dmg = _max_damage(cli_log, "CLIENT[c1]")
    fought = {wid: d for wid, d in srv_dmg.items() if d > 30}
    assert fought, "server never recorded meaningful damage:\n" + srv_log
    for wid, sd in fought.items():
        cd = cli_dmg.get(wid, 0.0)
        assert cd >= 0.5 * sd, (
            "client's damage for worm %s (%g) does not track the server's (%g):\n%s"
            % (wid, cd, sd, cli_log))
