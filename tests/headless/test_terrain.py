"""Terrain-carve tests.

Guard against the late-join test being vacuous:
prove that an explicit carve really changes the map,
and that it changes only where it happens.
The simulation is frozen for the check,
so worm activity can't move the map between the two measurements
and the carve is the only difference.
"""

import os
import re

from harness import OlxInstance, CONTROL_DIR

TERRAIN_CONTROL = os.path.join(CONTROL_DIR, "terrain_control.py")


def _row(log, marker):
    line = next((l for l in log.splitlines() if l.startswith(marker)), "")
    m = re.search(r"whole=(\d+) target=(\d+) control=(\d+)", line)
    assert m, "missing %s line:\n%s" % (marker, log)
    return {"whole": m.group(1), "target": m.group(2), "control": m.group(3)}


def test_carve_changes_map_and_is_localized(olx_binary, tmp_path):
    inst = OlxInstance(olx_binary, "terrain", str(tmp_path / "terrain"),
                       TERRAIN_CONTROL).start()
    try:
        assert inst.wait_for("TERRAIN_DONE", timeout=60), inst.read_log()
        log = inst.read_log()
    finally:
        inst.stop()

    carved = re.search(r"TERRAIN_CARVED count=(\d+)", log)
    assert carved and int(carved.group(1)) > 0, (
        "the carve removed no dirt, so the test proves nothing:\n" + log)

    pre = _row(log, "TERRAIN_PRE")
    post = _row(log, "TERRAIN_POST")

    # The carve changes the whole-map checksum ...
    assert pre["whole"] != post["whole"], (
        "carve did not change the map checksum: %s == %s"
        % (pre["whole"], post["whole"]))
    # ... and the carved region's checksum ...
    assert pre["target"] != post["target"], (
        "carve did not change its own region: %s == %s"
        % (pre["target"], post["target"]))
    # ... but leaves a region far from the carve untouched.
    assert pre["control"] == post["control"], (
        "carve changed an untouched region: %s != %s"
        % (pre["control"], post["control"]))
