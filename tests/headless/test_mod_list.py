"""Mod-list scan tests.

The Mod dropdown (and the ``listMods`` command that shares its cache)
scans every top-level directory in the search paths
and reports each one it recognises as a mod.
A Gusanos mod is recognised purely by an ``objects/`` subdirectory,
which a version-control directory like ``.git`` also has
(git's object store lives in ``.git/objects``),
so a checkout used to show up as ``.git [Gus]``.
Hidden dot-directories must be skipped.
"""

import os
import sys

from harness import OlxInstance, CONTROL_DIR

MODLIST_CONTROL = os.path.join(CONTROL_DIR, "modlist_control.py")


def _user_data_subdir():
    """Search-path subdirectory under HOME that OLX scans for user content."""
    if sys.platform == "darwin":
        return "Library/Application Support/OpenLieroX"
    return ".OpenLieroX"


def _seed_mod_dir(home, name):
    """Create ``<home>/<userdata>/<name>/objects`` so the scan sees a Gus mod."""
    path = os.path.join(home, _user_data_subdir(), name, "objects")
    os.makedirs(path, exist_ok=True)


def _scanned_mods(binary, home):
    """Run OLX headless, return the list of mod paths ``listMods`` reports."""
    inst = OlxInstance(
        binary, "modlist", home, MODLIST_CONTROL,
        env={"OLX_EXPECT_MOD": "zzz_test_gus"},
    ).start()
    try:
        assert inst.wait_for("MODLIST_DONE", timeout=40), (
            "mod list scan never finished:\n" + inst.read_log())
    finally:
        inst.stop()
    return [l[len("MOD:"):] for l in inst.read_log().splitlines()
            if l.startswith("MOD:")]


def test_git_dir_not_listed_as_mod(olx_binary, tmp_path):
    """A ``.git`` directory in a search path is not reported as a mod.

    ``zzz_test_gus`` is the positive control:
    a plain directory with an ``objects/`` subdir, which is a real Gus mod,
    so its presence proves the scan reached the seeded search path
    and the ``objects/`` heuristic still works.
    ``.git`` has the same ``objects/`` subdir but must be skipped.
    """
    home = str(tmp_path / "modlist")
    _seed_mod_dir(home, "zzz_test_gus")
    _seed_mod_dir(home, ".git")

    mods = _scanned_mods(olx_binary, home)

    assert "zzz_test_gus" in mods, (
        "positive-control Gus mod was not scanned; got %r" % mods)
    assert ".git" not in mods, (
        ".git directory was listed as a mod; got %r" % mods)
