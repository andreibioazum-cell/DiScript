#!/usr/bin/env python3
"""The disco loop used by the game: pattern, harmony and hook, all by hand.

Play this file with ``python3 tools/generate_disco_loops.py`` (or with
``--list`` for a summary).  The loop is 30 seconds long and its tempo is a
whole number of 4/4 bars inside 30 s, so it wraps on beat one.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import disco_loops as dl  # noqa: E402
from disco_arrange import Am, C, F, G, Track, render  # noqa: E402

# ---------------------------------------------------------------------------
# 01 Roller Rink Sunset - A minor, 120 BPM, 15 bars.  Old-school strings and
# congas, brass hook in the second half.
# ---------------------------------------------------------------------------
T01 = Track(
    "disco_01_roller_rink_sunset", 15, intro=2, turn=1,
    chords=[Am, F, C, G],
    pad={"kind": "strings", "amp": 0.062, "attack": 0.3, "release": 0.5, "send": 0.22,
         "detune": (0.996, 1.0, 1.004)},
    pad2={"kind": "organ", "amp": 0.022, "octave": 12, "attack": 0.05, "release": 0.2,
          "send": 0.12},
    bass={"marks": "8.8.8.85.8.8.o8.", "amp": 0.38, "extra_octave": True,
          "turn_double": True},
    drums={
        "kick": ("", 0.8),
        "clap": ("....x.......x...", 0.76, 0.0, 0.18, 0),
        "hat": ("x.x.x.x.x.x.x.x.", 0.48, 0.22, 0.0, 0),
        "ohat": (["..o...o...o...o.", "..o...o.o.o...o."], 0.32, -0.24, 0.1, 3),
        "shaker": ("x.xxx.xxx.xxx.xx", 0.195, 0.4, 0.0, 3),
        "conga_hi": ("....o.o...o.o.o.", 0.289, 0.36, 0.0, 0),
        "conga_lo": ("o.......o.......", 0.272, -0.36, 0.0, 0),
    },
    hook={"kind": "brass", "amp": 0.105, "pan": 0.05, "send": 0.3, "vib": 0.004,
          "scoop": 0.35,
          "transpose": (0, 0, 12)},
    phrase=((0, 76, 3), (3, 72, 1), (4, 69, 2), (6, 72, 2), (8, 76, 2), (10, 79, 2),
            (12, 77, 4), (16, 72, 3), (19, 69, 1), (20, 67, 2), (22, 69, 2),
            (24, 72, 2), (26, 76, 6), (32, 76, 3), (35, 72, 1), (36, 67, 2),
            (38, 72, 2), (40, 76, 2), (42, 79, 2), (44, 81, 4), (48, 79, 3),
            (51, 77, 1), (52, 76, 2), (54, 74, 2), (56, 72, 8)),
    phrase_alt=((0, 79, 3), (3, 77, 1), (4, 76, 2), (6, 74, 2), (8, 72, 2), (10, 74, 2),
                (12, 76, 4), (16, 74, 3), (19, 72, 1), (20, 71, 2), (22, 72, 2),
                (24, 74, 4), (28, 76, 2), (30, 79, 2), (32, 81, 4), (36, 79, 2),
                (38, 77, 2), (40, 76, 4), (44, 74, 4), (48, 72, 4), (52, 71, 4),
                (56, 69, 8)),
    stabs={"marks": "....8.......8...", "amp": 0.06, "kind": "brass", "length": 0.13,
           "from_bar": 8},
)


# The set written to disk: the single loop the game uses.
TRACKS = (T01,)


def main(argv: list[str]) -> int:
    if "--list" in argv:
        for t in TRACKS:
            print(f"{t.name}.wav  {t.bpm} BPM  {t.bars} bars  {dl.SECONDS:.0f} s")
        return 0
    wanted = {arg for arg in argv if not arg.startswith("-")}
    for t in TRACKS:
        if wanted and t.name not in wanted and f"{t.name}.wav" not in wanted:
            continue
        render(t)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
