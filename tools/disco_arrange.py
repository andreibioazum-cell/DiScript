#!/usr/bin/env python3
"""Writes ten original royalty-free disco loops into ``assets/audio/disco``.

Everything is synthesised from scratch by ``tools/disco_synth.py`` (oscillators,
noise, envelopes) and arranged here by hand: no samples, no downloaded music,
no third-party libraries.  Each file is exactly 30.000 s of 22.05 kHz stereo
16-bit PCM and loops seamlessly.

How the seam is made to work
----------------------------
The tempo of every loop is chosen so that exactly ``bars`` bars fit in 30 s
(``bpm = 8 * bars``), therefore the seam falls on beat 1 of a bar, and the mixer
is circular: note tails that run past 30 s continue at the very beginning and
nothing is faded.  A riser is placed so it lands on the loop point, so the file
can be repeated for hours without a click or a hole.

    python3 tools/generate_disco_loops.py                 # all ten loops
    python3 tools/generate_disco_loops.py 03_mirrorball_dreams
    python3 tools/generate_disco_loops.py --list
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import disco_loops as dl  # noqa: E402
from disco_loops import Mixer  # noqa: E402

# ---------------------------------------------------------------------------
# Harmony shorthand.  A chord is (bass root, triad/seventh); note numbers are
# MIDI, so 57 is A3, 60 is middle C, 69 is A4.
# ---------------------------------------------------------------------------
Am = (45, (57, 60, 64))
Am7 = (45, (57, 60, 64, 67))
F = (41, (53, 57, 60))
Fmaj7 = (41, (53, 57, 60, 64))
C = (48, (48, 52, 55))
G = (43, (55, 59, 62))
Dm = (38, (50, 53, 57))
Dm7 = (38, (50, 53, 57, 60))
Dm9 = (38, (50, 53, 57, 64))
Em = (40, (52, 55, 59))
Em7 = (40, (52, 55, 59, 62))
Bbmaj = (46, (58, 62, 65))
Bb7 = (46, (58, 62, 65, 68))
Bb = (34, (58, 62, 65))
C7 = (48, (48, 52, 55, 58))
Cm = (36, (48, 51, 55))
Cm7 = (36, (48, 51, 55, 58))
Ab = (32, (44, 48, 51))
Abmaj7 = (32, (44, 48, 51, 55))
Gm = (43, (55, 58, 62))
Gm7 = (43, (55, 58, 62, 65))
G7 = (43, (55, 59, 62, 65))
Eb = (39, (51, 55, 58))
Ebmaj7 = (39, (51, 55, 58, 62))
Fm = (41, (53, 56, 60))
Fm7 = (41, (53, 56, 60, 63))
Bm = (47, (59, 62, 66))
Bbm = (46, (46, 49, 53))
Bbm7 = (46, (46, 49, 53, 56))
D = (50, (62, 66, 69))
A = (45, (57, 61, 64))
A7 = (45, (57, 61, 64, 67))
BbM = (46, (58, 62, 65))
Cadd9 = (48, (48, 52, 55, 62))
Gmin = (43, (55, 58, 62))
AbM = (44, (56, 60, 63))


class Track:
    """One loop: harmony, rhythm patterns, hook and the section plan."""

    def __init__(self, name: str, bars: int, intro: int, turn: int, chords, *,
                 pad, bass, drums, phrase, phrase_alt, hook, stabs=None,
                 pad2=None, spice=None, ornament=None, swing: float = 0.0):
        self.name = name
        self.bars = bars
        # A whole number of bars must fit in 30 s, so the loop point is beat one
        # and the tempo follows from the bar count: rings = 30 s / (8 * bars).
        self.bpm = 8 * bars
        self.bar = dl.SECONDS / bars if bars else 0.0
        assert abs(self.bars * self.bar - dl.SECONDS) < 1e-9
        self.intro = intro
        self.turn = turn
        assert (bars - intro - turn) % 4 == 0, f"{name}: основная часть не кратна 4 тактам"
        self.chords = list(chords)
        self.pad = pad
        self.pad2 = pad2
        self.bass = bass
        self.drums = drums
        self.phrase = phrase
        self.phrase_alt = phrase_alt
        self.hook = hook
        self.stabs = stabs
        self.spice = spice
        self.ornament = ornament
        self.swing = swing

    @property
    def main(self) -> int:
        """Bars of the main body: whole 4-bar phrases between intro and turn."""
        return self.bars - self.intro - self.turn


def scale_shift(note: int, steps: int, root: int) -> int:
    """Diatonic transposition in A natural minor / C major family.

    Only used for little ornaments, so a simple seven-note scale is enough.
    """
    intervals = (0, 2, 3, 5, 7, 8, 10)
    octave, pitch = divmod(note - root, 12)
    degrees = list(intervals) + [12]
    index = min(range(len(degrees)), key=lambda i: abs(degrees[i] - pitch))
    index += steps
    octave += index // 7
    index %= 7
    return root + octave * 12 + intervals[index]


def pad_mix(spec: dict, octave: int | None = None, detune=None) -> dict:
    """Translates a pad spec from the arrangement table into pads() keywords."""
    return {
        "attack": spec.get("attack", 0.25),
        "release": spec.get("release", 0.5),
        "send": spec.get("send", 0.2),
        "detune": spec.get("detune", (0.997, 1.002) if detune is None else detune),
        "octave": spec.get("octave", 0 if octave is None else octave),
        "sustain_level": spec.get("sustain", 0.85),
    }


def arrange(m: Mixer, t: Track) -> None:
    """Lays the whole loop out: intro, main body and turnaround."""
    bar = t.bar
    total = t.bars * bar
    chords = t.chords * (t.bars // len(t.chords) + 2)
    main_start = t.intro * bar
    turn_start = (t.intro + t.main) * bar

    # --- pads ------------------------------------------------------------
    p = t.pad
    bleed = bar * 0.18
    dl.pads(m, 0.0, bar, chords, p["amp"] * PAD_TRIM * p.get("trim", 1.0), p["kind"],
            **{**pad_mix(p), "bars": t.bars, "bleed": bleed})
    if t.pad2:
        q = t.pad2
        dl.pads(m, 0.0, bar, chords, q["amp"] * PAD_TRIM * q.get("trim", 1.0), q["kind"],
                **{**pad_mix(q, octave=12, detune=()), "bars": t.bars, "bleed": bleed})

    # --- bass: enters after the first bar, doubles in the last turnaround --
    b = t.bass
    dl.bass_line(m, bar, bar, chords[1:], t.bars - 1, b["marks"], amp=b["amp"],
                 kind=b.get("kind", "bass"), octave=b.get("octave", 0),
                 gate=b.get("gate", 0.75), send=b.get("send", 0.0),
                 swing=t.swing, extra_octave=b.get("extra_octave", False))
    if b.get("turn_double"):
        dl.bass_line(m, turn_start, bar, chords[int(turn_start / bar):], t.turn,
                     b["marks"].replace(".", "8"), amp=b["amp"] * 0.55,
                     kind=b.get("kind", "bass"), octave=b.get("octave", 0),
                     gate=0.4, swing=t.swing)

    # --- drums -----------------------------------------------------------
    for voice, spec in t.drums.items():
        rows, amp = spec[0], spec[1]
        pan = spec[2] if len(spec) > 2 else 0.0
        send = spec[3] if len(spec) > 3 else 0.0
        start_bar = spec[4] if len(spec) > 4 else 0
        if voice == "kick":
            dl.four_on_floor(m, start_bar * bar, bar, t.bars - start_bar, amp)
            continue
        dl.pattern(m, start_bar * bar, bar, t.bars - start_bar, rows, voice, amp,
                   pan, send)
    # Fill on the last bar: extra snare/clap and a tom-ish conga run.
    last = (t.bars - 1) * bar
    dl.pattern(m, last, bar, 1, "............xxxx", "snare", t.drums.get("snare", ("", 0.3))[1],
               send=0.2)
    # Short fill on the last bar only when the loop is not a bare two-bar intro.
    if t.ornament != "nofill":
        dl.pattern(m, last, bar, 1, "..............x.", "conga_hi", 0.18, 0.35, 0.1)
        dl.pattern(m, last, bar, 1, "...............x", "conga_lo", 0.2, -0.35, 0.1)

    # --- hook: the 4-bar phrase over the main body -----------------------
    h = t.hook
    hook_slot = bar * 4.0
    blocks = max(1, t.main // 4)
    for block in range(blocks):
        start = main_start + block * hook_slot
        phrase = t.phrase_alt if (block == blocks - 1 and t.phrase_alt) else t.phrase
        trans = h.get("transpose", (0,))[block % len(h.get("transpose", (0,)))]
        for step, note, length in phrase:
            moment = start + step * bar / 16.0
            if moment >= total:
                continue
            pitch = note + trans
            dl.lead(m, moment, bar, ((0, pitch, length),), h["amp"] * HOOK_TRIM, h.get("kind", "brass"),
                    h.get("pan", 0.0), gate=h.get("gate", 0.88), send=h.get("send", 0.28),
                    vib=h.get("vib", 0.0), attack=h.get("attack", 0.02),
                    release=h.get("release", 0.1), scoop=h.get("scoop", 0.0))
        if h.get("double"):
            for step, note, length in phrase:
                moment = start + step * bar / 16.0
                if moment >= total:
                    continue
                dl.lead(m, moment, bar, ((0, note + trans - 12, length),),
                        h["amp"] * HOOK_TRIM * h["double"], h.get("kind", "brass"),
                        h.get("pan", 0.0) * -1 - 0.1, gate=h.get("gate", 0.88),
                        send=h.get("send", 0.28) * 0.6, attack=0.03)
    # Pickup that lands on the loop point.
    if h.get("pickup"):
        dl.lead(m, total - bar / 8.0, bar, ((0, h["pickup"], 2),), h["amp"] * HOOK_TRIM * 0.8,
                h.get("kind", "brass"), h.get("pan", 0.0), send=0.35)

    # --- stabs, shimmer, track-specific extras ---------------------------
    if t.stabs:
        s = t.stabs
        from_bar = s.get("from_bar", t.intro + 4)
        bars = max(0, t.bars - from_bar)
        dl.stabs(m, from_bar * bar, bar, chords[from_bar:], bars, s["marks"],
                 s["amp"], s.get("kind", "brass"), s.get("length", 0.14),
                 s.get("pan", 0.0), s.get("send", 0.24), s.get("octave", 0))
        if s.get("half_time"):
            for index in range(from_bar, t.bars):
                if index % 2:
                    continue
                dl.stabs(m, index * bar + bar / 2, bar, chords[index:], 1, "x...",
                         s["amp"] * STAB_TRIM * 0.6, s.get("kind", "brass"), 0.3, 0.2, 0.2)
    if t.ornament:
        t.ornament(m, t, chords)
    if t.spice:
        t.spice(m, t, chords)

    # --- transitions -----------------------------------------------------
    dl.fx(m, 0.0, "crash", 0.6 * FX_TRIM, 0.1, 0.2)
    dl.fx(m, main_start, "crash", 0.5 * FX_TRIM, -0.1, 0.2)
    dl.fx(m, turn_start, "crash", 0.4 * FX_TRIM, 0.15, 0.2)
    dl.fx(m, total - 1.6, "riser", 0.8 * FX_TRIM, 0.0, 0.3)


def echo_beat(t: Track) -> float:
    return t.bar / 4.0


# Section balance, measured with the stem report in docs/music.md: drums land
# around -16 dBFS RMS, bass -20, pads and hook -24, effects -30.
PAD_TRIM = 2.7
HOOK_TRIM = 3.0
STAB_TRIM = 2.2
FX_TRIM = 1.5


def render(t: Track, target_rms: float = 0.18) -> None:
    mixer = Mixer()
    arrange(mixer, t)
    mixer.echo(echo_beat(t))
    rms, peak = mixer.master(1.35, target_rms)
    dl.write_loop(t.name, mixer)
    print(f"  {t.bpm} BPM, {t.bars} bars, RMS {rms:.3f}, peak {peak:.3f}")
