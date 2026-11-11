#!/usr/bin/env python3
"""Checks the size limit on the project's own sources."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAX_LINES = 450
SOURCE_SUFFIXES = {".c", ".h", ".inc", ".cpp", ".java", ".xml"}
SOURCE_ROOTS = (ROOT / "src", ROOT / "platform")


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
    for path in project_sources():
        count = line_count(path)
        if count > MAX_LINES:
            oversized.append((path.relative_to(ROOT), count))
    if oversized:
        print(f"Исходники проекта не должны превышать {MAX_LINES} строк:", file=sys.stderr)
        for path, count in oversized:
            print(f"  {path}: {count}", file=sys.stderr)
        return 1
    print(f"Размер исходников: норма, не более {MAX_LINES} строк")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
