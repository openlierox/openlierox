"""Deterministic physics regression tests.

These pin down exact physics behaviour,
so a refactor of the physics code can be checked to still behave as before.
The simulation runs at a fixed timestep,
and the control script drives it frame by frame (``simStep``)
with a fixed seed and forced worm input,
so the outcome is reproducible.
We then assert it against a golden value.

If a change to the physics alters the outcome, this fails.
If the change is intended, update the golden below;
the failure message prints the actual values.

The tolerance is small but non-zero,
to absorb floating-point differences across compilers/CPUs
while still catching any real behaviour change
(a real change moves the worm by far more than a fraction of a pixel over many frames).
"""

import os
import re

from harness import OlxInstance, CONTROL_DIR

PHYSICS_CONTROL = os.path.join(CONTROL_DIR, "physics_control.py")

# Worm dropped at (456,212) on "Dirt Level.lxl", all-zero input, 50 fixed frames.
GRAVITY_GOLDEN = {"x": 456.0, "y": 223.979, "vx": 0.0, "vy": 48.4}
EPS_POS = 1.0
EPS_VEL = 2.0


def _parse_state(line):
    return {k: float(v) for k, v in re.findall(r"(\w+)=(-?[\d.eE+]+)", line)}


def _run_scenario(binary, home, env=None):
    inst = OlxInstance(binary, "phys", home, PHYSICS_CONTROL, env=env).start()
    try:
        assert inst.wait_for("PHYS_RESULT", timeout=60), (
            "physics scenario never finished:\n" + inst.read_log())
        line = next(l for l in inst.read_log().splitlines()
                    if l.startswith("PHYS_RESULT"))
    finally:
        inst.stop()
    return _parse_state(line)


def test_worm_falls_under_gravity(olx_binary, tmp_path):
    got = _run_scenario(olx_binary, str(tmp_path / "phys"))
    for key, eps in (("x", EPS_POS), ("y", EPS_POS), ("vx", EPS_VEL), ("vy", EPS_VEL)):
        assert abs(got[key] - GRAVITY_GOLDEN[key]) <= eps, (
            "physics changed: %s=%g, golden=%g (eps %g); full state %s.\n"
            "If this change is intended, update GRAVITY_GOLDEN."
            % (key, got[key], GRAVITY_GOLDEN[key], eps, got))
