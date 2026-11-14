#!/usr/bin/env python3
"""A/B benchmark: native JIT versus the closure interpreter.

Runs the same numeric kernels with DS_NOJIT unset and set, and reports the
execution time the VM itself measured (excluding compilation). The first run
of each path is discarded as a warm-up.
"""

import os
import re
import statistics
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNNER = os.path.join(ROOT, "build", "vm", "ds_run")
WORK = os.path.join(ROOT, "build", "vm")

KERNELS = {
    "range sum  (1M iters)": ("n = 1000000", """
def work(n):
    s = 0
    for i in range(n):
        s = s + i * 2
    return s
print(work(n))
"""),
    "while loop (1M iters)": ("n = 1000000", """
def work(n):
    i = 0
    s = 0
    while i < n:
        s = s + i
        i = i + 1
    return s
print(work(n))
"""),
    "branchy loop (1M)": ("n = 1000000", """
def work(n):
    s = 0
    for i in range(n):
        if i % 2 == 0:
            s = s + i
        else:
            s = s - 1
    return s
print(work(n))
"""),
    "arith mix (1M)": ("n = 1000000", """
def work(n):
    a = 1
    b = 2
    for i in range(n):
        a = a + b * 3 - 1
        b = b + 1
    return a + b
print(work(n))
"""),
}

TIME_RE = re.compile(r"run=([0-9.]+)ms")
JIT_RE = re.compile(r"jit=(\d+)/")


def run_once(path, nojit):
    env = dict(os.environ)
    if nojit:
        env["DS_NOJIT"] = "1"
    else:
        env.pop("DS_NOJIT", None)
    out = subprocess.run([RUNNER, path], capture_output=True, text=True, env=env, timeout=300)
    m = TIME_RE.search(out.stdout)
    j = JIT_RE.search(out.stdout)
    return float(m.group(1)) if m else float("nan"), int(j.group(1)) if j else -1


def main():
    if not os.path.exists(RUNNER):
        print(f"missing runner: {RUNNER}\nrun tools/host_test/build_vm.sh first")
        return 2
    print(f"{'kernel':<24}{'interp':>10}{'jit':>10}{'speedup':>10}{'jit fns':>9}")
    print("-" * 63)
    ratios = []
    for name, (params, body) in KERNELS.items():
        path = os.path.join(WORK, "bench_case.ds")
        with open(path, "w") as fh:
            fh.write(params + body)
        run_once(path, True)
        run_once(path, False)
        interp = statistics.median(run_once(path, True)[0] for _ in range(3))
        jit, njit = run_once(path, False)
        jit = statistics.median([jit] + [run_once(path, False)[0] for _ in range(2)])
        ratio = interp / jit if jit > 0 else 0.0
        if ratio > 0 and njit > 0:
            ratios.append(ratio)
        print(f"{name:<24}{interp:>8.1f}ms{jit:>8.1f}ms{ratio:>9.1f}x{njit:>9}")
    if ratios:
        print("-" * 63)
        print(f"{'geometric-ish mean':<24}{'':>18}{statistics.median(ratios):>9.1f}x")
    return 0


if __name__ == "__main__":
    sys.exit(main())
