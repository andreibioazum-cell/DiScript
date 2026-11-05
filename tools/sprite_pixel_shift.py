#!/usr/bin/env python3
"""Smooth pixel-shift transition between two sprite frames.

This script compares two PNG frames, finds pixels that changed,
and writes intermediate PNG frames where those changed pixels
gradually shift from their old positions to their new positions.
No new sprites or C parameters are needed — just generate frames
and use them (or observe the smooth effect).

Usage:
    python3 tools/sprite_pixel_shift.py old.png new.png out_dir/ 8
"""
import sys
import os
import math
from PIL import Image


def shift_pixels(old_path, new_path, out_dir, count=8):
    old_img = Image.open(old_path).convert("RGBA")
    new_img = Image.open(new_path).convert("RGBA")
    w, h = old_img.size
    if new_img.size != (w, h):
        min_w = min(w, new_img.size[0])
        min_h = min(h, new_img.size[1])
        old_img = old_img.crop((0, 0, min_w, min_h))
        new_img = new_img.crop((0, 0, min_w, min_h))
        w, h = min_w, min_h

    os.makedirs(out_dir, exist_ok=True)
    old_pixels = old_img.load()
    new_pixels = new_img.load()

    # Find changed pixels in old and new
    old_changed = {}
    new_changed = {}
    for y in range(h):
        for x in range(w):
            old_c = old_pixels[x, y]
            new_c = new_pixels[x, y]
            if old_c != new_c:
                old_changed[(x, y)] = old_c
                new_changed[(x, y)] = new_c

    if not old_changed:
        new_img.save(os.path.join(out_dir, "frame_00.png"))
        print("No differences.")
        return

    # Build nearest-neighbor mapping from old changed pixels to new changed pixels
    old_pts = list(old_changed.keys())
    new_pts = list(new_changed.keys())

    mapping = {}
    for (ox, oy) in old_pts:
        best = None
        best_d = float('inf')
        for (nx, ny) in new_pts:
            d = math.hypot(ox - nx, oy - ny)
            if d < best_d:
                best_d = d
                best = (nx, ny)
        mapping[(ox, oy)] = best

    for step in range(count + 1):
        t = step / count  # 0 = old, 1 = new
        frame = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        frame_pixels = frame.load()

        # Place unchanged pixels from old image
        for y in range(h):
            for x in range(w):
                if (x, y) not in old_changed:
                    frame_pixels[x, y] = old_pixels[x, y]

        # Shift changed pixels smoothly from old to new position
        for (ox, oy) in old_changed:
            (nx, ny) = mapping[(ox, oy)]
            # Linear interpolation of position
            ix = int(round(ox + (nx - ox) * t))
            iy = int(round(oy + (ny - oy) * t))
            # Interpolate color too for smoother visual
            old_c = old_changed[(ox, oy)]
            new_c = new_changed[mapping[(ox, oy)]]
            col = tuple(int(old_c[i] + (new_c[i] - old_c[i]) * t) for i in range(4))
            # Only write inside bounds
            if 0 <= ix < w and 0 <= iy < h:
                frame_pixels[ix, iy] = col

        # Fill remaining unchanged pixels from old (to preserve background context)
        for y in range(h):
            for x in range(w):
                if (x, y) not in old_changed:
                    # Only overwrite if the pixel hasn't been filled by a shifted pixel
                    # (since shifted pixels may overlap; overlap is fine for smooth effect)
                    pass
            # Actually keep unchanged pixels as set initially
        out_path = os.path.join(out_dir, f"frame_{step:02d}.png")
        frame.save(out_path)
        print(f"Wrote {out_path}")


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python3 sprite_pixel_shift.py old.png new.png out_dir/ [count]")
        sys.exit(1)
    shift_pixels(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 8)
