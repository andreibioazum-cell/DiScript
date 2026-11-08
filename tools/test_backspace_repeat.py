#!/usr/bin/env python3
"""Builds and runs the pure C Backspace hold-repeat test."""
from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    compiler = os.environ.get("CC", "clang")
    with tempfile.TemporaryDirectory(prefix="cb4-backspace-") as directory:
        executable = Path(directory) / "test_backspace_repeat"
        subprocess.run(
            [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror", "-pedantic",
             "-I", str(ROOT / "src"), str(ROOT / "tools/test_backspace_repeat.c"),
             "-o", str(executable)],
            cwd=ROOT, check=True,
        )
        subprocess.run([str(executable)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
