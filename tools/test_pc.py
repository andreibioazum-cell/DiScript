#!/usr/bin/env python3
"""Headless PC build, rendering, input, combat, and network integration tests."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "pc"
BINARY = BUILD / "cb4_headless"
SCRATCH = BUILD / "test_pc"
FAKE_PORT = 18779
FAILED: list[str] = []


def check(name: str, ok: bool, detail: str = "") -> None:
    print(f"  {'ok  ' if ok else 'FAIL'} {name}" + (f" - {detail}" if detail and not ok else ""))
    if not ok:
        FAILED.append(name)


def run(command: list[str], cwd: Path = ROOT, timeout: int = 600) -> subprocess.CompletedProcess:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=timeout)


def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return int(sock.getsockname()[1])


def build(fake_url: str) -> None:
    print("== 1. Сборка ==")
    result = run(
        [
            sys.executable,
            "tools/build_pc.py",
            "--linux",
            "--no-assets",
            "--firebase",
            fake_url,
            "--data-path",
            str(SCRATCH / "home"),
        ]
    )
    check("tools/build_pc.py собрал харнесс", result.returncode == 0, result.stderr[-2000:])
    check("бинарник на месте и непустой", BINARY.is_file() and BINARY.stat().st_size > 200_000)


def stage_assets() -> Path:
    """The APK staging layout (textures/, fonts/, sounds/) is what the game
    reads on both platforms; the harness gets its own copy."""
    assets = SCRATCH / "assets"
    if not (assets / "grass.png").is_file():
        shutil.rmtree(assets, ignore_errors=True)
        result = run([sys.executable, "stage_assets.py", "assets", str(assets)])
        check("ресурсы подготовлены stage_assets.py", result.returncode == 0, result.stderr[-1500:])
    return assets


def harness(assets: Path, data: Path, args: list[str]) -> subprocess.CompletedProcess:
    shutil.rmtree(data, ignore_errors=True)
    data.mkdir(parents=True, exist_ok=True)
    return run([str(BINARY), "--headless", "--size", "540x960", "--assets", str(assets), "--data", str(data), *args])


def parse_reports(stdout: str) -> dict[int, dict[str, str]]:
    reports: dict[int, dict[str, str]] = {}
    for line in stdout.splitlines():
        match = re.match(r"frame (\d+): (.*)", line)
        if not match:
            continue
        fields = dict(part.split("=", 1) for part in match.group(2).split() if "=" in part)
        reports[int(match.group(1))] = fields
    return reports


def read_bmp(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    width, height = struct.unpack_from("<ii", data, 18)
    offset = struct.unpack_from("<I", data, 10)[0]
    stride = (width * 3 + 3) // 4 * 4
    return width, height, data[offset : offset + stride * height]


def bmp_colors(pixels: bytes, width: int, height: int, step: int = 3) -> dict[bytes, int]:
    stride = (width * 3 + 3) // 4 * 4
    colors: dict[bytes, int] = {}
    for y in range(0, height, step):
        row = pixels[y * stride : (y + 1) * stride]
        for x in range(0, width, step):
            color = row[x * 3 : x * 3 + 3]
            colors[color] = colors.get(color, 0) + 1
    return colors


def test_pixels(assets: Path) -> None:
    print("\n== 2. Растеризатор: известные фигуры ==")
    shot = SCRATCH / "gfx.bmp"
    result = harness(assets, SCRATCH / "gfx_home", ["--gfx-test", "--dump", f"0:{shot}"])
    check("тестовая сцена нарисована", result.returncode == 0 and shot.is_file(), result.stderr[-1500:])
    if not shot.is_file():
        return
    width, height, pixels = read_bmp(shot)
    stride = (width * 3 + 3) // 4 * 4

    def pixel(x: int, y: int) -> str:
        row = pixels[(height - 1 - y) * stride : (height - y) * stride]  # BMP is bottom-up
        blue, green, red = row[x * 3], row[x * 3 + 1], row[x * 3 + 2]  # and stores B,G,R
        return f"{red:02x}{green:02x}{blue:02x}"

    check("фон залит цветом bg_screen", pixel(5, 5) == "1a1a2e", pixel(5, 5))
    check("прямоугольник нужного цвета", pixel(60, 40) == "e94560", pixel(60, 40))
    check("круг (треугольники) залит", pixel(240, 90) == "00ff00", pixel(240, 90))
    check("скруглённый прямоугольник залит", pixel(60, 330) == "3366ff", pixel(60, 330))
    check("текст отрисован глифами", any(c not in (b"\x1a\x1a\x2e", b"\x00\x00\x00") for c in
                                         bmp_colors(pixels[420 * stride : 470 * stride], width, 108)))


def test_menu_and_battle(assets: Path) -> None:
    print("\n== 3. Меню и бой ==")
    shot = SCRATCH / "battle.bmp"
    result = harness(
        assets,
        SCRATCH / "play",
        [
            "--frames", "780",
            "--touch", "220:360:686:0", "--touch", "223:360:686:1",   # accept warning
            "--touch", "400:270:336:0", "--touch", "403:270:336:1",   # Play
            "--touch", "460:270:404:0", "--touch", "463:270:404:1",   # Solo
            "--report", "236",
            "--report", "430",
            "--report", "590",
            "--stick", "600:0:-1", "--stick", "618:0:0",              # face + step toward enemy
            "--atk", "600:1", "--atk", "620:0",                       # attack (hits after 0.6 s = 36 frames)
            "--report", "650",
            "--dump", f"740:{shot}",
            "--report", "775",
        ],
    )
    check("игра дошла до конца без ошибок", result.returncode == 0, result.stderr[-2000:])
    reports = parse_reports(result.stdout)
    after_consent = reports.get(236, {})
    check("согласие принято, предупреждение закрылось", after_consent.get("state") == "0", str(after_consent))
    modes = reports.get(430, {})
    check("«Играть» открывает выбор режима", modes.get("state") == "2", str(modes))
    battle = reports.get(650, {})
    before_punch = reports.get(590, {})
    check("бой начался (ST_SOLO)", battle.get("state") == "1" and battle.get("battle") == "1", str(battle))
    check("игрок жив и двигается стиком", float(battle.get("y", "0") or 0) < 470, str(battle))
    check(
        "удар дошёл до врага",
        float(battle.get("foe", "inf")) < float(before_punch.get("foe", "-inf")),
        f"до удара {before_punch.get('foe')}, после {battle.get('foe')}",
    )
    check("в бою не было ошибок JS-хуков", battle.get("error") == "0", str(battle))
    end = reports.get(775, {})
    check(
        "враг отвечает (игрок получает урон)",
        float(end.get("hp", "inf")) < float(battle.get("hp", "inf")),
        f"после удара {battle.get('hp')}, к кадру 775 {end.get('hp')}",
    )
    check("кадр боя сохранён", shot.is_file() and shot.stat().st_size > 100_000)
    if shot.is_file():
        width, height, pixels = read_bmp(shot)
        colors = bmp_colors(pixels, width, height, 5)
        check("кадр боя не пустой (много цветов)", len(colors) > 50, f"{len(colors)} цветов")


def test_controls(assets: Path) -> None:
    """PC keys: E holds the block, Q does not touch the screen for a class with no
    super.

    The ladder of taps leads into a solo fight with the ordinary cube (it has no
    super). Q used to press empty screen, the game read that as the stick and
    teleported the player into the corner of the arena.
    """
    print("\n== 3b. PC controls (mouse / block / super) ==")
    start = [
        "--touch", "220:360:686:0", "--touch", "223:360:686:1",   # consent
        "--touch", "400:270:336:0", "--touch", "403:270:336:1",   # Play
        "--touch", "460:270:404:0", "--touch", "463:270:404:1",   # Solo
        "--stick", "590:0:-1", "--stick", "595:0:0",              # face enemy so shield covers front
    ]

    def play(name: str, extra: list[str]) -> dict[int, dict[str, str]]:
        result = harness(
            assets,
            SCRATCH / name,
            ["--frames", "900", *start, "--report", "600", "--report", "800", *extra],
        )
        return parse_reports(result.stdout)

    plain = play("ctl_plain", [])
    held = play("ctl_block", ["--blk", "600:1"])
    check(
        "E (block) reduces damage taken",
        float(held.get(800, {}).get("hp", "0")) > float(plain.get(800, {}).get("hp", "0")),
        f"no block {plain.get(800, {}).get('hp')}, with block {held.get(800, {}).get('hp')}",
    )
    super_run = play("ctl_super", ["--super", "700:1", "--super", "720:0"])
    check(
        "Q does not move a player whose class has no super",
        super_run.get(800, {}).get("x") == plain.get(800, {}).get("x")
        and super_run.get(800, {}).get("y") == plain.get(800, {}).get("y"),
        f"without Q {plain.get(800, {}).get('x')},{plain.get(800, {}).get('y')}; "
        f"with Q {super_run.get(800, {}).get('x')},{super_run.get(800, {}).get('y')}",
    )


def test_walk_feel(assets: Path) -> None:
    """The cube ramps up and stops in about 0.08 s instead of tearing off.

    A released stick used to stop the cube in the same frame; now the speed fades
    over walk_stop_time and the ramp takes walk_accel_time, both 0.08 s. Checked
    from the per-frame reports: the first frame after the press moves much less
    than a steady walk, and after the release the cube still moves for a few
    frames and then stands.
    """
    print("\n== 3c. Cube ramp up and brake ==")
    start = [
        "--touch", "220:360:686:0", "--touch", "223:360:686:1",
        "--touch", "400:270:336:0", "--touch", "403:270:336:1",
        "--touch", "460:270:404:0", "--touch", "463:270:404:1",
    ]
    events = ["--frames", "760", *start, "--stick", "600:1:0"]
    events += [item for frame in range(600, 641) for item in ("--report", str(frame))]
    events += ["--stick", "640:0:0"]
    events += [item for frame in range(641, 661) for item in ("--report", str(frame))]
    reports = parse_reports(harness(assets, SCRATCH / "walk", events).stdout)

    def x(frame: int) -> float:
        return float(reports.get(frame, {}).get("x", "nan"))

    steady = x(639) - x(638)                  # steady walk, px per frame
    first = x(601) - x(600)                   # the first frame after the press
    check("the cube starts softly instead of jumping to full speed",
          0 < first < steady * 0.5, f"first frame {first:.2f} px, steady {steady:.2f} px")
    ramp = next((f for f in range(600, 640) if x(f) - x(f - 1) > steady * 0.9), None)
    check("the ramp takes about 0.08-0.15 s",
          ramp is not None and 4 <= ramp - 600 <= 10,
          f"full speed on frame {ramp} ({(ramp - 600) / 60 if ramp else -1:.3f} s)")

    moved = [f for f in range(641, 661) if x(f) > x(f - 1) + 0.01]
    stop = max(moved) if moved else 640
    check("after the release the cube stops in about 0.08 s",
          3 <= stop - 640 <= 8, f"stopped after {(stop - 640 + 1) / 60:.3f} s")
    check("after stopping the cube stands still",
          abs(x(660) - x(stop)) < 0.01, f"{x(stop):.2f} -> {x(660):.2f}")


def fake_request(path: str, body: bytes | None = None, method: str = "GET") -> bytes:
    url = f"http://127.0.0.1:{FAKE_PORT}{path}"
    request = urllib.request.Request(url, body, {"Content-Type": "application/json"}, method=method)
    with urllib.request.urlopen(request, timeout=5) as response:
        return response.read()


def harness_async(assets: Path, data: Path, args: list[str], progress: str = "") -> subprocess.Popen:
    """Same command as harness(), but left running so two clients can meet."""
    shutil.rmtree(data, ignore_errors=True)
    data.mkdir(parents=True, exist_ok=True)
    if progress:
        (data / "progress.dat").write_text(progress, encoding="utf-8")
    return subprocess.Popen(
        [str(BINARY), "--headless", "--pace-ms", "16", "--size", "540x960",
         "--assets", str(assets), "--data", str(data), *args],
        cwd=ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def test_brick_online(assets: Path) -> None:
    """Online fight: ebuC's projectile syncs and damages the opponent."""
    print("\n== 3j. ebuC's brick in an online fight ==")
    room = f"brk{int(time.time()) % 100000}"
    nick = f"brk{int(time.time()) % 100000}"
    taps = [
        "--touch", "220:360:686:0", "--touch", "223:360:686:1",
        "--touch", "400:270:558:0", "--touch", "403:270:558:1",
    ]
    with fake_server():
        thrower = harness_async(
            assets,
            SCRATCH / "brick_net_a",
            ["--frames", "2400", *taps, "--login", "500:Dimasi4ek229:admin",
             "--class", "600:3", "--room", f"900:{room}:1",
             "--report", "1500",
             # switch mode, turn diagonally, and press ult: fires immediately
             "--touch", "1560:408:533:0:5", "--touch", "1572:408:533:1:5",
             "--stick", "1580:-0.7:-0.7", "--stick", "1592:0:0",
             "--touch", "1600:408:630:0:7", "--touch", "1608:408:630:1:7",
             "--report", "1615", "--report", "1750", "--report", "2000", "--report", "2300"],
        )
        time.sleep(0.5)
        victim = harness_async(
            assets,
            SCRATCH / "brick_net_b",
            ["--frames", "2400", *taps, "--login", "500:" + nick + ":pass1234",
             "--room", f"960:{room}:0",
             "--report", "1500", "--report", "1630", "--report", "1720", "--report", "1850", "--report", "2300"],
        )
        out_a, err_a = thrower.communicate(timeout=500)
        out_b, err_b = victim.communicate(timeout=500)
    rep_a = parse_reports(out_a)
    rep_b = parse_reports(out_b)
    a1500, a1615 = rep_a.get(1500, {}), rep_a.get(1615, {})
    b1500, b2300 = rep_b.get(1500, {}), rep_b.get(2300, {})
    seen = [rep_b.get(f, {}).get("rbricks") for f in (1630, 1720, 1850, 2300)]
    bounces_max = max(float(rep_a.get(f, {}).get("bounces", "0")) for f in (1615, 1750, 2000))
    check("метатель в онлайне", a1500.get("state") == "5" and a1500.get("battle") == "1", str(a1500))
    check("второй игрок в том же бою", b1500.get("state") == "5" and b1500.get("battle") == "1", str(b1500))
    check("кнопка режимов работает в онлайне", a1615.get("mode") == "1", str(a1615))
    check("кирпич улетел в онлайне",
          ("magic projectile thrown" in err_a or "brick thrown" in err_a) and bounces_max > 0,
          f"bounces={bounces_max} / {err_a[-400:]}")
    check("другой игрок видит кирпич", "1" in seen, f"rbricks по кадрам: {seen}")
    check(
        "кирпич снял здоровье второму игроку",
        float(b2300.get("hp", "nan")) < float(b1500.get("hp", "nan")),
        f"до {b1500.get('hp')}, после {b2300.get('hp')} / {err_b[-300:]}",
    )


def test_codecracker_online(assets: Path) -> None:
    """Non-admin buys CodeCracker for 475 cups and fights with it in an online room."""
    print("\n== 3k. CodeCracker for everyone in an online fight ==")
    stamp = int(time.time()) % 100000
    room = f"ccr{stamp}"
    buyer_nick = f"ccb{stamp}"
    victim_nick = f"ccv{stamp}"
    taps = [
        "--touch", "220:360:686:0", "--touch", "223:360:686:1",
        "--touch", "400:270:558:0", "--touch", "403:270:558:1",
    ]
    with fake_server():
        buyer = harness_async(
            assets,
            SCRATCH / "cc_net_a",
            ["--frames", "2200", *taps, f"--login", f"500:{buyer_nick}:pass1234",
             "--class", "720:6", "--room", f"900:{room}:1",
             "--report", "1500",
             "--stick", "1520:1:0",
             "--super", "1650:1", "--super", "1655:0",
             "--report", "1700", "--report", "2050"],
            progress="500 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n",
        )
        time.sleep(0.5)
        victim = harness_async(
            assets,
            SCRATCH / "cc_net_b",
            ["--frames", "2200", *taps, f"--login", f"500:{victim_nick}:pass1234",
             "--room", f"960:{room}:0",
             "--report", "1500",
             "--stick", "1520:1:0",
             "--report", "1640", "--report", "2050"],
        )
        out_a, err_a = buyer.communicate(timeout=500)
        out_b, err_b = victim.communicate(timeout=500)
        login = json.dumps({"email": "dimasi4ek229@cb4.game", "password": "admin|cb4v1"}).encode()
        token = json.loads(fake_request("/v1/accounts:signInWithPassword?key=K", login, "POST"))["idToken"]
        cloud_user = json.loads(fake_request(f"/users/{buyer_nick}.json?auth={token}").decode())
    rep_a = parse_reports(out_a)
    rep_b = parse_reports(out_b)
    a1500, a2050 = rep_a.get(1500, {}), rep_a.get(2050, {})
    b1500, b1640, b2050 = rep_b.get(1500, {}), rep_b.get(1640, {}), rep_b.get(2050, {})
    check(
        "обычный игрок купил CodeCracker за 475 кубков и сохранил в облако",
        isinstance(cloud_user, dict)
        and cloud_user.get("codecracker") == 1
        and cloud_user.get("cls") == 6
        and cloud_user.get("cups") in (25, 30),
        str(cloud_user),
    )
    check(
        "обычный игрок вошёл в онлайн-бой за CodeCracker (cls=6, hp=12)",
        a1500.get("state") == "5" and a1500.get("battle") == "1"
        and a1500.get("cls") == "6" and abs(float(a1500.get("hp", "0")) - 12.0) < 0.01,
        f"{a1500} / {err_a[-300:]}",
    )
    check(
        "второй игрок в той же комнате",
        b1500.get("state") == "5" and b1500.get("battle") == "1",
        f"{b1500} / {err_b[-300:]}",
    )
    check(
        "ульта CodeCracker в онлайне наносит урон сопернику при касании",
        float(b2050.get("hp", "nan")) < float(b1640.get("hp", "nan")),
        f"до ульты {b1640.get('hp')}, после {b2050.get('hp')} / {err_b[-300:]}",
    )
    check("в онлайне за CodeCracker нет ошибок", a2050.get("error") == "0" and b2050.get("error") == "0", f"{a2050} / {b2050}")


def fake_signs_in() -> bool:
    """The fake remembers every account it handed out, so a server left over
    from an older run can hold a password the game no longer sends. A sign-up
    with a throwaway address tells a live server from a stale one."""
    body = json.dumps({"email": f"probe{time.time()}@cb4.game", "password": "probe"}).encode()
    request = urllib.request.Request(
        f"http://127.0.0.1:{FAKE_PORT}/v1/accounts:signUp?key=K",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=5) as response:
            return response.status == 200
    except Exception:
        return False


class fake_server:
    """The fake database on FAKE_PORT: started here unless a test already has
    one running, and stopped again when the block ends."""

    def __enter__(self) -> "fake_server":
        self.process = None
        if wait_port(FAKE_PORT, timeout=0.3) and fake_signs_in():
            return self
        if wait_port(FAKE_PORT, timeout=0.1):
            raise RuntimeError(
                f"на порту {FAKE_PORT} висит чужая подставная база со старыми паролями - перезапусти "
                "test/fake_firebase.py"
            )
        self.process = subprocess.Popen(
            [sys.executable, "test/fake_firebase.py", str(FAKE_PORT)],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if not wait_port(FAKE_PORT):
            raise RuntimeError("подставная база не поднялась")
        return self

    def __exit__(self, *_exc) -> None:
        if self.process:
            self.process.terminate()
            self.process.wait(timeout=5)


def login_taps() -> list[str]:
    """Consent, the account screen and the admin sign-in, in one line."""
    return [
        "--touch", "220:360:686:0", "--touch", "223:360:686:1",
        "--touch", "400:270:558:0", "--touch", "403:270:558:1",
        "--login", "450:Dimasi4ek229:admin",
    ]


def test_multitouch(assets: Path) -> None:
    """Four fingers at once: stick, block, punch and an instant ball ult.

    Every control keeps its own pointer, so a finger that holds the stick does
    not take the pads away from the others. The run walks a step with the
    stick, keeps a finger on the block, taps the punch under the guard (the
    rules ignore it, the guard has to stay), then drops the guard and fires the
    ball immediately with a fourth finger while the stick is still held.
    """
    print("\n== 3e. Multi-touch: four fingers at once ==")
    with fake_server():
        result = harness(
            assets,
            SCRATCH / "multitouch",
            [
                "--frames", "1600",
                *login_taps(),
                "--class", "750:3",
                "--goto", "800:1",
                "--report", "890",
                # finger 0: the stick, walked up and tilted diagonally right-up, then centered
                "--touch", "900:130:810:0:0", "--touch", "910:130:745:2:0",
                "--touch", "924:182:775:2:0", "--touch", "930:130:810:2:0",
                # finger 1: guard up, with the stick finger still down
                "--touch", "940:282:726:0:1", "--report", "960",
                # finger 2: punch under the guard, guard must not fall
                "--touch", "970:408:822:0:2", "--touch", "995:408:822:1:2",
                "--report", "1000",
                # guard down, then the ult switched to the ball by a fourth finger
                "--touch", "1020:282:726:1:1",
                "--touch", "1040:408:533:0:5", "--touch", "1052:408:533:1:5",
                # the ult pad with the stick still held: fires immediately along facing angle
                "--touch", "1080:408:630:0:3", "--touch", "1090:408:630:1:3",
                "--report", "1085", "--report", "1300", "--report", "1500",
            ],
        )
        check("мультитач-бой без ошибок", result.returncode == 0, result.stderr[-1500:])
        reports = parse_reports(result.stdout)
        before, guarded = reports.get(890, {}), reports.get(960, {})
        punched, fired = reports.get(1000, {}), reports.get(1085, {})
        after = reports.get(1500, {})
        check(
            "палец на стике двигает кубика, пока второй держит блок",
            float(guarded.get("y", "999")) < float(before.get("y", "0")),
            f"было {before.get('y')}, стало {guarded.get('y')}",
        )
        check("второй палец держит блок", guarded.get("block") == "1", str(guarded))
        check("удар под блоком не сбивает блок", punched.get("block") == "1", str(punched))
        check("шар сразу вылетает, пока стик занят", fired.get("ball") == "1", str(fired))
        check(
            "стик не сбит ультой",
            float(fired.get("y", "999")) < float(before.get("y", "0")),
            f"было {before.get('y')}, стало {fired.get('y')}",
        )
        check("кирпич улетел и погас", after.get("ball") == "0", str(after))
        check(
            "кирпич снял здоровье врагу",
            float(after.get("foe", "nan")) < float(before.get("foe", "nan")),
            f"враг {before.get('foe')} -> {after.get('foe')}",
        )


def test_ball(assets: Path) -> None:
    """ebuC's ball ult launches immediately along the facing direction."""
    print("\n== 3f. Instant ebuC's ball ult ==")
    switch = ["--touch", "880:408:533:0:5", "--touch", "892:408:533:1:5"]
    with fake_server():
        def fight(name: str, events: list[str]) -> dict[int, dict[str, str]]:
            result = harness(
                assets,
                SCRATCH / name,
                ["--frames", "1800", *login_taps(), "--class", "750:3", "--goto", "800:1",
                 "--report", "860", *switch, "--report", "900", *events],
            )
            return parse_reports(result.stdout)

        hit_foe = fight("brick_hit", [
            "--stick", "910:0.8:-0.6", "--stick", "920:0:0",  # face diagonally toward walls + foe
            "--touch", "930:408:630:0:7", "--report", "935",
            "--touch", "940:408:630:1:7",
            "--report", "1450",
        ])
        wall = fight("brick_wall", [
            "--stick", "910:0.8:0.6", "--stick", "920:0:0",   # face diagonally right-down at walls
            "--touch", "930:408:630:0:7", "--report", "935",
            "--touch", "940:408:630:1:7",
            "--report", "1120", "--report", "1700",
        ])
    base = hit_foe.get(860, {})
    instant = hit_foe.get(935, {})
    landed = hit_foe.get(1450, {})
    bounced = wall.get(1120, {})
    spent = wall.get(1700, {})
    check("без отсчёта: нажатие не включает прицел", instant.get("charge") == "0", str(instant))
    check("нажатие сразу выпускает шар", instant.get("ball") == "1", str(instant))
    check(
        "шар попал во врага",
        float(landed.get("foe", "nan")) < float(base.get("foe", "nan")),
        f"до {base.get('foe')}, после {landed.get('foe')}",
    )
    check("шар отскакивает от стен", float(bounced.get("bounces", "0")) >= 2, str(bounced))
    check("шар исчезает сам", spent.get("ball") == "0", str(spent))


def test_ult_mode_switch(assets: Path) -> None:
    """The little button above the ult of "буК": dispenser or ball.

    The pad itself never charges anything: in dispenser mode a press fires the
    usual super at once, however long the finger stays. Switching the button
    over to the ball makes the next press immediately launch the ball.
    """
    print("\n== 3g. ebuC's ult switch ==\n")
    with fake_server():
        result = harness(
            assets,
            SCRATCH / "ult_mode",
            [
                "--frames", "1800",
                *login_taps(),
                "--class", "750:3",
                "--goto", "800:1",
                "--report", "880",
                # dispenser mode: the pad fires at once, a long press changes nothing
                "--touch", "900:408:630:0:6", "--report", "940", "--touch", "960:408:630:1:6",
                "--report", "1000",
                # the dispenser has a 6 s cooldown, so the switch waits for it
                "--touch", "1300:408:533:0:5", "--touch", "1312:408:533:1:5",
                "--report", "1330",
                # ball mode: the press immediately launches the ball
                "--touch", "1350:408:630:0:7", "--report", "1355", "--touch", "1365:408:630:1:7",
                "--report", "1450",
            ],
        )
        check("переключение ульты без ошибок", result.returncode == 0, result.stderr[-1500:])
        reports = parse_reports(result.stdout)
        before = reports.get(880, {})
        held = reports.get(940, {})
        later = reports.get(1000, {})
        switched = reports.get(1330, {})
        instant = reports.get(1355, {})
        flying = reports.get(1450, {})
        check("у буКа появилась своя кнопка режима", before.get("mode") == "0", str(before))
        check("в режиме деспенсера ничего не заряжается", held.get("charge") == "0", str(held))
        check("и через полсекунды удержания тоже", later.get("charge") == "0" and later.get("ball") == "0", str(later))
        check("кнопка переключает ульту на шар", switched.get("mode") == "1", str(switched))
        check("в режиме шара нажатие сразу запускает шар без прицела",
              instant.get("charge") == "0" and instant.get("ball") == "1", str(instant))
        check("шар летит", flying.get("ball") == "1" or flying.get("bounces") is not None, str(flying))


def test_longer_fights(assets: Path) -> None:
    """The pools are a bit bigger than the raw class ones, and not wildly so."""
    print("\n== 3h. Fight length ==")
    with fake_server():
        result = harness(
            assets,
            SCRATCH / "long_fight",
            # The first frames of the fight, before anyone has landed a hit.
            ["--frames", "900", *login_taps(), "--class", "750:3", "--goto", "800:1", "--report", "815"],
        )
        frame = parse_reports(result.stdout).get(815, {})
        hp = float(frame.get("hp", "0"))
        foe = float(frame.get("foe", "0"))
        # Base HP pools: Ordinary 20, Azum 12.74, Santa 20, ebuC 24, Astra 18, CodeCracker 12.
        raw = {"0": 20.0, "1": 12.74, "2": 20.0, "3": 24.0, "4": 18.0, "6": 12.0}
        cls = frame.get("cls", "0")
        base = raw.get(cls, 20.0)
        check("здоровья не меньше, чем даёт класс", hp >= base, f"hp={hp}, база={base}")
        check("ровно столько, сколько даёт класс", abs(hp - base) < 0.01, f"hp={hp}, база={base}")
        check("и не больше прежних раздутых пулов", hp <= 25, f"hp={hp}")
        # The enemy brings the pool of its own class, so only the range is fixed.
        check("у врага свой пул, тоже умеренный", 15 < foe <= 30, f"foe={foe}")


def wait_port(port: int, timeout: float = 5.0) -> bool:
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                return True
        except OSError:
            time.sleep(0.05)
    return False


def test_login(assets: Path) -> None:
    """Полный сетевой путь: экран входа -> форма -> Firebase Auth -> профиль.

    Подставная база поднимается на том же порту, что зашит в харнесс при
    сборке (FAKE_PORT), поэтому адрес совпадает с боевым кодом вызова.
    """
    print("\n== 4. Вход в аккаунт через настоящий HTTP ==")
    if not wait_port(FAKE_PORT, timeout=0.3):
        fake = subprocess.Popen(
            [sys.executable, "test/fake_firebase.py", str(FAKE_PORT)],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        own_fake = True
    else:
        fake = None
        own_fake = False
    try:
        if not wait_port(FAKE_PORT):
            check("подставная база поднялась", False)
            return
        result = harness(
            assets,
            SCRATCH / "login",
            [
                "--frames", "900",
                "--touch", "220:360:686:0", "--touch", "223:360:686:1",  # warning
                "--touch", "400:270:558:0", "--touch", "403:270:558:1",  # "Account"
                "--logininfo", "430",
                # The password goes in without the suffix: the game adds it,
                # as on a phone. The fake knows the admin password.
                "--login", "450:Dimasi4ek229:admin",
                "--report", "700",
                "--report", "890",
            ],
        )
        reports = parse_reports(result.stdout)
        info = next((ln for ln in result.stdout.splitlines() if ln.startswith("logininfo:")), "")
        field_line = next((ln for ln in result.stdout.splitlines() if "nick=(" in ln), "")
        check("экран входа найден (координаты полей известны)", "button=(" in field_line, field_line)
        frame = reports.get(700, {})
        check("вход выполнен (NET_LOGIN_OK)", frame.get("login") == "0" and frame.get("nick") == "Dimasi4ek229", str(frame))
        check("лобби после входа (экран переключился)", frame.get("state") == "0", str(frame))
        check(
            "профиль прочитан из базы (кубки админа 4200)",
            "cups=4200" in result.stderr or "cups=4200" in result.stdout,
            result.stderr[-1200:],
        )
        check("после входа не было ошибок", frame.get("error") == "0", str(frame))
        late = reports.get(890, {})
        check("сессия держится", late.get("login") == "0", str(late))
        if info:
            print("  " + info)
    finally:
        if own_fake and fake:
            fake.terminate()
            fake.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser(description="Проверка ПК-версии")
    parser.add_argument("--no-build", action="store_true", help="не пересобирать харнесс")
    parser.add_argument("--only", default="", help="прогнать только тесты, в названии которых есть это слово")
    args = parser.parse_args()

    SCRATCH.mkdir(parents=True, exist_ok=True)
    fake_url = f"http://127.0.0.1:{FAKE_PORT}"  # fixed port baked into harness build
    if not args.no_build or not BINARY.is_file():
        build(fake_url)
    assets = stage_assets()
    tests = [
        test_pixels,
        test_menu_and_battle,
        test_controls,
        test_walk_feel,
        test_multitouch,
        test_ball,
        test_ult_mode_switch,
        test_longer_fights,
        test_brick_online,
        test_codecracker_online,
        test_login,
    ]
    for test in tests:
        if args.only and args.only not in test.__name__:
            continue
        test(assets)

    print()
    if FAILED:
        print(f"ПРОВАЛЕНО ({len(FAILED)}): " + ", ".join(FAILED))
        return 1
    print("ПК-версия в порядке: игра собирается, меню нажимается, бой идёт, кадр рисуется.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
