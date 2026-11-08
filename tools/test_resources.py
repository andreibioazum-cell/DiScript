#!/usr/bin/env python3
"""Checks the game assets and the network storage schema."""

from __future__ import annotations

import json
import re
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def header_secret(prefix: str) -> str:
    """Rebuilds a secret from the pieces in engine/network.h (every byte xored)."""
    text = (ROOT / "src" / "engine" / "network.h").read_text(encoding="utf-8")
    mask = int(re.search(r"#define\s+NET_SECRET_XOR\s+0x([0-9a-fA-F]+)", text).group(1), 16)
    parts = []
    index = 0
    while True:
        chunk = re.search(r'#define\s+%s_%d\s+"((?:\\[0-7]{3})*)"' % (prefix, index), text)
        if not chunk:
            break
        parts.append("".join(chr(int(octet, 8) ^ mask) for octet in re.findall(r"\\([0-7]{3})", chunk.group(1))))
        index += 1
    assert index >= 2, f"network.h has no secret chunks for {prefix}"
    return "".join(parts)


def body_keys(name: str) -> set[str]:
    text = (ROOT / "src" / "engine" / "network" / name).read_text(encoding="utf-8")
    return set(re.findall(r'\\"([a-z0-9_]+)\\":', text))


def declared(node: dict) -> set[str]:
    return {key for key in node if not key.startswith((".", "$"))}


def check_firebase_rules() -> None:
    rules = json.loads((ROOT / "firebase.rules.json").read_text(encoding="utf-8"))["rules"]
    slot = declared(rules["rooms"]["$room"]["players"]["$slot"])
    user = declared(rules["users"]["$nick"])
    banner = declared(rules["banner"])
    message = declared(rules["rooms"]["$room"]["chat"]["$msg"])

    room = body_keys("room_sync.inc") | body_keys("room_chat.inc") | body_keys("room_threads.inc")
    missing = room - slot
    assert not missing, f"Rules for a game slot lack fields: {sorted(missing)}"
    assert {"gbx", "gby", "gbdx", "gbdy", "grab"} <= room

    auth_only = {"email", "password", "grant_type", "refresh_token"}
    profile = (
        body_keys("state_storage.inc")
        | body_keys("cloud_patch.inc")
        | body_keys("settings_storage.inc")
        | body_keys("promo.inc")
        | (body_keys("auth_session.inc") - auth_only)
    )
    missing = profile - user
    assert not missing, f"Profile rules lack fields: {sorted(missing)}"
    assert {"astra", "astra_level", "astra_levels"} <= profile
    assert {"codecracker", "codecracker_level", "codecracker_levels"} <= profile

    missing = body_keys("player_api.inc") - message
    assert not missing, f"Chat rules lack fields: {sorted(missing)}"
    missing = body_keys("room_control.inc") - banner
    assert not missing, f"Banner rules lack fields: {sorted(missing)}"

    ban_write = rules["bans"]["$nick"][".write"]
    assert "newData.exists()" in ban_write
    assert "$nick.toLowerCase() != 'dimasi4ek229'" in ban_write
    assert "qwertyuiopaj1234" not in ban_write.lower()


def check_write_secret() -> None:
    """Writes are gated by the secret: the client sends it, the rules require it."""
    write_secret = header_secret("NET_WRITE_SECRET")
    pass_secret = header_secret("NET_PASS_SECRET")
    for name, secret in (("write", write_secret), ("password", pass_secret)):
        assert len(secret) == 32, f"Secret {name} is too short: {len(secret)} characters"
        assert secret.isascii() and secret.isalnum(), f"Secret {name} must be letters and digits"
    assert write_secret != pass_secret, "The write secret and the password tail must differ"

    rules_text = (ROOT / "firebase.rules.json").read_text(encoding="utf-8")
    assert pass_secret not in rules_text, "The password tail leaks into the database rules"
    assert "|cb4v1" not in rules_text, "The old guessable password tail is in the rules"
    rules = json.loads(rules_text)["rules"]
    nick = rules["users"]["$nick"]
    slot = rules["rooms"]["$room"]["players"]["$slot"]
    message = rules["rooms"]["$room"]["chat"]["$msg"]
    wants = "newData.child('w').val() == '%s'" % write_secret
    for node, rule in ((nick, nick[".write"]), (nick, nick[".validate"]),
                       (slot, slot[".write"]), (slot, slot[".validate"]),
                       (message, message[".write"]), (message, message[".validate"])):
        assert wants in rule, f"Rule without the write-secret check: {rule[:60]}..."
        assert "w" in node, "The rules have no w field"
    for node in (nick, slot, message):
        assert "newData.val() == '%s'" % write_secret in node["w"][".validate"]

    net = ROOT / "src" / "engine" / "network"
    assert "write_secret.inc" in (ROOT / "src" / "engine" / "network.c").read_text(encoding="utf-8")
    for name, expected in (("cloud_patch.inc", 1), ("room_sync.inc", 2),
                           ("auth_session.inc", 1), ("leaderboard.inc", 1)):
        text = (net / name).read_text(encoding="utf-8")
        assert text.count("net_seal_body(") == expected, f"{name}: the write body is not sealed with the secret"
    assert "#define FB_PASS_SUFFIX" not in (net / "http_json.inc").read_text(encoding="utf-8"), \
        "The old guessable password tail is back"
    for name in ("auth_session.inc", "presence.inc"):
        assert "net_pass_suffix()" in (net / name).read_text(encoding="utf-8"), \
            f"{name}: the password is built without the secret tail"


def check_assets() -> None:
    names: set[str] = set()
    game = ROOT / "src" / "game"
    for directory in (game / "core", game / "ui", game / "combat", game / "fx", game / "state"):
        for source in directory.rglob("*.inc"):
            names |= set(re.findall(r'"([A-Za-z0-9_./-]+\.png)"', source.read_text(encoding="utf-8")))
    assert names, "No texture references found in the sources"
    assert (ROOT / "assets" / "textures" / "Event columns.png").is_file(), "Missing the Event columns texture"
    helpy = ROOT / "assets" / "textures" / "npc_helpy.png"
    assert helpy.is_file(), "Missing the NPC Helpy texture"
    with helpy.open("rb") as sprite:
        header = sprite.read(24)
    assert header[:8] == b"\x89PNG\r\n\x1a\n", "The Helpy texture must be PNG"
    assert (int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")) == (512, 512), "The Helpy texture must be 512x512"
    missing = [
        name for name in sorted(names)
        if not (ROOT / "assets" / "textures" / name.split("/")[-1]).is_file()
    ]
    assert not missing, f"Missing textures: {missing}"

    required = (
        ROOT / "assets" / "textures" / "astra_xastra_card.png",
        ROOT / "assets" / "audio" / "tracks" / "lobby.wav",
        ROOT / "assets" / "audio" / "tracks" / "winter_lobby.wav",
        ROOT / "assets" / "audio" / "tracks" / "duel.wav",
        ROOT / "assets" / "audio" / "sfx" / "punch.wav",
        ROOT / "assets" / "fonts" / "ComicRelief-Regular.ttf",
        ROOT / "assets" / "shaders" / "sprite.vert",
        ROOT / "assets" / "shaders" / "solid.frag",
        ROOT / "assets" / "shaders" / "image.frag",
        ROOT / "assets" / "shaders" / "tint.frag",
    )
    for asset in required:
        assert asset.is_file(), f"Missing asset: {asset.relative_to(ROOT)}"

    durations = {
        "lobby.wav": 70,
        "winter_lobby.wav": 70,
        "duel.wav": 45,
    }
    for name, expected in durations.items():
        path = ROOT / "assets" / "audio" / "tracks" / name
        with wave.open(str(path), "rb") as audio:
            assert audio.getnchannels() == 2 and audio.getsampwidth() == 2, f"Wrong music format: {name}"
            duration = audio.getnframes() / audio.getframerate()
            assert abs(duration - expected) < 0.001, f"Wrong duration for {name}: {duration}"

    # The old "Azum vs Astra" song and its reference mp3 must stay removed.
    assert not (ROOT / "assets" / "audio" / "azum_vs_astra.wav").exists(), "The old song azum_vs_astra.wav is back"
    assert not (ROOT / "assets" / "audio" / "tracks" / "azum_vs_astra.wav").exists(), "The old song is back"
    assert not (ROOT / "And_The_Seraphim_Wept.mp3").exists(), "A third-party mp3 is in the repository again"
    assert not (ROOT / "tools" / "generate_seraphim.py").exists(), "The generator of the old song is back"

    with wave.open(str(ROOT / "assets" / "audio" / "tracks" / "duel.wav"), "rb") as duel:
        assert duel.getnchannels() == 2 and duel.getsampwidth() == 2, "Wrong duel.wav format"
        rate = duel.getframerate()
        head = duel.readframes(rate * 5)
        import struct, math
        samples = struct.unpack("<" + "h" * (len(head) // 2), head)
        rms5 = math.sqrt(sum(x * x for x in samples) / len(samples))
        assert rms5 >= 4000.0, f"duel.wav is too quiet at the start of the fight (RMS={rms5:.1f})"

    assert not (ROOT / "assets" / "audio" / "halloween_lobby.wav").exists(), "The Halloween music is back"


def check_port_layout() -> None:
    assert not list(ROOT.rglob("*.ds")), "Old language sources are left in the C port"
    assert not (ROOT / "game").exists(), "The old game directory must not be used"
    assert not (ROOT / "native").exists(), "The old native directory must not be used"
    required = (
        ROOT / "src" / "game",
        ROOT / "src" / "engine",
        ROOT / "src" / "platform" / "android",
        ROOT / "assets" / "textures",
        ROOT / "assets" / "audio",
        ROOT / "assets" / "fonts",
        ROOT / "assets" / "shaders",
        ROOT / "platform" / "android",
    )
    for directory in required:
        assert directory.is_dir(), f"Missing project directory: {directory.relative_to(ROOT)}"

    old_prefix = "ds" + "_fn_"
    for source in (ROOT / "src" / "game").rglob("*.inc"):
        assert old_prefix not in source.read_text(encoding="utf-8"), source


def check_disco_loops() -> None:
    """The disco soundtrack lives in disco/ and must stay a real, loopable set."""
    import subprocess
    result = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "test_disco_loops.py")],
        capture_output=True, text=True,
    )
    assert result.returncode == 0, f"Disco loops failed the check:\n{result.stderr}{result.stdout}"


def main() -> int:
    check_port_layout()
    check_assets()
    check_firebase_rules()
    check_write_secret()
    # The rules check depends on a migrated live database, so it is reported but
    # never hides the other problems (and never blocks the disco soundtrack).
    try:
        check_disco_loops()
    except AssertionError as error:
        raise AssertionError(f"Disco loops failed the check: {error}") from error
    print("Assets and network schema: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
