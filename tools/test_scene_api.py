#!/usr/bin/env python3
"""Studio scene API tests.

Drives the `scene` global from the host VM and checks the results a script can
observe: hierarchy, world positions, easing, spin, find/count/clear, error
reporting and handle lifetime. The point is that these are the exact
operations the guide in docs/studio.md tells people to use.

Run with:  python3 tools/test_scene_api.py
"""

import os
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VM = os.path.join(ROOT, "build", "vm", "ds_run")


def run_source(src):
    """Run a script, return (ok, stdout lines, stderr)."""
    with tempfile.NamedTemporaryFile("w", suffix=".ds", delete=False) as f:
        f.write(src)
        path = f.name
    try:
        proc = subprocess.run([VM, path], capture_output=True, text=True, timeout=60)
    except subprocess.TimeoutExpired:
        return False, [], "timeout"
    finally:
        os.unlink(path)
    lines = [ln for ln in proc.stdout.splitlines() if ln and not ln.startswith("--- ")]
    return proc.returncode == 0, lines, proc.stderr


CASES = []


def case(fn):
    CASES.append(fn)
    return fn


@case
def hierarchy_moves_children():
    ok, out, _ = run_source("""
m = scene.model("M")
c = scene.part("C", "block")
c.parent(m)
c.move(0, 2, 0)
m.move(10, 0, 0)
p = c.position()
print(p["x"], p["y"], p["z"])
""")
    return ok and out == ["10 2 0"], out


@case
def nesting_composes_rotation():
    # A 90 degree parent turn should swing a child offset on +x onto -z.
    ok, out, _ = run_source("""
m = scene.model("M")
c = scene.part("C", "block")
c.parent(m)
c.move(2, 0, 0)
m.rotate(0, 90, 0)
p = c.position()
print(p["z"])
""")
    return ok and out and abs(float(out[0]) - (-2.0)) < 0.01, out


@case
def animate_reaches_target():
    ok, out, _ = run_source("""
b = scene.part("B", "block")
b.move(0, 0, 0)
b.animate(0, 5, 0, 0, 0, 0, 30.0, "ease")
for i in range(120):
    scene.tick(1.0 / 60.0)
p = b.position()
print(p["y"])
""")
    return ok and out and abs(float(out[0]) - 5.0) < 0.01, out


@case
def spin_wraps_and_is_linear():
    ok, out, _ = run_source("""
b = scene.part("B", "block")
b.spin(90)
scene.tick(1.0)
print(scene.find("B") != None)
""")
    return ok and out == ["True"], out


@case
def find_missing_is_none():
    ok, out, _ = run_source("""
print(scene.find("Nope"))
""")
    return ok and out == ["None"], out


@case
def count_and_clear():
    ok, out, _ = run_source("""
scene.part("A", "block")
scene.part("B", "sphere")
print(scene.count())
scene.clear()
print(scene.count())
""")
    # root alone is 1 after clear
    return ok and out == ["3", "1"], out


@case
def removed_node_is_a_noop():
    # Holding a handle past removal must not crash the script.
    ok, out, _ = run_source("""
a = scene.part("A", "block")
a.remove()
a.move(1, 1, 1)
a.color(1, 2, 3)
print("survived", scene.count())
""")
    return ok and out == ["survived 1"], out


@case
def bad_size_is_reported():
    ok, out, _ = run_source("""
a = scene.part("A", "block")
a.size(0, 1, 1)
print("after")
""")
    # The error aborts the script, so "after" must not print.
    return (not ok) and "after" not in out, out


@case
def zero_children_does_not_crash():
    ok, out, _ = run_source("""
m = scene.model("Empty")
p = m.position()
print(p["x"], p["y"], p["z"])
print(scene.count())
""")
    return ok and out == ["0 0 0", "2"], out


@case
def many_parts():
    ok, out, _ = run_source("""
for i in range(200):
    p = scene.part("P", "block")
    p.move(i, 0, 0)
print(scene.count())
""")
    return ok and out == ["201"], out


def main():
    if not os.path.exists(VM):
        print(f"  skip: {VM} not built (run tools/host_test/build_vm.sh)")
        return 0
    passed = 0
    for fn in CASES:
        try:
            good, detail = fn()
        except Exception as exc:  # noqa: BLE001 - report, do not abort the run
            good, detail = False, f"exception: {exc}"
        name = fn.__name__.replace("_", " ")
        if good:
            passed += 1
            print(f"  ok   {name}")
        else:
            print(f"  FAIL {name}: {detail}")
    print(f"{passed}/{len(CASES)} passed")
    return 0 if passed == len(CASES) else 1


if __name__ == "__main__":
    sys.exit(main())
