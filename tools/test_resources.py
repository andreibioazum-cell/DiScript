#!/usr/bin/env python3
"""Checks the game assets and the network storage schema."""

from __future__ import annotations

import hashlib
import json
import re
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
    assert index >= 2, f"В network.h нет кусков секрета {prefix}"
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
    assert not missing, f"В правилах игрового слота нет полей: {sorted(missing)}"
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
    assert not missing, f"В правилах профиля нет полей: {sorted(missing)}"
    assert {"astra", "astra_level", "astra_levels"} <= profile
    assert {"codecracker", "codecracker_level", "codecracker_levels"} <= profile

    missing = body_keys("player_api.inc") - message
    assert not missing, f"В правилах чата нет полей: {sorted(missing)}"
    missing = body_keys("room_control.inc") - banner
    assert not missing, f"В правилах баннера нет полей: {sorted(missing)}"

    ban_write = rules["bans"]["$nick"][".write"]
    assert "newData.exists()" in ban_write
    assert "$nick.toLowerCase() != 'dimasi4ek229'" in ban_write
    assert "$nick.toLowerCase() != 'qwertyuiopaj1234'" in ban_write
    assert "qwertyuiopaj1234@cb4.game" in ban_write


def check_write_secret() -> None:
    """Writes are gated by the secret: the client sends it, the rules require it."""
    write_secret = header_secret("NET_WRITE_SECRET")
    pass_secret = header_secret("NET_PASS_SECRET")
    for name, secret in (("записи", write_secret), ("пароля", pass_secret)):
        assert len(secret) == 32, f"Секрет {name} слишком короткий: {len(secret)} символов"
        assert secret.isascii() and secret.isalnum(), f"Секрет {name} должен быть из букв и цифр"
    assert write_secret != pass_secret, "Секрет записи и хвост пароля не должны совпадать"

    rules_text = (ROOT / "firebase.rules.json").read_text(encoding="utf-8")
    assert pass_secret not in rules_text, "Хвост пароля попадает в правила базы"
    assert "|cb4v1" not in rules_text, "Старый угадываемый хвост пароля попал в правила базы"
    rules = json.loads(rules_text)["rules"]
    nick = rules["users"]["$nick"]
    slot = rules["rooms"]["$room"]["players"]["$slot"]
    message = rules["rooms"]["$room"]["chat"]["$msg"]
    wants = "newData.child('w').val() == '%s'" % write_secret
    for node, rule in ((nick, nick[".write"]), (nick, nick[".validate"]),
                       (slot, slot[".write"]), (slot, slot[".validate"]),
                       (message, message[".write"]), (message, message[".validate"])):
        assert wants in rule, f"Правило без проверки секрета записи: {rule[:60]}..."
        assert "w" in node, "В правилах нет поля w"
    for node in (nick, slot, message):
        assert "newData.val() == '%s'" % write_secret in node["w"][".validate"]

    net = ROOT / "src" / "engine" / "network"
    assert "write_secret.inc" in (ROOT / "src" / "engine" / "network.c").read_text(encoding="utf-8")
    for name, expected in (("cloud_patch.inc", 1), ("room_sync.inc", 2),
                           ("auth_session.inc", 1), ("leaderboard.inc", 1)):
        text = (net / name).read_text(encoding="utf-8")
        assert text.count("net_seal_body(") == expected, f"{name}: тело записи не подписано секретом"
    assert "#define FB_PASS_SUFFIX" not in (net / "http_json.inc").read_text(encoding="utf-8"), \
        "Старый угадываемый хвост пароля вернулся"
    for name in ("auth_session.inc", "presence.inc"):
        assert "net_pass_suffix()" in (net / name).read_text(encoding="utf-8"), \
            f"{name}: пароль собирается без секретного хвоста"


def check_disco_music() -> None:
    """The disco asset must remain the verified recording, compressed to <= 1 MB."""
    source = ROOT / "third_party/music/Disco Medusae - Kevin MacLeod.mp3"
    output = ROOT / "assets/audio/disco_medusae_kevin_macleod.mp3"
    assert hashlib.sha256(source.read_bytes()).hexdigest() == (
        "0957c02613a361cd89b3f95ab8d0e708ded2849cc28787202e7a46931ff4bace"
    ), "Disco source recording changed"
    assert output.stat().st_size <= 1_000_000, "Disco asset must stay at or below 1 MB"
    assert hashlib.sha256(output.read_bytes()).hexdigest() == (
        "1552423b8d64f01b94a895acb3dec3ab23b9cbd7b7a6bf0677f606cb283c1a9f"
    ), "Disco MP3 differs from the verified compression"
    notice = (ROOT / "assets/audio/Disco-Medusae-NOTICE.txt").read_text(encoding="utf-8")
    for text in ("Kevin MacLeod", "CC BY 4.0", "https://creativecommons.org/licenses/by/4.0/",
                 "USUAN1500041", "22050", "MP3", "not AI-generated"):
        assert text in notice, f"Missing disco attribution: {text}"
    license_text = (ROOT / "assets/audio/licenses/Disco-Medusae-CC-BY-4.0.txt").read_text(encoding="utf-8")
    assert "Attribution 4.0 International" in license_text
    assert "Creative Commons Attribution 4.0 International" in license_text


def check_assets() -> None:
    names: set[str] = set()
    game = ROOT / "src" / "game"
    for directory in (game / "core", game / "ui", game / "combat", game / "fx", game / "state"):
        for source in directory.rglob("*.inc"):
            names |= set(re.findall(r'"([A-Za-z0-9_./-]+\.png)"', source.read_text(encoding="utf-8")))
    assert names, "В исходниках не найдены ссылки на текстуры"
    assert (ROOT / "assets" / "textures" / "Event columns.png").is_file(), "Не найдена текстура дня рождения колонок"
    helpy = ROOT / "assets" / "textures" / "npc_helpy.png"
    assert helpy.is_file(), "Не найдена текстура NPC Хелпи"
    with helpy.open("rb") as sprite:
        header = sprite.read(24)
    assert header[:8] == b"\x89PNG\r\n\x1a\n", "Текстура Хелпи должна быть PNG"
    assert (int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")) == (512, 512), "Текстура Хелпи должна быть 512x512"
    missing = [
        name for name in sorted(names)
        if not (ROOT / "assets" / "textures" / name.split("/")[-1]).is_file()
    ]
    assert not missing, f"Не найдены текстуры: {missing}"

    required = (
        ROOT / "assets" / "textures" / "astra_xastra_card.png",
        ROOT / "assets" / "audio" / "lobbymusic.wav",
        ROOT / "assets" / "audio" / "winter_jingle.wav",
        ROOT / "assets" / "audio" / "duel_theme.wav",
        ROOT / "assets" / "audio" / "disco_medusae_kevin_macleod.mp3",
        ROOT / "assets" / "audio" / "Disco-Medusae-NOTICE.txt",
        ROOT / "assets" / "audio" / "licenses" / "Disco-Medusae-CC-BY-4.0.txt",
        ROOT / "third_party" / "music" / "Disco Medusae - Kevin MacLeod.mp3",
        ROOT / "assets" / "fonts" / "ComicRelief-Regular.ttf",
        ROOT / "assets" / "shaders" / "sprite.vert",
        ROOT / "assets" / "shaders" / "solid.frag",
        ROOT / "assets" / "shaders" / "image.frag",
        ROOT / "assets" / "shaders" / "tint.frag",
    )
    for asset in required:
        assert asset.is_file(), f"Не найден ресурс: {asset.relative_to(ROOT)}"

    durations = {
        "lobbymusic.wav": 70,
        "winter_jingle.wav": 70,
        "duel_theme.wav": 45,
    }
    for name, expected in durations.items():
        path = ROOT / "assets" / "audio" / name
        with wave.open(str(path), "rb") as audio:
            assert audio.getnchannels() == 2 and audio.getsampwidth() == 2, f"Неверный формат музыки: {name}"
            duration = audio.getnframes() / audio.getframerate()
            assert abs(duration - expected) < 0.001, f"Неверная длительность {name}: {duration}"

    # The old "Azum vs Astra" song and its reference mp3 must stay removed.
    assert not (ROOT / "assets" / "audio" / "azum_vs_astra.wav").exists(), "Старая песня azum_vs_astra.wav вернулась"
    assert not (ROOT / "And_The_Seraphim_Wept.mp3").exists(), "Сторонний mp3 снова в репозитории"
    assert not (ROOT / "tools" / "generate_seraphim.py").exists(), "Генератор старой песни снова в репозитории"

    with wave.open(str(ROOT / "assets" / "audio" / "duel_theme.wav"), "rb") as duel:
        assert duel.getnchannels() == 2 and duel.getsampwidth() == 2, "Неверный формат duel_theme.wav"
        rate = duel.getframerate()
        head = duel.readframes(rate * 5)
        import struct, math
        samples = struct.unpack("<" + "h" * (len(head) // 2), head)
        rms5 = math.sqrt(sum(x * x for x in samples) / len(samples))
        assert rms5 >= 4000.0, f"duel_theme.wav слишком тихий в начале боя (RMS={rms5:.1f})"

    assert not (ROOT / "assets" / "audio" / "halloween_lobby.wav").exists(), "Хэллоуинская музыка вернулась"


def check_port_layout() -> None:
    assert not list(ROOT.rglob("*.ds")), "В C-порте остались исходники старого языка"
    assert not (ROOT / "game").exists(), "Старый каталог game не должен использоваться"
    assert not (ROOT / "native").exists(), "Старый каталог native не должен использоваться"
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
        assert directory.is_dir(), f"Нет каталога проекта: {directory.relative_to(ROOT)}"

    old_prefix = "ds" + "_fn_"
    for source in (ROOT / "src" / "game").rglob("*.inc"):
        assert old_prefix not in source.read_text(encoding="utf-8"), source


def main() -> int:
    check_port_layout()
    check_assets()
    check_disco_music()
    check_firebase_rules()
    check_write_secret()
    print("Ресурсы и схема сети: норма")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
