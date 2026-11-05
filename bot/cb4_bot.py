# -*- coding: utf-8 -*-
"""Telegram bot for Cubic Battle 4: cups, news, premium.

In game naming rules (do not rename):
  * Russian class name    - "буК"  (exactly: lowercase "бу", uppercase "К");
  * Russian genitive      - "буКа" ("Подарок буКа", "Отскоки буКа");
  * English               - "ebuC" (reverse of "cube"), possessive - "ebuC's"
                            ("ebuC's gift").
  * Sanch has a gold nick in game (0xFFFFD54A), so in the bot he is premium already.
"""
import os
import sqlite3
import time
import requests
from datetime import datetime, timedelta

# ==================== CONFIG ====================
# The token comes from the environment so it never lives in the repository.
# Locally: export CB4_BOT_TOKEN="123456:AA..." (or paste the string below).
BOT_TOKEN = os.environ.get("CB4_BOT_TOKEN", "").strip()
PREMIUM_USERNAME = "dimasi4ek01"
PRICE_STARS = 2

CUPS_REWARD = 10
CUPS_COOLDOWN_HOURS = 1

BASE_URL = f"https://api.telegram.org/bot{BOT_TOKEN}"
LAST_UPDATE_ID = 0
ADMIN_IDS = [8533924915, 6451428066]

# Sanch has a gold nick in game, he is premium already and has nothing to buy.
SANCH_NICK = "Sanch"
# To bind the gold nick to concrete Telegram accounts, list their IDs here
# (look them up with @userinfobot). Empty list = anyone who types the Sanch nick.
SANCH_TG_IDS = []

DB_PATH = os.environ.get("CB4_BOT_DB", "bot_data.db")


# ==================== DATABASE ====================
def init_db():
    conn = sqlite3.connect(DB_PATH)
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY, username TEXT, first_name TEXT,
        nickname TEXT, premium INTEGER DEFAULT 0, pending INTEGER DEFAULT 0,
        registered TEXT, last_cups_time TEXT, local_cups INTEGER DEFAULT 0,
        premium_src TEXT)''')
    try: c.execute("ALTER TABLE users ADD COLUMN pending INTEGER DEFAULT 0")
    except: pass
    try: c.execute("ALTER TABLE users ADD COLUMN last_cups_time TEXT")
    except: pass
    try: c.execute("ALTER TABLE users ADD COLUMN local_cups INTEGER DEFAULT 0")
    except: pass
    try: c.execute("ALTER TABLE users ADD COLUMN premium_src TEXT")
    except: pass

    c.execute('''CREATE TABLE IF NOT EXISTS news (
        id INTEGER PRIMARY KEY AUTOINCREMENT, content TEXT,
        created TEXT, visible INTEGER DEFAULT 1,
        media_type TEXT, media_id TEXT)''')
    try: c.execute("ALTER TABLE news ADD COLUMN media_type TEXT")
    except: pass
    try: c.execute("ALTER TABLE news ADD COLUMN media_id TEXT")
    except: pass
    conn.commit(); conn.close()
    # Migration: legacy premium counts as admin granted, and the Sanch nick gets
    # premium for the gold nick, he has it already and has nothing to buy.
    db_exec("UPDATE users SET premium_src='admin' WHERE premium=1 AND premium_src IS NULL")
    db_exec("UPDATE users SET premium=1, pending=0, premium_src='gold_nick' WHERE lower(nickname)=lower(?)",
            (SANCH_NICK,))


def db_exec(q, p=()):
    conn = sqlite3.connect(DB_PATH); c = conn.cursor()
    c.execute(q, p); conn.commit(); conn.close()


def db_fetch(q, p=()):
    conn = sqlite3.connect(DB_PATH); c = conn.cursor()
    c.execute(q, p); r = c.fetchall(); conn.close(); return r


def add_user(uid, username, first_name):
    db_exec("INSERT OR IGNORE INTO users (id, username, first_name, registered) VALUES (?,?,?,?)",
            (uid, username, first_name, datetime.now().isoformat()))


def set_nickname(uid, n):
    db_exec("UPDATE users SET nickname=? WHERE id=?", (n, uid))
    sync_premium_on_nick(uid)


def set_premium(uid, src="admin"):
    db_exec("UPDATE users SET premium=1, pending=0, premium_src=? WHERE id=?", (src, uid))


def set_pending(uid):     db_exec("UPDATE users SET pending=1 WHERE id=?", (uid,))
def clear_pending(uid):   db_exec("UPDATE users SET pending=0 WHERE id=?", (uid,))
def set_last_cups_time(uid, iso): db_exec("UPDATE users SET last_cups_time=? WHERE id=?", (iso, uid))


def get_user(uid):
    r = db_fetch("SELECT nickname, premium, pending, last_cups_time, local_cups FROM users WHERE id=?", (uid,))
    return r[0] if r else (None, 0, 0, None, 0)


def premium_src_of(uid):
    r = db_fetch("SELECT premium_src FROM users WHERE id=?", (uid,))
    return r[0][0] if r else None


def is_admin(uid): return uid in ADMIN_IDS


# ==================== SANCH AND GOLD NICK ====================
def nick_is_sanch(nick):
    """True when the nick is Sanch, ignoring case and spaces (like admin_nick_equal)."""
    if not nick:
        return False
    return nick.strip().lower() == SANCH_NICK.lower()


def is_sanch(uid, nick=None):
    """True for Sanch: gold nick plus the optional Telegram ID restriction."""
    if SANCH_TG_IDS and uid not in SANCH_TG_IDS:
        return False
    if nick is None:
        nick = get_user(uid)[0]
    return nick_is_sanch(nick)


def ensure_gold_premium(uid):
    """Sanch has a gold nick, so premium is granted here automatically.

    Returns True when the gold nick holds premium, new or existing.
    """
    nick, prem, pending, _, _ = get_user(uid)
    if not is_sanch(uid, nick):
        return False
    if prem != 1 or pending != 0:
        set_premium(uid, "gold_nick")
    elif premium_src_of(uid) is None:
        db_exec("UPDATE users SET premium_src='gold_nick' WHERE id=?", (uid,))
    return True


def sync_premium_on_nick(uid):
    """Premium for the gold nick lives with the nick: changed the nick, lost it.

    Premium granted by an admin through /give is left alone.
    """
    if ensure_gold_premium(uid):
        return True
    if premium_src_of(uid) == "gold_nick":
        db_exec("UPDATE users SET premium=0, premium_src=NULL WHERE id=?", (uid,))
    return False


# Cups live in SQLite now
def get_game_cups(nick):
    r = db_fetch("SELECT local_cups FROM users WHERE nickname=?", (nick,))
    return r[0][0] if r else 0


def add_game_cups(nick, amount):
    db_exec("UPDATE users SET local_cups = local_cups + ? WHERE nickname=?", (amount, nick))
    return True


# ==================== TELEGRAM API ====================
def api(method, data):
    try:
        r = requests.post(f"{BASE_URL}/{method}", json=data, timeout=15).json()
        if not r.get("ok"):
            print(f"ОШИБКА TG API: {r}", flush=True)
        return r
    except Exception as e:
        print(f"ОШИБКА СЕТИ: {e}", flush=True)
        return {}


def send_message(chat_id, text, reply_markup=None, parse_mode="HTML"):
    d = {"chat_id": chat_id, "text": text}
    if parse_mode: d["parse_mode"] = parse_mode
    if reply_markup: d["reply_markup"] = reply_markup
    return api("sendMessage", d)


def edit_message(chat_id, msg_id, text, reply_markup=None, parse_mode="HTML"):
    d = {"chat_id": chat_id, "message_id": msg_id, "text": text}
    if parse_mode: d["parse_mode"] = parse_mode
    if reply_markup: d["reply_markup"] = reply_markup
    return api("editMessageText", d)


def answer_cb(cb_id, text=None):
    d = {"callback_query_id": cb_id}
    if text: d["text"] = text
    api("answerCallbackQuery", d)


# ==================== KEYBOARDS ====================
def main_menu(nick=None):
    return {"keyboard": [
        [{"text": "🏆 Получить 10 кубков"}],
        [{"text": "👤 Профиль"}, {"text": "📰 Новости"}],
        [{"text": "💎 Премиум"}, {"text": "✏️ Сменить ник"}]
    ], "resize_keyboard": True, "is_persistent": True}


def pay_menu():
    return {"inline_keyboard": [
        [{"text": f"💸 Отправить {PRICE_STARS} звезды @{PREMIUM_USERNAME}", "url": f"https://t.me/{PREMIUM_USERNAME}"}],
        [{"text": "✅ Я оплатил", "callback_data": "ipaid"}]
    ]}


# ==================== LOGIC ====================
waiting_change_nick, waiting_premium = set(), set()


def try_claim_cups(chat_id):
    nick, prem, pend, last_time, cups = get_user(chat_id)
    if not nick:
        return send_message(chat_id, "❌ Сначала укажи ник через /start", main_menu())

    now = datetime.now()
    if last_time:
        try:
            last_dt = datetime.fromisoformat(last_time)
            next_dt = last_dt + timedelta(hours=CUPS_COOLDOWN_HOURS)
            if now < next_dt:
                remain = next_dt - now
                mins = int(remain.total_seconds() // 60)
                hrs, mins = divmod(mins, 60)
                return send_message(chat_id,
                    f"⏳ Следующие <b>{CUPS_REWARD}</b> кубков через <b>{hrs}ч {mins}м</b>.\n"
                    f"🏆 Сейчас у тебя: <b>{cups}</b>",
                    main_menu(nick))
        except: pass

    add_game_cups(nick, CUPS_REWARD)
    set_last_cups_time(chat_id, now.isoformat())
    total = get_game_cups(nick)
    send_message(chat_id,
        f"🎉 <b>+{CUPS_REWARD} кубков!</b>\n"
        f"🏆 Всего: <b>{total}</b>\n"
        f"⏳ Следующие через {CUPS_COOLDOWN_HOURS} ч.",
        main_menu(nick))


def handle_start(chat_id, user):
    add_user(chat_id, user.get("username", ""), user.get("first_name", ""))
    ensure_gold_premium(chat_id)
    nick = get_user(chat_id)[0]
    if not nick:
        waiting_change_nick.add(chat_id)
        send_message(chat_id, "👋 Привет! Напиши свой игровой ник:", main_menu())
        return
    send_message(chat_id, f"👋 Привет! Твой ник: <b>{nick}</b>", main_menu(nick))


def profile_status(chat_id, nick, prem, pend):
    """Status line: Sanch always has premium from the gold nick."""
    if is_sanch(chat_id, nick):
        return "💎 Премиум (золотой ник)"
    if prem:
        return "💎 Премиум"
    if pend:
        return "⏳ На проверке"
    return "Обычный"


def handle_profile(chat_id):
    ensure_gold_premium(chat_id)
    nick, prem, pend, last_time, cups = get_user(chat_id)
    if not nick:
        send_message(chat_id, "❌ Сначала укажи ник через /start", main_menu())
        return
    status = profile_status(chat_id, nick, prem, pend)
    cd_text = ""
    if last_time:
        try:
            next_dt = datetime.fromisoformat(last_time) + timedelta(hours=CUPS_COOLDOWN_HOURS)
            now = datetime.now()
            if now < next_dt:
                mins = int((next_dt - now).total_seconds() // 60)
                hrs, mins = divmod(mins, 60)
                cd_text = f"\n⏳ Кубки через: <b>{hrs}ч {mins}м</b>"
            else:
                cd_text = "\n✅ Кубки доступны!"
        except: pass
    send_message(chat_id, f"👤 <b>Профиль</b>\n\n🎮 Ник: <b>{nick}</b>\n🏆 Кубков: <b>{cups}</b>\n📊 Статус: {status}{cd_text}", main_menu(nick))


def handle_premium(chat_id):
    ensure_gold_premium(chat_id)
    nick, prem, pend, _, _ = get_user(chat_id)
    if is_sanch(chat_id, nick):
        return send_message(chat_id,
            "💎 У тебя уже есть премиум - твой ник <b>Sanch</b> золотой, платить не нужно!",
            main_menu(nick))
    if prem: return send_message(chat_id, "💎 У тебя уже есть премиум!", main_menu(nick))
    if pend: return send_message(chat_id, "⏳ Заявка уже на проверке.", main_menu(nick))
    waiting_premium.add(chat_id)
    send_message(chat_id, "💎 Напиши свой ник для получения премиума:", main_menu())


def handle_ipaid(chat_id, msg_id, cb_id):
    nick = get_user(chat_id)[0]
    if is_sanch(chat_id, nick):
        if cb_id: answer_cb(cb_id, "Премиум уже активен!")
        return edit_message(chat_id, msg_id,
            "💎 Премиум уже активен: ник <b>Sanch</b> золотой, заявка не нужна.",
            main_menu(nick))
    if cb_id: answer_cb(cb_id, "Заявка отправлена!")
    if not nick: return edit_message(chat_id, msg_id, "❌ Сначала укажи ник.", main_menu())
    set_pending(chat_id)
    edit_message(chat_id, msg_id, "✅ Заявка отправлена! Премиум придет в течение дня.", main_menu(nick))
    for admin in ADMIN_IDS: send_message(admin, f"🔔 Заявка на премиум от {nick}")


def show_news_item(chat_id):
    news = db_fetch("SELECT content, media_type, media_id FROM news WHERE visible=1 ORDER BY id DESC LIMIT 1")
    if not news: return send_message(chat_id, "📰 Пока новостей нет.", main_menu(get_user(chat_id)[0]))
    content, media_type, media_id = news[0]
    html_text = f"📰 <b>Новость</b>\n\n<tg-spoiler>{content}</tg-spoiler>"
    if media_type == 'photo': api("sendPhoto", {"chat_id": chat_id, "photo": media_id, "caption": html_text, "parse_mode": "HTML"})
    elif media_type == 'video': api("sendVideo", {"chat_id": chat_id, "video": media_id, "caption": html_text, "parse_mode": "HTML"})
    else: api("sendMessage", {"chat_id": chat_id, "text": html_text, "parse_mode": "HTML"})


# ==================== ADMIN PANEL ====================
def admin_help(chat_id):
    send_message(chat_id, "👑 <b>Админ-команды:</b>\n\n/news Текст\n/hidenews ID\n/shownews ID\n/delnews ID\n/give ID\n/deny ID\n/pending\n/stats")


def admin_pending(chat_id):
    rows = db_fetch("SELECT id, nickname FROM users WHERE pending=1")
    if not rows: return send_message(chat_id, "📭 Нет заявок")
    text = "📋 <b>Заявки:</b>\n\n"
    for uid, nick in rows: text += f"• ID: <code>{uid}</code> | Ник: <b>{nick}</b>\n"
    send_message(chat_id, text)


def admin_give(chat_id, target):
    set_premium(target)
    send_message(chat_id, f"✅ Премиум выдан {target}")
    send_message(target, "🎉 Премиум активирован!", main_menu(get_user(target)[0]))


def admin_deny(chat_id, target):
    clear_pending(target)
    send_message(chat_id, f"❌ Заявка {target} отклонена")
    send_message(target, "❌ Заявка отклонена.", main_menu(get_user(target)[0]))


def admin_stats(chat_id):
    total = db_fetch("SELECT COUNT(*) FROM users")[0][0]
    prem = db_fetch("SELECT COUNT(*) FROM users WHERE premium=1")[0][0]
    pend = db_fetch("SELECT COUNT(*) FROM users WHERE pending=1")[0][0]
    send_message(chat_id, f"📊 Юзеров: {total}\n💎 Премиум: {prem}\n⏳ Заявок: {pend}")


def admin_news_control(chat_id, text):
    parts = text.split()
    if len(parts) < 2 or not parts[1].isdigit(): return send_message(chat_id, "❌ Укажи ID.")
    nid = int(parts[1])
    if text.startswith("/hidenews"): db_exec("UPDATE news SET visible=0 WHERE id=?", (nid,)); send_message(chat_id, "🙈 Скрыта")
    elif text.startswith("/shownews"): db_exec("UPDATE news SET visible=1 WHERE id=?", (nid,)); send_message(chat_id, "👁 Показана")
    elif text.startswith("/delnews"): db_exec("DELETE FROM news WHERE id=?", (nid,)); send_message(chat_id, "🗑 Удалена")


# ==================== POLLING ====================
def process_updates():
    global LAST_UPDATE_ID
    print("=====================================", flush=True)
    print("БОТ УСПЕШНО ЗАПУЩЕН И ЖДЕТ СООБЩЕНИЙ", flush=True)
    print("=====================================", flush=True)

    while True:
        try:
            r = requests.get(f"{BASE_URL}/getUpdates", params={"offset": LAST_UPDATE_ID + 1, "timeout": 30}, timeout=35)

            if r.status_code == 409:
                print("ОШИБКА 409: КОНФЛИКТ! Бот уже запущен где-то еще. Остановите старую версию.", flush=True)
                time.sleep(5); continue

            if r.status_code != 200:
                print(f"HTTP ERROR: {r.status_code} - {r.text}", flush=True)
                time.sleep(2); continue

            data = r.json()
            for upd in data.get("result", []):
                LAST_UPDATE_ID = upd.get("update_id", LAST_UPDATE_ID)

                if "callback_query" in upd:
                    cb = upd["callback_query"]
                    chat_id = cb["message"]["chat"]["id"]
                    msg_id = cb["message"]["message_id"]
                    cb_id = cb["id"]
                    if cb.get("data") == "ipaid": handle_ipaid(chat_id, msg_id, cb_id)
                    else: answer_cb(cb_id)
                    continue

                if "message" in upd:
                    msg = upd["message"]
                    chat_id = msg["chat"]["id"]
                    text = msg.get("text", "").strip()

                    print(f">>> ПОЛУЧЕНО СООБЩЕНИЕ ОТ {chat_id}: {text}", flush=True)

                    if is_admin(chat_id):
                        if "photo" in msg or "video" in msg:
                            mtype = "photo" if "photo" in msg else "video"
                            mid = msg["photo"][-1]["file_id"] if mtype == "photo" else msg["video"]["file_id"]
                            db_exec("INSERT INTO news (content, created, visible, media_type, media_id) VALUES (?,?,1,?,?)", (msg.get("caption",""), datetime.now().isoformat(), mtype, mid))
                            send_message(chat_id, "✅ Медиа-новость добавлена!"); continue
                        if text.startswith("/news "):
                            c = text.replace("/news", "", 1).strip()
                            if c: db_exec("INSERT INTO news (content, created, visible) VALUES (?,?,1)", (c, datetime.now().isoformat())); send_message(chat_id, "✅ Добавлено!")
                            continue
                        if text.startswith("/give "):
                            try: admin_give(chat_id, int(text.split()[1]))
                            except: send_message(chat_id, "❌ /give ID")
                            continue
                        if text.startswith("/deny "):
                            try: admin_deny(chat_id, int(text.split()[1]))
                            except: send_message(chat_id, "❌ /deny ID")
                            continue
                        if text == "/pending": admin_pending(chat_id); continue
                        if text == "/stats": admin_stats(chat_id); continue
                        if text == "/admin": admin_help(chat_id); continue
                        if text.startswith("/hidenews") or text.startswith("/shownews") or text.startswith("/delnews"):
                            admin_news_control(chat_id, text); continue

                    if chat_id in waiting_change_nick:
                        if 2 <= len(text) <= 16:
                            set_nickname(chat_id, text); waiting_change_nick.discard(chat_id)
                            if is_sanch(chat_id, text):
                                send_message(chat_id, f"✅ Ник <b>{text}</b> сохранен!\n💎 Премиум уже твой - ник золотой.", main_menu(text))
                            else:
                                send_message(chat_id, f"✅ Ник <b>{text}</b> сохранен!", main_menu(text))
                        else: send_message(chat_id, "❌ Ник от 2 до 16 символов.", main_menu())
                        continue

                    if chat_id in waiting_premium:
                        if 2 <= len(text) <= 16:
                            set_nickname(chat_id, text); waiting_premium.discard(chat_id)
                            if is_sanch(chat_id, text):
                                send_message(chat_id, "💎 У тебя уже есть премиум - ник <b>Sanch</b> золотой, платить не нужно!", main_menu(text))
                            else:
                                send_message(chat_id, f"💎 Отправь <b>{PRICE_STARS} звезды</b> на @{PREMIUM_USERNAME} и нажми кнопку.", pay_menu())
                        else: send_message(chat_id, "❌ Ник от 2 до 16 символов.", main_menu())
                        continue

                    if text == "/start": handle_start(chat_id, msg)
                    elif text == "🏆 Получить 10 кубков": try_claim_cups(chat_id)
                    elif text == "👤 Профиль": handle_profile(chat_id)
                    elif text == "✏️ Сменить ник": waiting_change_nick.add(chat_id); send_message(chat_id, "Напиши новый ник:", main_menu())
                    elif text == "💎 Премиум": handle_premium(chat_id)
                    elif text == "📰 Новости": show_news_item(chat_id)
            time.sleep(1)
        except Exception as e:
            print(f"КРИТИЧЕСКАЯ ОШИБКА: {e}", flush=True)
            time.sleep(5)


if __name__ == "__main__":
    if not BOT_TOKEN:
        raise SystemExit('Нет токена бота: export CB4_BOT_TOKEN="123456:AA..." и запусти снова.')
    try:
        requests.delete(f"{BASE_URL}/deleteWebhook")
    except: pass
    init_db()
    process_updates()
