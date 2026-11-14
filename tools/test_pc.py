#!/usr/bin/env python3
"""Headless checks for the DiScript editor on PC.

Each test drives the real binary through the scripted-event harness
(`--open`, `--type`, `--key`, `--run`, `--report`, `--dump`) and asserts on what
the editor reports. Typing alone hides rendering bugs, so the font test also
renders a frame and counts lit pixels.
"""

from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "pc"
BINARY = BUILD / "discript_headless"
ASSETS = BUILD / "assets"
DATA = BUILD / "hdata"
SCRATCH = BUILD / "test_pc"
FAILED: list[str] = []


def check(name: str, ok: bool, detail: str = "") -> None:
    print(f"  {'ok  ' if ok else 'FAIL'} {name}" + (f" - {detail}" if detail and not ok else ""))
    if not ok:
        FAILED.append(name)


def run(*events: str, timeout: int = 120) -> subprocess.CompletedProcess:
    args = [
        str(BINARY),
        "--headless",
        "--size", "540x960",
        "--assets", str(ASSETS),
        "--data", str(DATA),
        "--frames", "40",
        *events,
    ]
    return subprocess.run(args, cwd=ROOT, capture_output=True, text=True, timeout=timeout)


def output_lines(stdout: str) -> list[str]:
    """Console lines the editor printed, in order.

    Error rows look like `out[3] ERR: text`, so the prefix after the closing
    bracket is stripped generically rather than assuming `]: `.
    """
    lines = []
    for line in stdout.splitlines():
        stripped = line.strip()
        if not stripped.startswith("out["):
            continue
        close = stripped.find("]")
        if close < 0:
            continue
        text = stripped[close + 1:].lstrip()
        # Rows are either `]: text` or `] ERR: text`.
        text = text[1:] if text.startswith(":") else text
        text = text.lstrip()
        if text.startswith("ERR:"):
            text = text[len("ERR:"):].lstrip()
        lines.append(text)
    return lines


def source_line(stdout: str) -> str:
    for line in stdout.splitlines():
        if line.strip().startswith("src: "):
            return line.strip()[len("src: "):]
    return ""


def count_lit_pixels(path: Path, threshold: int = 90) -> int:
    """Bright pixels in a 24/32-bit BMP: a cheap proxy for "something rendered"."""
    data = path.read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width = struct.unpack_from("<i", data, 18)[0]
    height = abs(struct.unpack_from("<i", data, 22)[0])
    bpp = struct.unpack_from("<H", data, 28)[0] // 8
    stride = ((width * bpp + 3) // 4) * 4
    lit = 0
    for y in range(height):
        row = offset + y * stride
        for x in range(width):
            o = row + x * bpp
            if data[o] > threshold or data[o + 1] > threshold or data[o + 2] > threshold:
                lit += 1
    return lit


def test_font_and_render() -> None:
    font = ASSETS / "fonts" / "DejaVuSansMono.ttf"
    check("the monospaced font is found", font.exists(), str(font))

    SCRATCH.mkdir(parents=True, exist_ok=True)
    src = SCRATCH / "render.ds"
    src.write_text("def fib(n):\n    return n\n\nprint(fib(7))\n")
    shot = SCRATCH / "render.bmp"
    if shot.exists():
        shot.unlink()
    result = run("--open", f"3:{src}", "--dump", f"20:{shot}")
    lit = count_lit_pixels(shot) if shot.exists() else 0
    # An empty editor still draws the toolbar and the title; real code adds the
    # glyphs. 400 is comfortably above "background only".
    check("the editor renders text", lit > 400, f"{lit} lit pixels")


def test_typing() -> None:
    result = run("--type", "5:print(1)")
    check("typed text reaches the buffer", "print(1)" in source_line(result.stdout),
          source_line(result.stdout))


def test_caret_follows_text() -> None:
    result = run("--type", "5:abc", "--report")
    line = [ln for ln in result.stdout.splitlines() if ln.startswith("frame ")][-1]
    check("the caret follows the text", "caret=3" in line, line)


def test_run_output() -> None:
    SCRATCH.mkdir(parents=True, exist_ok=True)
    src = SCRATCH / "run.ds"
    src.write_text("print('hello from discript')\n")
    result = run("--open", f"3:{src}", "--run", "6", "--report")
    lines = output_lines(result.stdout)
    check("print() output reaches the output pane",
          any("hello from discript" in ln for ln in lines), str(lines))


def test_run_reports_success() -> None:
    SCRATCH.mkdir(parents=True, exist_ok=True)
    src = SCRATCH / "ok.ds"
    src.write_text("print(2 + 2)\n")
    result = run("--open", f"3:{src}", "--run", "6", "--report")
    lines = output_lines(result.stdout)
    check("the run reports success",
          any(ln.startswith("--- finished") for ln in lines), str(lines))
    check("the program produced its value", any("4" == ln.strip() for ln in lines), str(lines))


def test_jit_is_used() -> None:
    SCRATCH.mkdir(parents=True, exist_ok=True)
    src = SCRATCH / "jit.ds"
    src.write_text(
        "def total(n):\n"
        "    s = 0\n"
        "    for i in range(n):\n"
        "        s = s + i\n"
        "    return s\n"
        "\n"
        "print(total(1000))\n"
    )
    result = run("--open", f"3:{src}", "--run", "6", "--report")
    lines = output_lines(result.stdout)
    check("the loop kernel gives the right answer",
          any(ln.strip() == "499500" for ln in lines), str(lines))
    check("the run reports compiled functions",
          any("jit fn" in ln for ln in lines), str(lines))


def test_syntax_error() -> None:
    SCRATCH.mkdir(parents=True, exist_ok=True)
    src = SCRATCH / "bad.ds"
    src.write_text("def bad(:\n")
    result = run("--open", f"3:{src}", "--run", "6", "--report")
    lines = output_lines(result.stdout)
    check("a syntax error is reported", any("ERROR" in ln for ln in lines), str(lines))


def test_indent_on_colon() -> None:
    result = run("--type", "5:def f(x):", "--enter", "6", "--type", "8:return x")
    src = source_line(result.stdout)
    check("Enter after ':' indents by one level", "def f(x):|    return x" in src, src)


def test_blank_line_dedent() -> None:
    result = run("--type", "5:x = 1", "--enter", "6", "--enter", "7", "--type", "9:y = 2")
    src = source_line(result.stdout)
    check("Enter on a blank line returns to column 0", "x = 1||y = 2" in src, src)


def main() -> int:
    if not BINARY.exists():
        print(f"missing binary: {BINARY}\nrun: python3 tools/build_pc.py --linux")
        return 2
    print("PC editor checks")
    test_font_and_render()
    test_typing()
    test_caret_follows_text()
    test_run_output()
    test_run_reports_success()
    test_jit_is_used()
    test_syntax_error()
    test_indent_on_colon()
    test_blank_line_dedent()
    total = 10
    passed = total - len(FAILED)
    print(f"{passed}/{total} passed")
    return 0 if not FAILED else 1


if __name__ == "__main__":
    sys.exit(main())
