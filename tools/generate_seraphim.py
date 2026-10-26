#!/usr/bin/env python3
"""Generates Azum vs Astra battle track in the style of A GRAVE SOUL (NOW, RUN):
high-BPM heavy metalcore + electronic industrial (48 bars, D Phrygian/Harmonic Minor, 192 BPM)."""

from __future__ import annotations

from array import array
import math
from pathlib import Path
import wave

ROOT = Path(__file__).resolve().parents[1]
AUDIO = ROOT / "assets" / "audio"
RATE = 22050
TAU = math.tau
_noise_state = 0x5A17C9E3


def stereo(seconds: float) -> tuple[array, array]:
    frames = int(round(seconds * RATE))
    return array("f", [0.0]) * frames, array("f", [0.0]) * frames


def midi(note: float) -> float:
    return 440.0 * (2.0 ** ((note - 69.0) / 12.0))


def pan_gains(level: float, position: float) -> tuple[float, float]:
    angle = (max(-1.0, min(1.0, position)) + 1.0) * (math.pi * 0.25)
    return level * math.cos(angle), level * math.sin(angle)


def rand_bi() -> float:
    global _noise_state
    x = _noise_state
    x ^= (x << 13) & 0xFFFFFFFF
    x ^= (x >> 17) & 0xFFFFFFFF
    x ^= (x << 5) & 0xFFFFFFFF
    _noise_state = x & 0xFFFFFFFF
    return (_noise_state / 0xFFFFFFFF) * 2.0 - 1.0


def mix_mono(left: array, right: array, start: float, buf: array, level: float, pos: float = 0.0) -> None:
    begin = int(round(start * RATE))
    if begin >= len(left):
        return
    src_off = 0
    if begin < 0:
        src_off = -begin
        begin = 0
    count = min(len(buf) - src_off, len(left) - begin)
    if count <= 0:
        return
    gl, gr = pan_gains(level, pos)
    for i in range(count):
        s = buf[src_off + i]
        left[begin + i] += s * gl
        right[begin + i] += s * gr


def make_kick() -> array:
    n = int(0.20 * RATE)
    out = array("f", [0.0]) * n
    ph = 0.0
    ph_click = 0.0
    lp = 0.0
    for i in range(n):
        t = i / RATE
        # Tight metalcore double-kick: 56 Hz punch + 2.7 kHz metal beater click
        freq = 54.0 + 135.0 * math.exp(-t * 48.0)
        ph += TAU * freq / RATE
        ph_click += TAU * (2650.0 - 1400.0 * min(1.0, t * 80.0)) / RATE
        sub = math.sin(ph) * math.exp(-t * 15.5)
        beater = math.sin(ph_click) * math.exp(-t * 95.0) * 0.55
        snap = rand_bi() * math.exp(-t * 140.0) * 0.35
        lp = lp + 0.45 * (snap - lp)
        out[i] = math.tanh((sub * 1.45 + beater + lp) * 1.35)
    return out


def make_snare(accent: float = 1.0) -> array:
    n = int(0.24 * RATE)
    out = array("f", [0.0]) * n
    ph1 = 0.0
    ph2 = 0.0
    bp = 0.0
    prev = 0.0
    for i in range(n):
        t = i / RATE
        f1 = 192.0 + 90.0 * math.exp(-t * 55.0)
        f2 = 330.0 + 110.0 * math.exp(-t * 60.0)
        ph1 += TAU * f1 / RATE
        ph2 += TAU * f2 / RATE
        body = (math.sin(ph1) * 0.72 + math.sin(ph2) * 0.38) * math.exp(-t * 24.0)
        raw = rand_bi()
        hp = raw - prev
        prev = raw
        bp = bp + 0.52 * (hp - bp)
        wires = bp * math.exp(-t * 14.5) * 0.95
        crack = math.sin(TAU * 1450.0 * t) * math.exp(-t * 110.0) * 0.35
        out[i] = math.tanh((body + wires + crack) * 1.45 * accent)
    return out


def make_hat(open_hat: bool = False) -> array:
    dur = 0.22 if open_hat else 0.055
    decay = 11.5 if open_hat else 52.0
    n = int(dur * RATE)
    out = array("f", [0.0]) * n
    ratios = (317.0, 463.0, 659.0, 887.0, 1319.0, 1997.0)
    prev = 0.0
    for i in range(n):
        t = i / RATE
        metal = 0.0
        for r in ratios:
            metal += 1.0 if math.sin(TAU * r * 6.2 * t) >= 0.0 else -1.0
        raw = rand_bi() * 0.7 + metal / len(ratios) * 0.55
        hp = raw - prev
        prev = raw
        env = min(1.0, t * 700.0) * math.exp(-t * decay)
        out[i] = hp * env * 0.65
    return out


def make_china_crash() -> array:
    n = int(0.58 * RATE)
    out = array("f", [0.0]) * n
    inharm = (392.0, 547.0, 743.0, 1031.0, 1489.0, 2137.0, 3011.0)
    prev = 0.0
    lp = 0.0
    for i in range(n):
        t = i / RATE
        dish = 0.0
        for idx, f in enumerate(inharm):
            dish += math.sin(TAU * f * t + 0.8 * math.sin(TAU * (f * 0.51) * t)) * (0.9 ** idx)
        raw = rand_bi() * 0.75 + dish * 0.32
        hp = raw - prev
        prev = raw
        lp = lp + 0.62 * (hp - lp)
        env = min(1.0, t * 500.0) * math.exp(-t * 5.6)
        out[i] = math.tanh(lp * 1.65) * env * 0.82
    return out


def make_industrial_clang() -> array:
    n = int(0.30 * RATE)
    out = array("f", [0.0]) * n
    modes = (418.0, 662.0, 991.0, 1543.0, 2380.0, 3410.0)
    for i in range(n):
        t = i / RATE
        fm = math.sin(TAU * 279.0 * t) * 2.4 * math.exp(-t * 18.0)
        sig = 0.0
        for k, f in enumerate(modes):
            sig += math.sin(TAU * f * t + fm * (k + 1) * 0.35) * math.exp(-t * (9.0 + k * 3.2))
        noise = rand_bi() * math.exp(-t * 65.0) * 0.6
        out[i] = math.tanh((sig * 0.38 + noise) * 1.8) * min(1.0, t * 900.0)
    return out


_GTR_CACHE: dict[tuple[int, int, bool, bool, int], array] = {}


def render_guitar_note(note: int, dur_ms: int, palm_mute: bool, tritone: bool, detune_cents: int) -> array:
    key = (note, dur_ms, palm_mute, tritone, detune_cents)
    cached = _GTR_CACHE.get(key)
    if cached is not None:
        return cached
    n = max(1, int(round((dur_ms / 1000.0) * RATE)))
    out = array("f", [0.0]) * n
    f0 = midi(note + detune_cents / 100.0)
    f5 = f0 * (2.0 ** (6.0 / 12.0 if tritone else 7.0 / 12.0))
    f8 = f0 * 2.0
    lp = 0.0
    hp_prev_in = 0.0
    hp_prev_out = 0.0
    # Cabinet filter coefficients: darker tight chug when palm-muted, fierce bite when open
    lp_a = 0.42 if palm_mute else 0.68
    decay = 15.5 if palm_mute else 2.8
    drive = 3.6 if palm_mute else 4.5
    rel_n = max(1, int(0.012 * RATE))
    for i in range(n):
        t = i / RATE
        # Saw + pulse hybrid string excitation for rich odd & even harmonics
        p0 = (f0 * t) % 1.0
        p5 = (f5 * t) % 1.0
        p8 = (f8 * t) % 1.0
        str0 = (2.0 * p0 - 1.0) * 0.65 + (0.35 if p0 < 0.32 else -0.35)
        str5 = (2.0 * p5 - 1.0) * 0.56
        str8 = (2.0 * p8 - 1.0) * 0.38
        pick = rand_bi() * math.exp(-t * 170.0) * 0.55
        env = min(1.0, t * 650.0) * math.exp(-t * decay)
        if n - i < rel_n:
            env *= (n - i) / rel_n
        raw = (str0 + str5 + str8 + pick) * env
        # Asymmetric high-gain tube amp stage
        amp = math.tanh(raw * drive + 0.22 * raw * raw)
        # 4x12 speaker cabinet high-pass (~85 Hz) + low-pass (~2.1-3.8 kHz)
        hp = 0.975 * (hp_prev_out + amp - hp_prev_in)
        hp_prev_in = amp
        hp_prev_out = hp
        lp = lp + lp_a * (hp - lp)
        out[i] = lp
    _GTR_CACHE[key] = out
    return out


def add_double_guitar(
    left: array,
    right: array,
    start: float,
    dur: float,
    note: int,
    level: float,
    palm_mute: bool = True,
    tritone: bool = False,
) -> None:
    dur_ms = max(28, int(round(dur * 1000.0)))
    buf_l = render_guitar_note(note, dur_ms, palm_mute, tritone, -5)
    buf_r = render_guitar_note(note, dur_ms, palm_mute, tritone, 5)
    # Slight 3.5 ms Haas offset between L and R guitars for massive metalcore width
    mix_mono(left, right, start, buf_l, level, -0.78)
    mix_mono(left, right, start + 0.0035, buf_r, level, 0.78)


def add_metal_bass(left: array, right: array, start: float, dur: float, note: int, level: float) -> None:
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(dur * RATE)), len(left) - begin)
    if count <= 0:
        return
    f0 = midi(note)
    rel_n = max(1, int(0.015 * RATE))
    lp = 0.0
    for i in range(count):
        t = i / RATE
        env = min(1.0, t * 500.0) * math.exp(-t * 4.5)
        if count - i < rel_n:
            env *= (count - i) / rel_n
        sub = math.sin(TAU * f0 * t) * 0.78
        grind = math.tanh((2.0 * ((f0 * 2.0 * t) % 1.0) - 1.0) * 2.8) * 0.48
        lp = lp + 0.36 * (grind - lp)
        val = (sub + lp) * env * level
        left[begin + i] += val
        right[begin + i] += val


def add_shred_lead(
    left: array,
    right: array,
    start: float,
    dur: float,
    note: float,
    level: float,
    pos: float = 0.0,
    glide_from: float | None = None,
) -> None:
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(dur * RATE)), len(left) - begin)
    if count <= 0:
        return
    f_end = midi(note)
    f_start = midi(glide_from) if glide_from is not None else f_end
    gl, gr = pan_gains(level, pos)
    ph = 0.0
    lp = 0.0
    rel_n = max(1, int(0.020 * RATE))
    for i in range(count):
        t = i / RATE
        glide_k = min(1.0, t / 0.055) if glide_from is not None else 1.0
        vib = math.sin(TAU * 6.4 * t) * 0.007 * min(1.0, max(0.0, (t - 0.05) * 8.0))
        freq = (f_start + (f_end - f_start) * glide_k) * (1.0 + vib)
        ph += freq / RATE
        p1 = ph % 1.0
        p2 = (ph * 2.002) % 1.0
        osc = (2.0 * p1 - 1.0) * 0.70 + (2.0 * p2 - 1.0) * 0.30 + math.sin(TAU * p1) * 0.35
        env = min(1.0, t * 380.0) * math.exp(-t * 1.9)
        if count - i < rel_n:
            env *= (count - i) / rel_n
        sat = math.tanh(osc * 3.1) * env
        lp = lp + 0.58 * (sat - lp)
        left[begin + i] += lp * gl
        right[begin + i] += lp * gr


def add_industrial_synth(
    left: array,
    right: array,
    start: float,
    dur: float,
    note: float,
    level: float,
    pos: float = 0.0,
    siren_drop: float = 0.0,
) -> None:
    begin = max(0, int(round(start * RATE)))
    count = min(int(round(dur * RATE)), len(left) - begin)
    if count <= 0:
        return
    gl, gr = pan_gains(level, pos)
    ph_a = 0.0
    ph_b = 0.0
    ph_m = 0.0
    rel_n = max(1, int(0.014 * RATE))
    for i in range(count):
        t = i / RATE
        frac = i / max(1, count)
        cur_note = note + siren_drop * (1.0 - frac)
        fa = midi(cur_note - 0.11)
        fb = midi(cur_note + 0.11)
        fm_f = fa * 1.5
        ph_a += fa / RATE
        ph_b += fb / RATE
        ph_m += fm_f / RATE
        mod = math.sin(TAU * ph_m) * 0.42
        saw_a = 2.0 * ((ph_a + mod * 0.12) % 1.0) - 1.0
        saw_b = 2.0 * ((ph_b - mod * 0.12) % 1.0) - 1.0
        env = min(1.0, t * 520.0) * math.exp(-t * 3.4)
        if count - i < rel_n:
            env *= (count - i) / rel_n
        sig = math.tanh((saw_a + saw_b) * 1.65) * env
        left[begin + i] += sig * gl
        right[begin + i] += sig * gr


def grave_soul_track() -> tuple[array, array]:
    # 48 bars at 192 BPM in D Phrygian / Harmonic Minor = exact 60.00s seamless loop.
    bpm = 192.0
    beat = 60.0 / bpm          # 0.3125 s
    step = beat / 4.0          # 16th note = 0.078125 s
    bar = beat * 4.0           # 1.25 s
    bars = 48
    total = bars * bar         # 60.00 s
    left, right = stereo(total)

    kick = make_kick()
    snare = make_snare(1.0)
    snare_ghost = make_snare(0.55)
    hat_c = make_hat(False)
    hat_o = make_hat(True)
    china = make_china_crash()
    clang = make_industrial_clang()

    # Riff patterns: 16 steps per bar -> (offset_semitones_from_D2, palm_mute, tritone, gate_steps)
    # D2 = MIDI 38; Eb2 = +1; F2 = +3; G2 = +5; G#2 (tritone) = +6; A2 = +7; Bb2 = +8; C2 = -2; C#2 = -1
    riff_intro = [
        (0, False, False, 2), (0, True, False, 1), (0, True, False, 1),
        (1, False, False, 2), (0, True, False, 1), (0, True, False, 1),
        (6, False, True, 2),  (0, True, False, 1), (0, True, False, 1),
        (3, False, False, 2), (1, False, False, 2),
    ]
    riff_verse_a = [
        (0, True, False, 1), (0, True, False, 1), (3, False, False, 2),
        (0, True, False, 1), (0, True, False, 1), (1, False, False, 2),
        (0, True, False, 1), (0, True, False, 1), (6, False, True, 2),
        (5, False, False, 1), (3, False, False, 1), (1, False, False, 2),
    ]
    riff_verse_b = [
        (0, True, False, 1), (0, True, False, 1), (8, False, False, 2),
        (0, True, False, 1), (7, False, False, 2), (0, True, False, 1),
        (6, False, True, 2), (5, False, False, 2),
        (3, False, False, 2), (-1, False, False, 2),
    ]
    riff_breakdown = [
        (0, True, False, 1), (0, True, False, 1), (0, True, False, 1), (0, False, False, 1),
        (1, True, False, 2), (0, True, False, 1), (0, True, False, 1),
        (6, False, True, 2), (0, True, False, 1), (0, True, False, 1),
        (1, True, False, 1), (0, True, False, 1), (-1, False, False, 2),
    ]

    chorus_roots = [0, 8, 5, 7, 0, 8, 3, 7]  # Dm - Bb - Gm - A - Dm - Bb - F - A

    def stamp_riff(b_idx: int, pattern: list[tuple[int, bool, bool, int]], root_base: int = 38, g_lvl: float = 0.27):
        t0 = b_idx * bar
        pos_s = 0
        for semi, pm, tri, length in pattern:
            st = t0 + pos_s * step
            dur = length * step * (0.86 if pm else 1.04)
            n = root_base + semi
            add_double_guitar(left, right, st, dur, n, g_lvl, palm_mute=pm, tritone=tri)
            add_metal_bass(left, right, st, dur, n - 12, 0.24)
            pos_s += length

    # 1. Guitars, Bass & Drums across all 48 bars
    for b in range(bars):
        t0 = b * bar
        in_intro = b < 4
        in_verse = 4 <= b < 12
        in_chorus = 12 <= b < 20
        in_break = 20 <= b < 28
        in_climax = 28 <= b < 40
        is_fill = (b % 4 == 3)

        if in_intro:
            stamp_riff(b, riff_intro, 38, 0.29)
            # Alarm industrial panic sirens ("NOW, RUN!") right from t = 0.0s
            add_industrial_synth(left, right, t0, beat * 1.4, 80 if b % 2 == 0 else 81, 0.16, -0.35, siren_drop=6.0)
            add_industrial_synth(left, right, t0 + beat * 2.0, beat * 1.4, 74, 0.15, 0.35, siren_drop=7.0)
            mix_mono(left, right, t0, china, 0.34, 0.42)
            mix_mono(left, right, t0 + beat * 2.0, clang, 0.25, -0.30)
        elif in_verse:
            stamp_riff(b, riff_verse_a if b % 2 == 0 else riff_verse_b, 38, 0.27)
            if b % 2 == 0:
                mix_mono(left, right, t0, china, 0.26, -0.42)
        elif in_chorus:
            r_semi = chorus_roots[b - 12]
            for q in range(4):
                st = t0 + q * beat
                pm = (q % 2 == 1)
                add_double_guitar(left, right, st, beat * 1.05, 38 + r_semi, 0.28, palm_mute=pm)
                add_metal_bass(left, right, st, beat * 0.95, 26 + r_semi, 0.25)
                add_metal_bass(left, right, st + beat * 0.5, beat * 0.45, 38 + r_semi, 0.20)
            mix_mono(left, right, t0, china, 0.28, 0.45 if b % 2 == 0 else -0.45)
        elif in_break:
            stamp_riff(b, riff_breakdown, 38, 0.30)
            # Old-Roblox-Horror dissonant minor-second industrial panic stabs on off-beats
            add_industrial_synth(left, right, t0 + beat * 1.5, beat * 0.65, 75, 0.14, -0.5, siren_drop=2.0)
            add_industrial_synth(left, right, t0 + beat * 1.5, beat * 0.65, 76, 0.14, 0.5, siren_drop=-1.0)
            mix_mono(left, right, t0 + beat * 2.0, clang, 0.30, 0.22 if b % 2 == 0 else -0.22)
            mix_mono(left, right, t0, china, 0.30, -0.45)
        elif in_climax:
            r_semi = chorus_roots[(b - 28) % 8] if b < 36 else (0 if b % 2 == 0 else 1)
            if b < 36:
                for s16 in range(8):
                    st = t0 + s16 * (beat * 0.5)
                    pm = (s16 not in (0, 3, 6))
                    add_double_guitar(left, right, st, beat * 0.52, 38 + r_semi, 0.28, palm_mute=pm)
                    add_metal_bass(left, right, st, beat * 0.48, 26 + r_semi, 0.24)
            else:
                stamp_riff(b, riff_verse_a if b % 2 == 0 else riff_verse_b, 38, 0.29)
            mix_mono(left, right, t0, china, 0.30, 0.48 if b % 2 == 0 else -0.48)
            if b % 2 == 1:
                mix_mono(left, right, t0 + beat * 2.0, china, 0.25, -0.40)
        else:
            # Bars 40-47: Final Countdown sprint -> resolves into loop point
            pat = riff_intro if b >= 44 else (riff_verse_b if b % 2 == 1 else riff_verse_a)
            stamp_riff(b, pat, 38, 0.30)
            mix_mono(left, right, t0, china, 0.32, -0.46 if b % 2 == 0 else 0.46)
            mix_mono(left, right, t0 + beat * 2.0, clang, 0.24, 0.30)

        # Drum programming per section
        if in_break:
            # Half-time crushing breakdown snare on beat 3 + machine-gun double-kick bursts
            kick_steps = (0, 1, 2, 4, 5, 8, 9, 11, 12, 13, 15)
            for ks in kick_steps:
                mix_mono(left, right, t0 + ks * step, kick, 0.40)
            mix_mono(left, right, t0 + 8 * step, snare, 0.46, -0.04)
            for hs in (0, 4, 8, 12):
                mix_mono(left, right, t0 + hs * step, china if hs == 0 else hat_o, 0.22, 0.35)
        elif in_climax and b >= 36:
            # Hammering metalcore blast/thrash hybrid
            for s16 in range(16):
                mix_mono(left, right, t0 + s16 * step, kick, 0.36 if s16 % 2 == 0 else 0.30)
                if s16 % 2 == 0:
                    mix_mono(left, right, t0 + s16 * step, hat_o if s16 % 4 == 0 else hat_c, 0.18, 0.34)
                if s16 % 4 == 2:
                    mix_mono(left, right, t0 + s16 * step, snare, 0.42, -0.05)
        else:
            # High-speed 192 BPM punk-metal D-beat / double-kick gallop:
            # Kick on every 8th + 16th double-bass pickups; cracking snare on every "&" or 2 & 4 thrash!
            thrash_skank = in_intro or in_verse or b >= 40
            for s16 in range(16):
                st = t0 + s16 * step
                if thrash_skank:
                    # True 192 BPM thrash/metalcore skank beat: kick on 0,3,4,7,8,11,12,14; snare on 2,6,10,14
                    if s16 in (0, 3, 4, 7, 8, 11, 12, 15):
                        mix_mono(left, right, st, kick, 0.39)
                    if s16 in (2, 6, 10, 14):
                        mix_mono(left, right, st, snare, 0.41, -0.05)
                else:
                    # Chorus: relentless 16th-note double-bass roll + huge snare on beats 2 & 4
                    mix_mono(left, right, st, kick, 0.37 if s16 % 2 == 0 else 0.31)
                    if s16 in (4, 12):
                        mix_mono(left, right, st, snare, 0.45, -0.05)
                if s16 % 2 == 0:
                    mix_mono(left, right, st, hat_o if s16 % 4 == 2 else hat_c, 0.17, 0.32)

        # Explosive snare + double-kick fills at end of every 4-bar phrase
        if is_fill:
            for fs in range(12, 16):
                mix_mono(left, right, t0 + fs * step, snare if fs % 2 == 0 else snare_ghost, 0.40, (fs - 13.5) * 0.18)
                mix_mono(left, right, t0 + fs * step + step * 0.5, kick, 0.32)

    # 2. Shredding Lead Guitar + Electronic Industrial Arp/Hooks
    # 8-note 8th-note phrases per bar in D Phrygian / Harmonic Minor (D5=74, Eb5=75, F5=77, G5=79, G#5=80, A5=81, Bb5=82, C#6=85, D6=86)
    verse_leads = [
        (74, 75, 77, 75, 80, 79, 77, 75),
        (74, 77, 81, 80, 79, 77, 75, 73),
        (74, 75, 80, 79, 77, 75, 74, 73),
        (82, 81, 79, 77, 76, 77, 81, 85),
    ]
    chorus_leads = [
        (86, 85, 86, 81, 82, 81, 79, 77),
        (82, 81, 79, 77, 81, 79, 77, 76),
        (79, 82, 86, 85, 86, 88, 86, 85),
        (81, 85, 86, 88, 86, 85, 81, 79),
    ]
    climax_leads = [
        (86, 89, 88, 86, 85, 86, 88, 89),
        (91, 89, 88, 86, 85, 82, 81, 85),
        (86, 88, 89, 93, 91, 89, 88, 86),
        (85, 86, 88, 89, 88, 86, 85, 81),
    ]

    # Intro urgent lead motif (bars 0-3) so melody is blazing in the first 5 seconds
    intro_motif = [
        (74, 75, 80, 79, 77, 75, 74, 73),
        (74, 77, 81, 80, 82, 81, 79, 77),
        (86, 85, 81, 80, 79, 77, 75, 74),
        (75, 77, 80, 81, 85, 86, 85, 81),
    ]
    for b in range(4):
        phrase = intro_motif[b]
        for idx, n in enumerate(phrase):
            st = b * bar + idx * (beat * 0.5)
            prev_n = phrase[idx - 1] if idx > 0 else n - 2
            add_shred_lead(left, right, st, beat * 0.48, n, 0.17, 0.18, glide_from=prev_n if idx % 4 == 0 else None)

    # Verse industrial cyber-arp + guitar answers (bars 4-11)
    for b in range(4, 12):
        phrase = verse_leads[(b - 4) % 4]
        for idx, n in enumerate(phrase):
            st = b * bar + idx * (beat * 0.5)
            add_industrial_synth(left, right, st, beat * 0.42, n, 0.13, -0.28 if idx % 2 == 0 else 0.28)
            if idx % 2 == 0 or b >= 8:
                add_shred_lead(left, right, st, beat * 0.46, n, 0.15, 0.22)

    # Chorus dual-harmony metalcore leads + 16th industrial pulse (bars 12-19)
    harm_map = {76: 79, 77: 81, 79: 82, 81: 85, 82: 86, 85: 88, 86: 89, 88: 91}
    for b in range(12, 20):
        phrase = chorus_leads[(b - 12) % 4]
        for idx, n in enumerate(phrase):
            st = b * bar + idx * (beat * 0.5)
            h_note = harm_map.get(n, n + 3)
            add_shred_lead(left, right, st, beat * 0.52, n, 0.18, -0.30, glide_from=n - 2 if idx == 0 else None)
            add_shred_lead(left, right, st, beat * 0.52, h_note, 0.14, 0.34)
        # 16th-note industrial arpeggiator underneath
        r_note = 62 + chorus_roots[b - 12]
        arp_offsets = (0, 3, 7, 12)
        for s16 in range(16):
            add_industrial_synth(left, right, b * bar + s16 * step, step * 0.85, r_note + arp_offsets[s16 % 4], 0.085, 0.0)

    # Breakdown industrial glitch pulses & rising panic alarm (bars 24-27)
    for b in range(24, 28):
        for s8 in range(8):
            st = b * bar + s8 * (beat * 0.5)
            pitch = 74 + (b - 24) * 2 + (s8 % 3)
            add_industrial_synth(left, right, st, beat * 0.35, pitch, 0.13, -0.4 if s8 % 2 == 0 else 0.4, siren_drop=3.0)

    # Climax overdrive shredding solo + harmony + industrial synth (bars 28-39)
    for b in range(28, 40):
        phrase = climax_leads[(b - 28) % 4]
        for idx, n in enumerate(phrase):
            st = b * bar + idx * (beat * 0.5)
            add_shred_lead(left, right, st, beat * 0.50, n, 0.19, -0.25, glide_from=n - 1 if idx == 0 else None)
            add_shred_lead(left, right, st, beat * 0.50, n - 12, 0.13, 0.25)
            # 16th-note tremolo-picked shred bursts on every second beat
            if idx % 2 == 1:
                add_industrial_synth(left, right, st + step, step * 0.9, n + 3, 0.11, 0.38)

    # Final sprint (bars 40-47): "NOW, RUN!" siren + main motif climax resolving to D
    for b in range(40, 48):
        phrase = intro_motif[(b - 40) % 4]
        for idx, n in enumerate(phrase):
            st = b * bar + idx * (beat * 0.5)
            add_shred_lead(left, right, st, beat * 0.48, n + 12 if b >= 44 else n, 0.18, -0.22)
            add_industrial_synth(left, right, st, beat * 0.42, n, 0.13, 0.26, siren_drop=2.0 if idx == 0 else 0.0)

    return left, right


def write_track(path: Path, left: array, right: array, fade_in: float, fade_out: float) -> None:
    fade_in_n = max(1, int(fade_in * RATE))
    fade_out_n = max(1, int(fade_out * RATE))
    frames = len(left)
    peak = 0.0
    hp_l_in = 0.0
    hp_l_out = 0.0
    hp_r_in = 0.0
    hp_r_out = 0.0
    for i in range(frames):
        gain = 1.0
        if i < fade_in_n:
            gain *= i / fade_in_n
        remaining = frames - 1 - i
        if remaining < fade_out_n:
            gain *= remaining / fade_out_n
        # Remove sub-30 Hz DC offset, then master bus saturation for punchy metal loudness
        hp_l_out = 0.991 * (hp_l_out + left[i] - hp_l_in)
        hp_l_in = left[i]
        hp_r_out = 0.991 * (hp_r_out + right[i] - hp_r_in)
        hp_r_in = right[i]
        left[i] = math.tanh(hp_l_out * 1.48 * gain)
        right[i] = math.tanh(hp_r_out * 1.48 * gain)
        peak = max(peak, abs(left[i]), abs(right[i]))
    scale = 0.93 / max(peak, 1e-9)
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


def main() -> int:
    left, right = grave_soul_track()
    write_track(AUDIO / "azum_vs_astra.wav", left, right, 0.015, 0.030)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
