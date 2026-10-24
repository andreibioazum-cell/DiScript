#!/usr/bin/env python3
"""Generates Azum vs Astra battle track in style of And_The_Seraphim_Wept (32 bars, D minor, 112 BPM)."""

from __future__ import annotations

from array import array
import math
from pathlib import Path
import wave

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT / "assets" / "audio"
RATE = 22050
TAU = math.tau
_noise_state = 0x6A09E667


def stereo(seconds: float):
    frames = int(round(seconds * RATE))
    return array("f", [0.0]) * frames, array("f", [0.0]) * frames


def midi(note: float) -> float:
    return 440.0 * 2.0 ** ((note - 69.0) / 12.0)


def pan(level: float, position: float):
    angle = (max(-1.0, min(1.0, position)) + 1.0) * math.pi / 4.0
    return level * math.cos(angle), level * math.sin(angle)


def add_tone(left, right, start, duration, note, level, position=0.0, shape="soft", attack=0.01, release=0.18):
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
        partials = ((1.0, 0.85), (2.0, 0.65), (3.0, 0.35), (4.0, 0.16))
    elif shape == "string":
        partials = ((1.0, 1.0), (2.0, 0.48), (3.0, 0.28), (4.0, 0.14))
    elif shape == "brass":
        partials = ((1.0, 1.0), (2.0, 0.56), (3.0, 0.36), (4.0, 0.22), (5.0, 0.12))
    elif shape == "choir":
        partials = ((1.0, 1.0), (2.0, 0.38), (3.0, 0.24), (5.0, 0.12), (6.0, 0.07))
    else:
        partials = ((1.0, 1.0), (2.0, 0.22), (3.0, 0.10))
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


def add_pluck(left, right, start, duration, note, level, position=0.0):
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(duration * RATE)), len(left) - begin)
    if count <= 0:
        return
    freq = midi(note)
    gl, gr = pan(level, position)
    for i in range(count):
        t = i / RATE
        env = min(1.0, t * 180.0) * math.exp(-4.4 * t)
        value = (math.sin(TAU * freq * t) + 0.42 * math.sin(TAU * freq * 2.01 * t) +
                 0.20 * math.sin(TAU * freq * 3.0 * t)) * env / 1.62
        left[begin + i] += value * gl
        right[begin + i] += value * gr


def add_bell(left, right, start, duration, note, level, position=0.0):
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


def add_noise(left, right, start, duration, level, position=0.0, decay=24.0):
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


def add_kick(left, right, start, level=0.4):
    begin = max(0, int(round(start * RATE)))
    count = min(int(0.32 * RATE), len(left) - begin)
    phase = 0.0
    for i in range(count):
        t = i / RATE
        freq = 54.0 + 96.0 * math.exp(-t * 36.0)
        phase += TAU * freq / RATE
        click = math.sin(TAU * 190.0 * t) * math.exp(-t * 48.0) * 0.35
        value = (math.sin(phase) * math.exp(-t * 10.5) + click) * level
        left[begin + i] += value
        right[begin + i] += value


def write_track(path, left, right, fade_in, fade_out):
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
        # Soft saturation gives consistent loudness across the whole battle loop
        left[i] = math.tanh(left[i] * 1.35 * gain)
        right[i] = math.tanh(right[i] * 1.35 * gain)
        peak = max(peak, abs(left[i]), abs(right[i]))
    scale = 0.92 / max(peak, 1e-9)
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
    print(f"{path.relative_to(ROOT)}: {frames / RATE:.2f} s peak {peak:.3f}")


def seraphim_track():
    # 32-bar seamless battle loop at 112 BPM in D minor (~68.57s).
    # Battles last ~10-25s, so the weeping lead melody, choir, bells, bass, and
    # drums hit immediately at t = 0.0s with strong mid/high presence for both
    # phone speakers and desktop headphones.
    bpm = 112.0
    beat = 60.0 / bpm
    bar = beat * 4
    chord_roots = [38, 43, 46, 45, 38, 41, 43, 45, 38, 43, 36, 41, 38, 45, 38, 45]
    chord_voicings = [
        (50, 53, 57),  # Dm
        (55, 58, 62),  # Gm
        (58, 62, 65),  # Bb
        (57, 61, 64),  # A
        (50, 53, 57),  # Dm
        (53, 57, 60),  # F
        (55, 58, 62),  # Gm
        (57, 61, 64),  # A
        (50, 53, 57),  # Dm
        (55, 58, 62),  # Gm
        (48, 52, 55),  # C
        (53, 57, 60),  # F
        (50, 53, 57),  # Dm
        (57, 61, 64),  # A
        (50, 53, 57),  # Dm
        (57, 61, 64),  # A -> resolves cleanly back to Dm on loop
    ]
    total = len(chord_roots) * bar * 2
    left, right = stereo(total)

    melody_phrases = [
        (74, 77, 81, 79, 77, 76, 74, 73),
        (79, 82, 86, 84, 82, 81, 79, 77),
        (77, 74, 70, 74, 77, 81, 79, 77),
        (76, 73, 69, 73, 76, 79, 77, 76),
        (74, 77, 81, 86, 84, 81, 77, 74),
        (72, 77, 81, 79, 77, 76, 74, 72),
        (79, 82, 86, 86, 84, 82, 79, 77),
        (69, 73, 76, 79, 77, 76, 74, 73),
        (86, 84, 82, 81, 79, 77, 76, 74),
        (79, 82, 86, 84, 82, 81, 79, 77),
        (72, 76, 79, 84, 82, 79, 76, 72),
        (77, 81, 84, 83, 81, 79, 77, 76),
        (86, 84, 81, 79, 77, 76, 74, 73),
        (76, 73, 69, 73, 76, 81, 79, 76),
        (74, 77, 81, 86, 84, 81, 77, 74),
        (76, 79, 81, 85, 81, 79, 76, 73),
    ]

    for i, root in enumerate(chord_roots):
        start = i * bar * 2
        dur = bar * 2 + 0.08
        climax = 1.12 if i >= 8 else 1.0

        # Sub + upper-octave bass so phone speakers reproduce the bassline clearly
        add_tone(left, right, start, dur, root, 0.14 * climax, 0.0, "bass", 0.05, 0.25)
        add_tone(left, right, start, dur, root + 12, 0.15 * climax, 0.0, "bass", 0.03, 0.22)

        # Driving eighth-note bass plucks in the audible mid-bass register
        for pulse in range(16):
            lift = 24 if pulse % 4 == 3 else 12
            add_pluck(left, right, start + pulse * beat / 2, beat * 0.52, root + lift, 0.18 * climax, -0.08)

        # Gothic strings + choir pad in mid/upper octaves right from t = 0
        for v, note in enumerate(chord_voicings[i]):
            pos = (v - 1) * 0.55
            add_tone(left, right, start, dur, note + 12, 0.115 * climax, pos, "string", 0.08, 0.30)
            add_tone(left, right, start, dur, note + 12, 0.085 * climax, -pos * 0.7, "choir", 0.12, 0.35)
            if i >= 8:
                add_tone(left, right, start, dur, note + 19, 0.055, pos * 0.4, "brass", 0.15, 0.35)

        # Ethereal weeping bell arpeggios throughout the track
        voicing = chord_voicings[i]
        for arp in range(8):
            t = start + arp * beat
            bnote = voicing[arp % 3] + 24
            add_bell(left, right, t, 0.95, bnote, 0.11 * climax, 0.45 if arp % 2 == 0 else -0.45)

    # Lead weeping melody + octave choir/pluck reinforcement from the very first beat
    for idx, phrase in enumerate(melody_phrases):
        start = idx * bar * 2
        climax = 1.12 if idx >= 8 else 1.0
        for p, note in enumerate(phrase):
            t = start + p * beat
            dur = beat * 0.92 if p % 4 != 3 else beat * 1.25
            pos = 0.24 if p % 2 == 0 else -0.24
            add_tone(left, right, t, dur, note, 0.16 * climax, pos, "choir", 0.02, 0.18)
            add_pluck(left, right, t, beat * 0.80, note, 0.15 * climax, -pos * 0.6)
            if p % 2 == 0:
                add_bell(left, right, t, 1.1, note + 12, 0.08 * climax, pos * 0.8)

    # Battle percussion from beat 0: kick on every beat, snare on 2 & 4, crisp hi-hat on off-beats
    total_beats = int(round(total / beat))
    for b in range(total_beats):
        t = b * beat
        add_kick(left, right, t, 0.27)
        add_noise(left, right, t + beat / 2, 0.055, 0.075, 0.32, 34.0)
        if b % 2 == 1:
            add_noise(left, right, t, 0.14, 0.125, -0.18, 14.0)

    return left, right


def main():
    left, right = seraphim_track()
    write_track(AUDIO / "azum_vs_astra.wav", left, right, 0.025, 0.05)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
