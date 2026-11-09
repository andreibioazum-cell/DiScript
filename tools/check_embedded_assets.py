#!/usr/bin/env python3
"""Checks that an executable carries the asset payload appended to it.

The Windows build ships as a single .exe: ``tools/embed_assets.py`` appends the
staged asset tree to the binary and writes a trailer, and this check proves the
trailer is there and points at a sane index. Run it on any built binary:

    python3 tools/check_embedded_assets.py build/pc/CubicBattle4.exe
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path

MAGIC = b"CB4EMBED"
TRAILER = struct.Struct("<8sIQQ")  # magic, file count, index size, index offset


def main(argv: list[str]) -> int:
    if len(argv) != 1:
        print("usage: check_embedded_assets.py <binary>", file=sys.stderr)
        return 2
    binary = Path(argv[0])
    if not binary.is_file():
        print(f"нет файла: {binary}", file=sys.stderr)
        return 1
    size = binary.stat().st_size
    with binary.open("rb") as handle:
        if size < TRAILER.size:
            print(f"{binary.name}: слишком маленький файл", file=sys.stderr)
            return 1
        handle.seek(-TRAILER.size, 2)
        magic, count, index_size, index_offset = TRAILER.unpack(handle.read())
        if magic != MAGIC:
            print(f"{binary.name}: нет хвоста {MAGIC!r} (нашли {magic!r})", file=sys.stderr)
            return 1
        if count == 0 or index_size == 0:
            print(f"{binary.name}: пустое оглавление", file=sys.stderr)
            return 1
        # The payload starts right after the image; the index is offset from
        # there, and both are measured from the end of the file.
        payload_start = size - TRAILER.size - index_size - index_offset
        if payload_start <= 0:
            print(f"{binary.name}: оглавление вне файла", file=sys.stderr)
            return 1
        handle.seek(payload_start + index_offset)
        index = handle.read(index_size)
    names = 0
    pos = 0
    for _ in range(count):
        if pos + 20 > len(index):
            break
        name_len, offset, length = struct.unpack_from("<IQQ", index, pos)
        pos += 20
        if name_len == 0 or pos + name_len > len(index):
            break
        pos += name_len
        if offset + length > index_offset:
            print(f"{binary.name}: файл выходит за пределы данных", file=sys.stderr)
            return 1
        names += 1
    if names != count:
        print(f"{binary.name}: оглавление читается не полностью ({names} из {count})", file=sys.stderr)
        return 1
    print(f"{binary.name}: вшито файлов {count}, оглавление {index_size} байт, данные {index_offset} байт")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
