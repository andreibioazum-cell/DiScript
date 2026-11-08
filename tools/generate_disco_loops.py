#!/usr/bin/env python3
"""The eight disco loops: patterns, harmony and hooks, all written by hand.

Play this file with ``python3 tools/generate_disco_loops.py`` (or with
``--list`` for a summary).  Each entry is one 30 second loop; tempos are whole
numbers of 4/4 bars inside 30 s so every loop wraps on beat one.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import disco_loops as dl  # noqa: E402
from disco_arrange import (A, Ab, Abmaj7, Am, Am7, Bb, BbM, Bbm, Bm, C, Cm, Cm7, D, Dm, Dm7, Dm9, Eb, Ebmaj7, Em, Em7, F, Fm, Fmaj7, G, G7, Gm,  # noqa: E402
                           Track, render)

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


# ---------------------------------------------------------------------------
# 02 Neon Boots - F major, 128 BPM, 16 bars.  Filtered strings, octave bass,
# saw lead and a shouty brass answer.
# ---------------------------------------------------------------------------
def spice_02(m, t, chords) -> None:
    """Off-beat octave-bass bounce and shimmering glass on the 4-bar phrase."""
    bar = t.bar
    for b in range(t.intro, t.bars):
        for step in (2, 6, 10, 14):
            root = chords[b][0]
            m.sustain("bass", dl.midi(root + 12), b * bar + step * bar / 16.0,
                      bar * 0.1, 0.2, 0.0, 0.004, 0.3, 0.5, 0.05, 0.0, 0.0)
    dl.arp(m, t.intro * bar, bar, chords[t.intro:], t.bars - t.intro,
           "1.2.3.4.3.2.1.2.", 0.085, "glass", 12, 0.3, gate=0.5)


T02 = Track(
    "disco_02_neon_boots", 16, intro=1, turn=3,
    chords=[F, Dm, BbM, C],
    pad={"kind": "strings", "amp": 0.064, "attack": 0.22, "release": 0.4, "send": 0.2,
         "detune": (0.9965, 1.0, 1.0035)},
    pad2={"kind": "choir", "amp": 0.03, "octave": 12, "attack": 0.5, "release": 0.6,
          "send": 0.35, "detune": (0.999, 1.001)},
    bass={"marks": "8o8o8o8o8o8o8o8o", "amp": 0.33, "gate": 0.42, "turn_double": True},
    drums={
        "kick": ("", 0.8),
        "snare": ("....x.......x..x", 0.414, 0.05, 0.16, 2),
        "shaker": ("x.xxx.xxx.xxx.xx", 0.221, 0.42, 0.0, 2),
        "hat": ("..x...x...x...x.", 0.51, -0.3, 0.0, 2),
        "tamb": ("x...x...x...x...", 0.3, 0.46, 0.12, 6),
        "rim": ("..............x.", 0.352, -0.26, 0.2, 10),
    },
    hook={"kind": "saw", "amp": 0.1, "send": 0.3, "vib": 0.003, "gate": 0.8,
          "transpose": (0, 12)},
    phrase=((0, 81, 2), (2, 79, 2), (4, 77, 2), (6, 74, 2), (8, 77, 2), (10, 81, 2),
            (12, 84, 3), (15, 81, 1), (16, 77, 2), (18, 76, 2), (20, 74, 2),
            (22, 72, 2), (24, 74, 4), (28, 77, 2), (30, 79, 2), (32, 77, 2),
            (34, 74, 2), (36, 70, 2), (38, 74, 2), (40, 77, 2), (42, 79, 2),
            (44, 81, 4), (48, 79, 2), (50, 77, 2), (52, 76, 2), (54, 74, 2),
            (56, 72, 8)),
    phrase_alt=((0, 84, 2), (2, 81, 2), (4, 79, 2), (6, 77, 2), (8, 81, 2), (10, 84, 2),
                (12, 86, 4), (16, 84, 2), (18, 81, 2), (20, 79, 2), (22, 77, 2),
                (24, 76, 4), (28, 79, 2), (30, 81, 2), (32, 79, 2), (34, 77, 2),
                (36, 74, 2), (38, 77, 2), (40, 79, 2), (42, 81, 2), (44, 84, 4),
                (48, 81, 2), (50, 79, 2), (52, 77, 2), (54, 76, 2), (56, 74, 8)),
    stabs={"marks": "..8...8...8...8.", "amp": 0.075, "kind": "clav", "length": 0.15,
           "pan": 0.22, "from_bar": 6, "half_time": True},
    spice=spice_02,
)


# ---------------------------------------------------------------------------
# 03 Mirrorball Dreams - D minor, 112 BPM, 14 bars.  Slow luxury: lush pads,
# electric-piano comping, choir lead.
# ---------------------------------------------------------------------------
def spice_03(m, t, chords) -> None:
    bar = t.bar
    for b in range(1, t.bars):
        root, triad = chords[b]
        for step in (4, 7, 10, 14):
            for note in triad:
                m.sustain("glass", dl.midi(note + 12), b * bar + step * bar / 16.0,
                          bar * 0.22, 0.055, (note - triad[0] - 3) * 0.07,
                          0.004, 0.5, 0.4, 0.12, 0.0, 0.22)


T03 = Track(
    "disco_03_mirrorball_dreams", 14, intro=1, turn=1,
    chords=[Dm7, Gm, BbM, Am],
    pad={"kind": "strings", "amp": 0.07, "attack": 0.4, "release": 0.6, "send": 0.28,
         "detune": (0.997, 1.0, 1.003)},
    pad2={"kind": "organ", "amp": 0.03, "octave": 12, "attack": 0.08, "release": 0.3,
          "send": 0.2},
    bass={"marks": "8...8...8...8.5.", "amp": 0.36, "gate": 0.95, "turn_double": True},
    drums={
        "kick": ("", 0.7),
        "clap": ("....x.......x...", 0.646, 0.0, 0.22, 2),
        "shaker": ("x.x.x.x.x.x.x.x.", 0.247, 0.36, 0.0, 2),
        "hat": ("..x...x...x...x.", 0.36, -0.34, 0.16, 2),
        "ohat": ("......x.......x.", 0.288, 0.26, 0.2, 4),
        "rim": ("o.....o.....o...", 0.264, -0.2, 0.26, 6),
    },
    hook={"kind": "choir", "amp": 0.1, "pan": -0.1, "send": 0.42, "vib": 0.006,
          "attack": 0.05, "release": 0.3, "gate": 0.92, "transpose": (0, 0, 12)},
    phrase=((0, 74, 4), (4, 77, 2), (6, 81, 2), (8, 79, 3), (11, 77, 1), (12, 74, 4),
            (16, 72, 4), (20, 74, 2), (22, 77, 2), (24, 79, 4), (28, 77, 2),
            (30, 74, 2), (32, 77, 4), (36, 79, 2), (38, 82, 2), (40, 81, 4),
            (44, 79, 2), (46, 77, 2), (48, 76, 4), (52, 74, 2), (54, 72, 2),
            (56, 69, 8)),
    phrase_alt=((0, 81, 4), (4, 79, 2), (6, 77, 2), (8, 76, 4), (12, 74, 4), (16, 77, 4),
                (20, 79, 2), (22, 81, 2), (24, 82, 4), (28, 81, 2), (30, 79, 2),
                (32, 77, 4), (36, 76, 2), (38, 74, 2), (40, 72, 4), (44, 74, 4),
                (48, 76, 4), (52, 74, 4), (56, 69, 8)),
    stabs={"marks": "......x.......x.", "amp": 0.035, "kind": "glass", "length": 0.3,
           "from_bar": 8, "half_time": True},
    spice=spice_03,
)


# ---------------------------------------------------------------------------
# 04 Saturday Night Bus - C minor, 124 BPM, 16 bars.  Brass stabs, clav answers
# and a punchy snare.
# ---------------------------------------------------------------------------
def spice_04(m, t, chords) -> None:
    bar = t.bar
    for b in range(t.intro, t.bars):
        root, triad = chords[b]
        for step in (6, 14):
            for note in triad:
                m.sustain("clav", dl.midi(note), b * bar + step * bar / 16.0,
                          bar * 0.18, 0.055, 0.26, 0.003, 0.25, 0.3, 0.07, 0.0, 0.2)


T04 = Track(
    "disco_04_saturday_night_bus", 15, intro=2, turn=1,
    chords=[Cm, Ab, BbM, Gm],
    pad={"kind": "brass", "amp": 0.055, "attack": 0.1, "release": 0.25, "send": 0.2,
         "detune": (0.999, 1.001)},
    pad2={"kind": "strings", "amp": 0.035, "octave": 12, "attack": 0.35, "release": 0.5,
          "send": 0.25, "detune": (0.997, 1.003)},
    bass={"marks": "8.88.8.88.8.88.o", "amp": 0.37, "gate": 0.6, "turn_double": True},
    drums={
        "kick": ("", 0.82),
        "snare": ("....x.......x...", 0.483, 0.04, 0.16, 2),
        "shaker": ("x.xxx.xxx.xxx.xx", 0.208, 0.42, 0.0, 2),
        "hat": ("..x...x...x...x.", 0.48, -0.32, 0.0, 2),
        "cowbell": ("x.......x.......", 0.286, 0.3, 0.18, 6),
    },
    hook={"kind": "brass", "amp": 0.105, "send": 0.28, "vib": 0.004, "attack": 0.03,
          "scoop": 0.5, "transpose": (0, 12)},
    phrase=((0, 79, 3), (3, 77, 1), (4, 75, 2), (6, 72, 2), (8, 75, 2), (10, 79, 2),
            (12, 82, 4), (16, 80, 3), (19, 79, 1), (20, 77, 2), (22, 75, 2),
            (24, 72, 4), (28, 75, 2), (30, 77, 2), (32, 79, 3), (35, 82, 1),
            (36, 84, 4), (40, 82, 2), (42, 79, 2), (44, 77, 4), (48, 75, 2),
            (50, 74, 2), (52, 72, 4), (56, 70, 8)),
    phrase_alt=((0, 84, 3), (3, 82, 1), (4, 79, 2), (6, 77, 2), (8, 79, 2), (10, 82, 2),
                (12, 87, 4), (16, 84, 3), (19, 82, 1), (20, 80, 2), (22, 79, 2),
                (24, 77, 4), (28, 79, 2), (30, 82, 2), (32, 84, 3), (35, 87, 1),
                (36, 89, 4), (40, 87, 2), (42, 84, 2), (44, 82, 4), (48, 80, 2),
                (50, 79, 2), (52, 77, 4), (56, 75, 8)),
    stabs={"marks": "x..x..x.x..x..x.", "amp": 0.085, "kind": "brass", "length": 0.13,
           "from_bar": 2},
    spice=spice_04,
)


# ---------------------------------------------------------------------------
# 05 Feel the Floor - A minor, 132 BPM, 16 bars.  Cowbell, phased clav riff,
# swing on the bass.
# ---------------------------------------------------------------------------
def spice_05(m, t, chords) -> None:
    bar = t.bar
    riff = "1..2..3..2..1.."
    for b in range(t.intro, t.bars):
        root, triad = chords[b]
        tones = list(triad) + [triad[0] + 12]
        for index, mark in enumerate(riff):
            if mark in ". ":
                continue
            note = tones["1234".find(mark)] + 12
            moment = b * bar + index * bar / 16.0
            m.sustain("clav", dl.midi(note), moment, bar * 0.2, 0.07, 0.3,
                      0.003, 0.3, 0.3, 0.06, 0.0, 0.2)
            m.sustain("clav", dl.midi(note), moment + 0.014, bar * 0.18, 0.035, -0.3,
                      0.003, 0.3, 0.3, 0.06, 0.0, 0.2)


T05 = Track(
    "disco_05_feel_the_floor", 16, intro=3, turn=1,
    chords=[Am, Dm, Em, Am],
    pad={"kind": "strings", "amp": 0.052, "attack": 0.2, "release": 0.35, "send": 0.18,
         "detune": (0.9965, 1.0035)},
    pad2={"kind": "organ", "amp": 0.022, "octave": 12, "attack": 0.06, "release": 0.2,
          "send": 0.12},
    bass={"marks": "8.8.8.8.8.8.8o8o", "amp": 0.37, "gate": 0.55, "turn_double": True},
    drums={
        "kick": ("", 0.8),
        "clap": ("....x.......x...", 0.76, 0.0, 0.15, 0),
        "cowbell": ("x...x...x...x...", 0.364, 0.26, 0.2, 0),
        "shaker": ("x.xxx.xxx.xxx.xx", 0.208, -0.4, 0.0, 0),
        "ohat": ("..x...x...x...x.", 0.272, 0.35, 0.12, 0),
        "conga_lo": ("o.......o.......", 0.272, -0.3, 0.0, 0),
    },
    hook={"kind": "brass", "amp": 0.1, "pan": -0.05, "send": 0.26, "vib": 0.004,
          "scoop": 0.4, "transpose": (0, 12)},
    phrase=((0, 81, 3), (3, 79, 1), (4, 76, 2), (6, 72, 2), (8, 76, 2), (10, 79, 2),
            (12, 81, 4), (16, 79, 2), (18, 77, 2), (20, 76, 2), (22, 74, 2),
            (24, 76, 4), (28, 74, 2), (30, 72, 2), (32, 79, 3), (35, 81, 1),
            (36, 83, 4), (40, 81, 2), (42, 79, 2), (44, 76, 4), (48, 74, 2),
            (50, 76, 2), (52, 77, 4), (56, 76, 8)),
    phrase_alt=((0, 88, 3), (3, 86, 1), (4, 84, 2), (6, 81, 2), (8, 84, 2), (10, 86, 2),
                (12, 88, 4), (16, 86, 2), (18, 84, 2), (20, 83, 2), (22, 81, 2),
                (24, 83, 4), (28, 81, 2), (30, 79, 2), (32, 84, 3), (35, 86, 1),
                (36, 88, 4), (40, 86, 2), (42, 84, 2), (44, 81, 4), (48, 79, 2),
                (50, 81, 2), (52, 83, 4), (56, 81, 8)),
    stabs={"marks": "..8...8...8...8.", "amp": 0.07, "kind": "brass", "length": 0.12,
           "from_bar": 3},
    swing=0.16,
    spice=spice_05,
)


# ---------------------------------------------------------------------------
# 06 Boogie Boulevard - Bb major, 116 BPM, 14 bars.  Boogie piano, live congas.
# ---------------------------------------------------------------------------
def spice_06(m, t, chords) -> None:
    bar = t.bar
    boogie = "8.58.68.78.58.6."
    for b in range(t.intro, t.bars):
        root, triad = chords[b]
        for index, mark in enumerate(boogie):
            if mark in ". ":
                continue
            note = root + {"5": 7, "6": 9, "7": 10, "8": 0}[mark]
            m.sustain("bass", dl.midi(note), b * bar + index * bar / 16.0,
                      bar * 0.17, 0.32, 0.0, 0.004, 0.3, 0.55, 0.05, 0.0, 0.1)
        for step in (0, 3, 6, 8, 11, 14):
            for voice, note in enumerate(triad):
                m.sustain("organ", dl.midi(note + 12), b * bar + step * bar / 16.0,
                          bar * 0.16, 0.05, (voice - 1) * 0.28, 0.004, 0.25, 0.4,
                          0.06, 0.0, 0.14)


T06 = Track(
    "disco_06_boogie_boulevard", 14, intro=1, turn=1,
    chords=[BbM, Gm, Cm, F],
    pad={"kind": "strings", "amp": 0.045, "attack": 0.2, "release": 0.35, "send": 0.18,
         "detune": (0.997, 1.003)},
    bass={"marks": "8.8.8.8.8.8.8.5.", "amp": 0.3, "gate": 0.6},
    drums={
        "kick": ("", 0.74),
        "snare": ("....x.......x...", 0.414, 0.0, 0.14, 1),
        "hat": ("x.x.x.x.x.x.x.x.", 0.39, 0.28, 0.0, 1),
        "conga_hi": ("..o...o...o.o...", 0.289, 0.36, 0.0, 1),
        "conga_lo": ("o.........o.....", 0.289, -0.36, 0.0, 1),
        "tamb": ("x.......x.......", 0.33, -0.3, 0.1, 1),
    },
    hook={"kind": "brass", "amp": 0.095, "pan": 0.08, "send": 0.26, "vib": 0.004,
          "scoop": 0.3, "transpose": (0, 0, 12)},
    phrase=((0, 74, 3), (3, 77, 1), (4, 82, 4), (8, 81, 2), (10, 79, 2), (12, 77, 4),
            (16, 75, 3), (19, 74, 1), (20, 72, 4), (24, 74, 2), (26, 75, 2),
            (28, 77, 4), (32, 79, 3), (35, 81, 1), (36, 84, 4), (40, 81, 2),
            (42, 79, 2), (44, 77, 4), (48, 77, 2), (50, 74, 2), (52, 74, 4),
            (56, 72, 8)),
    phrase_alt=((0, 79, 3), (3, 82, 1), (4, 86, 4), (8, 82, 2), (10, 82, 2), (12, 79, 4),
                (16, 79, 3), (19, 77, 1), (20, 75, 4), (24, 77, 2), (26, 79, 2),
                (28, 82, 4), (32, 84, 3), (35, 86, 1), (36, 89, 4), (40, 86, 2),
                (42, 84, 2), (44, 81, 4), (48, 81, 2), (50, 79, 2), (52, 77, 4),
                (56, 77, 8)),
    stabs={"marks": "..8...8...8...8.", "amp": 0.06, "kind": "clav", "length": 0.14,
           "pan": 0.2, "from_bar": 5},
    spice=spice_06,
)


# ---------------------------------------------------------------------------
# 07 Disco Nebula - F minor, 128 BPM, 16 bars.  Space laser arps, glass bells,
# wide detuned strings.
# ---------------------------------------------------------------------------
def spice_07(m, t, chords) -> None:
    bar = t.bar
    dl.arp(m, t.intro * bar, bar, chords[t.intro:], t.bars - t.intro,
           "1234123412341234", 0.075, "glass", 12, 0.3, gate=0.45)
    dl.arp(m, t.intro * bar, bar, chords[t.intro:], t.bars - t.intro,
           "1.2.3.4.3.2.1...", 0.05, "saw", 24, 0.35,
           pan_cycle=(0.5, 0.2, -0.2, -0.5), gate=0.3, decay=0.12)
    dl.fx(m, 8 * bar, "laser", 0.1, 0.4)
    dl.fx(m, 12 * bar, "laser", 0.09, -0.4)


T07 = Track(
    "disco_07_disco_nebula", 16, intro=1, turn=3,
    chords=[Fm, Bbm, Eb, Cm],
    pad={"kind": "strings", "amp": 0.062, "attack": 0.3, "release": 0.5, "send": 0.25,
         "detune": (0.995, 1.0, 1.005)},
    pad2={"kind": "choir", "amp": 0.028, "octave": 12, "attack": 0.5, "release": 0.7,
          "send": 0.4, "detune": (0.997, 1.003)},
    bass={"marks": "8...8...8.8.8...", "amp": 0.36, "gate": 0.8, "turn_double": True},
    drums={
        "kick": ("", 0.8),
        "clap": ("....x.......x...", 0.76, 0.0, 0.18, 2),
        "hat": ("x.xxx.xxx.xxx.xx", 0.42, 0.3, 0.0, 2),
        "ohat": ("..x...x...x...x.", 0.272, -0.3, 0.2, 2),
        "rim": ("......o.......o.", 0.286, 0.2, 0.3, 4),
    },
    hook={"kind": "glass", "amp": 0.1, "send": 0.45, "vib": 0.005, "gate": 0.75,
          "transpose": (0, 0, 12)},
    phrase=((0, 77, 4), (4, 80, 2), (6, 84, 2), (8, 82, 3), (11, 80, 1), (12, 77, 4),
            (16, 82, 4), (20, 84, 2), (22, 87, 2), (24, 85, 4), (28, 84, 2),
            (30, 82, 2), (32, 84, 4), (36, 82, 2), (38, 80, 2), (40, 79, 4),
            (44, 77, 2), (46, 75, 2), (48, 72, 4), (52, 75, 2), (54, 77, 2),
            (56, 80, 8)),
    phrase_alt=((0, 84, 4), (4, 87, 2), (6, 91, 2), (8, 89, 3), (11, 87, 1), (12, 84, 4),
                (16, 87, 4), (20, 91, 2), (22, 94, 2), (24, 92, 4), (28, 91, 2),
                (30, 89, 2), (32, 91, 4), (36, 89, 2), (38, 87, 2), (40, 84, 4),
                (44, 84, 2), (46, 82, 2), (48, 79, 4), (52, 82, 2), (54, 84, 2),
                (56, 87, 8)),
    stabs={"marks": "..8...8...8...8.", "amp": 0.055, "kind": "brass", "length": 0.12,
           "pan": -0.2, "from_bar": 6},
    spice=spice_07,
)


# ---------------------------------------------------------------------------
# 08 Strobe Hearts - D major, 122 BPM, 16 bars.  Bright party disco, saw lead
# and brass answers.
# ---------------------------------------------------------------------------
T08 = Track(
    "disco_08_strobe_hearts", 15, intro=2, turn=1,
    chords=[D, A, Bm, G],
    pad={"kind": "strings", "amp": 0.06, "attack": 0.25, "release": 0.4, "send": 0.2,
         "detune": (0.997, 1.0, 1.003)},
    pad2={"kind": "brass", "amp": 0.024, "octave": 12, "attack": 0.08, "release": 0.2,
          "send": 0.15},
    bass={"marks": "8.8.8.8.8.8.8.5o", "amp": 0.37, "gate": 0.7, "turn_double": True},
    drums={
        "kick": ("", 0.8),
        "clap": ("....x.......x...", 0.798, 0.0, 0.16, 2),
        "tamb": ("x...x...x...x...", 0.33, 0.4, 0.12, 2),
        "shaker": ("x.x.x.x.x.x.x.x.", 0.221, -0.4, 0.0, 2),
        "hat": ("..x...x...x...x.", 0.42, 0.3, 0.0, 2),
        "ohat": ("....o.......o...", 0.288, -0.3, 0.2, 4),
    },
    hook={"kind": "saw", "amp": 0.1, "send": 0.3, "vib": 0.003, "gate": 0.82,
          "transpose": (0, 12)},
    phrase=((0, 78, 2), (2, 81, 2), (4, 85, 4), (8, 83, 2), (10, 81, 2), (12, 78, 4),
            (16, 83, 2), (18, 85, 2), (20, 88, 4), (24, 86, 2), (26, 85, 2),
            (28, 83, 4), (32, 81, 3), (35, 83, 1), (36, 85, 4), (40, 83, 2),
            (42, 81, 2), (44, 78, 4), (48, 79, 2), (50, 81, 2), (52, 83, 4),
            (56, 85, 8)),
    phrase_alt=((0, 88, 2), (2, 85, 2), (4, 81, 4), (8, 83, 2), (10, 88, 2), (12, 93, 4),
                (16, 88, 2), (18, 86, 2), (20, 83, 4), (24, 85, 2), (26, 88, 2),
                (28, 90, 4), (32, 88, 3), (35, 86, 1), (36, 85, 4), (40, 83, 2),
                (42, 81, 2), (44, 79, 4), (48, 78, 2), (50, 81, 2), (52, 83, 4),
                (56, 85, 8)),
    stabs={"marks": "..8...8...8...8.", "amp": 0.065, "kind": "brass", "length": 0.12,
           "pan": -0.2, "from_bar": 4},
    spice=lambda m, t, chords: dl.arp(m, 4 * t.bar, t.bar, chords[4:], t.bars - 4,
                                      "1.3.2.4.1.3.2.4.", 0.08, "glass", 12, 0.28),
)


# The set written to disk: eight loops, numbered 01..08.
TRACKS = (T01, T02, T03, T04, T05, T06, T07, T08)


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
