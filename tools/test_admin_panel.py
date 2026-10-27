#!/usr/bin/env python3
"""Static and live-mock checks for index.html and firebase.rules.json."""
import json
import os
import re
import shutil
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PANEL = ROOT / "index.html"
RULES = ROOT / "firebase.rules.json"
FAKE = ROOT / "test" / "fake_firebase.py"

ERRORS: list[str] = []


def error(message: str) -> None:
    ERRORS.append(message)
    print(f"  FAIL {message}", file=sys.stderr)


def scripts_of(html: str) -> list[str]:
    return re.findall(r"<script>(.*?)</script>", html, re.S)


def extract_const(js: str, name: str) -> str | None:
    """The initializer text of `const name = ...` (with parens if they are there)."""
    match = re.search(r"\bconst\s+%s\s*=\s*" % re.escape(name), js)
    if not match:
        return None
    i = match.end()
    while i < len(js) and js[i] in " \n\t":
        i += 1
    if i >= len(js):
        return None
    if js[i] in "[{":
        opener, closer = js[i], ("]" if js[i] == "[" else "}")
        depth = 0
        for j in range(i, len(js)):
            if js[j] == opener:
                depth += 1
            elif js[j] == closer:
                depth -= 1
                if depth == 0:
                    return js[i:j + 1]
        return None
    end = js.find(";", i)
    return js[i:end] if end > 0 else None


def list_of(block: str | None) -> list[str]:
    if not block:
        return []
    return re.findall(r'"([^"]+)"', block)


SPEC_RE = re.compile(
    r"([A-Za-z_][A-Za-z0-9_]*)\s*:\s*"
    r"(num\([^)]*\)|flag\(\)|str\([^)]*\)|secret\(\)|\{[^{}]*\})")


def spec_fields(block: str | None) -> dict[str, str]:
    if not block:
        return {}
    body = re.sub(r"/\*.*?\*/", "", block, flags=re.S)
    body = re.sub(r"//[^\n]*", "", body)
    return {m.group(1): m.group(2) for m in SPEC_RE.finditer(body)}


def panel_spec(raw: str):
    """`num(0,3)` -> ("num", 0.0, 3.0); `flag()` -> ("num", 0, 1); and so on."""
    raw = raw.strip()
    if raw == "flag()":
        return ("num", 0.0, 1.0)
    if raw == "secret()":
        return ("str", None, None)
    if raw.startswith("num("):
        args = [float(a) for a in re.findall(r"-?\d+(?:\.\d+)?", raw)]
        return ("num", args[0] if args else None, args[1] if len(args) > 1 else None)
    if raw.startswith("str("):
        args = [float(a) for a in re.findall(r"-?\d+(?:\.\d+)?", raw)]
        return ("str", args[0] if args else None, args[1] if len(args) > 1 else None)
    if raw.startswith("{"):
        if '"forbidden"' in raw:
            return ("forbidden",)
        if '"obj"' in raw:
            return ("obj",)
    return None


def rules_spec(validate):
    """The same shape, but from the `.validate` of the rules."""
    if validate is False:
        return ("forbidden",)
    if not isinstance(validate, str):
        return None
    if "isNumber()" in validate:
        if re.search(r"newData\.val\(\)\s*==\s*0\s*\|\|\s*newData\.val\(\)\s*==\s*1", validate):
            return ("num", 0.0, 1.0)
        bounds = {op: float(val) for op, val in
                  re.findall(r"newData\.val\(\)\s*(>=|<=)\s*(-?\d+(?:\.\d+)?)", validate)}
        if not bounds:
            return ("num", None, None)
        return ("num", bounds.get(">="), bounds.get("<="))
    if "isString()" in validate:
        if re.search(r"newData\.val\(\)\s*==\s*'", validate):
            return ("str", None, None)          # exact write-secret match ("w")
        lens = {op: float(val) for op, val in
                re.findall(r"\.length\s*(>=|<=)\s*(\d+)", validate)}
        return ("str", lens.get(">="), lens.get("<="))
    if "isBoolean()" in validate:
        return ("bool",)
    if "hasChildren" in validate:
        return ("obj",)
    return None


def required_children(node: dict) -> list[str]:
    match = re.search(r"hasChildren\(\[(.*?)\]\)", node.get(".validate", "") or "")
    if not match:
        return []
    return re.findall(r"'([^']+)'", match.group(1))


def child_keys(node: dict) -> list[str]:
    return [k for k in node if not k.startswith(".") and k != "$other" and not k.startswith("$")]


def compare(title: str, panel: dict[str, str], rules: dict, allow_extra=()) -> None:
    """Compare the panel fields with the rules: which fields, and their bounds."""
    panel_keys = set(panel)
    rule_keys = {k for k in child_keys(rules)}
    for key in rule_keys - panel_keys:
        error(f"{title}: панель не знает поле «{key}» из правил - "
              f"оно будет предложено к удалению как мусор")
    for key in panel_keys - rule_keys - set(allow_extra):
        error(f"{title}: панель разрешает «{key}», которого нет в правилах - "
              f"база такую запись всё равно отклонит")
    for key in sorted(panel_keys & rule_keys):
        got = panel_spec(panel[key])
        want = rules_spec(rules[key].get(".validate") if isinstance(rules[key], dict) else None)
        if want is None:
            continue
        if got is None:
            error(f"{title}/{key}: не разобрал спецификацию панели «{panel[key]}»")
            continue
        if got[0] != want[0]:
            error(f"{title}/{key}: тип {got[0]}, а правила ждут {want[0]}")
            continue
        if got[0] in ("num", "str"):
                        # Bounds: a rule may have no lower bound for a string (the nick),
                        # so the panel may then require a non-empty string.
            if want[1] is not None and got[1] != want[1]:
                error(f"{title}/{key}: нижняя граница {got[1]}, правила ждут {want[1]}")
            if want[2] is not None and got[2] != want[2]:
                error(f"{title}/{key}: верхняя граница {got[2]}, правила ждут {want[2]}")


def check_syntax(js_blocks: list[str]) -> None:
    node = shutil.which("node")
    if not node:
        print("  node не найден - синтаксис скриптов не проверяется (установи node для полной проверки)")
        return
    for index, block in enumerate(js_blocks):
        tmp = Path(os.environ.get("TMPDIR", "/tmp")) / f"cb4_panel_block{index}.js"
        tmp.write_text(block, encoding="utf-8")
        proc = subprocess.run([node, "--check", str(tmp)], capture_output=True, text=True)
        if proc.returncode != 0:
            error(f"скрипт №{index + 1} не компилируется - страница будет мёртвой:\n{proc.stderr.strip()}")
        tmp.unlink(missing_ok=True)


def check_dom_wiring(html: str, js: str) -> None:
    ids = set(re.findall(r'\bid="([^"]+)"', html))
    classes = set()
    for attr in re.findall(r'\bclass="([^"]+)"', html):
        classes.update(attr.split())

    used_ids = set(re.findall(r"""\$\$?\(\s*"#([A-Za-z0-9_-]+)\"""", js))
    used_ids |= set(re.findall(r"""\bon(?:All|Change)?\(\s*"#([A-Za-z0-9_-]+)\"""", js))
    for used in sorted(used_ids - ids):
        error(f"скрипт обращается к #{used}, которого нет в разметке - кнопка/поле не будет работать")

    used_classes = set(re.findall(r"""\$\$\(\s*"\.([A-Za-z0-9_-]+)\"""", js))
    for used in sorted(used_classes - classes):
        error(f"скрипт обращается к классу .{used}, которого нет в разметке")

        # Every button gets a handler: by id (on("#id", ...)) or by data attribute.
    bound_ids = set(re.findall(r"""\bon\(\s*"#([A-Za-z0-9_-]+)\"""", js))
    bound_attrs = set(re.findall(r"""\bonAll\(\s*"\[data-([A-Za-z0-9_-]+)\]\"""", js))
    for tag in re.findall(r"<button\b[^>]*>", html):
        button_id = re.search(r'\bid="([^"]+)"', tag)
        data = re.findall(r"\bdata-([A-Za-z0-9_-]+)=", tag)
        if button_id and button_id.group(1) in bound_ids:
            continue
        if any(name in bound_attrs for name in data):
            continue
        label = button_id.group(1) if button_id else tag
        error(f"у кнопки {label} нет обработчика - она не нажимается")

        # Attribute selectors from onAll must find something.
    for attr in sorted(bound_attrs):
        if f"data-{attr}=" not in html:
            error(f"onAll([data-{attr}]) не находит ни одного элемента")


def check_guard(js_blocks: list[str]) -> None:
    if len(js_blocks) < 2:
        error("в панели один скрипт: сторож ошибок должен грузиться первым, отдельным блоком")
        return
    guard = js_blocks[0]
    if 'addEventListener("error"' not in guard or "cb4Fatal" not in guard:
        error("первый скрипт не ловит ошибки страницы - поломка снова будет молчаливой")
    if "cb4Fatal" not in js_blocks[1]:
        error("основной скрипт не сообщает о фатальной ошибке")


def check_schema(html: str, js: str, rules: dict) -> None:
    root_keys = [k for k in rules if not k.startswith(".") and k != "$other"]

    # Obsolete fields from older game versions must be listed, findable and
    # cleanable from the panel, while the schema still mirrors the rules.
    legacy = re.findall(r'"([^"]+)"\s*:', extract_const(js, "LEGACY_FIELDS") or "")
    if not legacy:
        error("в панели нет LEGACY_FIELDS - чистить мусор старых версий нечем")
    for path in ("users/*/gifts/emerald_hue", "users/*/gifts/rose"):
        if path not in legacy:
            error(f"в LEGACY_FIELDS нет устаревшего поля {path}")
    gift_panel = spec_fields(extract_const(js, "GIFT_FIELDS"))
    user_panel = spec_fields(extract_const(js, "USER_FIELDS"))
    for path in legacy:
        key = path.rsplit("/", 1)[-1]
        if key not in gift_panel and key not in user_panel:
            error(f"устаревшее поле {key} не описано в схеме панели, а правила его пускают")
    if 'data-k="legacy"' not in html:
        error("у панели нет фильтра «Устаревшие поля»")
    if "legacy" not in list_of(extract_const(js, "OLD_CLEAN_KINDS")):
        error("общая чистка старых данных не захватывает устаревшие поля")
    if "legacyWhy" not in js or "resetJanitorFilters" not in js:
        error("панель не помечает и не чистит устаревшие поля")

    # All visible labels start with a capital letter (except buK/ebuC names).
    for needle in ('id="modePill">боевая', 'placeholder="ник"', '"готово: удалено',
                   '"чистка завершена:', '"чищу ', '"удаляю ', '"meta - не объект"',
                   '"больше допустимого ', '"неверный пароль или такого аккаунта'):
        if needle in html:
            error(f"надпись начинается с маленькой буквы: {needle}")

    for name in ("ROOT_OK", "ROOT_PARTS"):
        panel = list_of(extract_const(js, name))
        if not panel:
            error(f"не нашёл {name} в панели")
            continue
        missing = sorted(set(root_keys) - set(panel))
        extra = sorted(set(panel) - set(root_keys))
        if missing:
            error(f"{name}: нет узлов из правил {missing} - панель сочтёт их мусором")
        if extra:
            error(f"{name}: лишние узлы {extra} - правила их не пускают")

    users = rules["users"]["$nick"]
    want_req = required_children(users)
    got_req = list_of(extract_const(js, "USER_REQ"))
    if got_req != want_req:
        error(f"USER_REQ {got_req} != обязательные поля правил {want_req}")

    compare("users", spec_fields(extract_const(js, "USER_FIELDS")), users)
    compare("gifts", spec_fields(extract_const(js, "GIFT_FIELDS")), users["gifts"])

    slot = rules["rooms"]["$room"]["players"]["$slot"]
    got_req = list_of(extract_const(js, "SLOT_REQ"))
    if got_req != required_children(slot):
        error(f"SLOT_REQ {got_req} != обязательные поля слота из правил {required_children(slot)}")
    compare("slot", spec_fields(extract_const(js, "SLOT_FIELDS")), slot)

    chat = rules["rooms"]["$room"]["chat"]["$msg"]
    got_req = list_of(extract_const(js, "CHAT_REQ"))
    if got_req != required_children(chat):
        error(f"CHAT_REQ {got_req} != обязательные поля сообщения из правил {required_children(chat)}")
    compare("chat", spec_fields(extract_const(js, "CHAT_FIELDS")), chat)

    meta = rules["rooms"]["$room"]["meta"]
    got_req = list_of(extract_const(js, "META_REQ"))
    if got_req != required_children(meta):
        error(f"META_REQ {got_req} != обязательные поля meta из правил {required_children(meta)}")
    compare("meta", spec_fields(extract_const(js, "META_FIELDS")), meta)

    compare("banner", spec_fields(extract_const(js, "BANNER_FIELDS")), rules["banner"])

        # Event range: the rules allow 0..5, so the panel buttons must match.
    event_rule = rules["event"][".validate"]
    upper = re.search(r"<=\s*(\d+)", event_rule)
    panel_max = extract_const(js, "EVENT_MAX")
    if not upper or not panel_max:
        error("не удалось сверить диапазон ивента")
    elif panel_max.strip() != upper.group(1):
        error(f"EVENT_MAX = {panel_max.strip()}, а правила пускают 0..{upper.group(1)}")
    buttons = sorted(int(v) for v in re.findall(r'data-ev="(\d+)"', html))
    if buttons != list(range(int(upper.group(1)) + 1)):
        error(f"кнопки ивента {buttons} не покрывают 0..{upper.group(1)}")

        # Panel admins are the rule admins.
    admins = set(a.lower() for a in re.findall(r'"([a-z0-9]+@cb4\.game)"',
                                                extract_const(js, "ADMINS") or ""))
    rule_admins = set(re.findall(r"([a-z0-9]+@cb4\.game)", json.dumps(rules)))
    if admins and not admins <= rule_admins:
        error(f"панель считает админами {sorted(admins - rule_admins)}, которых нет в правилах")
    if not admins:
        error("в панели пустой список админов")

        # The password suffix must match the game one, or signing in fails silently.
    suffix = re.search(r'#define\s+FB_PASS_SUFFIX\s+"([^"]*)"',
                       (ROOT / "src" / "engine" / "network" / "http_json.inc").read_text(encoding="utf-8"))
    if suffix:
        known = list_of(extract_const(js, "KNOWN_SUFFIXES"))
        if known and known[0] != suffix.group(1):
            error(f"первый известный суффикс {known[0]!r} != FB_PASS_SUFFIX {suffix.group(1)!r}")


# ---------------------------------------------------------------------------
# The fake database run: without it the panel can be neither opened nor checked.
# ---------------------------------------------------------------------------
def free_port() -> int:
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def request(url: str, method: str = "GET", body=None, expect_json: bool = True):
    data = json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(url, data=data, method=method,
                                 headers={"Content-Type": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            raw = resp.read().decode()
            return resp.status, (json.loads(raw) if expect_json and raw else raw)
    except urllib.error.HTTPError as exc:
        return exc.code, exc.read().decode()


def check_fake_server() -> None:
    if not FAKE.exists():
        error("нет test/fake_firebase.py - тестовый режим панели не запустить")
        return
    port = free_port()
    base = f"http://127.0.0.1:{port}"
    env = dict(os.environ, FAKE_FB_QUIET="1")
    proc = subprocess.Popen([sys.executable, str(FAKE), str(port)], env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
    try:
        deadline = time.time() + 20
        alive = False
        while time.time() < deadline:
            if proc.poll() is not None:
                error(f"fake_firebase.py не запустился: {proc.stderr.read()[:400]}")
                return
            try:
                if request(f"{base}/__stats.json")[0] == 200:
                    alive = True
                    break
            except OSError:
                time.sleep(0.1)
        if not alive:
            error("fake_firebase.py не отвечает")
            return

        code, page = request(f"{base}/", expect_json=False)
        if code != 200 or '<button id="login"' not in page:
            error(f"подставная база не раздаёт панель: HTTP {code}")
        if request(f"{base}/.json")[0] != 403:
            error("корень базы читается без входа - правила не воспроизведены")
        if request(f"{base}/AzumBuff.json")[0] != 200:
            error("AzumBuff должен читаться всеми (\".read\": true)")

        code, body = request(f"{base}/v1/accounts:signInWithPassword?key=K", "POST",
                             {"email": "dimasi4ek229@cb4.game", "password": "admin|cb4v1"})
        if code != 200:
            error(f"админ не входит в тестовую базу: HTTP {code} {body}")
            return
        admin = body["idToken"]
        if request(f"{base}/.json?auth={admin}")[0] != 200:
            error("админ не читает корень")
        if request(f"{base}/event.json?auth={admin}", "PUT", 4)[0] != 200:
            error("админ не пишет event=4 (кубик)")
        if request(f"{base}/freeCupsHack.json?auth={admin}", "DELETE")[0] != 200:
            error("админ не удаляет корневой мусор")

        code, body = request(f"{base}/v1/accounts:signUp?key=K", "POST",
                             {"email": "player9@cb4.game", "password": "secret|cb4v1"})
        if code != 200:
            error(f"обычный игрок не создаётся в тестовой базе: HTTP {code}")
            return
        player = body["idToken"]
        if request(f"{base}/.json?auth={player}")[0] != 403:
            error("игрок читает корень - панель не проверит режим «снимок по узлам»")
        if request(f"{base}/event.json?auth={player}", "PUT", 1)[0] != 403:
            error("игрок пишет event - правила не воспроизведены")
        if request(f"{base}/users.json?auth={player}")[0] != 200:
            error("игрок не читает users")
        if request(f"{base}/users/Dimasi4ek229/cups.json?auth={player}", "PUT", 1)[0] != 403:
            error("игрок пишет чужой профиль")
        if request(f"{base}/users.json?auth=broken")[0] != 401:
            error("чужой токен не отклоняется (нужен 401)")
    finally:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()


def main() -> int:
    if not PANEL.exists():
        print("index.html не найден - веб-панель не возвращена в репозиторий", file=sys.stderr)
        return 1
    html = PANEL.read_text(encoding="utf-8")
    blocks = scripts_of(html)
    if len(blocks) < 2:
        error("в index.html меньше двух скриптов")
        return 1
    js = blocks[1]
    rules = json.loads(RULES.read_text(encoding="utf-8"))["rules"]

    print("Веб-панель: синтаксис")
    check_syntax(blocks)
    print("Веб-панель: сторож ошибок")
    check_guard(blocks)
    print("Веб-панель: кнопки и элементы")
    check_dom_wiring(html, js)
    print("Веб-панель: схема против firebase.rules.json")
    check_schema(html, js, rules)
    print("Подставная база: test/fake_firebase.py")
    check_fake_server()

    if ERRORS:
        print(f"\nВеб-панель: {len(ERRORS)} проблем(ы)", file=sys.stderr)
        return 1
    buttons = len(re.findall(r"<button\b", html))
    print(f"Веб-панель в порядке: {buttons} кнопок привязаны, схема совпадает с правилами")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
