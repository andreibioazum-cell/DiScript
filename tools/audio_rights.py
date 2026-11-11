#!/usr/bin/env python3
"""Copyright notice for every sound the game ships.

The whole audio set of Cubic Battle 4 - the three game themes, the disco loop
and the nine one-shot effects - is computed by the scripts in ``tools/``. There
is no sample, no downloaded track and no borrowed melody in it, so the rights
belong to the project alone.

This module is the single source of that statement. All three generators import
it and write the same notice into every WAV file as a RIFF ``LIST``/``INFO``
chunk, the standard place for audio metadata:

* ``INAM`` - the name of the track;
* ``IART`` - the author (the rights holder of the project);
* ``ICOP`` - the copyright line;
* ``ICMT`` - how the sound was made ("original, synthesised by ...");
* ``ICRD`` - the year;
* ``ISFT`` - the generator script that wrote the file.

Players cannot see the chunk, but any editor, ``ffprobe``, ``mediainfo`` or
``grep`` can, so the ownership of a leaked file can always be established. The
header text is deliberately plain ASCII: a UTF-8 INFO chunk is not read
consistently by every tool, and the rights holder's name is spelled in Latin
letters here for the same reason. The canonical wording lives in
[LICENSE](../LICENSE) and [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).
"""

from __future__ import annotations

import struct
from pathlib import Path

# Used by the notices in LICENSE, the legal texts and every WAV header.
RIGHTS_HOLDER_RU = "Дима Густенёв (Dimasi4ek229)"
RIGHTS_HOLDER_LATIN = "Dima Gustenev (Dimasi4ek229)"
PROJECT = "Cubic Battle 4"
YEAR = "2026"
COPYRIGHT = f"(C) {YEAR} {RIGHTS_HOLDER_LATIN}. All rights reserved."
ORIGIN = "Original work synthesised by the Cubic Battle 4 tools. No samples, no third-party recordings or melodies."


def _info_chunk(track_name: str, generator: str) -> bytes:
    """RIFF ``LIST``/``INFO`` chunk with the project's copyright fields."""
    fields = (
        (b"INAM", track_name.encode("ascii", "replace")),
        (b"IART", RIGHTS_HOLDER_LATIN.encode("ascii", "replace")),
        (b"ICOP", COPYRIGHT.encode("ascii", "replace")),
        (b"ICMT", ORIGIN.encode("ascii", "replace")),
        (b"ICRD", YEAR.encode("ascii")),
        (b"ISFT", generator.encode("ascii", "replace")),
    )
    body = b"INFO"
    for tag, value in fields:
        # Every INFO value is a NUL-terminated string, padded to an even size.
        data = value + b"\x00"
        if len(data) % 2:
            data += b"\x00"
        body += tag + struct.pack("<I", len(data)) + data
    return b"LIST" + struct.pack("<I", len(body)) + body


def wav_header(channels: int, rate: int, frames: int, track_name: str, generator: str) -> bytes:
    """RIFF header for a 16-bit PCM file that is ``frames`` samples long."""
    block = channels * 2
    data_bytes = frames * block
    info = _info_chunk(track_name, generator)
    fmt = struct.pack("<4sIHHIIHH", b"fmt ", 16, 1, channels, rate, rate * block, block, 16)
    total = 4 + len(fmt) + len(info) + 8 + data_bytes
    return (b"RIFF" + struct.pack("<I", total) + b"WAVE" + fmt + info +
            b"data" + struct.pack("<I", data_bytes))


def write_wav(path: Path, channels: int, rate: int, pcm: bytes, track_name: str,
              generator: str = "tools") -> None:
    """Write a whole 16-bit PCM file with the project's notice in its header."""
    path.parent.mkdir(parents=True, exist_ok=True)
    frames = len(pcm) // (channels * 2)
    with path.open("wb") as handle:
        handle.write(wav_header(channels, rate, frames, track_name, generator))
        handle.write(pcm)


def open_wav(path: Path, channels: int, rate: int, frames: int, track_name: str,
             generator: str = "tools"):
    """Open a stream for generators that write the samples in pieces.

    The header needs the final length, so ``frames`` must be known up front;
    the caller then writes exactly ``frames`` samples and closes the file.
    """
    path.parent.mkdir(parents=True, exist_ok=True)
    handle = path.open("wb")
    handle.write(wav_header(channels, rate, frames, track_name, generator))
    return handle


def read_info(path: Path) -> dict[str, str]:
    """Read the INFO chunk back. Used by the tests; raises on a broken header."""
    raw = path.read_bytes()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError(f"{path.name}: not a RIFF/WAVE file")
    pos = 12
    found: dict[str, str] = {}
    while pos + 8 <= len(raw):
        tag = raw[pos:pos + 4]
        size = struct.unpack("<I", raw[pos + 4:pos + 8])[0]
        body = pos + 8
        if tag == b"LIST" and raw[body:body + 4] == b"INFO":
            inner = body + 4
            end = min(len(raw), body + size)
            while inner + 8 <= end:
                field = raw[inner:inner + 4]
                length = struct.unpack("<I", raw[inner + 4:inner + 8])[0]
                found[field.decode("ascii", "replace")] = raw[inner + 8:inner + 8 + length].split(b"\x00")[0].decode(
                    "ascii", "replace")
                inner += 8 + length + (length % 2)
        pos = body + size + (size % 2)
    return found
