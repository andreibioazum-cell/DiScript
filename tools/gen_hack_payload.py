#!/usr/bin/env python3
"""Builds the H4c4k0r raid payload scripts shipped in assets/hack/.

The raid scene in the game "loads" a hacker script at round start: the PC
build reads the .so container, phones read the .bin one. Both files carry the
same little command table in different containers, so the event code parses
one format and only the magic/platform byte differs.

Layout (v1, every field one byte unless noted):
  0  magic[8]      "H4CKSO1\\0" (PC) or "H4CKBIN1" (phone)
  8  platform      2 = PC, 1 = phone
  9  version       1
 10  cmd_count
 11  cmds[]        id, kind (1 = event prop, 2 = custom cloud/tornado), life_s
 ... button_count  how many menu buttons the avalanche drops
 ... cane_count    New Year canes of the compensation rain
 ... cane_candy    candies ("tips") per collected cane
  -1 checksum      sum of all previous bytes mod 256

Run from the repo root: python3 tools/gen_hack_payload.py
"""

from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "hack"

# Every command becomes a persistent inert prop. Real event ids 1..6 are
# disco, plates, Santa, cube, Columns and weak-hit plates; custom 10/11 are
# a decorative tornado and one cloud. life=0 means "until the ban cleanup".
COMMANDS = (
    (1, 1, 0),    # /event 1  -> silent disco ball
    (2, 1, 0),    # /event 2  -> two dead plates at the real plate anchors
    (3, 1, 0),    # /event 3  -> Santa mannequin
    (4, 1, 0),    # /event 4  -> collectible-cube mannequin
    (5, 1, 0),    # /event 5  -> Columns mannequin
    (6, 1, 0),    # /event 6  -> weak-hit plates stay equally dead
    (10, 2, 0),   # /event 10 -> tornado that never spins or moves anything
    (11, 2, 0),   # /event 11 -> one cloud, spawned only after this line types
)
BUTTON_COUNT = 14   # menu buttons that rain down as physical cover
CANE_COUNT = 10     # exactly ten Ded Moroz canes in the compensation rain
CANE_CANDY = 3      # candies per collected cane


def payload(magic: bytes, platform: int) -> bytes:
    body = bytearray(magic)
    body.append(platform)
    body.append(1)
    body.append(len(COMMANDS))
    for cmd in COMMANDS:
        body.extend(cmd)
    body.append(BUTTON_COUNT)
    body.append(CANE_COUNT)
    body.append(CANE_CANDY)
    body.append(sum(body) & 0xFF)
    return bytes(body)


PC = payload(b"H4CKSO1\0", 2)
PHONE = payload(b"H4CKBIN1", 1)


def write_all() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "h4c4k0r.so").write_bytes(PC)
    (OUT / "h4c4k0r.bin").write_bytes(PHONE)


if __name__ == "__main__":
    write_all()
    for name, blob in (("h4c4k0r.so", PC), ("h4c4k0r.bin", PHONE)):
        print(f"{name}: {len(blob)} bytes, checksum {blob[-1]:02x}")
