#!/usr/bin/env python3
"""Generational GC regression tests.

These cover a class of bug where the remembered set (old containers pointing
into the nursery) could be corrupted: an object stayed linked after its
"queued" flag was cleared, so the next write barrier pushed it onto the list
again and made `touch_next` point at itself. The symptom was not a wrong
answer but a run that never finished (infinite loop walking a cyclic list) or
a segfault from walking recycled slab memory. Every case below used to hang.

Run with:  python3 tools/test_gc.py
"""

import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VM = os.path.join(ROOT, "build", "vm", "ds_run")
TESTS = os.path.join(ROOT, "tools", "tests")
TIMEOUT = 120.0


def run(name, expect_lines):
    path = os.path.join(TESTS, name)
    if not os.path.exists(VM):
        print(f"  skip {name}: {VM} not built (run tools/host_test/build_vm.sh)")
        return True
    started = time.time()
    try:
        proc = subprocess.run([VM, path], capture_output=True, text=True, timeout=TIMEOUT)
    except subprocess.TimeoutExpired:
        print(f"  FAIL {name}: still running after {TIMEOUT:.0f}s")
        return False
    elapsed = time.time() - started
    out = [ln for ln in proc.stdout.splitlines() if ln and not ln.startswith("--- ")]
    if proc.returncode != 0:
        print(f"  FAIL {name}: exit {proc.returncode} ({elapsed:.1f}s)")
        return False
    if out != expect_lines:
        print(f"  FAIL {name}: got {out}, expected {expect_lines}")
        return False
    print(f"  ok   {name} ({elapsed:.1f}s)")
    return True


def main():
    # The interpreter reports crashes through the exit code; -1 means the
    # script itself failed, which the runner turns into a failed assertion.
    cases = [
        ("gc_many_strings.ds", ["8000"]),
        ("gc_growing_list.ds", ["60000"]),
        ("gc_generational.ds", ["1", "20000", "19999"]),
    ]
    passed = 0
    for name, expect in cases:
        if run(name, expect):
            passed += 1
    total = len(cases)
    print(f"{passed}/{total} passed")
    return 0 if passed == total else 1


if __name__ == "__main__":
    sys.exit(main())
