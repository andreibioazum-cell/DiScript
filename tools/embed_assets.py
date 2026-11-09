#!/usr/bin/env python3
"""Appends the game assets to the executable, so a bare .exe is playable.

People download ``CubicBattle4.exe`` on its own, drop it somewhere and run it -
without an ``assets`` folder next to it the game had no textures, no font and
no music. Instead of compiling a 20 MB C array (slow to build, slow to link),
the staged asset tree is appended to the binary as a payload:

    [PE image][file bytes...][index][trailer "CB4EMBED", count, index size, index offset]

Trailing bytes after the image are ignored by Windows, and the runtime reader
(``src/platform/pc/pc_embedded.inc``) reads its own executable to serve any
asset that is not on disk. A loose ``assets`` folder or a texture pack still
wins over the embedded copy.

Usage: embed_assets.py <assets dir> <binary>   (rewrites the binary in place)
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

MAGIC = b"CB4EMBED"
TRAILER = struct.Struct("<8sIQQ")  # magic, file count, index size, index offset
ENTRY = struct.Struct("<IQQ")     # name length, data offset, data length


def asset_files(source: Path) -> list[tuple[str, Path]]:
    """Every file of a staged asset tree under the name the engine asks for."""
    return [
        (path.relative_to(source).as_posix(), path)
        for path in sorted(source.rglob("*"))
        if path.is_file()
    ]


def embed(source: Path, binary: Path) -> tuple[int, int]:
    files = asset_files(source)
    if not files:
        raise SystemExit(f"нет ресурсов для встраивания: {source}")
    payload = bytearray()
    offsets: list[tuple[str, int, int]] = []
    for name, path in files:
        data = path.read_bytes()
        offsets.append((name, len(payload), len(data)))
        payload += data
    index_offset = len(payload)
    index = bytearray()
    for name, offset, size in offsets:
        encoded = name.encode("utf-8")
        index += ENTRY.pack(len(encoded), offset, size) + encoded
    payload += index
    payload += TRAILER.pack(MAGIC, len(offsets), len(index), index_offset)

    body = binary.read_bytes()
    # Re-running on an already patched binary must not stack payloads.
    body = strip(body)
    binary.write_bytes(body + bytes(payload))
    return len(offsets), len(payload)


def strip(data: bytes) -> bytes:
    """Remove a payload this tool appended earlier, if there is one."""
    if len(data) < TRAILER.size:
        return data
    magic, count, index_size, index_offset = TRAILER.unpack(data[-TRAILER.size:])
    if magic != MAGIC or index_size > len(data) - TRAILER.size:
        return data
    start = len(data) - TRAILER.size - index_size - index_offset
    if start <= 0:
        return data
    # Sanity check: the index must parse back to the same number of entries.
    pos = start + index_offset
    for _ in range(count):
        if pos + ENTRY.size > len(data) - TRAILER.size:
            return data
        name_len = ENTRY.unpack(data[pos:pos + ENTRY.size])[0]
        pos += ENTRY.size + name_len
    return data[:start]


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: embed_assets.py <assets dir> <binary>", file=sys.stderr)
        return 2
    source, binary = Path(argv[0]), Path(argv[1])
    if not binary.is_file():
        raise SystemExit(f"нет файла для встраивания: {binary}")
    count, size = embed(source, binary)
    print(f"встроено файлов: {count}, байт: {size} -> {binary.name} "
          f"({binary.stat().st_size} байт всего)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
