#!/usr/bin/env python3
"""Convert an OpenLieroX binary player profile file (cfg/players.dat) into the
new YAML format (cfg/players.yaml).

The binary format is a fixed-width record layout (see src/client/ProfileSystem.cpp,
LoadProfile/SaveProfile):

    header:
        char[32]   magic "lx:profile" (zero padded)
        int32      version (must be 5)
        int32      profile count
    per profile:
        char[32]   name
        char[128]  skin filename
        int32      type          (0 = human, 1 = computer)
        int32      difficulty     (0..3)
        char[16]   username       (unused/vestigial - dropped from the YAML)
        char[16]   password       (unused/vestigial - dropped from the YAML)
        uint8      R
        uint8      G
        uint8      B
        char[64]   weapon slot 0
        ...
        char[64]   weapon slot 4  (always 5 slots)

Integers are stored in the machine's native byte order. OpenLieroX's EndianSwap
is a no-op on little-endian machines, so the profiles shipped/created on the
common x86 platforms are little-endian. This is the default here; pass
--big-endian for a file produced on a big-endian machine.

The YAML this emits mirrors the layout written by SaveProfilesYaml() in
ProfileSystem.cpp. It carries the same data except the unused username/password
fields, which are dropped (they are vestigial and always empty).
"""

import argparse
import struct
import sys

PROFILE_VERSION = 5
MAGIC = "lx:profile"
NUM_WEAPON_SLOTS = 5


def read_fixed_cstr(data, offset, width):
    """Read a fixed-width block and cut it off at the first NUL byte, matching
    OpenLieroX's freadfixedcstr()."""
    raw = data[offset:offset + width]
    if len(raw) < width:
        raise ValueError("unexpected end of file while reading a %d-byte field" % width)
    nul = raw.find(b"\0")
    if nul != -1:
        raw = raw[:nul]
    return raw.decode("latin-1"), offset + width


def parse_dat(data, big_endian=False):
    order = ">" if big_endian else "<"
    offset = 0

    magic, offset = read_fixed_cstr(data, offset, 32)
    if magic != MAGIC:
        raise ValueError('bad magic: %r is not %r' % (magic, MAGIC))

    (version,) = struct.unpack_from(order + "i", data, offset)
    offset += 4
    if version != PROFILE_VERSION:
        raise ValueError("unsupported profile version %d (expected %d)" % (version, PROFILE_VERSION))

    (count,) = struct.unpack_from(order + "i", data, offset)
    offset += 4
    if count < 0:
        raise ValueError("negative profile count %d" % count)

    profiles = []
    for _ in range(count):
        name, offset = read_fixed_cstr(data, offset, 32)
        skin, offset = read_fixed_cstr(data, offset, 128)
        (ptype,) = struct.unpack_from(order + "i", data, offset); offset += 4
        (difficulty,) = struct.unpack_from(order + "i", data, offset); offset += 4
        # username/password are read to advance past them but intentionally not
        # emitted - they are unused vestigial fields (always empty).
        _username, offset = read_fixed_cstr(data, offset, 16)
        _password, offset = read_fixed_cstr(data, offset, 16)
        r, g, b = struct.unpack_from("BBB", data, offset); offset += 3
        weapons = []
        for _slot in range(NUM_WEAPON_SLOTS):
            weapon, offset = read_fixed_cstr(data, offset, 64)
            weapons.append(weapon)
        profiles.append({
            "name": name,
            "type": ptype,
            "difficulty": difficulty,
            "skin": skin,
            "color": [r, g, b],
            "weapons": weapons,
        })

    return version, profiles


def yaml_quote(s):
    """Quote a scalar the way it needs to be for YAML. Keeps things readable:
    only quote when necessary, always quote the empty string."""
    if s == "":
        return '""'
    # Double-quote and escape anything that could confuse a YAML parser.
    needs_quote = (
        s[0] in "!&*[]{}#|>@`\"'%,-? \t"
        or s[-1] in " \t"
        or ":" in s
        or "#" in s
        or s.lower() in ("true", "false", "null", "yes", "no", "on", "off", "~")
    )
    try:
        # A pure number should be quoted so it stays a string.
        float(s)
        needs_quote = True
    except ValueError:
        pass
    if needs_quote:
        return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'
    return s


def emit_yaml(version, profiles):
    """Emit YAML mirroring SaveProfilesYaml() in ProfileSystem.cpp."""
    lines = []
    lines.append("version: %d" % version)
    lines.append("profiles:")
    if not profiles:
        lines[-1] = "profiles: []"
    for p in profiles:
        lines.append("  - name: %s" % yaml_quote(p["name"]))
        lines.append("    type: %d" % p["type"])
        lines.append("    difficulty: %d" % p["difficulty"])
        lines.append("    skin: %s" % yaml_quote(p["skin"]))
        lines.append("    color: [%d, %d, %d]" % tuple(p["color"]))
        lines.append("    weapons:")
        for w in p["weapons"]:
            lines.append("      - %s" % yaml_quote(w))
    return "\n".join(lines) + "\n"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", help="path to players.dat")
    parser.add_argument("output", nargs="?",
                        help="path to write players.yaml (default: stdout)")
    parser.add_argument("--big-endian", action="store_true",
                        help="treat the .dat as big-endian (file made on a big-endian machine)")
    args = parser.parse_args(argv)

    with open(args.input, "rb") as f:
        data = f.read()

    try:
        version, profiles = parse_dat(data, big_endian=args.big_endian)
    except (ValueError, struct.error) as e:
        sys.stderr.write("error: could not parse %s: %s\n" % (args.input, e))
        return 1

    text = emit_yaml(version, profiles)

    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(text)
        sys.stderr.write("wrote %d profile(s) to %s\n" % (len(profiles), args.output))
    else:
        sys.stdout.write(text)

    return 0


if __name__ == "__main__":
    sys.exit(main())
