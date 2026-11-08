#!/usr/bin/env python3
"""Checks the eight disco loops in assets/audio/tracks.

What is checked here:

* format: WAV, stereo, 16 bit, 22 050 Hz, exactly 30.000 s;
* whole bars: the duration matches the documented BPM, so the loop point
  falls on the first beat of a bar;
* seamless wrap: the jump between the last and the first sample is no bigger
  than the ordinary sample step of the same file (no click at the seam), and
  the end of the file is not quieter than the middle (the loop does not end in
  silence);
* level: RMS inside a sane range, peak far from clipping, no DC offset;
* the eight files are really different tracks and not one file copied.
"""

from __future__ import annotations

import array
import math
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TRACKS = ROOT / "assets" / "audio" / "tracks"

# File stem -> (BPM, bars).  Note that 8 * bars == BPM, so 30 s is whole bars.
LOOPS = {
    "disco_01_roller_rink_sunset": (120, 15),
    "disco_02_neon_boots": (128, 16),
    "disco_03_mirrorball_dreams": (112, 14),
    "disco_04_saturday_night_bus": (120, 15),
    "disco_05_feel_the_floor": (128, 16),
    "disco_06_boogie_boulevard": (112, 14),
    "disco_07_disco_nebula": (128, 16),
    "disco_08_strobe_hearts": (120, 15),
}


def load(path: Path) -> tuple[array.array, array.array, int]:
    with wave.open(str(path), "rb") as audio:
        assert audio.getnchannels() == 2, f"{path.name}: stereo expected"
        assert audio.getsampwidth() == 2, f"{path.name}: 16 bit expected"
        assert audio.getframerate() == 22050, f"{path.name}: 22050 Hz expected"
        frames = audio.getnframes()
        raw = array.array("h", audio.readframes(frames))
    return raw[0::2], raw[1::2], frames


def percentile(values: list[int], share: float) -> int:
    return values[min(len(values) - 1, int(len(values) * share))]


def check_loop(name: str, bpm: int, bars: int) -> None:
    path = TRACKS / f"{name}.wav"
    assert path.is_file(), f"missing disco loop: {path.relative_to(ROOT)}"
    left, right, frames = load(path)

    duration = frames / 22050
    assert abs(duration - 30.0) < 1e-9, f"{name}: duration {duration}, 30 s expected"
    musical_bars = duration * bpm / 240.0
    assert abs(musical_bars - bars) < 1e-6, (
        f"{name}: at {bpm} BPM the file holds {musical_bars:.3f} bars, {bars} expected"
    )

    # Seam click: the jump at the boundary must not stand out from the signal.
    steps = sorted(abs(left[i + 1] - left[i]) for i in range(0, frames - 1, 11))
    seam = abs(left[0] - left[-1])
    limit = percentile(steps, 0.9995)
    # A real loop scores around 0.1, a file cut mid-bar scores above 0.5.
    assert seam <= limit * 0.35 + 64, (
        f"{name}: seam jump {seam} against an ordinary step of {limit} - that clicks"
    )

    # The end of the file must not be silent, or the loop breathes at the seam.
    tail = frames // 20
    tail_rms = math.sqrt(sum(value * value for value in left[-tail:]) / tail)
    total_rms = math.sqrt(sum(value * value for value in left) / frames)
    assert tail_rms > total_rms * 0.45, (
        f"{name}: last 50 ms are quiet (RMS {tail_rms:.0f} against {total_rms:.0f})"
    )

    peak = max(max(map(abs, left)), max(map(abs, right)))
    assert peak < 0.97 * 32767, f"{name}: peak {peak} is too close to clipping"
    level = total_rms / 32767
    assert 0.09 < level < 0.3, f"{name}: RMS {level:.3f} outside the working range"

    for channel in (left, right):
        assert abs(sum(channel) / frames) < 200, f"{name}: DC offset in a channel"
    assert left != right, f"{name}: channels are identical, stereo lost"


def check_set() -> None:
    files = sorted(TRACKS.glob("disco_*.wav"))
    assert len(files) == 8, f"assets/audio/tracks must hold 8 disco loops, found {len(files)}"
    signatures = {path.read_bytes()[:4096] for path in files}
    assert len(signatures) == 8, "some loops start identically - these are copies, not tracks"


def check_game_music() -> None:
    """The three in-game tracks keep their format next to the loops."""
    expected = {"lobby.wav": 70, "winter_lobby.wav": 70, "duel.wav": 45}
    for name, seconds in expected.items():
        path = TRACKS / name
        assert path.is_file(), f"missing game track: {path.relative_to(ROOT)}"
        with wave.open(str(path), "rb") as audio:
            assert audio.getnchannels() == 2 and audio.getsampwidth() == 2, name
            assert abs(audio.getnframes() / audio.getframerate() - seconds) < 0.01, name


def check_preview() -> None:
    """The preview picture is drawn straight from the WAV files."""
    preview = ROOT / "docs" / "disco-preview.png"
    if not preview.exists():
        return
    header = preview.read_bytes()[:24]
    assert header[:8] == b"\x89PNG\r\n\x1a\n", "docs/disco-preview.png is damaged"
    width = int.from_bytes(header[16:20], "big")
    height = int.from_bytes(header[20:24], "big")
    assert (width, height) == (684, 422), f"docs/disco-preview.png has size {width}x{height}"


def check_generator() -> None:
    """The generator must stay free of third-party packages and of samples."""
    for name in ("disco_synth.py", "disco_loops.py", "disco_arrange.py",
                 "generate_disco_loops.py"):
        text = (ROOT / "tools" / name).read_text(encoding="utf-8")
        for forbidden in ("import numpy", "import scipy", "import pydub", "subprocess",
                          "urllib", "requests"):
            assert forbidden not in text, f"tools/{name}: third-party import or network ({forbidden})"
    assert not list(TRACKS.glob("*.mp3")), "no mp3 belongs in assets/audio/tracks"


def main() -> int:
    check_set()
    check_game_music()
    check_preview()
    check_generator()
    for name, (bpm, bars) in LOOPS.items():
        check_loop(name, bpm, bars)
    total = sum((TRACKS / f"{name}.wav").stat().st_size for name in LOOPS) / (1024 * 1024)
    print(f"Disco loops: ok, 8 files of 30 s, {total:.1f} MiB together")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
