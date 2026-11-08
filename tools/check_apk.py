#!/usr/bin/env python3
"""Checks a downloaded CubicBattle4.apk before installing it on a phone.

Verifies the zip container, the required entries and the signature blocks, so
a truncated download is caught before Android says "package is corrupt".
"""

from __future__ import annotations

import hashlib
import sys
import zipfile
from pathlib import Path

REQUIRED = [
    "AndroidManifest.xml",
    "classes.dex",
    "resources.arsc",
    "assets/grass.png",
    "assets/fonts/ComicRelief-Regular.ttf",
    "assets/sounds/disco_medusae_kevin_macleod.mp3",
    "assets/sounds/Disco-Medusae-NOTICE.txt",
    "assets/sounds/licenses/Disco-Medusae-CC-BY-4.0.txt",
    "assets/hack/h4c4k0r.bin",
]
ABIS = ["lib/arm64-v8a/libcb_game.so", "lib/armeabi-v7a/libcb_game.so", "lib/x86_64/libcb_game.so"]
SIG_BLOCK = b"APK Sig Block 42"


def check(path: Path) -> int:
    size = path.stat().st_size
    print(f"file: {path}")
    print(f"size: {size} bytes")
    print(f"sha256: {hashlib.sha256(path.read_bytes()).hexdigest()}")
    try:
        with zipfile.ZipFile(path) as archive:
            broken = archive.testzip()
            if broken is not None:
                print(f"FAIL: broken entry {broken}")
                return 1
            names = set(archive.namelist())
    except zipfile.BadZipFile as error:
        print(f"FAIL: not a valid zip container ({error})")
        print("the file is truncated or was saved as something other than .apk")
        return 1
    missing = [name for name in REQUIRED + ABIS if name not in names]
    if missing:
        print("FAIL: missing entries: " + ", ".join(missing))
        return 1
    v1 = [name for name in names if name.startswith("META-INF/") and name.endswith((".RSA", ".DSA", ".EC"))]
    if not v1:
        print("FAIL: no v1 signature in META-INF")
        return 1
    blob = path.read_bytes()
    if SIG_BLOCK not in blob:
        print("FAIL: no v2 signature block")
        return 1
    print("ok: container, entries, v1 and v2 signatures are in place")
    return 0


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: python3 tools/check_apk.py CubicBattle4.apk")
        return 2
    path = Path(sys.argv[1])
    if not path.is_file():
        print(f"FAIL: no such file {path}")
        return 1
    return check(path)


if __name__ == "__main__":
    raise SystemExit(main())
