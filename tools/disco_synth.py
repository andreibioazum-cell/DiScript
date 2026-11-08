#!/usr/bin/env python3
"""Disco synthesis engine: original loops, no samples, standard library only.

Every voice is built from oscillators, noise and envelopes, so the loops in
``assets/audio/disco`` contain no third-party recordings.  The renderer always
mixes 30.000 s of 22.05 kHz stereo 16-bit PCM and wraps every note tail around
the loop point, so the last sample flows into the first one without a gap, a
click or a fade.  That makes each file a true seamless loop.

Only ``array``, ``math``, ``struct`` and ``wave`` are used; there is no numpy
and no audio tooling to install.
"""

from __future__ import annotations

from array import array
import math

RATE = 22050
TAU = math.tau
TABLE_SIZE = 2048
_VIBRATO_SIZE = 1024
_VIBRATO = array("f", (math.sin(TAU * i / _VIBRATO_SIZE) for i in range(_VIBRATO_SIZE)))

_TABLES: dict[tuple[str, int], array] = {}


def midi(note: float) -> float:
    """MIDI note number to frequency in Hz."""
    return 440.0 * 2.0 ** ((note - 69.0) / 12.0)


# ---------------------------------------------------------------------------
# Wavetables.  Each table is additive, so the harmonic count can be capped for
# high notes and the top octave does not turn into aliasing noise.
# ---------------------------------------------------------------------------
def _partials(kind: str, cap: int) -> tuple[tuple[float, float], ...]:
    if kind == "sine":
        return ((1.0, 1.0),)
    if kind == "saw":
        return tuple((float(k), 1.0 / k) for k in range(1, cap + 1))
    if kind == "square":
        return tuple((float(k), 1.0 / k) for k in range(1, cap + 1, 2))
    if kind == "pulse":
        return tuple(
            (float(k), abs(math.sin(math.pi * k * 0.32)) / k) for k in range(1, cap + 1)
        )
    if kind == "strings":
        return tuple(
            (float(k), (1.0 / k ** 0.85) * (1.15 if k % 2 else 0.85) *
             (1.0 if k < 10 else 0.55))
            for k in range(1, cap + 1)
        )
    if kind == "brass":
        return tuple(
            (float(k), (1.0 / k) * (1.0 + 2.1 * math.exp(-((k - 4.5) / 2.3) ** 2)))
            for k in range(1, min(cap, 20) + 1)
        )
    if kind == "organ":
        return ((1.0, 1.0), (2.0, 0.66), (3.0, 0.52), (4.0, 0.44),
                (5.0, 0.16), (6.0, 0.3), (8.0, 0.19), (10.0, 0.1))
    if kind == "choir":
        return ((1.0, 1.0), (2.0, 0.42), (3.0, 0.3), (4.0, 0.12),
                (5.0, 0.16), (6.0, 0.08), (8.0, 0.05))
    if kind == "glass":
        return ((1.0, 1.0), (2.0, 0.24), (3.0, 0.36), (4.0, 0.13),
                (5.0, 0.19), (7.0, 0.1), (9.0, 0.06))
    if kind == "hollow":
        return tuple((float(k), 1.0 / k ** 1.5) for k in range(1, cap + 1, 2))
    if kind == "bass":
        return ((1.0, 1.0), (2.0, 0.44), (3.0, 0.24), (4.0, 0.13), (5.0, 0.07))
    if kind == "clav":
        return ((1.0, 1.0), (2.0, 0.52), (3.0, 0.44), (4.0, 0.3),
                (5.0, 0.22), (6.0, 0.15), (7.0, 0.11))
    raise ValueError(f"unknown wavetable: {kind}")


def table(kind: str, cap: int = 32) -> array:
    """Band-limited wavetable, cached and normalised to a peak of 1.0."""
    cap = 6 if cap <= 8 else (12 if cap <= 16 else (20 if cap <= 24 else 32))
    key = (kind, cap)
    cached = _TABLES.get(key)
    if cached is not None:
        return cached
    partials = _partials(kind, cap)
    raw = [0.0] * TABLE_SIZE
    for harmonic, weight in partials:
        step = TAU * harmonic / TABLE_SIZE
        for i in range(TABLE_SIZE):
            raw[i] += weight * math.sin(step * i)
    peak = max(max(raw), -min(raw)) or 1.0
    result = array("f", (value / peak for value in raw))
    _TABLES[key] = result
    return result


def harmonics_for(freq: float) -> int:
    """How many harmonics fit under Nyquist for this pitch."""
    room = int((RATE * 0.46) / max(freq, 20.0))
    return 6 if room < 8 else (12 if room < 16 else (20 if room < 24 else 32))


# ---------------------------------------------------------------------------
# Core oscillators and envelopes.
# ---------------------------------------------------------------------------
def tone(
    kind: str,
    freq: float,
    dur: float,
    amp: float = 0.2,
    attack: float = 0.006,
    decay: float = 0.4,
    sustain: float = 0.55,
    release: float = 0.12,
    cap: int | None = None,
    vib: float = 0.0,
    vib_rate: float = 5.0,
    phase: float = 0.0,
    scoop: float = 0.0,
    scoop_time: float = 0.035,
) -> array:
    """One note: wavetable oscillator shaped by an ADSR envelope.

    ``vib`` is vibrato depth as a fraction of the pitch, ``scoop`` is a start
    offset in semitones that glides away during ``scoop_time`` (brass/blips).
    """
    n = int(dur * RATE)
    if n <= 0 or amp <= 0.0 or freq <= 0.0:
        return array("f")
    out = array("f", bytes(4 * n))
    tab = table(kind, harmonics_for(freq) if cap is None else cap)
    size = len(tab)
    a_n = max(1, min(n, int(attack * RATE)))
    r_n = max(1, min(n, int(release * RATE)))
    d_n = max(1, int(decay * RATE))
    dk = math.exp(-4.0 / d_n)
    step = freq * size / RATE
    p = (phase % 1.0) * size
    dec = 1.0
    sus = sustain
    inv_a = 1.0 / a_n
    vib_step = _VIBRATO_SIZE * vib_rate / RATE
    vib_pos = 0.0
    scoop_n = max(1, int(scoop_time * RATE))
    scoop_ratio = 2.0 ** (scoop / 12.0)
    for i in range(n):
        if i < a_n:
            gain = i * inv_a
        else:
            gain = sus + (1.0 - sus) * dec
            dec *= dk
        remaining = n - i
        if remaining < r_n:
            gain *= remaining / r_n
        if vib:
            p += step * (1.0 + vib * _VIBRATO[vib_pos])
            vib_pos += vib_step
            if vib_pos >= _VIBRATO_SIZE:
                vib_pos -= _VIBRATO_SIZE
        elif scoop and i < scoop_n:
            p += step * (1.0 + (scoop_ratio - 1.0) * (1.0 - i / scoop_n))
        else:
            p += step
        if p >= size:
            p -= size
        out[i] = tab[int(p)] * gain * amp
    return out


def noise_note(
    dur: float,
    amp: float,
    decay: float = 30.0,
    seed: int = 12345,
    tone_hz: float = 0.0,
    tone_amp: float = 0.0,
    tone_decay: float = 20.0,
    highpass: float = 0.0,
    lowpass: float = 0.0,
) -> array:
    """Decaying noise burst, optionally mixed with a pitched body."""
    n = int(dur * RATE)
    out = array("f", bytes(4 * n))
    if n <= 0:
        return out
    state = seed & 0xFFFFFFFF or 1
    previous = 0.0
    lp_state = 0.0
    lp_alpha = 1.0 - math.exp(-TAU * lowpass / RATE) if lowpass else 0.0
    hp_alpha = 1.0 - math.exp(-TAU * highpass / RATE) if highpass else 0.0
    hp_state = 0.0
    phase = 0.0
    tone_step = TAU * tone_hz / RATE
    attack = 1.0 / max(1.0, 0.0015 * RATE)
    for i in range(n):
        state ^= (state << 13) & 0xFFFFFFFF
        state ^= state >> 17
        state ^= (state << 5) & 0xFFFFFFFF
        state &= 0xFFFFFFFF
        raw = (state / 0xFFFFFFFF) * 2.0 - 1.0
        value = raw - previous
        previous = raw
        if hp_alpha:
            hp_state += hp_alpha * (value - hp_state)
            value -= hp_state
        if lp_alpha:
            lp_state += lp_alpha * (value - lp_state)
            value = lp_state
        env = min(1.0, i * attack) * math.exp(-decay * i / RATE)
        value *= env
        if tone_amp:
            phase += tone_step
            value += math.sin(phase) * math.exp(-tone_decay * i / RATE) * tone_amp
        out[i] = value * amp
    return out


def lowpass(buf: array, cutoff: float, drive: float = 1.0) -> array:
    """One-pole low-pass with optional soft saturation, in place."""
    if cutoff >= RATE * 0.45:
        return buf
    alpha = 1.0 - math.exp(-TAU * cutoff / RATE)
    state = 0.0
    if drive == 1.0:
        for i in range(len(buf)):
            state += alpha * (buf[i] - state)
            buf[i] = state
    else:
        norm = math.tanh(drive)
        for i in range(len(buf)):
            state += alpha * (math.tanh(buf[i] * drive) / norm - state)
            buf[i] = state
    return buf


def highpass(buf: array, cutoff: float) -> array:
    """One-pole high-pass, in place."""
    alpha = 1.0 - math.exp(-TAU * cutoff / RATE)
    state = 0.0
    for i in range(len(buf)):
        state += alpha * (buf[i] - state)
        buf[i] -= state
    return buf


def sweep(buf: array, start_hz: float, end_hz: float, res: float = 0.12) -> array:
    """Resonant state-variable low-pass sweeping from start to end, in place."""
    n = len(buf)
    if n == 0:
        return buf
    low = band = 0.0
    damp = 1.0 - res
    for i in range(n):
        cutoff = start_hz * (end_hz / start_hz) ** (i / n)
        f = 2.0 * math.sin(math.pi * min(cutoff, RATE * 0.4) / RATE)
        low += f * band
        high = buf[i] - low - damp * band
        band += f * high
        buf[i] = low
    return buf


def saturate(buf: array, drive: float, normalise: bool = False) -> array:
    norm = math.tanh(drive) if normalise else 1.0
    for i in range(len(buf)):
        buf[i] = math.tanh(buf[i] * drive) / norm
    return buf


def normalise(buf: array, target_rms: float) -> array:
    """Scales a one-shot (in place) so its RMS matches the kit reference."""
    if not buf:
        return buf
    rms = math.sqrt(sum(value * value for value in buf) / len(buf))
    if rms > 1e-9:
        gain = target_rms / rms
        # Never let a transient clip: cap the peak at 0.95.
        peak = max(abs(value) for value in buf)
        gain = min(gain, 0.95 / peak if peak > 0 else gain)
        for i in range(len(buf)):
            buf[i] *= gain
    return buf


def sum_arrays(*parts: tuple[array, float]) -> array:
    """Adds scaled mono buffers into one."""
    length = max((len(buf) for buf, _ in parts), default=0)
    out = array("f", bytes(4 * length))
    for buf, gain in parts:
        for i in range(len(buf)):
            out[i] += buf[i] * gain
    return out


# ---------------------------------------------------------------------------
# Drum kit.
# ---------------------------------------------------------------------------
def kick(amp: float = 0.9, dur: float = 0.4, pitch: float = 50.0) -> array:
    n = int(dur * RATE)
    out = array("f", bytes(4 * n))
    phase = 0.0
    for i in range(n):
        t = i / RATE
        freq = pitch + (pitch * 2.5) * math.exp(-t * 30.0)
        phase += TAU * freq / RATE
        body = math.sin(phase) * math.exp(-t * 7.6)
        click = math.exp(-t * 320.0) * 0.16 * math.sin(TAU * 1500.0 * t)
        out[i] = math.tanh((body + click) * 1.7) * amp
    return out


def snare(amp: float = 0.5, dur: float = 0.25) -> array:
    noise = noise_note(dur, amp, decay=23.0, highpass=1100.0, lowpass=7000.0, seed=9182)
    body = noise_note(dur, amp * 0.85, decay=30.0, tone_hz=196.0, tone_amp=0.8,
                      tone_decay=26.0, seed=551)
    return sum_arrays((noise, 1.0), (body, 0.75))


def clap(amp: float = 0.42, dur: float = 0.26) -> array:
    out = array("f", bytes(4 * int(dur * RATE)))
    for offset, level in ((0.000, 1.0), (0.009, 0.85), (0.019, 0.7)):
        burst = noise_note(0.035, amp * level, decay=52.0, highpass=900.0,
                           lowpass=5200.0, seed=8123 + int(offset * 1000))
        begin = int(offset * RATE)
        for i in range(len(burst)):
            if begin + i < len(out):
                out[begin + i] += burst[i]
    tail = noise_note(dur * 0.8, amp * 0.4, decay=16.0, highpass=1000.0,
                      lowpass=4600.0, seed=771)
    for i in range(len(tail)):
        out[i] += tail[i]
    return out


def hat(amp: float = 0.18, dur: float = 0.055, open_hat: bool = False,
        bright: bool = False) -> array:
    """Closed/open hat. ``bright`` lifts the band so it survives phone speakers."""
    decay = 7.5 if open_hat else 46.0
    length = max(dur, 0.26 if open_hat else dur)
    high = 8200.0 if bright else 6800.0
    return noise_note(length, amp * (1.6 if bright else 1.0), decay=decay,
                      highpass=high, seed=3211)


def shaker(amp: float = 0.08, dur: float = 0.09) -> array:
    return noise_note(dur, amp, decay=26.0, highpass=4200.0, lowpass=9000.0, seed=4412)


def tambourine(amp: float = 0.12, dur: float = 0.14) -> array:
    jingle = noise_note(dur, amp, decay=17.0, highpass=5200.0, lowpass=10000.0, seed=6060)
    ring = 0.0
    n = len(jingle)
    for i in range(n):
        t = i / RATE
        ring = (math.sin(TAU * 5230.0 * t) + math.sin(TAU * 6710.0 * t)
                + 0.6 * math.sin(TAU * 8420.0 * t)) * math.exp(-t * 24.0) * amp * 0.22
        jingle[i] += ring
    return jingle


def cowbell(amp: float = 0.16, dur: float = 0.22) -> array:
    n = int(dur * RATE)
    out = array("f", bytes(4 * n))
    for i in range(n):
        t = i / RATE
        env = math.exp(-t * 18.0) * min(1.0, t * 400.0)
        value = (math.sin(TAU * 543.0 * t) + math.sin(TAU * 807.0 * t)
                 + 0.5 * math.sin(TAU * 1190.0 * t))
        out[i] = math.tanh(value * 1.5) * env * amp
    return out


def conga(amp: float = 0.3, pitch: float = 210.0, dur: float = 0.18) -> array:
    n = int(dur * RATE)
    out = array("f", bytes(4 * n))
    phase = 0.0
    for i in range(n):
        t = i / RATE
        freq = pitch * (1.0 + 0.55 * math.exp(-t * 45.0))
        phase += TAU * freq / RATE
        out[i] = math.sin(phase) * math.exp(-t * 19.0) * amp
    return out


def rim(amp: float = 0.24, dur: float = 0.09) -> array:
    return noise_note(dur, amp, decay=48.0, highpass=1500.0, lowpass=5000.0,
                      tone_hz=420.0, tone_amp=0.5, tone_decay=52.0, seed=2323)


def crash(amp: float = 0.22, dur: float = 1.2) -> array:
    shimmer = noise_note(dur, amp, decay=4.6, highpass=3600.0, lowpass=10500.0, seed=9111)
    return shimmer


def riser(dur: float = 1.4, amp: float = 0.16) -> array:
    """Noise sweep that rises into the next section."""
    raw = noise_note(dur, 0.55, decay=0.4, highpass=400.0, lowpass=6500.0, seed=4711)
    sweep(raw, 600.0, 6500.0, res=0.55)
    n = len(raw)
    phase = 0.0
    for i in range(n):
        fade = (i / n) ** 1.8
        phase += TAU * (180.0 + 900.0 * fade) / RATE
        raw[i] = math.tanh((raw[i] + 0.25 * math.sin(phase)) * (0.4 + fade) * 2.2) * amp * fade
    return raw


def laser(amp: float = 0.2, dur: float = 0.32, high: float = 2400.0) -> array:
    n = int(dur * RATE)
    out = array("f", bytes(4 * n))
    phase = 0.0
    for i in range(n):
        t = i / RATE
        freq = high * math.exp(-t * 9.0) + 160.0
        phase += TAU * freq / RATE
        out[i] = math.sin(phase) * math.exp(-t * 8.5) * amp
    return out
