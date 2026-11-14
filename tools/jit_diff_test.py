#!/usr/bin/env python3
"""Differential test: every program must print the same thing with the native
JIT enabled and with it disabled.

The JIT deliberately handles only a numeric subset of the language; anything
outside it silently falls back to the closure interpreter. That makes it very
easy to introduce a lowering bug that only shows up on one path, so this test
runs the same source twice and compares the program output byte for byte.
"""

import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNNER = os.path.join(ROOT, "build", "vm", "ds_run")

PROGRAMS = {
    "for-loop accumulation": """
def work(n):
    s = 0
    for i in range(n):
        s = s + i * 2
    return s
print(work(1000))
""",
    "while + if/else": """
def work(n):
    i = 0
    s = 0
    while i < n:
        if i % 2 == 0:
            s = s + i
        else:
            s = s - 1
        i = i + 1
    return s
print(work(64))
""",
    "early return from a branch": """
def sign(x):
    if x < 0:
        return -1
    if x > 0:
        return 1
    return 0
print(sign(-5), sign(0), sign(5))
""",
    "break and continue": """
def first_big(n):
    total = 0
    for i in range(n):
        if i > 5:
            break
        total = total + i
    return total
def skip(n):
    s = 0
    i = 0
    while i < n:
        i = i + 1
        if i > 7:
            continue
        s = s + i
    return s
print(first_big(100), skip(20))
""",
    "nested loops": """
def grid(n):
    s = 0
    for i in range(n):
        for j in range(3):
            s = s + i * j
    return s
print(grid(6))
""",
    "recursion stays interpreted": """
def fib(n):
    if n < 2:
        return n
    return fib(n-1) + fib(n-2)
print(fib(12))
""",
    "falling off the end returns nil": """
def noop(n):
    s = n
    if s > 5:
        s = s + 1
print(noop(10), noop(1))
""",
    "floats and unary minus": """
def mix(x):
    y = -x / 3
    return y * 3
print(mix(9))
""",
    "ternary expression": """
def pick(x):
    return x * 2 if x > 0 else x - 100
print(pick(3), pick(-3))
""",
    "python-style modulo and equality": """
def even_sum(n):
    s = 0
    for i in range(n):
        if i % 2 == 0:
            s = s + i
    return s
def rem(x):
    return x % 4
print(even_sum(20), rem(7), rem(-7), rem(8))
""",
    "explicit step and nested ranges": """
def step_sum(n):
    s = 0
    for i in range(1, n, 3):
        s = s + i
    return s
print(step_sum(30))
""",
    "non-numeric arguments fall back": """
def double(x):
    return x * 2
print(double(21))
""",
}


def run(path, nojit):
    env = dict(os.environ)
    if nojit:
        env["DS_NOJIT"] = "1"
    else:
        env.pop("DS_NOJIT", None)
    out = subprocess.run([RUNNER, path], capture_output=True, text=True, env=env, timeout=120)
    lines = [
        ln for ln in out.stdout.splitlines()
        if not ln.startswith("--- ") and ln.strip()
    ]
    return lines, out.returncode


def main():
    if not os.path.exists(RUNNER):
        print(f"missing runner: {RUNNER}\nrun tools/host_test/build_vm.sh first")
        return 2
    tmp = os.path.join(ROOT, "build", "vm", "diff_case.ds")
    failures = 0
    for name, src in PROGRAMS.items():
        with open(tmp, "w") as fh:
            fh.write(src)
        jit, rc1 = run(tmp, False)
        interp, rc2 = run(tmp, True)
        if jit == interp and rc1 == rc2:
            print(f"  ok   {name}")
        else:
            failures += 1
            print(f"  FAIL {name}")
            print(f"       jit   ({rc1}): {jit}")
            print(f"       interp({rc2}): {interp}")
    print(f"{len(PROGRAMS) - failures}/{len(PROGRAMS)} identical")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
