#!/usr/bin/env python3
"""Build script for the Windows .exe and the Linux headless harness. Builds the
DiScript code editor, not a game."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD = ROOT / "build" / "pc"
C_SOURCES = (
    "src/engine/runtime.c",
    "src/engine/anim.c",
    "src/engine/scene/ds_scene.c",
    "src/engine/scene/ds_render3d.c",
    # The language runtime that runs the scripts the editor edits.
    "src/discript/ds_gc.c",
    "src/discript/ds_lexer.c",
    "src/discript/ds_parser.c",
    "src/discript/ds_compiler.c",
    "src/discript/ds_vm.c",
    "src/discript/ds_stdlib.c",
    "src/discript/ds_scene_api.c src/discript/ds_sprite_api.c",
    "src/discript/ds_love.c",
    "src/platform/pc/pc_main.c",
    "src/ide/ide_buffer.c",
    "src/ide/ide_highlight.c",
    "src/ide/ide_view.c",
    "src/ide/ide_viewport.c",
    "src/ide/ide_app.c",
)
CPP_SOURCES = (
    # Language runtime support: the JIT, plus the noise/physics and
    # colour/gradient/audio primitives the standard library exposes.
    "src/cpp/native_jit.cpp",
    "src/cpp/love_noise_physics.cpp",
    "src/cpp/ui_fx.cpp",
)
INCLUDES = ("-I.", "-Isrc", "-Isrc/platform/pc/shim")
COMMON_FLAGS = ("-std=c99", "-DCB_PC", "-O2")


def log(message: str) -> None:
    print(f"[pc] {message}", flush=True)


def fail(message: str) -> "SystemExit":
    print(f"[pc] ОШИБКА: {message}", file=sys.stderr, flush=True)
    raise SystemExit(2)


def which(*names: str) -> str | None:
    for name in names:
        found = shutil.which(name)
        if found:
            return found
    return None


def zig_argv() -> list[str] | None:
    """Returns the zig driver command, or None when zig is not installed.

    Two ways in: a plain `zig` on PATH, or the pip package `ziglang`, whose
    binary runs as `python -m ziglang` (it can also be installed into any
    virtualenv - ZIG env var points at that interpreter)."""
    zig = which("zig")
    if zig:
        return [zig]
    interpreters = []
    if os.environ.get("ZIG_PYTHON"):
        interpreters.append(os.environ["ZIG_PYTHON"])
    interpreters.append(sys.executable)
    for python in interpreters:
        probe = subprocess.run(
            [python, "-m", "ziglang", "version"],
            capture_output=True,
            text=True,
        )
        if probe.returncode == 0 and probe.stdout.strip():
            return [python, "-m", "ziglang"]
    for candidate in (Path(sys.executable).parent, Path.home() / ".local" / "bin"):
        exe = candidate / ("zig.exe" if os.name == "nt" else "zig")
        if exe.is_file():
            return [str(exe)]
    return None


def run(cmd: list[str], what: str) -> None:
    log(what)
    shown = " ".join(str(part) for part in cmd)
    log(f"  {shown}")
    result = subprocess.run(cmd, cwd=ROOT)
    if result.returncode != 0:
        fail(f"команда не выполнена: {shown}")


def stage_assets(destination: Path) -> Path:
    log(f"ресурсы -> {destination.relative_to(ROOT)}")
    shutil.rmtree(destination, ignore_errors=True)
    run([sys.executable, "stage_assets.py", "assets", str(destination)], "stage_assets.py")


def compile_linux(extra: list[str]) -> Path:
    cc = which("cc", "gcc", "clang")
    cxx = which("c++", "g++", "clang++")
    if not cc or not cxx:
        fail("нужен gcc/g++ (или clang); поставь build-essential")
    BUILD.mkdir(parents=True, exist_ok=True)
    objects: list[Path] = []
    for source in C_SOURCES:
        obj = BUILD / (Path(source).stem + ".o")
        run([cc, *COMMON_FLAGS, *extra, "-g", *INCLUDES, "-c", source, "-o", str(obj)], f"компилирую {source}")
        objects.append(obj)
    for source in CPP_SOURCES:
        obj = BUILD / (Path(source).stem + ".o")
        run([cxx, "-std=c++17", "-O2", "-g", *INCLUDES, "-c", source, "-o", str(obj)], f"компилирую {source}")
        objects.append(obj)
    binary = BUILD / "discript_headless"
    run([cxx, "-O2", "-g", *[str(obj) for obj in objects], "-o", str(binary), "-lm", "-lpthread"], "линкую")
    return binary


def compile_windows(extra: list[str]) -> tuple[Path, list[str]]:
    driver = zig_argv()
    if not driver:
        fail(
            "нужен zig: `pip install ziglang` (или положи zig в PATH). "
            "Zig несёт собственные заголовки Win32, MSVC и Windows SDK не требуются"
        )
    BUILD.mkdir(parents=True, exist_ok=True)
    target = ["-target", "x86_64-windows"]
    objects: list[Path] = []
    for source in C_SOURCES:
        obj = BUILD / ("win_" + Path(source).stem + ".o")
        run(
            [*driver, "cc", *target, *COMMON_FLAGS, *extra, *INCLUDES, "-c", source, "-o", str(obj)],
            f"компилирую {source} для Windows",
        )
        objects.append(obj)
    for source in CPP_SOURCES:
        obj = BUILD / ("win_" + Path(source).stem + ".o")
        run([*driver, "c++", *target, "-std=c++17", "-O2", *INCLUDES, "-c", source, "-o", str(obj)], f"компилирую {source} для Windows")
        objects.append(obj)
    binary = BUILD / "DiScript.exe"
    link = [
        *driver,
        "c++",
        *target,
        *[str(obj) for obj in objects],
        "-o",
        str(binary),
        "-lwinhttp",
        "-lwinmm",
        "-luser32",
        "-lgdi32",
        "-lcomdlg32",
        # GUI subsystem: windowed app, no console window. Logs still go to
        # discript.log next to the exe.
        "-Wl,--subsystem,windows",
    ]
    run(link, "линкую")
    return binary, driver


def smoke(binary: Path) -> None:
    """A quick run without a window: consent -> lobby -> battle."""
    data = BUILD / "smoke_data"
    shutil.rmtree(data, ignore_errors=True)
    data.mkdir(parents=True, exist_ok=True)
    assets = BUILD / "assets"
    shots = BUILD / "smoke"
    shutil.rmtree(shots, ignore_errors=True)
    shots.mkdir(parents=True, exist_ok=True)
    command = [
        str(binary),
        "--headless",
        "--size",
        "540x960",
        "--assets",
        str(assets),
        "--data",
        str(data),
        "--frames",
        "700",
        "--touch",
        "220:360:686:0",
        "--touch",
        "223:360:686:1",
        "--touch",
        "400:270:336:0",
        "--touch",
        "403:270:336:1",
        "--touch",
        "460:270:404:0",
        "--touch",
        "463:270:404:1",
        "--stick",
        "600:0:-1",
        "--atk",
        "620:1",
        "--atk",
        "640:0",
        "--report",
        "650",
        "--dump",
        f"690:{shots / 'battle.bmp'}",
    ]
    log("дымовой прогон: лобби -> соло -> удар")
    result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True)
    output = (result.stdout or "") + (result.stderr or "")
    if result.returncode != 0:
        print(output[-4000:])
        fail(f"игра завершилась с кодом {result.returncode}")
    line = next((ln for ln in output.splitlines() if ln.startswith("frame 650:")), "")
    if "error=0" not in line:
        print(output[-4000:])
        fail("игра сообщила об ошибке")
    if "state=1" not in line or "battle=1" not in line:
        print(output[-4000:])
        fail(f"бой не начался: {line!r}")
    if "hp=20.00" in line and "foe=20.00" in line:
        fail(f"за 700 кадров никто не ударил: {line!r}")
    shot = shots / "battle.bmp"
    if not shot.is_file() or shot.stat().st_size < 100_000:
        fail("кадр боя не сохранился")
    log(f"бой идёт: {line}")
    log(f"кадр: {shot.relative_to(ROOT)} ({shot.stat().st_size} байт)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Сборка ПК-версии редактора DiScript")
    parser.add_argument("--windows", action="store_true", help="собрать x86_64 .exe (zig)")
    parser.add_argument("--linux", action="store_true", help="собрать Linux-харнесс (gcc)")
    parser.add_argument("--check", action="store_true", help="после сборки прогнать дымовой тест редактора")
    parser.add_argument("--no-assets", action="store_true", help="не готовить папку ресурсов заново")
    parser.add_argument(
        "--embed-assets",
        action="store_true",
        help="встроить ресурсы в Linux-бинарник (Windows делает это всегда)",
    )
    parser.add_argument(
        "--firebase",
        metavar="URL",
        help="адрес базы для этой сборки (по умолчанию боевой; харнессу - http://127.0.0.1:18765)",
    )
    parser.add_argument("--data-path", help="папка сохранений по умолчанию (например, данные из fake_firebase.py)")
    args = parser.parse_args()

    build_windows = args.windows or (not args.linux and os.name == "nt")
    build_linux = args.linux or (not args.windows and not build_windows)
    if not build_windows and not build_linux:
        build_linux = True

    extra: list[str] = []
    if args.firebase:
                # The harness checks the real network path: the fake database in
                # test/fake_firebase.py over plain http on localhost.
        extra.append(f'-DNET_FIREBASE_URL="{args.firebase}"')
        log(f"база для сборки: {args.firebase}")
    if args.data_path:
        extra.append(f'-DPC_DATA_PATH="{args.data_path}"')

    assets = BUILD / "assets"
    if args.no_assets and assets.is_dir():
        log("ресурсы не трогаю (--no-assets)")
    else:
        stage_assets(assets)

    if build_windows:
        binary, _driver = compile_windows(extra)
        # One self-contained .exe: the asset tree (the editor font) is appended
        # to the image, so no loose assets folder is needed next to it.
        run([sys.executable, "tools/embed_assets.py", str(assets), str(binary)], "встраиваю ресурсы в .exe")
        log(f"готово: {binary.relative_to(ROOT)} ({binary.stat().st_size} байт)")
    else:
        binary = compile_linux(extra)
        if args.embed_assets:
            run([sys.executable, "tools/embed_assets.py", str(assets), str(binary)],
                "встраиваю ресурсы в бинарник")
        log(f"готово: {binary.relative_to(ROOT)}")

    if args.check:
        smoke(binary)
    log("рядом с бинарником должна лежать папка ресурсов (assets/) - её готовит этот же скрипт")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
