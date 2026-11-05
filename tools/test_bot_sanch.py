# -*- coding: utf-8 -*-
"""Offline checks for the Cubic Battle 4 bot.

Run: python3 tools/test_bot_sanch.py
Uses a temporary SQLite database only, the Telegram API is never called.
"""
import importlib.util
import os
import sys
import tempfile
import types

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOT = os.path.join(ROOT, "bot", "cb4_bot.py")


class _NoNetwork:
    """requests stub: the test never touches the network, even without the package."""

    def __getattr__(self, name):
        def call(*a, **k):
            raise AssertionError(f"network call in test: {name}")
        return call


def load_bot(db_path):
    sys.modules.setdefault("requests", types.SimpleNamespace(post=None, get=None, delete=None,
                                                            Session=_NoNetwork))
    os.environ["CB4_BOT_DB"] = db_path
    os.environ.pop("CB4_BOT_TOKEN", None)
    spec = importlib.util.spec_from_file_location("cb4_bot_test", BOT)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def main():
    with tempfile.TemporaryDirectory() as tmp:
        bot = load_bot(os.path.join(tmp, "bot_data.db"))
        bot.init_db()
        sent = []
        bot.send_message = lambda chat_id, text, *a, **k: sent.append((chat_id, text)) or {}
        bot.edit_message = lambda *a, **k: {}
        bot.answer_cb = lambda *a, **k: {}

        # 1. A regular player without premium.
        bot.add_user(10, "player", "Player")
        bot.set_nickname(10, "Player2")
        nick, prem, pend, _, _ = bot.get_user(10)
        assert nick == "Player2" and prem == 0 and pend == 0
        assert bot.is_sanch(10) is False
        assert bot.profile_status(10, nick, prem, pend) == "Обычный"

        # 2. The Sanch nick turns premium on by itself (any case, any spaces).
        for raw in ("Sanch", "sanch", "SANCH", "  Sanch  "):
            bot.add_user(20, "sanch_user", "Sanch")
            bot.set_nickname(20, raw)
            nick, prem, pend, _, _ = bot.get_user(20)
            assert bot.nick_is_sanch(raw) is True
            assert bot.is_sanch(20, nick) is True
            assert prem == 1, raw
            assert bot.profile_status(20, nick, prem, pend) == "💎 Премиум (золотой ник)"

        # 3. The premium button asks for no money and creates no request.
        bot.handle_premium(20)
        assert bot.get_user(20)[2] == 0
        assert "уже есть премиум" in sent[-1][1]

        # 4. The paid button for Sanch sends no request to the admins.
        bot.handle_ipaid(20, 1, "cb")
        assert bot.get_user(20)[2] == 0

        # 5. Gold nick premium goes away with the nick, otherwise anyone could
        #    type Sanch, take the premium and switch the nick back.
        bot.set_nickname(20, "Sanchik")
        nick, prem, _, _, _ = bot.get_user(20)
        assert bot.is_sanch(20, nick) is False and prem == 0
        assert bot.premium_src_of(20) is None

        # 5b. Admin granted premium survives a nick change.
        bot.add_user(21, "vip", "")
        bot.set_premium(21)
        bot.set_nickname(21, "Sanchik")
        assert bot.get_user(21)[1] == 1
        bot.set_nickname(21, "Sanch")
        assert bot.get_user(21)[1] == 1 and bot.premium_src_of(21) == "admin"
        bot.set_nickname(21, "Other")
        assert bot.get_user(21)[1] == 1

        # 6. A user without a nick and the legacy premium migration.
        bot.add_user(30, "", "")
        assert bot.ensure_gold_premium(30) is False
        bot.db_exec("UPDATE users SET premium=1, premium_src=NULL WHERE id=31")
        bot.add_user(31, "old", "")
        bot.db_exec("UPDATE users SET premium=1, premium_src=NULL, nickname='Sanch' WHERE id=31")
        bot.init_db()
        assert bot.get_user(31)[1] == 1 and bot.premium_src_of(31) == "gold_nick"

        # 7. Telegram ID restriction: a foreign account with the Sanch nick gets nothing.
        bot = load_bot(os.path.join(tmp, "bot_data2.db"))
        bot.init_db()
        bot.SANCH_TG_IDS = [777]
        bot.add_user(20, "fake", "")
        bot.set_nickname(20, "Sanch")
        assert bot.get_user(20)[1] == 0
        bot.add_user(777, "real", "")
        bot.set_nickname(777, "Sanch")
        assert bot.get_user(777)[1] == 1

        # 8. In game character names must never change.
        loc = open(os.path.join(ROOT, "src", "game", "ui", "locale_shop.inc"), encoding="utf-8").read()
        core = open(os.path.join(ROOT, "src", "game", "ui", "locale_core.inc"), encoding="utf-8").read()
        assert 'return "буК";' in loc and 'return "ebuC";' in loc
        assert 'return "Подарок буКа";' in core and "return \"ebuC's gift\";" in core

    puts("bot: Sanch premium (gold nick), no payment, naming буК/буКа/ebuC/ebuC's OK")


def puts(msg):
    sys.stdout.write(msg + "\n")


if __name__ == "__main__":
    main()
