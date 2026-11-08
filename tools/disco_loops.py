#!/usr/bin/env python3
"""Disco loop engine: circular stereo mixer, sequencer helpers, WAV writer.

The mixer never fades anything: buffers wrap around the loop point, so a note
that starts near 29.9 s finishes at the top of the file.  That is what makes the
thirty-second files in ``assets/audio/disco`` seamless loops.
"""

from __future__ import annotations

from array import array
import math
import sys
import wave
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import disco_synth as ds  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
# A separate soundtrack folder, so the ten loops never end up inside the
# APK/PC build (assets/audio is copied into both by stage_assets.py).
OUT = ROOT / "disco"
RATE = ds.RATE
SECONDS = 30.0
FRAMES = int(SECONDS * RATE)
TAU = math.tau

# One line of 16 characters is one 4/4 bar of sixteenth notes.
VELOCITY = {"X": 1.15, "x": 1.0, "8": 1.0, "o": 0.72, ":": 0.5, "-": 0.34}


def midi(note: float) -> float:
    return ds.midi(note)


def pan_gains(position: float, gain: float) -> tuple[float, float]:
    angle = (max(-1.0, min(1.0, position)) + 1.0) * math.pi / 4.0
    return gain * math.cos(angle), gain * math.sin(angle)


class Mixer:
    """Circular dry bus plus an echo send bus."""

    def __init__(self) -> None:
        self.dry_left = array("f", bytes(4 * FRAMES))
        self.dry_right = array("f", bytes(4 * FRAMES))
        self.send_left = array("f", bytes(4 * FRAMES))
        self.send_right = array("f", bytes(4 * FRAMES))

    def hit(self, start: float, buf: array, gain: float = 1.0, pan: float = 0.0,
            send: float = 0.0) -> None:
        if not buf or gain == 0.0:
            return
        begin = int(round(start * RATE)) % FRAMES
        gl, gr = pan_gains(pan, gain)
        left, right = self.dry_left, self.dry_right
        n = len(buf)
        for i in range(n):
            k = begin + i
            if k >= FRAMES:
                k -= FRAMES
            value = buf[i]
            left[k] += value * gl
            right[k] += value * gr
        if send:
            sl, sr = pan_gains(pan, gain * send)
            send_left, send_right = self.send_left, self.send_right
            for i in range(n):
                k = begin + i
                if k >= FRAMES:
                    k -= FRAMES
                send_left[k] += buf[i] * sl
                send_right[k] += buf[i] * sr

    def sustain(self, kind: str, freq: float, start: float, duration: float,
                amp: float, pan: float = 0.0, attack: float = 0.02,
                decay: float = 0.5, sustain_level: float = 0.85,
                release: float = 0.3, vib: float = 0.0, send: float = 0.0,
                detune: tuple[float, ...] = ()) -> None:
        """Sustained/plucked note rendered straight onto the circular timeline."""
        if duration <= 0.0 or amp == 0.0 or freq <= 0.0:
            return
        n = int(duration * RATE)
        if n <= 0:
            return
        begin = int(round(start * RATE)) % FRAMES
        copies = detune or (1.0,)
        attack_n = max(1, int(attack * RATE))
        release_n = max(1, int(min(release, duration * 0.9) * RATE))
        decay_mul = math.exp(-3.6 / max(1.0, decay * RATE))
        tab = ds.table(kind, ds.harmonics_for(freq))
        size = len(tab)
        left, right = self.dry_left, self.dry_right
        send_left, send_right = self.send_left, self.send_right
        spread = 0.55 if len(copies) > 1 else 0.0
        step_factor = 1.0 / math.sqrt(len(copies))
        for copy, ratio in enumerate(copies):
            side = pan if not spread else pan + spread * (2.0 * copy / (len(copies) - 1) - 1.0)
            gl, gr = pan_gains(side, amp * step_factor)
            step = freq * ratio * size / RATE
            p = (copy * 0.37 % 1.0) * size
            vib_pos = 0.0
            vib_step = 1024 * 5.4 / RATE
            dec = 1.0
            for i in range(n):
                if i < attack_n:
                    gain = i / attack_n
                else:
                    gain = sustain_level + (1.0 - sustain_level) * dec
                    dec *= decay_mul
                remaining = n - i
                if remaining < release_n:
                    gain *= remaining / release_n
                if vib:
                    p += step * (1.0 + vib * math.sin(TAU * vib_pos / 1024))
                    vib_pos += vib_step
                    if vib_pos >= 1024:
                        vib_pos -= 1024
                else:
                    p += step
                if p >= size:
                    p -= size
                value = tab[int(p)] * gain
                k = begin + i
                if k >= FRAMES:
                    k -= FRAMES
                left[k] += value * gl
                right[k] += value * gr
                if send:
                    send_left[k] += value * gl * send
                    send_right[k] += value * gr * send

    def echo(self, beat: float, steps: float = 3.0, feedback: float = 0.42,
             taps: int = 3, damping: float = 4200.0, mix: float = 0.55) -> None:
        """Ping-pong echo on the send bus; wrapping keeps it loop-safe."""
        delay = int(beat * steps / 4.0 * RATE)
        if delay <= 0:
            return
        left = array("f", self.send_left)
        right = array("f", self.send_right)
        ds.lowpass(left, damping)
        ds.lowpass(right, damping)
        for tap in range(1, taps + 1):
            gain = feedback ** tap * mix
            offset = (tap * delay) % FRAMES
            source = left if tap % 2 else right
            target = self.dry_right if tap % 2 else self.dry_left
            for i in range(FRAMES):
                j = i + offset
                if j >= FRAMES:
                    j -= FRAMES
                target[i] += source[j] * gain

    def master(self, drive: float = 1.2, target_rms: float = 0.15,
               air: float = 0.3) -> tuple[float, float]:
        """DC fix, soft clip, high-shelf "air" and a loudness match.

        Returns (rms, peak) in 0..1.  The shelf is what keeps the mix from
        sounding muffled on phone speakers: it adds the top octave that the
        additive voices lose at 22.05 kHz.
        """
        total = 0.0
        for bus in (self.dry_left, self.dry_right):
            mean = sum(bus) / FRAMES
            for i in range(FRAMES):
                bus[i] = math.tanh((bus[i] - mean) * drive)
            if air:
                # y = x + air * (x - lowpassed x) == gentle high shelf
                low = array("f", bytes(4 * FRAMES))
                state = 0.0
                alpha = 1.0 - math.exp(-TAU * 2400.0 / RATE)
                for i in range(FRAMES):
                    state += alpha * (bus[i] - state)
                    low[i] = state
                for i in range(FRAMES):
                    bus[i] = math.tanh(bus[i] + air * (bus[i] - low[i]))
        for i in range(FRAMES):
            total += self.dry_left[i] ** 2 + self.dry_right[i] ** 2
        rms = math.sqrt(total / (FRAMES * 2))
        peak = max(max(map(abs, self.dry_left)), max(map(abs, self.dry_right)), 1e-9)
        gain = min(target_rms / max(rms, 1e-9), 0.9 / peak)
        if gain != 1.0:
            for bus in (self.dry_left, self.dry_right):
                for i in range(FRAMES):
                    bus[i] *= gain
        return rms * gain, peak * gain


# ---------------------------------------------------------------------------
# Shared one-shot kit, built once per run.
# ---------------------------------------------------------------------------
KIT: dict[str, array] = {}


def kit() -> dict[str, array]:
    """The one-shot drum kit, gain-staged so the patterns can mix it evenly.

    Raw one-shots are wildly different in loudness (a kick is ~20 dB hotter than
    a hi-hat), so every voice is normalised to a reference RMS first.  The
    numbers below are that staging, not musical taste; the patterns decide taste.
    """
    if KIT:
        return KIT
    KIT.update(
        kick=ds.normalise(ds.kick(0.95), 0.30),
        kick_soft=ds.normalise(ds.kick(0.8, 0.45, 46.0), 0.26),
        snare=ds.normalise(ds.snare(0.5), 0.11),
        clap=ds.normalise(ds.clap(0.44), 0.12),
        hat=ds.normalise(ds.hat(0.19, 0.05, bright=True), 0.10),
        hat_open=ds.normalise(ds.hat(0.17, 0.26, open_hat=True, bright=True), 0.09),
        shaker=ds.normalise(ds.shaker(0.09), 0.07),
        tamb=ds.normalise(ds.tambourine(0.12), 0.10),
        cowbell=ds.normalise(ds.cowbell(0.15), 0.10),
        conga_hi=ds.normalise(ds.conga(0.28, 272.0), 0.10),
        conga_lo=ds.normalise(ds.conga(0.3, 172.0), 0.11),
        rim=ds.normalise(ds.rim(0.26), 0.085),
        crash=ds.normalise(ds.crash(0.18, 1.4), 0.075),
        riser=ds.normalise(ds.riser(1.5, 0.12), 0.05),
        laser=ds.normalise(ds.laser(0.16, 0.34, 2600.0), 0.09),
    )
    return KIT


VOICES = {"kick": "kick", "snare": "snare", "clap": "clap", "hat": "hat",
          "ohat": "hat_open", "shaker": "shaker", "tamb": "tamb", "cowbell": "cowbell",
          "conga_hi": "conga_hi", "conga_lo": "conga_lo", "rim": "rim", "crash": "crash",
          "kick_soft": "kick_soft"}


def pattern(m: Mixer, start: float, bar_len: float, bars: int, rows, voice: str,
            amp: float, pan: float = 0.0, send: float = 0.0, on=None) -> None:
    """Repeats 16-step rows over ``bars`` bars; ``rows`` cycles bar by bar."""
    rows = (rows,) if isinstance(rows, str) else tuple(rows)
    sample = kit()[VOICES[voice]]
    slot = bar_len / 16.0
    for bar in range(bars):
        if on is not None and not on(bar):
            continue
        row = rows[bar % len(rows)]
        base = start + bar * bar_len
        for index, mark in enumerate(row):
            velocity = VELOCITY.get(mark, 0.0)
            if velocity:
                m.hit(base + index * slot, sample, amp * velocity, pan, send)


def four_on_floor(m: Mixer, start: float, bar_len: float, bars: int, amp: float = 0.62,
                  voice: str = "kick", on=None) -> None:
    for bar in range(bars):
        if on is not None and not on(bar):
            continue
        for beat in range(4):
            m.hit(start + bar * bar_len + beat * bar_len / 4.0, kit()[voice], amp)


def pads(m: Mixer, start: float, bar_len: float, chords, amp: float, kind: str,
         bars_per_chord: float = 1.0, attack: float = 0.22, release: float = 0.45,
         send: float = 0.22, detune: tuple[float, ...] = (0.9985, 1.0015),
         octave: int = 0, sustain_level: float = 0.85, bars: int | None = None,
         on=None, bleed: float = 0.0) -> None:
    """Sustained chord bed. ``chords`` is a list of (root, triad) per chord.

    ``octave`` is a transpose in semitones (12 = one octave up).
    """
    slot = bar_len * bars_per_chord
    if bars is None:
        bars = int(round(len(chords) * bars_per_chord))
    index = 0
    position = start
    while position < start + bars * bar_len - 1e-9:
        root, triad = chords[index % len(chords)]
        bar_now = int(round((position - start) / bar_len))
        if on is None or on(bar_now):
            for voice, note in enumerate(triad):
                span = 0.34 if len(triad) > 1 else 0.0
                side = (voice - (len(triad) - 1) / 2.0) * span
                m.sustain(kind, midi(note + octave), position,
                          slot * 1.04, amp * (0.94 if voice else 1.0), side, attack,
                          0.7, sustain_level, release, 0.0, send, detune)
        position += slot
        index += 1


def bass_line(m: Mixer, start: float, bar_len: float, chords, bars: int, marks: str,
              amp: float = 0.34, kind: str = "bass", octave: int = 0,
              gate: float = 0.75, send: float = 0.0, swing: float = 0.0,
              extra_octave: bool = False) -> None:
    """Bass from a 16-step grid.

    ``8`` root, ``o`` root up an octave, ``5`` fifth, ``3`` third, ``7`` seventh,
    ``-`` step below, ``+`` step above, ``.`` rest.
    """
    slot = bar_len / 16.0
    for bar in range(bars):
        root, triad = chords[bar % len(chords)]
        base = start + bar * bar_len
        for index, mark in enumerate(marks):
            if mark in ". ":
                continue
            note = root
            if mark == "o":
                note = root + 12
            elif mark == "5":
                note = root + 7
            elif mark == "3":
                note = root + (triad[1] - triad[0])
            elif mark == "7":
                note = root + (triad[3] - triad[0] if len(triad) > 3 else 10)
            elif mark == "-":
                note = root - 2
            elif mark == "+":
                note = root + 2
            moment = base + index * slot + (swing * slot if index % 2 else 0.0)
            length = slot * 1.6 * gate
            m.sustain(kind, midi(note + 12 * octave), moment, length, amp, 0.0,
                      0.005, 0.35, 0.7, 0.07, 0.0, send)
            if extra_octave:
                m.sustain("sine", midi(note + 12 + 12 * octave), moment, length * 0.8,
                          amp * 0.14, 0.0, 0.004, 0.3, 0.5, 0.05, 0.0, send)


def stabs(m: Mixer, start: float, bar_len: float, chords, bars: int, marks: str,
          amp: float, kind: str = "brass", length: float = 0.14, pan: float = 0.0,
          send: float = 0.24, octave: int = 0, on=None, spread: float = 0.24) -> None:
    slot = bar_len / 16.0
    for bar in range(bars):
        if on is not None and not on(bar):
            continue
        root, triad = chords[bar % len(chords)]
        base = start + bar * bar_len
        for index, mark in enumerate(marks):
            velocity = VELOCITY.get(mark, 0.0)
            if not velocity:
                continue
            for voice, note in enumerate(triad):
                m.sustain(kind, midi(note + 12 * octave), base + index * slot,
                          length, amp * velocity,
                          pan + (voice - (len(triad) - 1) / 2.0) * spread,
                          0.004, 0.22, 0.45, 0.05, 0.0, send)


def lead(m: Mixer, start: float, bar_len: float, notes, amp: float, kind: str = "brass",
         pan: float = 0.0, gate: float = 0.88, octave: int = 0, vib: float = 0.0,
         send: float = 0.28, attack: float = 0.015, release: float = 0.1,
         scoop: float = 0.0, bars_offset: float = 0.0) -> None:
    """Hook line: ``notes`` is a tuple of (16th step, midi note, 16th length).

    ``octave`` is a transpose in semitones (12 = one octave up).
    """
    slot = bar_len / 16.0
    for step, note, length in notes:
        moment = start + (bars_offset * 16.0 + step) * slot
        pitch = midi(note + octave)
        m.sustain(kind, pitch, moment, length * slot * gate, amp, pan, attack, 0.5,
                  0.8, release, vib, send)
        if scoop:
            m.sustain(kind, pitch, moment, length * slot * 0.45, amp * 0.55, pan,
                      0.003, 0.3, 0.5, 0.06, 0.0, send)


def arp(m: Mixer, start: float, bar_len: float, chords, bars: int, marks: str,
        amp: float, kind: str = "glass", octave: int = 12, send: float = 0.3,
        pan_cycle: tuple[float, ...] = (0.34, -0.34), gate: float = 0.6,
        decay: float = 0.2) -> None:
    slot = bar_len / 16.0
    for bar in range(bars):
        root, triad = chords[bar % len(chords)]
        tones = list(triad) + [triad[0] + 12]
        base = start + bar * bar_len
        for index, mark in enumerate(marks):
            if mark in ". ":
                continue
            pick = "1234".find(mark)
            note = tones[pick if pick >= 0 else 0] + octave
            m.hit(base + index * slot,
                  ds.tone(kind, midi(note), slot * 4.0, amp, 0.002, decay, 0.3, 0.07),
                  1.0, pan_cycle[index % len(pan_cycle)], 0.0)


def fx(m: Mixer, when: float, name: str = "crash", amp: float = 0.2,
       pan: float = 0.0, send: float = 0.2) -> None:
    m.hit(when, kit()[name], amp, pan, send)


def write_loop(name: str, m: Mixer) -> None:
    left, right = m.dry_left, m.dry_right
    path = OUT / f"{name}.wav"
    path.parent.mkdir(parents=True, exist_ok=True)
    chunk = RATE * 5
    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(RATE)
        for begin in range(0, FRAMES, chunk):
            end = min(FRAMES, begin + chunk)
            pcm = array("h", bytes(4 * (end - begin)))
            for i in range(begin, end):
                pcm[2 * (i - begin)] = int(max(-32767, min(32767, left[i] * 32767)))
                pcm[2 * (i - begin) + 1] = int(max(-32767, min(32767, right[i] * 32767)))
            output.writeframes(pcm.tobytes())
    print(f"{path.relative_to(ROOT)}: {FRAMES / RATE:.3f} с, {path.stat().st_size / 1024:.0f} КиБ")
