#!/usr/bin/env python3
"""Generates clean PNG launcher icons and built-in LÖVE2D demo sprites for DiScript IDE."""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def write_png(path: Path, w: int, h: int, pixels: bytes) -> None:
    def chunk(tag: bytes, body: bytes) -> bytes:
        return (
            struct.pack(">I", len(body))
            + tag
            + body
            + struct.pack(">I", zlib.crc32(tag + body) & 0xFFFFFFFF)
        )

    raw = bytearray()
    stride = w * 4
    for y in range(h):
        raw.append(0)
        raw.extend(pixels[y * stride : (y + 1) * stride])
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    data = (
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", ihdr)
        + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
        + chunk(b"IEND", b"")
    )
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def clamp8(v: float) -> int:
    return max(0, min(255, int(v + 0.5)))


def sd_segment(px: float, py: float, ax: float, ay: float, bx: float, by: float) -> float:
    vx, vy = bx - ax, by - ay
    wx, wy = px - ax, py - ay
    denom = vx * vx + vy * vy
    t = 0.0 if denom <= 1e-9 else max(0.0, min(1.0, (wx * vx + wy * vy) / denom))
    dx = px - (ax + t * vx)
    dy = py - (ay + t * vy)
    return math.hypot(dx, dy)


def sd_round_box(px: float, py: float, cx: float, cy: float, hw: float, hh: float, r: float) -> float:
    qx = abs(px - cx) - (hw - r)
    qy = abs(py - cy) - (hh - r)
    return math.hypot(max(qx, 0.0), max(qy, 0.0)) + min(max(qx, qy), 0.0) - r


def make_icon(size: int) -> bytes:
    buf = bytearray(size * size * 4)
    c = size * 0.5
    rad = size * 0.22
    hw = size * 0.46
    for y in range(size):
        for x in range(size):
            px, py = x + 0.5, y + 0.5
            d_bg = sd_round_box(px, py, c, c, hw, hw, rad)
            if d_bg > 1.0:
                continue
            alpha_bg = max(0.0, min(1.0, 0.5 - d_bg))
            u, v = px / size, py / size
            # Deep space gradient (#0E1324 -> #1B1638) with cyan/violet glow
            r = 14 + 28 * (1.0 - v) + 24 * u
            g = 19 + 22 * (1.0 - u)
            b = 40 + 52 * v

            # Subtle inner border
            if -size * 0.03 < d_bg <= 0.5:
                r += 45
                g += 95
                b += 145

            # '>' prompt chevron on the left
            th = size * 0.055
            d_gt = min(
                sd_segment(px, py, size * 0.24, size * 0.33, size * 0.44, size * 0.50),
                sd_segment(px, py, size * 0.44, size * 0.50, size * 0.24, size * 0.67),
            ) - th
            if d_gt < 1.2:
                k = max(0.0, min(1.0, 0.7 - d_gt))
                r = r * (1 - k) + 56 * k
                g = g * (1 - k) + 218 * k
                b = b * (1 - k) + 252 * k

            # 'S' / lightning cursor bar on the right
            d_bar = sd_segment(px, py, size * 0.52, size * 0.66, size * 0.76, size * 0.66) - th
            if d_bar < 1.2:
                k = max(0.0, min(1.0, 0.7 - d_bar))
                r = r * (1 - k) + 192 * k
                g = g * (1 - k) + 132 * k
                b = b * (1 - k) + 252 * k

            # Glowing spark dot top-right (LÖVE2D graphics nod)
            d_dot = math.hypot(px - size * 0.68, py - size * 0.36) - size * 0.075
            if d_dot < 1.5:
                k = max(0.0, min(1.0, 0.8 - d_dot))
                r = r * (1 - k) + 74 * k
                g = g * (1 - k) + 242 * k
                b = b * (1 - k) + 168 * k

            idx = (y * size + x) * 4
            buf[idx] = clamp8(r)
            buf[idx + 1] = clamp8(g)
            buf[idx + 2] = clamp8(b)
            buf[idx + 3] = clamp8(alpha_bg * 255.0)
    return bytes(buf)


def make_ship(size: int = 64) -> bytes:
    buf = bytearray(size * size * 4)
    cx, cy = size * 0.5, size * 0.5
    for y in range(size):
        for x in range(size):
            px, py = (x + 0.5 - cx) / (size * 0.5), (y + 0.5 - cy) / (size * 0.5)
            # Sleek upward-pointing starfighter
            ax = abs(px)
            in_hull = py >= -0.82 and py <= 0.68 and ax <= (py + 0.88) * 0.52 and py <= 0.72 - ax * 0.35
            in_wing = py >= -0.05 and py <= 0.72 and ax <= 0.88 and py >= ax * 0.72 - 0.12
            in_cockpit = math.hypot(px * 1.4, (py + 0.12) * 1.1) <= 0.24
            in_thruster = ax <= 0.22 and 0.62 <= py <= 0.88
            idx = (y * size + x) * 4
            if in_cockpit:
                buf[idx : idx + 4] = bytes((96, 235, 255, 255))
            elif in_thruster:
                glow = max(0.0, 1.0 - (py - 0.62) / 0.28)
                buf[idx : idx + 4] = bytes((255, clamp8(150 + 90 * glow), 64, clamp8(255 * glow)))
            elif in_hull:
                shade = clamp8(215 - ax * 110)
                buf[idx : idx + 4] = bytes((clamp8(shade * 0.82), shade, 250, 255))
            elif in_wing:
                buf[idx : idx + 4] = bytes((99, 102, 241, 255))
    return bytes(buf)


def make_gem(size: int = 48) -> bytes:
    buf = bytearray(size * size * 4)
    c = size * 0.5
    for y in range(size):
        for x in range(size):
            dx = (x + 0.5 - c) / (size * 0.44)
            dy = (y + 0.5 - c) / (size * 0.44)
            m = abs(dx) + abs(dy)
            if m <= 1.0:
                facet = 1.18 if (dx + dy) < 0 else 0.84
                r = clamp8(52 * facet)
                g = clamp8(225 * facet)
                b = clamp8(178 * facet)
                if m > 0.86:
                    r, g, b = 180, 255, 235
                idx = (y * size + x) * 4
                buf[idx : idx + 4] = bytes((r, g, b, 255))
    return bytes(buf)


def make_enemy(size: int = 56) -> bytes:
    buf = bytearray(size * size * 4)
    c = size * 0.5
    for y in range(size):
        for x in range(size):
            dx = (x + 0.5 - c) / (size * 0.45)
            dy = (y + 0.5 - c) / (size * 0.45)
            d = math.hypot(dx, dy)
            idx = (y * size + x) * 4
            if d <= 0.92:
                eye = math.hypot(abs(dx) - 0.32, dy + 0.08) <= 0.16
                core = d <= 0.32
                if eye:
                    buf[idx : idx + 4] = bytes((255, 245, 150, 255))
                elif core:
                    buf[idx : idx + 4] = bytes((255, 95, 135, 255))
                else:
                    s = 1.0 - d * 0.35
                    buf[idx : idx + 4] = bytes((clamp8(232 * s), clamp8(62 * s), clamp8(108 * s), 255))
    return bytes(buf)


def main() -> None:
    densities = {
        "mipmap-mdpi": 48,
        "mipmap-hdpi": 72,
        "mipmap-xhdpi": 96,
        "mipmap-xxhdpi": 144,
        "mipmap-xxxhdpi": 192,
    }
    for folder, px in densities.items():
        write_png(ROOT / "platform" / "android" / "res" / folder / "ic_launcher.png", px, px, make_icon(px))
    write_png(ROOT / "assets" / "textures" / "logo.png", 128, 128, make_icon(128))
    write_png(ROOT / "assets" / "textures" / "ship.png", 64, 64, make_ship(64))
    write_png(ROOT / "assets" / "textures" / "gem.png", 48, 48, make_gem(48))
    write_png(ROOT / "assets" / "textures" / "enemy.png", 56, 56, make_enemy(56))
    print("Generated DiScript icons and built-in textures.")


if __name__ == "__main__":
    main()
