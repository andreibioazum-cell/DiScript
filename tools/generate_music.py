#!/usr/bin/env python3
"""Writes every in-game music track with no third-party libraries.

All tracks are 22.05 kHz stereo 16-bit PCM WAV:

  * ``lobby.wav``        - 70 s ordinary lobby theme (4 developed sections)
  * ``winter_lobby.wav`` - 70 s New Year lobby theme (4 developed sections)
  * ``ambient_lobby.wav`` / ``ambient_winter.wav`` - 32 s quiet beds for the menus
  * ``ambient_lobby.wav``  - 32 s quiet ambient bed over the ordinary theme
  * ``ambient_winter.wav`` - 32 s quiet ambient bed over the New Year theme

All of them are written to ``assets/audio/tracks`` next to the eight disco loops.

The old ``azum_vs_astra.wav`` imitation of "And The Seraphim Wept" and its
reference mp3 are gone; ``duel.wav`` is a completely original, brighter
composition. Run ``python3 tools/generate_music.py`` to rebuild all three;
pass one or more file names to rebuild only those.
"""

from __future__ import annotations

from array import array
import math
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parent))

import audio_rights  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT / "assets" / "audio" / "tracks"
RATE = 22050
TAU = math.tau
_noise_state = 0x6A09E667


def stereo(seconds: float) -> tuple[array, array]:
    frames = int(round(seconds * RATE))
    return array("f", [0.0]) * frames, array("f", [0.0]) * frames


def midi(note: float) -> float:
    return 440.0 * 2.0 ** ((note - 69.0) / 12.0)


def pan(level: float, position: float) -> tuple[float, float]:
    angle = (max(-1.0, min(1.0, position)) + 1.0) * math.pi / 4.0
    return level * math.cos(angle), level * math.sin(angle)


def add_tone(
    left: array,
    right: array,
    start: float,
    duration: float,
    note: float,
    level: float,
    position: float = 0.0,
    shape: str = "soft",
    attack: float = 0.01,
    release: float = 0.18,
) -> None:
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    freq = midi(note)
    gl, gr = pan(level, position)
    attack_n = max(1, int(attack * RATE))
    release_n = max(1, int(release * RATE))
    if shape == "sine":
        partials = ((1.0, 1.0),)
    elif shape == "bass":
        partials = ((1.0, 1.0), (2.0, 0.30), (3.0, 0.12))
    elif shape == "string":
        partials = ((1.0, 1.0), (2.0, 0.34), (3.0, 0.20), (4.0, 0.09))
    elif shape == "brass":
        partials = ((1.0, 1.0), (2.0, 0.48), (3.0, 0.30), (4.0, 0.18), (5.0, 0.10))
    elif shape == "choir":
        partials = ((1.0, 1.0), (2.0, 0.32), (3.0, 0.18), (5.0, 0.09), (6.0, 0.05))
    else:
        partials = ((1.0, 1.0), (2.0, 0.18), (3.0, 0.08))
    norm = sum(weight for _, weight in partials)
    for i in range(count):
        env = min(1.0, i / attack_n)
        remaining = count - i
        if remaining < release_n:
            env *= remaining / release_n
        t = i / RATE
        value = 0.0
        for ratio, weight in partials:
            value += math.sin(TAU * freq * ratio * t) * weight
        value = value / norm * env
        left[begin + i] += value * gl
        right[begin + i] += value * gr


def add_pluck(
    left: array,
    right: array,
    start: float,
    duration: float,
    note: float,
    level: float,
    position: float = 0.0,
) -> None:
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    freq = midi(note)
    gl, gr = pan(level, position)
    for i in range(count):
        t = i / RATE
        env = min(1.0, t * 180.0) * math.exp(-4.8 * t)
        value = (math.sin(TAU * freq * t) + 0.32 * math.sin(TAU * freq * 2.01 * t)) * env / 1.32
        left[begin + i] += value * gl
        right[begin + i] += value * gr


def add_bell(
    left: array,
    right: array,
    start: float,
    duration: float,
    note: float,
    level: float,
    position: float = 0.0,
) -> None:
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    freq = midi(note)
    gl, gr = pan(level, position)
    partials = ((1.0, 1.0, 2.1), (2.01, 0.46, 3.4), (2.74, 0.30, 4.8), (4.08, 0.17, 6.2))
    for i in range(count):
        t = i / RATE
        attack = min(1.0, t * 300.0)
        value = 0.0
        for ratio, weight, decay in partials:
            value += weight * math.exp(-decay * t) * math.sin(TAU * freq * ratio * t)
        value = value * attack / 1.93
        left[begin + i] += value * gl
        right[begin + i] += value * gr


def noise_value() -> float:
    global _noise_state
    value = _noise_state
    value ^= value << 13
    value ^= value >> 17
    value ^= value << 5
    _noise_state = value & 0xFFFFFFFF
    return (_noise_state / 0xFFFFFFFF) * 2.0 - 1.0


def add_noise(
    left: array,
    right: array,
    start: float,
    duration: float,
    level: float,
    position: float = 0.0,
    decay: float = 24.0,
) -> None:
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    gl, gr = pan(level, position)
    previous = 0.0
    for i in range(count):
        t = i / RATE
        raw = noise_value()
        value = (raw - previous) * math.exp(-decay * t) * min(1.0, t * 400.0)
        previous = raw
        left[begin + i] += value * gl
        right[begin + i] += value * gr


def add_kick(left: array, right: array, start: float, level: float = 0.4) -> None:
    begin = max(0, int(round(start * RATE)))
    count = min(int(0.34 * RATE), len(left) - begin)
    phase = 0.0
    for i in range(count):
        t = i / RATE
        freq = 46.0 + 78.0 * math.exp(-t * 35.0)
        phase += TAU * freq / RATE
        value = math.sin(phase) * math.exp(-t * 10.5) * level
        left[begin + i] += value
        right[begin + i] += value


def write_track(path: Path, left: array, right: array, fade_in: float, fade_out: float,
                level: float = 1.0) -> None:
    fade_in_n = max(1, int(fade_in * RATE))
    fade_out_n = max(1, int(fade_out * RATE))
    frames = len(left)
    peak = 0.0
    for i in range(frames):
        gain = 1.0
        if i < fade_in_n:
            gain *= i / fade_in_n
        remaining = frames - 1 - i
        if remaining < fade_out_n:
            gain *= remaining / fade_out_n
        left[i] = math.tanh(left[i] * gain)
        right[i] = math.tanh(right[i] * gain)
        peak = max(peak, abs(left[i]), abs(right[i]))
    scale = 0.88 * level / max(peak, 1e-9)
    pcm = array("h")
    for i in range(frames):
        pcm.append(int(max(-32767, min(32767, left[i] * scale * 32767))))
        pcm.append(int(max(-32767, min(32767, right[i] * scale * 32767))))
    audio_rights.write_wav(path, 2, RATE, pcm.tobytes(), path.stem,
                          "tools/generate_music.py")
    print(f"{path.relative_to(ROOT)}: {frames / RATE:.2f} с")


# ---------------------------------------------------------------------------
# Ordinary lobby theme - 28 bars at 96 BPM = 70 seconds. The same 7-chord
# progression runs four times, but every section adds a little more colour so
# the longer loop never sounds like a copy/paste of a 35-second fragment.
# ---------------------------------------------------------------------------
LOBBY_CHORDS = (
    (50, (62, 65, 69)), (46, (58, 62, 65)), (53, (65, 69, 72)), (48, (60, 64, 67)),
    (43, (55, 58, 62)), (46, (58, 62, 65)), (45, (57, 61, 64)),
)
LOBBY_MELODY = (
    (74, 77, 81, 77, 74, 72, 69, 72), (70, 74, 77, 74, 70, 69, 65, 69),
    (77, 81, 84, 81, 79, 77, 72, 74), (72, 76, 79, 76, 72, 71, 67, 71),
    (67, 70, 74, 70, 67, 65, 62, 65), (70, 74, 77, 74, 72, 70, 69, 65),
    (69, 73, 76, 73, 69, 67, 64, 61),
)


def lobby_track() -> tuple[array, array]:
    left, right = stereo(70.0)
    beat = 0.625
    bar = beat * 4
    chords = LOBBY_CHORDS * 4
    melody = LOBBY_MELODY * 4
    for index, (root, tones) in enumerate(chords):
        section = index // 7
        start = index * bar
        for voice, note in enumerate(tones):
            add_tone(left, right, start, bar + 0.05, note, 0.105 + 0.008 * section,
                     (voice - 1) * 0.55, "string", 0.12, 0.35)
            if section >= 2:
                add_tone(left, right, start, bar + 0.05, note + 12, 0.032,
                         (voice - 1) * 0.35, "soft", 0.35, 0.6)
        for pulse in range(8):
            lift = 12 if pulse in (3, 7) else 0
            add_pluck(left, right, start + pulse * beat / 2, beat * 0.48, root + lift, 0.23, -0.08)
            if section >= 1 and pulse % 2 == 1:
                add_bell(left, right, start + pulse * beat / 2, 1.1, tones[pulse % 3] + 12,
                         0.04 + 0.012 * section, 0.42)
        for pulse, note in enumerate(melody[index]):
            if pulse % 2 == 0 or section > 0:
                add_pluck(left, right, start + pulse * beat / 2, beat * 0.72, note, 0.13, 0.24)
            if section >= 3 and pulse % 4 == 0:
                add_tone(left, right, start + pulse * beat / 2, beat * 1.3, note + 12, 0.038,
                         -0.3, "choir", 0.06, 0.35)
        for pulse in range(4):
            add_kick(left, right, start + pulse * beat, 0.26 if section != 2 else 0.2)
            add_noise(left, right, start + pulse * beat + beat / 2, 0.055, 0.065, 0.35, 35.0)
        for pulse in (1, 3):
            add_noise(left, right, start + pulse * beat, 0.15, 0.12, -0.15, 13.0)
    return left, right


# ---------------------------------------------------------------------------
# New Year lobby theme - also 28 bars = 70 seconds, with the same gradual
# build: pure bells first, then sleigh shimmer, octave pads and a bright finale.
# ---------------------------------------------------------------------------
WINTER_ROOTS = (55, 52, 48, 50, 55, 48, 50)
WINTER_CHORDS = (
    (67, 71, 74), (64, 67, 71), (60, 64, 67), (62, 66, 69),
    (67, 71, 74), (60, 64, 67), (62, 66, 69),
)
WINTER_MELODY = (
    (79, 81, 83, 86, 83, 81, 79, 74), (76, 79, 83, 81, 79, 76, 74, 71),
    (72, 76, 79, 84, 79, 76, 74, 72), (74, 78, 81, 86, 81, 78, 76, 74),
    (79, 83, 86, 91, 86, 83, 81, 79), (84, 83, 79, 76, 79, 76, 74, 72),
    (74, 76, 78, 81, 79, 78, 74, 71),
)


def winter_track() -> tuple[array, array]:
    left, right = stereo(70.0)
    beat = 0.625
    bar = beat * 4
    roots = WINTER_ROOTS * 4
    chords = WINTER_CHORDS * 4
    melody = WINTER_MELODY * 4
    for index, tones in enumerate(chords):
        section = index // 7
        start = index * bar
        for voice, note in enumerate(tones):
            add_tone(left, right, start, bar + 0.08, note - 12, 0.075 + 0.008 * section,
                     (voice - 1) * 0.6, "string", 0.2, 0.5)
            if section >= 2:
                add_tone(left, right, start, bar + 0.08, note, 0.028,
                         (voice - 1) * 0.4, "soft", 0.4, 0.7)
        for pulse in range(8):
            position = -0.35 if pulse % 2 == 0 else 0.35
            add_bell(left, right, start + pulse * beat / 2, 1.0, melody[index][pulse], 0.26, position)
            add_noise(left, right, start + pulse * beat / 2, 0.09, 0.055, -position, 28.0)
            if section >= 1 and pulse % 2 == 0:
                add_bell(left, right, start + pulse * beat / 2, 0.9, melody[index][pulse] + 12,
                         0.035 + 0.012 * section, -position)
        for pulse in range(4):
            add_pluck(left, right, start + pulse * beat, beat * 0.8,
                      roots[index] + (12 if pulse == 3 else 0), 0.15, 0.0)
        if section >= 3:
            for pulse in range(8):
                add_pluck(left, right, start + pulse * beat / 2, beat * 0.35,
                          roots[index] + 24, 0.05, 0.2 if pulse % 2 else -0.2)
    return left, right



AMBIENT_LENGTH = 32.0   # ambient beds loop every 32 s


def ambient_track(chords, seed_beats) -> tuple[array, array]:
    """A quiet 32 s ambient bed for the menus of one theme.

    No melody and no drums: only long, slowly breathing pads with a soft
    shimmer on top, so it sits under the interface instead of competing with
    it. ``chords`` is a four-chord progression (MIDI roots) and ``seed_beats``
    walk the shimmer one scale step at a time; both are fixed, so every build
    makes the same file.
    """
    left, right = stereo(AMBIENT_LENGTH)
    slot = AMBIENT_LENGTH / len(chords)
    # Two seeded random walks: which pad voice moves and which shimmer note.
    walk = []
    step = 0
    for i in range(64):
        step = (step + (1 if (i * 7 + 3) % 5 else -1) + 4) % 4
        walk.append(step)
        del i
    scale = (0, 2, 4, 7, 9, 12, 14, 16)
    for index, root in enumerate(chords):
        start = index * slot
        # Three detuned voices, each an octave-spread triad, breathing in.
        for voice, offset in enumerate((0, 7, 12)):
            note = root + offset
            drift = walk[(index * 3 + voice) % len(walk)]
            add_tone(left, right, start, slot + 1.6, note, 0.20,
                     (voice - 1) * 0.5, "soft", 1.6, 2.2)
            add_tone(left, right, start + 0.4, slot + 1.4, note + 12, 0.055,
                     -(voice - 1) * 0.4, "choir", 2.0, 2.4)
            if drift >= 2:
                add_tone(left, right, start + slot * 0.45, slot * 0.9, note + 19, 0.03,
                         0.35 if voice else -0.35, "sine", 1.8, 2.0)
        # Long bass pedal under the chord, with a gentle fifth.
        add_tone(left, right, start, slot + 1.2, root - 12, 0.16, 0.0, "bass", 0.9, 2.0)
        add_tone(left, right, start + slot * 0.5, slot * 0.9, root - 5, 0.055, 0.0,
                 "sine", 1.2, 1.8)
        # Shimmer: two soft bells per chord, panned across the field.
        for pulse in range(2):
            note = root + 24 + scale[seed_beats[(index * 2 + pulse) % len(seed_beats)] % len(scale)]
            add_bell(left, right, start + slot * (0.25 + 0.5 * pulse), 2.4, note, 0.05,
                     0.45 if pulse == 0 else -0.45)
    return left, right


def lobby_ambient_track() -> tuple[array, array]:
    """Ordinary theme: warm A minor pads (Am - F - C - G)."""
    return ambient_track((57, 53, 48, 55), (0, 3, 5, 2, 7, 4, 1, 6))


def winter_ambient_track() -> tuple[array, array]:
    """New Year theme: bright D major pads with a sleigh-bell shimmer (D - Bm - G - A)."""
    return ambient_track((50, 47, 43, 45), (4, 7, 5, 2, 6, 3, 7, 5))


DUEL_BEAT = 60.0 / 128.0
DUEL_SLOT = DUEL_BEAT * 4
DUEL_SLOTS = (
    (57, (57, 60, 64), (76, 72, 69, 72)), (57, (57, 60, 64), (76, 79, 81, 79)),
    (53, (53, 57, 60), (72, 74, 77, 74)), (55, (55, 59, 62), (74, 76, 79, 77)),
    (57, (57, 60, 64), (81, 79, 76, 72)), (57, (57, 60, 64), (84, 81, 79, 76)),
    (48, (48, 52, 55), (79, 76, 72, 76)), (55, (55, 59, 62), (83, 79, 74, 79)),
    (57, (57, 60, 64), (76, 77, 79, 81)), (52, (52, 55, 59), (79, 76, 72, 74)),
    (53, (53, 57, 60), (81, 84, 81, 77)), (55, (55, 59, 62), (79, 83, 86, 83)),
    (57, (57, 60, 64), (84, 81, 79, 76)), (52, (52, 55, 59), (76, 79, 83, 79)),
    (53, (53, 57, 60), (77, 81, 84, 81)), (55, (55, 59, 62), (79, 83, 86, 88)),
    (57, (57, 60, 64), (88, 84, 81, 84)), (57, (57, 60, 64), (86, 84, 81, 79)),
    (48, (48, 52, 55), (84, 81, 76, 81)), (55, (55, 59, 62), (83, 84, 86, 83)),
    (57, (57, 60, 64), (81, 84, 88, 84)), (52, (52, 55, 59), (79, 83, 86, 83)),
    (53, (53, 57, 60), (84, 81, 77, 81)), (57, (57, 60, 64), (88, 84, 81, 76)),
)


def duel_track() -> tuple[array, array]:
    left, right = stereo(45.0)
    beat = DUEL_BEAT
    slot = DUEL_SLOT
    for index, (root, triad, phrase) in enumerate(DUEL_SLOTS):
        start = index * slot
        section = 0 if index < 4 else (1 if index < 6 else (2 if index < 8 else 3))
        swell = (1.0, 1.08, 0.82, 1.16)[section]

        # Warm three-voice pad, gently spread across the stereo field.
        for voice, note in enumerate(triad):
            pos = (voice - 1) * 0.55
            add_tone(left, right, start, slot + 0.12, note, 0.095 * swell, pos, "string", 0.28, 0.55)
            add_tone(left, right, start, slot + 0.12, note + 12, 0.038 * swell, -pos * 0.6,
                     "choir", 0.4, 0.6)

        # Round bass: a low body plus a mid octave that phone speakers hear.
        add_tone(left, right, start, slot + 0.1, root - 12, 0.15, 0.0, "bass", 0.03, 0.3)
        add_tone(left, right, start, slot + 0.1, root, 0.12, 0.0, "soft", 0.02, 0.25)

        # Eighth-note arpeggio, alternating left/right for a wide picture.
        for pulse in range(16):
            t = start + pulse * beat / 2
            note = triad[(pulse // 2) % 3] + (12 if pulse % 4 == 3 else 0)
            add_pluck(left, right, t, beat * 0.46, note + 12, 0.115 * swell,
                      0.32 if pulse % 2 else -0.32)

        # Singing lead with a soft bell an octave above on the strong beats.
        for pulse, note in enumerate(phrase):
            t = start + pulse * beat
            dur = beat * (1.35 if pulse % 4 == 3 else 0.94)
            pos = 0.2 if pulse % 2 == 0 else -0.2
            add_tone(left, right, t, dur, note, 0.145 * swell, pos, "choir", 0.02, 0.22)
            add_pluck(left, right, t, beat * 0.78, note, 0.085 * swell, -pos * 0.7)
            if pulse % 2 == 0:
                add_bell(left, right, t, 1.05, note + 12, 0.075 * swell, pos * 0.8)
            if section == 3 and pulse == 0:
                add_tone(left, right, t, beat * 3.2, note - 12, 0.07, -0.15, "brass", 0.08, 0.7)

        # Light but steady drums; the bridge (section 2) breathes.
        for b in range(8):
            t = start + b * beat
            if section != 2 or b % 2 == 0:
                add_kick(left, right, t, 0.3 * swell)
            add_noise(left, right, t + beat / 2, 0.05, 0.06, 0.34, 34.0)
            if b % 2 == 1:
                add_noise(left, right, t, 0.13, 0.105 * swell, -0.18, 14.0)

        # A high shimmer in the last section glues the loop back to the start.
        if section == 3:
            for pulse in range(8):
                add_bell(left, right, start + pulse * beat / 2, 0.8,
                         triad[pulse % 3] + 24, 0.04, 0.4 if pulse % 2 else -0.4)
    return left, right


def main(argv: list[str] | None = None) -> int:
    # The ambient beds are deliberately quieter than the themes (level ~1/3):
    # they sit under the interface, they never lead it.
    tracks = (
        ("lobby.wav", lobby_track, 0.025, 0.025, 1.0),
        ("winter_lobby.wav", winter_track, 0.025, 0.025, 1.0),
        ("ambient_lobby.wav", lobby_ambient_track, 0.9, 0.9, 0.34),
        ("ambient_winter.wav", winter_ambient_track, 0.9, 0.9, 0.34),
    )
    wanted = set(argv or [])
    for name, builder, fade_in, fade_out, level in tracks:
        if wanted and name not in wanted:
            continue
        left, right = builder()
        write_track(AUDIO / name, left, right, fade_in, fade_out, level)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
