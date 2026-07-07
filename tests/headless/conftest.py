"""Shared pytest fixtures for the OpenLieroX headless test suite."""

import pytest

from harness import NetworkGame, find_binary


@pytest.fixture(scope="session")
def olx_binary():
    """Path to the built openlierox binary; fails the suite if it is missing."""
    binary = find_binary()
    if binary is None:
        pytest.fail(
            "openlierox binary not found; "
            "build it first (./tests/headless/build.sh) or set OLX_BINARY",
            pytrace=False,
        )
    return binary


@pytest.fixture
def network_game(olx_binary, tmp_path):
    """A :class:`NetworkGame` whose processes are all torn down after the test."""
    game = NetworkGame(olx_binary, str(tmp_path))
    try:
        yield game
    finally:
        game.stop_all()
