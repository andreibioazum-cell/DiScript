#!/usr/bin/env python3
"""Writes every in-game music track with no third-party libraries.

All tracks are 22.05 kHz stereo 16-bit PCM WAV:

  * ``lobbymusic.wav``    - 70 s ordinary lobby theme (4 developed sections)
  * ``winter_jingle.wav`` - 70 s New Year lobby theme (4 developed sections)
  * ``duel_theme.wav``    - 45 s Azum-vs-Astra duel theme (24 bars @ 128 BPM)

The old ``azum_vs_astra.wav`` imitation of "And The Seraphim Wept" and its
reference mp3 are gone; ``duel_theme.wav`` is a completely original, brighter
composition. Run ``python3 tools/generate_music.py`` to rebuild all three;
pass one or more file names to rebuild only those.
"""

from __future__ import annotations

from array import array
import math
from pathlib import Path
import sys
import wave

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT / "assets" / "audio"
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


def write_track(path: Path, left: array, right: array, fade_in: float, fade_out: float) -> None:
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
    scale = 0.88 / max(peak, 1e-9)
    pcm = array("h")
    for i in range(frames):
        pcm.append(int(max(-32767, min(32767, left[i] * scale * 32767))))
        pcm.append(int(max(-32767, min(32767, right[i] * scale * 32767))))
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(pcm.tobytes())
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


# ---------------------------------------------------------------------------
# Duel theme - a brand-new original 45-second battle loop (24 bars at 128 BPM).
# A minor with a C-major lift: warm pads, an eighth-note arpeggio, a singing
# lead, bright bells and light drums. No gloomy imitation, no outside sample.
# ---------------------------------------------------------------------------
DUEL_BPM = 128.0
DUEL_BEAT = 60.0 / DUEL_BPM          # 0.46875 s
DUEL_BAR = DUEL_BEAT * 4            # 1.875 s
DUEL_SLOT = DUEL_BAR * 2            # 3.75 s, 12 slots = 45 s

# (bass root, triad, 8-note lead phrase)
DUEL_SLOTS = (
    (45, (57, 60, 64), (69, 72, 76, 72, 69, 67, 64, 67)),          # Am
    (41, (53, 57, 60), (69, 72, 77, 72, 69, 65, 64, 65)),          # F
    (48, (60, 64, 67), (72, 76, 79, 76, 72, 71, 67, 72)),          # C
    (43, (55, 59, 62), (71, 74, 79, 74, 71, 67, 62, 67)),          # G
    (45, (57, 60, 64), (76, 79, 81, 79, 76, 72, 69, 72)),          # Am, lift
    (41, (53, 57, 60), (77, 81, 84, 81, 77, 76, 72, 69)),          # F, lift
    (38, (50, 53, 57), (74, 77, 81, 77, 74, 72, 69, 65)),          # Dm, bridge
    (40, (52, 56, 59), (76, 80, 83, 80, 76, 71, 68, 71)),          # E, tension
    (45, (57, 60, 64), (69, 72, 76, 81, 79, 76, 72, 69)),          # Am, return
    (41, (53, 57, 60), (72, 77, 81, 84, 81, 77, 72, 69)),          # F
    (48, (60, 64, 67), (79, 84, 88, 84, 79, 76, 72, 76)),          # C, climax
    (43, (55, 59, 62), (81, 79, 76, 74, 71, 74, 76, 69)),          # G, resolves to Am
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
    tracks = (
        ("lobbymusic.wav", lobby_track, 0.025, 0.025),
        ("winter_jingle.wav", winter_track, 0.025, 0.025),
        ("duel_theme.wav", duel_track, 0.02, 0.06),
    )
    wanted = set(argv or [])
    for name, builder, fade_in, fade_out in tracks:
        if wanted and name not in wanted:
            continue
        left, right = builder()
        write_track(AUDIO / name, left, right, fade_in, fade_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
