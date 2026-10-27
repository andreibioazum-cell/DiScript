#!/usr/bin/env python3
"""Checks the size limit and the stdio-free game half of the sources."""

import re
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAX_LINES = 450
SOURCE_SUFFIXES = {".c", ".h", ".inc", ".cpp", ".java", ".xml"}
SOURCE_ROOTS = (ROOT / "src", ROOT / "platform")
# src/game is compiled without <stdio.h> (the Windows build goes through zig cc
# and C99 forbids implicit declarations), so C library I/O has no business there.
GAME_ROOT = ROOT / "src" / "game"
STDIO_CALL = re.compile(r"\b(snprintf|vsnprintf|printf|fprintf|sprintf|fopen|fread|fwrite)\s*\(")


def project_sources():
    for directory in SOURCE_ROOTS:
        for path in sorted(directory.rglob("*")):
            if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
                yield path


def line_count(path: Path) -> int:
    with path.open("rb") as source:
        return sum(1 for _ in source)


def main() -> int:
    oversized = []
    stdio_users = []
    for path in project_sources():
        count = line_count(path)
        if count > MAX_LINES:
            oversized.append((path.relative_to(ROOT), count))
        if GAME_ROOT in path.parents and path.suffix.lower() in {".c", ".h", ".inc", ".cpp"}:
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if STDIO_CALL.search(line):
                    stdio_users.append((path.relative_to(ROOT), number))
    if oversized:
        print(f"Исходники проекта не должны превышать {MAX_LINES} строк:", file=sys.stderr)
        for path, count in oversized:
            print(f"  {path}: {count}", file=sys.stderr)
        return 1
    if stdio_users:
        print("src/game собирается без stdio, эти вызовы ломают сборку Windows:", file=sys.stderr)
        for path, number in stdio_users:
            print(f"  {path}:{number}", file=sys.stderr)
        return 1
    print(f"Размер исходников: норма, не более {MAX_LINES} строк, без stdio в src/game")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
