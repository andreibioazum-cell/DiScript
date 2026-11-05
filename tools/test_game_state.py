#!/usr/bin/env python3
"""Builds and checks creation and reset of the game state."""

from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main() -> int:
    compiler = os.environ.get("CC", "clang")
    with tempfile.TemporaryDirectory(prefix="cb4-state-") as directory:
        executable = Path(directory) / "test_game_state"
        subprocess.run(
            [
                compiler,
                "-std=c99",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-Wno-unused-function",
                "-pedantic",
                "-ffunction-sections",
                "-fdata-sections",
                "-I",
                str(ROOT / "src"),
                "-I",
                str(ROOT),
                "-I",
                str(ROOT / "tools/host_test/stub"),
                str(ROOT / "tools/test_game_state.c"),
                "-Wl,--gc-sections",
                "-lm",
                "-o",
                str(executable),
            ],
            cwd=ROOT,
            check=True,
        )
        subprocess.run([str(executable)], cwd=ROOT, check=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
