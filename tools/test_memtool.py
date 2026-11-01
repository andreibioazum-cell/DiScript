#!/usr/bin/env python3
"""End-to-end test for the memtool daemon (memtool/memtool.c + panel.html).

Builds the host binaries, launches the fake Unity game THROUGH the daemon
(so the game is a daemon descendant and /proc/<pid>/mem stays readable even
with kernel.yama.ptrace_scope=1 on CI), then walks the whole user story:
games list -> launch -> attach (IL2CPP detection) -> first/next scan ->
live write -> freeze -> libil2cpp static by module+offset -> f64/UTF-16 ->
unknown snapshot + changed -> pointer values -> peeks -> global-metadata.dat
serving and the panel's JS metadata parser (via node, if present).

  python3 tools/test_memtool.py
"""

from __future__ import annotations

import json
import os
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "memtool"
DAEMON = BUILD / "memtool"
FAKE_GAME = BUILD / "fake_unity_game"
PANEL = ROOT / "memtool" / "panel.html"
TOKEN = "test-token"

FAILURES: list[str] = []


def check(name: str, cond: bool, extra: str = "") -> void:
    if cond:
        print(f"[test] ok: {name}")
    else:
        print(f"[test] FAIL: {name} {extra}")
        FAILURES.append(name)


def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def make_fake_metadata(path: Path) -> bytes:
    """Minimal IL2CPP global-metadata.dat v29 (L27 TypeDefinition layout):
    PlayerData{coins,gems,health,maxHealth,score,ammo}, EnemyAI{speed,damage}."""
    strings = ["", "coins", "gems", "health", "maxHealth", "score", "ammo",
               "PlayerData", "Game", "speed", "EnemyAI", "damage"]
    heap = b""
    offs: dict[str, int] = {}
    for s in strings:
        offs[s] = len(heap)
        heap += s.encode() + b"\x00"
    field_names = ["coins", "gems", "health", "maxHealth", "score", "ammo", "speed", "damage"]
    fields = b"".join(struct.pack("<IiI", offs[n], 0, 0x04000001 + i)
                      for i, n in enumerate(field_names))

    def typedef(name: str, ns: str, field_start: int, field_count: int) -> bytes:
        t = bytearray(84)
        struct.pack_into("<I", t, 0, offs[name])
        struct.pack_into("<I", t, 4, offs[ns])
        struct.pack_into("<i", t, 28, field_start)
        struct.pack_into("<H", t, 64, field_count)
        return bytes(t)

    types = typedef("PlayerData", "Game", 0, 6) + typedef("EnemyAI", "Game", 6, 2)
    header = bytearray(8 + 32 * 8)
    struct.pack_into("<I", header, 0, 0xFAB11BAF)
    struct.pack_into("<i", header, 4, 29)

    def pair(idx: int, off: int, size: int) -> None:
        struct.pack_into("<ii", header, 8 + idx * 8, off, size)

    pos = len(header)
    pair(2, pos, len(heap)); pos += len(heap)
    pair(11, pos, len(fields)); pos += len(fields)
    pair(16, pos, len(types)); pos += len(types)
    blob = bytes(header) + heap + fields + types
    path.write_bytes(blob)
    return blob


class Daemon:
    def __init__(self, port: int):
        self.base = f"http://127.0.0.1:{port}"

    def api(self, path: str, body: dict | None = None, token: str = TOKEN):
        data = json.dumps(body).encode() if body is not None else None
        req = urllib.request.Request(
            self.base + path, data=data,
            headers={"X-Token": token, "Content-Type": "application/json"},
            method="POST" if data is not None else "GET")
        with urllib.request.urlopen(req, timeout=30) as r:
            return r.status, r.read()

    def j(self, path: str, body: dict | None = None) -> dict:
        _, raw = self.api(path, body)
        return json.loads(raw)

    def wait_scan(self, timeout: float = 30.0) -> dict:
        t0 = time.time()
        while time.time() - t0 < timeout:
            st = self.j("/api/status")
            if not st["scanning"]:
                if st["scan_error"]:
                    raise RuntimeError(f"scan error: {st['scan_error']}")
                return st
            time.sleep(0.05)
        raise RuntimeError("scan did not finish")

    def results_until(self, want_addr: str, max_pages: int = 40) -> tuple[bool, list]:
        """Page through results until want_addr shows up; return (found, first_page_items)."""
        off = 0
        first_page: list = []
        for _ in range(max_pages):
            j = self.j(f"/api/results?offset={off}&limit=1000")
            if off == 0:
                first_page = j["items"]
            if any(it["addr"] == want_addr for it in j["items"]):
                return True, first_page
            if not j["items"]:
                break
            off += len(j["items"])
        return False, first_page


def main() -> int:
    if not os.environ.get("MEMTOOL_NO_BUILD"):
        print("[test] building host binaries...")
        r = subprocess.run([sys.executable, str(ROOT / "tools" / "build_memtool.py")], cwd=ROOT)
        check("build_memtool.py succeeds", r.returncode == 0)
        if r.returncode != 0:
            return 1
    check("daemon binary exists", DAEMON.exists())
    check("fake game binary exists", FAKE_GAME.exists())

    tmp = Path(tempfile.mkdtemp(prefix="memtool_test_"))
    state_path = tmp / "state.json"
    meta_blob = make_fake_metadata(tmp / "global-metadata.dat")
    port = free_port()
    d = Daemon(port)
    demo_cmd = f"{FAKE_GAME} {state_path} --dir {tmp} --tick-ms 1000 --dec"
    daemon = subprocess.Popen(
        [str(DAEMON), "--port", str(port), "--token", TOKEN,
         "--workdir", str(tmp / "work"), "--metadata", str(tmp / "global-metadata.dat"),
         "--demo-game", demo_cmd],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    game_pid = 0
    try:
        # wait for daemon
        for _ in range(100):
            try:
                st = d.j("/api/status")
                break
            except Exception:
                time.sleep(0.1)
        else:
            out = daemon.stdout.read() if daemon.stdout else ""
            print(f"[test] FAIL: daemon never came up\n{out}")
            return 1
        check("daemon status ok", st.get("ok") is True)
        check("status reports no target yet", st.get("pid", 0) == 0)

        # token enforcement
        code = None
        try:
            code, _ = d.api("/api/status", token="WRONG")
        except urllib.error.HTTPError as e:
            code = e.code
        check("bad token rejected with 401", code == 401, f"got {code}")

        # panel embedded and served
        with urllib.request.urlopen(d.base + "/", timeout=10) as r:
            html = r.read().decode()
        check("panel served at /", "MemTool" in html and "Сканер памяти" in html)

        # games list: the demo game is offered, marked IL2CPP, not running yet
        games = d.j("/api/games")["games"]
        demo = [g for g in games if g["pkg"] == "fake.unity.game"]
        check("demo game listed", len(demo) == 1 and demo[0]["engine"] == "il2cpp", str(games))

        # launch the game through the daemon, wait for its state file
        r = d.j("/api/launch", {"pkg": "fake.unity.game"})
        game_pid = r["pid"]
        check("launch returns pid", game_pid > 0, str(r))

        def state() -> dict:
            for _ in range(100):
                try:
                    return json.loads(state_path.read_text())
                except Exception:
                    time.sleep(0.1)
            raise RuntimeError("fake game state file never appeared")

        s0 = state()
        check("launched pid is the game", s0["pid"] == game_pid, f"{s0['pid']} vs {game_pid}")
        procs = d.j("/api/processes?q=fake_unity_game")["processes"]
        check("game visible in processes", any(p["pid"] == game_pid for p in procs))
        games = d.j("/api/games")["games"]
        check("game now running in games list", any(g.get("pid") == game_pid for g in games))

        # attach + Unity detection
        st = d.j("/api/attach", {"pid": game_pid})
        check("attach keeps pid", st["pid"] == game_pid)
        check("engine detected: il2cpp", st["engine"] == "il2cpp", st["engine"])
        check("unity version sniffed", st["unity_version"] == "2022.3.15f1", st["unity_version"])
        ui = d.j("/api/unity/info")
        modnames = [m["name"] for m in ui["modules"]]
        check("unity modules listed", "libil2cpp.so" in modnames and "libunity.so" in modnames, str(modnames))
        check("module bases for rebase", ui["bases"].get("libil2cpp.so", "").startswith("0x"))

        # let coins drop below the 1337 decoy crowd, then exact-scan the value
        while state()["coins"] >= 1331:
            time.sleep(0.15)
        s = state()
        coins_addr = s["coins_addr"]
        d.j("/api/scan/first", {"type": "i32", "mode": "exact", "value": str(s["coins"]),
                                "regions": "unity-il2cpp", "align": 0})
        st = d.wait_scan()
        check("first scan finds a manageable crowd", 1 <= st["results"] < 100, str(st["results"]))
        found, _ = d.results_until(coins_addr)
        check("coins address in results", found)

        # narrow: the game spends 3/tick, decoys do not move
        before = st["results"]
        time.sleep(2.2)
        d.j("/api/scan/next", {"type": "i32", "mode": "DEC", "value": ""})
        st = d.wait_scan()
        found, page = d.results_until(coins_addr)
        check("decreased scan keeps coins", found)
        check("decreased scan shrinks results", 1 <= st["results"] <= before, f"{before} -> {st['results']}")

        # live write: 777 coins, the game itself must report them
        d.j("/api/write", {"addr": coins_addr, "type": "i32", "value": "777"})
        ok = False
        for _ in range(30):
            if state()["coins"] in (777, 774, 771):
                ok = True
                break
            time.sleep(0.2)
        check("write visible inside the game", ok, f"coins={state()['coins']}")

        # freeze holds the value despite the game spending every second
        d.j("/api/freeze", {"addr": coins_addr, "type": "i32", "value": "999", "on": True})
        time.sleep(2.2)  # let stale pre-freeze snapshots wash out of the state file
        samples, mem_samples = [], []
        for _ in range(6):
            time.sleep(0.55)
            samples.append(state()["coins"])
            rm = d.j("/api/readmany", {"items": [{"addr": coins_addr, "type": "i32"}]})["values"]
            mem_samples.append(rm[0]["value"] if rm else None)
        # In memory the freeze holds 999; the game file may snapshot 996 in the
        # <=50ms gap right after its own decrement — but unfrozen it would fall
        # below 990 within a few ticks, so "never below 996" still proves it.
        check("freeze holds coins at 999 in memory", all(v == "999" for v in mem_samples), str(mem_samples))
        check("freeze keeps game snapshots near 999", all(c in (999, 996) for c in samples), str(samples))
        d.j("/api/freeze", {"addr": coins_addr, "type": "i32", "value": "999", "on": False})
        frz = d.j("/api/freeze/list")["items"]
        check("freeze list shows the entry off",
              any(x["addr"] == coins_addr and x["on"] is False for x in frz), str(frz))

        # static field inside libil2cpp.so by value, module+offset label
        d.j("/api/scan/first", {"type": "i32", "mode": "exact", "value": "424242",
                                "regions": "il2cpp", "align": 0})
        st = d.wait_scan()
        res = d.j("/api/results?offset=0&limit=10")["items"]
        check("static scan finds exactly one", st["results"] == 1, str(st["results"]))
        check("static labelled libil2cpp.so+0x8000",
              bool(res) and res[0]["module"] == "libil2cpp.so+0x8000", str(res))
        if res:
            d.j("/api/write", {"addr": res[0]["addr"], "type": "i32", "value": "31337"})
            for _ in range(30):
                if state()["static_coins"] == 31337:
                    break
                time.sleep(0.2)
            check("static write visible in game", state()["static_coins"] == 31337)

        # f64 score at player+0x20
        s = state()
        score_addr = "0x%x" % (int(s["player"], 16) + 0x20)
        d.j("/api/scan/first", {"type": "f64", "mode": "exact", "value": "12345.5",
                                "regions": "unity-il2cpp", "align": 0})
        d.wait_scan()
        found, _ = d.results_until(score_addr)
        check("f64 score found at object+0x20", found)

        # UTF-16 nickname: find, read back, replace
        d.j("/api/scan/first", {"type": "utf16", "mode": "exact", "value": "Dimasi4ek",
                                "regions": "unity-il2cpp", "align": 0})
        st = d.wait_scan()
        res = d.j("/api/results?offset=0&limit=10")["items"]
        check("utf16 nick found once", st["results"] == 1, str(st["results"]))
        if res:
            nick_addr = res[0]["addr"]
            check("utf16 value decoded", (res[0]["value"] or "").startswith("Dimasi4ek229"), str(res))
            d.j("/api/write", {"addr": nick_addr, "type": "utf16", "value": "Azum"})
            rm = d.j("/api/readmany", {"items": [{"addr": nick_addr, "type": "utf16"}]})["values"]
            check("utf16 replace works", bool(rm) and rm[0]["value"].startswith("Azum"), str(rm))

        # unknown snapshot of libil2cpp, then "changed" after touching the static
        d.j("/api/scan/first", {"type": "i32", "mode": "unknown", "value": "",
                                "regions": "il2cpp", "align": 0})
        st = d.wait_scan()
        check("unknown snapshot covers the region", st["results"] > 100000, str(st["results"]))
        static_addr = "0x%x" % (int(ui["bases"]["libil2cpp.so"], 16) + 0x8000)
        d.j("/api/write", {"addr": static_addr, "type": "i32", "value": "424243"})
        d.j("/api/scan/next", {"type": "i32", "mode": "changed", "value": ""})
        st = d.wait_scan()
        found, _ = d.results_until(static_addr)
        check("changed narrowing finds the touched static", found, f"results={st['results']}")

        # pointer hunt: every fake object header points at libil2cpp base
        ilbase = ui["bases"]["libil2cpp.so"]
        d.j("/api/scan/first", {"type": "i64", "mode": "exact", "value": str(int(ilbase, 16)),
                                "regions": "anon", "align": 8})
        st = d.wait_scan()
        player_addr = state()["player"]
        found, _ = d.results_until(player_addr)
        check("object header pointer scan finds Player object", found, f"results={st['results']}")

        # peeks: window contains the live coins at the right offset
        import base64
        pk = d.j("/api/peeks", {"addr": coins_addr, "radius": 128})
        raw = base64.b64decode(pk["data"])
        off = int(coins_addr, 16) - int(pk["base"], 16)
        got = struct.unpack_from("<i", raw, off)[0]
        now = state()["coins"]
        check("peeks window shows live coins", abs(got - now) <= 6, f"peek={got} state={now}")

        # metadata endpoint serves exactly the file the daemon was given
        _, raw = d.api("/api/unity/metadata")
        check("metadata endpoint serves the .dat", raw == meta_blob, f"{len(raw)} vs {len(meta_blob)}")

        # panel JS parser against the same fixture (needs node)
        node = shutil.which("node")
        if node:
            panel_src = PANEL.read_text()
            begin = panel_src.index("//__METADATA_PARSER_BEGIN__")
            end = panel_src.index("//__METADATA_PARSER_END__")
            parser_js = panel_src[begin:end]
            runner = tmp / "parse_meta.mjs"
            runner.write_text(
                "import fs from 'node:fs';\n" + parser_js +
                "\nconst buf = fs.readFileSync(process.argv[2]);"
                "\nconst m = parseIl2CppMetadata(buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.byteLength));"
                "\nconsole.log(JSON.stringify(m));\n")
            r = subprocess.run([node, str(runner), str(tmp / "global-metadata.dat")],
                               capture_output=True, text=True, timeout=30)
            check("node runs the panel metadata parser", r.returncode == 0, r.stderr[-300:])
            if r.returncode == 0:
                m = json.loads(r.stdout)
                check("parser: version 29", m["version"] == 29)
                types = {t["name"]: t for t in (m["types"] or [])}
                check("parser: PlayerData fields",
                      "coins" in types.get("PlayerData", {}).get("fields", []) and
                      "health" in types.get("PlayerData", {}).get("fields", []), json.dumps(m)[:300])
                check("parser: EnemyAI fields",
                      types.get("EnemyAI", {}).get("fields") == ["speed", "damage"])
        else:
            print("[test] skip: node не найден, парсер панели не проверяю")

        # detach cleans up
        d.j("/api/detach", {})
        st = d.j("/api/status")
        check("detach clears target", st["pid"] == 0)
    finally:
        if game_pid:
            try:
                os.kill(game_pid, signal.SIGTERM)
            except ProcessLookupError:
                pass
        daemon.terminate()
        try:
            daemon.wait(timeout=5)
        except subprocess.TimeoutExpired:
            daemon.kill()
        shutil.rmtree(tmp, ignore_errors=True)

    if FAILURES:
        print(f"[test] FAILED: {len(FAILURES)}: {FAILURES}")
        return 1
    print("[test] ALL OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
