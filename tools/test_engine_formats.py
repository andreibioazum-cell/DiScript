#!/usr/bin/env python3
"""Verify scanf/printf format specifier counts against argument counts across src/."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "src"

CALLS = ("fscanf", "sscanf", "snprintf", "sprintf")
CONV = re.compile(r"%[-+ #0]*[0-9*]*(?:\.[0-9*]+)?(?:hh|h|ll|l|j|z|t|L)?[diouxXeEfgGaAcspn%]")


def split_args(text):
    """Splits an argument list on top-level commas."""
    args, depth, cur = [], 0, ""
    in_str = False
    escape = False
    for ch in text:
        if in_str:
            cur += ch
            if escape:
                escape = False
            elif ch == "\\":
                escape = True
            elif ch == '"':
                in_str = False
            continue
        if ch == '"':
            in_str = True
            cur += ch
            continue
        if ch in "([{":
            depth += 1
        elif ch in ")]}":
            depth -= 1
        if ch == "," and depth == 0:
            args.append(cur.strip())
            cur = ""
            continue
        cur += ch
    if cur.strip():
        args.append(cur.strip())
    return args


def calls(text):
    """Every scanf/printf call with its arguments."""
    for name in CALLS:
        for m in re.finditer(r"\b" + name + r"\s*\(", text):
            start = m.end()
            depth, i, in_str, escape = 1, start, False, False
            while i < len(text) and depth > 0:
                ch = text[i]
                if in_str:
                    if escape:
                        escape = False
                    elif ch == "\\":
                        escape = True
                    elif ch == '"':
                        in_str = False
                elif ch == '"':
                    in_str = True
                elif ch == "(":
                    depth += 1
                elif ch == ")":
                    depth -= 1
                i += 1
            yield name, text[start:i - 1]


def check(path):
    problems = []
    text = path.read_text(encoding="utf-8", errors="ignore")
    for name, arg_text in calls(text):
        parts = split_args(arg_text)
        if not parts:
            continue
        if name in ("fscanf", "sscanf"):
                        # The first argument is the file, the second is the format.
            if len(parts) < 3:
                continue
            fm = re.match(r'\s*"((?:[^"\\]|\\.)*)"', parts[1])
            if not fm:
                continue
            spec = fm.group(1)
            rest = parts[2:]
        else:
            fm = re.match(r'\s*"((?:[^"\\]|\\.)*)"', parts[0])
            if not fm:
                continue
            spec = fm.group(1)
            if name == "snprintf" and len(parts) > 2:
                rest = parts[2:]
            else:
                rest = parts[1:]
        need = len([c for c in CONV.findall(spec) if c != "%%"])
        have = len(rest)
        if need != have:
            line = text[: text.find(arg_text)].count("\n") + 1
            problems.append(f"{path.relative_to(ROOT)}:{line}: {name} ждёт {need} аргументов, передано {have}")
    return problems


def main() -> int:
    problems = []
    for path in sorted(SRC.rglob("*")):
        if path.is_file() and path.suffix in (".c", ".h", ".inc"):
            problems.extend(check(path))
    if problems:
        print("Формат и аргументы не совпадают:", file=sys.stderr)
        for p in problems:
            print("  " + p, file=sys.stderr)
        return 1
    print("Форматы scanf/printf совпадают с аргументами: норма")
    return 0


if __name__ == "__main__":
    sys.exit(main())
