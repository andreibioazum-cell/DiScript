#!/usr/bin/env python3
"""Stages assets and the Android platform code for APK packaging."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
ASSET_GROUPS = (
    ("textures", Path()),
    ("fonts", Path("fonts")),
    ("audio", Path("sounds")),
    ("hack", Path("hack")),
)


def copy_asset_group(source: Path, destination: Path, prefix: Path) -> list[Path]:
    staged: list[Path] = []
    if not source.is_dir():
        return staged

    for path in sorted(source.rglob("*")):
        relative = path.relative_to(source)
        target_relative = prefix / relative
        target = destination / target_relative
        if path.is_symlink():
            raise ValueError(f"Ссылки в ресурсах не поддерживаются: {relative}")
        if path.is_dir():
            target.mkdir(parents=True, exist_ok=True)
        elif path.name != ".gitkeep":
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            staged.append(target_relative)
    return staged


def stage_assets(source: Path, destination: Path) -> list[Path]:
    """Builds a clean asset tree in the Android APK layout."""
    source = source.resolve()
    destination = destination.resolve()
    if not source.is_dir():
        raise ValueError(f"Каталог ресурсов не найден: {source}")
    if source == destination or source in destination.parents or destination in source.parents:
        raise ValueError("Каталоги исходных и подготовленных ресурсов не должны пересекаться")

    shutil.rmtree(destination, ignore_errors=True)
    destination.mkdir(parents=True)
    staged: list[Path] = []
    for directory, prefix in ASSET_GROUPS:
        staged += copy_asset_group(source / directory, destination, prefix)
    if not staged:
        raise ValueError(f"В каталоге нет ресурсов для APK: {source}")
    return staged


def build_android_activity(source: Path, apk_root: Path) -> Path | None:
    """Builds the Android system bridge into classes.dex."""
    sources = sorted(source.rglob("*.java")) if source.is_dir() else []
    if not sources:
        return None

    sdk_value = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
    if not sdk_value:
        print("Android SDK не найден; компиляция Java-моста пропущена")
        return None

    sdk = Path(sdk_value).resolve()
    android_jar = sdk / "platforms" / "android-35" / "android.jar"
    d8 = sdk / "build-tools" / "34.0.0" / "d8"
    javac = shutil.which("javac")
    if not javac:
        raise ValueError("javac не найден")
    if not android_jar.is_file():
        raise ValueError(f"Android API 35 не установлен: {android_jar}")
    if not d8.is_file():
        raise ValueError(f"Android build-tools 34.0.0 не установлен: {d8}")

    apk_root = apk_root.resolve()
    classes_dir = apk_root / ".java-classes"
    dex_dir = apk_root / ".java-dex"
    shutil.rmtree(classes_dir, ignore_errors=True)
    shutil.rmtree(dex_dir, ignore_errors=True)
    classes_dir.mkdir(parents=True)
    dex_dir.mkdir(parents=True)
    try:
        subprocess.run(
            [
                javac,
                "-source",
                "8",
                "-target",
                "8",
                "-bootclasspath",
                str(android_jar),
                "-d",
                str(classes_dir),
                *(str(path) for path in sources),
            ],
            check=True,
        )
        class_files = sorted(classes_dir.rglob("*.class"))
        if not class_files:
            raise ValueError("javac не создал class-файлы")
        subprocess.run(
            [str(d8), "--min-api", "26", "--output", str(dex_dir), *(str(path) for path in class_files)],
            check=True,
        )
        generated = dex_dir / "classes.dex"
        if not generated.is_file():
            raise ValueError("d8 не создал classes.dex")
        destination = apk_root / "classes.dex"
        shutil.copy2(generated, destination)
        return destination
    finally:
        shutil.rmtree(classes_dir, ignore_errors=True)
        shutil.rmtree(dex_dir, ignore_errors=True)


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Подготовить ресурсы игры для APK.")
    parser.add_argument("source", nargs="?", default="assets")
    parser.add_argument("destination", nargs="?", default="staging/assets")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        destination = Path(args.destination)
        staged = stage_assets(Path(args.source), destination)
        java_source = ROOT / "platform" / "android" / "java"
        dex = build_android_activity(java_source, destination.parent)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Ошибка: {error}", file=sys.stderr)
        return 1

    print(f"Подготовлено ресурсов: {len(staged)}; каталог: {args.destination}")
    for path in staged:
        print(f"  {path.as_posix()}")
    if dex:
        print(f"Собран Android-мост: {dex}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
