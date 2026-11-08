#!/usr/bin/env python3
"""Draws ``docs/disco-preview.png``: a peak/rms strip of the disco loop.

Only the standard library is used (``wave``, ``struct``, ``zlib``).  The picture
makes the section build-up and the seamless loop point easy to check by eye:
every strip starts and ends at full level, and the bars stay regular.
"""

from __future__ import annotations

import array
import struct
import wave
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DISCO = ROOT / "assets" / "audio" / "tracks"
OUT = ROOT / "docs" / "disco-preview.png"

ROWS = 1
COLS = 660          # one column is ~45 ms of audio
ROW_H = 42
GAP = 10
TOP = 8
WIDTH = COLS + 2 * 12
HEIGHT = TOP * 2 + ROWS * ROW_H + (ROWS - 1) * GAP
BACKGROUND = (11, 6, 24)
GRID = (36, 22, 62)
BRIGHT = (255, 79, 163)
COOL = (70, 230, 255)


def rgb_pixel(buffer: bytearray, index: int, colour: tuple[int, int, int]) -> None:
    buffer[index] = colour[0]
    buffer[index + 1] = colour[1]
    buffer[index + 2] = colour[2]


def write_png(path: Path, buffer: bytearray, width: int, height: int) -> None:
    def chunk(tag: bytes, payload: bytes) -> bytes:
        body = tag + payload
        return (struct.pack(">I", len(payload)) + body
                + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF))

    raw = bytearray()
    stride = width * 3
    for y in range(height):
        raw.append(0)
        raw += buffer[y * stride:(y + 1) * stride]
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header)
                     + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
                     + chunk(b"IEND", b""))


def load_peaks(path: Path) -> tuple[list[float], list[float]]:
    """Peak and rms per output column."""
    with wave.open(str(path), "rb") as audio:
        total = audio.getnframes()
        raw = array.array("h", audio.readframes(total))
    left = raw[0::2]
    size = max(1, total // COLS)
    peaks, levels = [], []
    for column in range(COLS):
        chunk = left[column * size:(column + 1) * size]
        if not chunk:
            peaks.append(0.0)
            levels.append(0.0)
            continue
        peaks.append(max(abs(value) for value in chunk) / 32768.0)
        levels.append((sum(value * value for value in chunk) / len(chunk)) ** 0.5 / 32768.0)
    return peaks, levels


def main() -> int:
    files = sorted(DISCO.glob("disco_*.wav"))
    assert len(files) == ROWS, f"expected {ROWS} loop, found {len(files)}"
    buffer = bytearray(bytes(BACKGROUND) * (WIDTH * HEIGHT))

    for y in range(HEIGHT):
        for x in range(WIDTH):
            if (x - 12) % 66 == 0 or y % 4 == 0:
                rgb_pixel(buffer, (y * WIDTH + x) * 3, GRID)

    for row, path in enumerate(files):
        peaks, levels = load_peaks(path)
        top = TOP + row * (ROW_H + GAP)
        middle = top + ROW_H // 2
        for column in range(COLS):
            x = 12 + column
            peak = min(1.0, peaks[column] * 1.05)
            level = min(1.0, levels[column] * 1.6)
            half_peak = int(peak * (ROW_H // 2))
            half_level = int(level * (ROW_H // 2))
            for offset in range(-half_peak, half_peak + 1):
                y = middle + offset
                if 0 <= y < HEIGHT:
                    blend = min(1.0, abs(offset) / max(1, ROW_H // 2))
                    colour = tuple(
                        int(BRIGHT[channel] * (1 - blend) + COOL[channel] * blend)
                        for channel in range(3)
                    )
                    rgb_pixel(buffer, (y * WIDTH + x) * 3, colour)
            for offset in range(-half_level, half_level + 1):
                y = middle + offset
                if 0 <= y < HEIGHT:
                    index = (y * WIDTH + x) * 3
                    r, g, b = buffer[index], buffer[index + 1], buffer[index + 2]
                    rgb_pixel(buffer, index, (min(255, r + 70), min(255, g + 60), min(255, b + 80)))

    write_png(OUT, buffer, WIDTH, HEIGHT)
    print(f"{OUT.relative_to(ROOT)}: {WIDTH}x{HEIGHT}, {OUT.stat().st_size / 1024:.0f} КиБ")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
