#!/usr/bin/env python3
"""Synthesize ability WAV sound effects into assets/audio/sfx/.

Every effect is original: it is computed from oscillators, noise and envelopes,
never cut from a sample library. Each file carries the project's copyright
notice in a RIFF INFO chunk through ``tools/audio_rights.py``.
"""

from __future__ import annotations

import math
import random
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import audio_rights  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "assets" / "audio" / "sfx"
RATE = 22050


def synth(seconds: float, fn) -> list[int]:
    n = int(RATE * seconds)
    out = []
    for i in range(n):
        t = i / RATE
        v = fn(t, i)
        v = max(-1.0, min(1.0, v))
        out.append(int(v * 32767 * 0.85))
    return out


def save(name: str, samples: list[int]) -> None:
    path = OUT / name
    pcm = b"".join(struct.pack("<h", s) for s in samples)
    audio_rights.write_wav(path, 1, RATE, pcm, path.stem, "tools/gen_ability_sounds.py")
    print("%s: %d сэмплов" % (name, len(samples)))


def env(t: float, dur: float, attack: float = 0.005) -> float:
    if t < attack:
        return t / attack
    k = (t - attack) / max(0.001, dur - attack)
    return max(0.0, 1.0 - k) ** 1.6


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    rng = random.Random(42)
    noise = [rng.uniform(-1, 1) for _ in range(RATE)]

    def nz(t: float) -> float:
        return noise[int(t * RATE) % RATE]

        # Punch: a short muffled click plus a low chirp push 200 -> 60 Hz.
        # It used to be a long burst of white noise - it sounded like falling sand,
        # so the noise stays on the first ~20 ms and the body is a clean falling tone
        # with a second harmonic (audible on small speakers too).
    def punch(t: float, i: int) -> float:
        snap = 0.35 * nz(t * 3.1) * math.exp(-t * 110)
        ph = 2 * math.pi * (200 * t - 345 * t * t)
        body = 0.8 * math.sin(ph) * math.exp(-t * 16)
        h2 = 0.3 * math.sin(2 * ph) * math.exp(-t * 22)
        return 0.85 * (snap + body + h2)
    save("punch.wav", synth(0.20, punch))

        # Dash: a noise whistle, volume in a hump, the tone going up.
    save("dash.wav", synth(0.35, lambda t, i:
                           math.sin(math.pi * min(1.0, t / 0.35)) ** 1.5 *
                           (0.5 * nz(t * (1 + t * 2)) + 0.25 * math.sin(2 * math.pi * (300 + 900 * t) * t))))

        # Astra grab: a zoom 900 -> 180 with vibrato.
    save("grab.wav", synth(0.40, lambda t, i:
                           env(t, 0.40, 0.01) * 0.55 *
                           math.sin(2 * math.pi * (900 - 1750 * t) * t + 0.6 * math.sin(2 * math.pi * 30 * t))))

        # Santa gift: three bells E5-G5-C6.
    def gift(t: float, i: int) -> float:
        v = 0.0
        for start, freq in ((0.0, 659.25), (0.12, 783.99), (0.24, 1046.5)):
            if t >= start:
                lt = t - start
                v += 0.33 * math.exp(-lt * 9) * (math.sin(2 * math.pi * freq * lt) +
                                                0.4 * math.sin(2 * math.pi * freq * 2 * lt))
        return v
    save("gift.wav", synth(0.55, gift))

    # ebuC's station: metal clang plus a short beep.
    def station(t: float, i: int) -> float:
        clang = math.exp(-t * 16) * (0.4 * math.sin(2 * math.pi * 620 * t) +
                                     0.3 * math.sin(2 * math.pi * 935 * t) +
                                     0.2 * math.sin(2 * math.pi * 1470 * t))
        beep = 0.25 * math.sin(2 * math.pi * 1250 * t) if 0.16 < t < 0.28 else 0.0
        return clang + beep * env(t, 0.35)
    save("station.wav", synth(0.35, station))

        # Overclock: a sweep 180 -> 1400 Hz with sparks.
    save("overclock.wav", synth(0.50, lambda t, i:
                                env(t, 0.50, 0.02) * (0.45 * math.sin(2 * math.pi * (180 + 2440 * t) * t) +
                                                      0.18 * math.sin(2 * math.pi * (720 + 9760 * t) * t))))

        # Button click: a short crisp tick. It used to be the one sound in the
        # repository without a generator, so it is synthesised here now: a tiny
        # pitched blip plus a softened noise transient, 45 ms, no tail.
    def click(t: float, i: int) -> float:
        blip = 0.55 * math.sin(2 * math.pi * (1450 - 1800 * t) * t)
        transient = 0.30 * nz(t * 5.3) * math.exp(-t * 240)
        return math.sin(math.pi * min(1.0, t / 0.045)) * (blip + transient)
    save("button_click.wav", synth(0.045, click))

        # Zombie punch: the same push as the normal punch plus a growl - a saw
        # 105 -> 50 Hz with tremolo - and a bit of crunch. The growl used to slide to
        # 18 Hz (inaudible on small speakers) and left only hissing noise.
    def zpunch(t: float, i: int) -> float:
        ph = 2 * math.pi * (210 * t - 387 * t * t)
        thump = 0.8 * math.sin(ph) * math.exp(-t * 13)
        f = 105 - 145 * t
        saw = 2 * ((f * t) % 1.0) - 1
        growl = 0.42 * saw * (0.65 + 0.35 * math.sin(2 * math.pi * 10 * t)) * math.exp(-t * 6)
        grit = 0.15 * nz(t * 1.7) * math.exp(-t * 9)
        return env(t, 0.38, 0.004) * 0.95 * (thump + growl + grit)
    save("zombie_punch.wav", synth(0.38, zpunch))

        # Zombie dash: a moan - a sine 320 -> 85 with tremolo plus noisy breath.
    def zdash(t: float, i: int) -> float:
        f = 320 - 470 * t
        moan = math.sin(2 * math.pi * f * t) * (0.65 + 0.35 * math.sin(2 * math.pi * 9 * t))
        return math.sin(math.pi * min(1.0, t / 0.5)) * (0.5 * moan + 0.3 * nz(t * 0.7))
    save("zombie_dash.wav", synth(0.50, zdash))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
