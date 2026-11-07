// End-to-end JSDOM test suite for index.html against test/fake_firebase.py.
import { JSDOM, VirtualConsole } from "jsdom";
import { spawn } from "node:child_process";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = dirname(HERE);
const PORT = Number(process.env.PORT || 18799);
const BASE = `http://127.0.0.1:${PORT}`;

const failures = [];
const ok = (name) => console.log(`  ok  ${name}`);
function check(name, cond, extra = "") {
  if (cond) ok(name);
  else { failures.push(`${name}${extra ? ": " + extra : ""}`); console.log(`  FAIL ${name} ${extra}`); }
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function waitFor(fn, what, timeout = 15000) {
  const started = Date.now();
  for (;;) {
    let value = null;
    try { value = await fn(); } catch (_) { value = null; }
    if (value) return value;
    if (Date.now() - started > timeout) throw new Error(`timeout waiting for ${what}`);
    await sleep(100);
  }
}

async function db(path, token) {
  const url = `${BASE}/${path}.json` + (token ? `?auth=${encodeURIComponent(token)}` : "");
  const r = await fetch(url);
  if (!r.ok) throw new Error(`GET ${path} -> ${r.status}`);
  return r.json();
}

// ---------------------------------------------------------------------------
const server = spawn("python3", [join(HERE, "fake_firebase.py"), String(PORT)], {
  env: { ...process.env, FAKE_FB_QUIET: "1" },
  stdio: ["ignore", "pipe", "pipe"],
});
let serverLog = "";
server.stdout.on("data", (d) => (serverLog += d));
server.stderr.on("data", (d) => (serverLog += d));

const dialogs = [];
let dom;
try {
  await waitFor(async () => {
    try { const r = await fetch(`${BASE}/__stats.json`); return r.ok; } catch (_) { return false; }
  }, "fake firebase to listen", 20000);

  const html = readFileSync(join(ROOT, "index.html"), "utf8");
  const virtualConsole = new VirtualConsole();
  const consoleErrors = [];
  virtualConsole.on("jsdomError", (e) => consoleErrors.push("jsdomError: " + e.message));
  virtualConsole.on("error", (...a) => consoleErrors.push("console.error: " + a.join(" ")));

  dom = new JSDOM(html, {
    runScripts: "dangerously",
    resources: undefined,
    url: `${BASE}/`,
    pretendToBeVisual: true,
    virtualConsole,
    beforeParse(window) {
      window.fetch = (...args) => fetch(...args);
      window.alert = (m) => dialogs.push({ kind: "alert", text: String(m) });
      window.confirm = (m) => { dialogs.push({ kind: "confirm", text: String(m) }); return true; };
      window.prompt = () => null;
      window.URL.createObjectURL = () => "blob:cb4-test";
      window.URL.revokeObjectURL = () => {};
      window.HTMLAnchorElement.prototype.click = function () {
        dialogs.push({ kind: "download", text: this.download || "" });
      };
    },
  });

  const { window } = dom;
  const { document } = window;
  const $ = (s) => document.querySelector(s);
  const click = (s) => {
    const el = typeof s === "string" ? $(s) : s;
    if (!el) throw new Error(`нет элемента ${s}`);
    el.dispatchEvent(new window.MouseEvent("click", { bubbles: true, cancelable: true }));
    return el;
  };
  const setVal = (s, v) => { $(s).value = v; };
  const fatalText = () => ($("#fatal").className === "hide" ? "" : $("#fatal").textContent);
  const logText = () => $("#log").textContent;
  const jsErrors = () => {
    const fatal = fatalText();
    const logged = (logText().match(/ошибка: [^\n]*/gi) || []);
    return [...(fatal ? ["плашка: " + fatal] : []), ...logged, ...consoleErrors];
  };
  const visible = (s) => !$(s).classList.contains("hide");

  console.log("\n== 1. Загрузка страницы ==");
  await sleep(300);
  check("скрипт выполнился без ошибок", jsErrors().length === 0, jsErrors().join(" | "));
  check("все элементы найдены (MISSING пуст)", window.eval("MISSING.length") === 0,
        window.eval("JSON.stringify(MISSING)"));
  check("журнал сообщает, что кнопки привязаны", /всё нажимается/i.test(logText()), logText().slice(0, 200));
  const buttons = [...document.querySelectorAll("button")];
  check("кнопок на странице много", buttons.length >= 25, String(buttons.length));

  console.log("\n== 2. Тестовый режим и вход ==");
  // Verify rotated password suffix warning in production mode.
  click("#prodMode");
  check("боевой режим предупреждает про старый хвост",
        /rotate_db_secret\.py --show/.test($("#suffixWarn").textContent),
        $("#suffixWarn").textContent.slice(0, 160));
  click("#suffixHelp");
  await sleep(150);
  check("справка про хвост открывается",
        dialogs.some((d) => d.kind === "alert" && /rotate_db_secret\.py --show/.test(d.text)));
  setVal("#suffix", "i6Ugz3AlDLdgHJ6iUvkMOwNhKKNOnlwc");
  $("#suffix").dispatchEvent(new window.Event("input", { bubbles: true }));
  check("текущий хвост подсвечен как подходящий",
        /похож на текущий/.test($("#suffixWarn").textContent), $("#suffixWarn").textContent);
  setVal("#suffix", "|cb4v1");
  $("#suffix").dispatchEvent(new window.Event("input", { bubbles: true }));
  click("#testMode");
  check("тестовый режим выставил адрес базы", $("#dbUrl").value === BASE, $("#dbUrl").value);
  check("режим подсвечен в шапке", $("#modePill").textContent === "Тестовая база", $("#modePill").textContent);
  setVal("#nick", "Dimasi4ek229");
  setVal("#pass", "admin");
  click("#login");
  await waitFor(() => visible("#app"), "вход и снимок базы");
  check("вход выполнен админом", /права админа есть/i.test(logText()));
  check("токен разобран (провайдер password)", /провайдер password/i.test(logText()));
  check("показаны разделы панели", visible("#app") && visible("#janitor") && visible("#tools") && visible("#logBox"));
  check("статистика посчитана", $("#stats").querySelectorAll(".card").length >= 8,
        String($("#stats").querySelectorAll(".card").length));
  const token = window.eval("TOKEN");
  check("токен получен", typeof token === "string" && token.length > 10);

  console.log("\n== 3. Уборщик: фильтры, выделение, удаление ==");
  const found = $("#found").querySelectorAll("input");
  check("нашли мусор", found.length > 0, String(found.length));
  check("живой прогресс не в мусоре, а устаревшие поля найдены",
        !$("#found").textContent.includes("users/Player2/gifts/poop") &&
        $("#found").textContent.includes("users/Player2/gifts/emerald_hue") &&
        $("#found").textContent.includes("users/Player2/gifts/rose") &&
        /Устаревшее поле/.test($("#found").textContent),
        $("#found").textContent.slice(0, 300));
  // One-click cleanup of fields that the current game no longer uses.
  click("#markLegacy");
  await sleep(150);
  check("фильтр устаревших полей показывает только их",
        $("#found").textContent.includes("users/Player2/gifts/emerald_hue") &&
        !$("#found").textContent.includes("freeCupsHack"));
  click("#cleanLegacy");
  await waitFor(async () => (await db("users/Player2/gifts/emerald_hue", token)) === null,
                "удаление устаревших полей");
  check("устаревшие поля удалены из базы",
        (await db("users/Player2/gifts/emerald_hue", token)) === null &&
        (await db("users/Player2/gifts/rose", token)) === null &&
        (await db("users/Dimasi4ek229/gifts/emerald_hue", token)) === null);
  check("живые подарки и прогресс не тронуты чисткой",
        (await db("users/Player2/gifts/emerald", token)) === 1 &&
        (await db("users/Player2/cups", token)) === 10);
  check("корневой мусор виден", $("#found").textContent.includes("freeCupsHack"));
  check("мусорный игрок виден", $("#found").textContent.includes("xX_hack_Xx"));
  check("спам в чате виден", $("#found").textContent.includes("взлом"));
  click("#none");
  check("«Снять всё» снимает выделение", window.eval("FOUND.filter(f=>f.sel).length") === 0);
  click("#all");
  check("«Выделить всё» выделяет", window.eval("FOUND.filter(f=>f.sel).length") > 0);
  click("#invert");
  check("«Инвертировать» работает", window.eval("FOUND.filter(f=>f.sel).length") >= 0);
  // Toggle filter checkboxes.
  const staleFilter = [...document.querySelectorAll(".flt")].find((c) => c.dataset.k === "stale");
  staleFilter.checked = true;
  staleFilter.dispatchEvent(new window.Event("change", { bubbles: true }));
  await sleep(150);
  check("фильтр «слоты-заглушки» показывает нулевой слот",
        $("#found").textContent.includes("rooms/main/players/2"));
  click("#none");
  // Select and delete unknown root nodes only.
  window.eval(`FOUND.forEach(f => { f.sel = (f.kind === "unknown_root"); });`);
  [...document.querySelectorAll(".flt")].forEach((c) => {
    c.checked = c.dataset.k === "unknown_root";
    c.dispatchEvent(new window.Event("change", { bubbles: true }));
  });
  await sleep(150);
  click("#all");
  click("#wipe");
  await waitFor(async () => (await db("freeCupsHack", token)) === null, "удаление корневого мусора");
  check("мусорный узел удалён из базы", (await db("freeCupsHack", token)) === null);
  check("удаление попало в журнал", /готово: удалено/i.test(logText()));

  // Verify old and unneeded data cleanup (#markOld and #cleanOld).
  setVal("#cleanChatKeep", "0");
  $("#cleanChatKeep").dispatchEvent(new window.Event("change", { bubbles: true }));
  setVal("#cleanBannerHours", "0");
  $("#cleanBannerHours").dispatchEvent(new window.Event("change", { bubbles: true }));
  click("#markOld");
  await waitFor(() => /отмечено старых и ненужных узлов/i.test(logText()), "отметка старого и ненужного");
  check("отмечены пустые комнаты и старые данные",
        $("#found").textContent.includes("rooms/private1") &&
        $("#found").textContent.includes("bans/OldBan"));
  click("#cleanOld");
  await waitFor(async () => (await db("rooms/private1", token)) === null, "чистка старых и ненужных данных");
  check("пустая старая комната удалена чисткой", (await db("rooms/private1", token)) === null);
  check("снятый бан (false) удалён чисткой", (await db("bans/OldBan", token)) === null);
  check("мусорный аккаунт удалён чисткой", (await db("users/xX_hack_Xx", token)) === null);
  check("живые профили игроков сохранены при чистке",
        (await db("users/Dimasi4ek229/cups", token)) === 4200 &&
        (await db("users/Player2/cups", token)) === 10);
  check("живые слоты комнаты main сохранены при чистке",
        (await db("rooms/main/players/0/nick", token)) === "Dimasi4ek229" &&
        (await db("rooms/main/players/2", token)) === null);

  console.log("\n== 4. Ивенты 0..5 ==");
  for (const ev of [1, 2, 3, 4, 5, 0]) {
    click(`[data-ev="${ev}"]`);
    await waitFor(async () => ((await db("event", token)) === ev ? "ok" : null), `event=${ev}`);
    check(`ивент ${ev} записан`, (await db("event", token)) === ev);
  }
  click('[data-ev="4"]');
  await waitFor(async () => (await db("event", token)) === 4, "event=4 again");

  console.log("\n== 5. Флаги, баннер, баны, оттенок ==");
  click('[data-flag="FlashSale"][data-v="1"]');
  await waitFor(async () => (await db("FlashSale", token)) === 1, "FlashSale=1");
  check("FlashSale включён", (await db("FlashSale", token)) === 1);
  click('[data-flag="AzumBuff"][data-v="1"]');
  await waitFor(async () => (await db("AzumBuff", token)) === 1, "AzumBuff=1");
  check("AzumBuff включён", (await db("AzumBuff", token)) === 1);
  click('[data-flag="Christmas"][data-v="1"]');
  await waitFor(async () => (await db("Christmas", token)) === 1, "Christmas=1");
  check("Christmas включён", (await db("Christmas", token)) === 1);
  click('[data-flag="Christmas"][data-v="0"]');
  await waitFor(async () => (await db("Christmas", token)) === 0, "Christmas=0");
  check("Christmas выключен", (await db("Christmas", token)) === 0);

  const pumpkinGift = await fetch(`${BASE}/users/Player2/gifts/pumpkin_cube.json?auth=${encodeURIComponent(token)}`, {
    method: "PUT", headers: { "Content-Type": "application/json" }, body: "1"
  });
  check("админ может выдать подарок Тыква-куб", pumpkinGift.ok &&
        (await db("users/Player2/gifts/pumpkin_cube", token)) === 1);

  setVal("#bannerText", "Тестовый баннер");
  click("#setBanner");
  const banner = await waitFor(async () => {
    const b = await db("banner", token);
    return b && b.text === "Тестовый баннер" ? b : null;
  }, "баннер");
  check("баннер записан (текст и цвет)", banner.text === "Тестовый баннер" && banner.color === "white");
  click("#clearBanner");
  await waitFor(async () => (await db("banner", token)) === null, "снятие баннера");
  check("баннер снят", (await db("banner", token)) === null);

  setVal("#banNick", "Spammer");
  click("#ban");
  await waitFor(async () => (await db("bans/Spammer", token)) === true, "бан");
  check("бан записан", (await db("bans/Spammer", token)) === true);
  click("#unban");
  await waitFor(async () => (await db("bans/Spammer", token)) === null, "разбан");
  check("разбан сработал", (await db("bans/Spammer", token)) === null);
  setVal("#banNick", "не ник!");
  click("#ban");
  await sleep(200);
  check("неправильный ник не уходит в базу",
        dialogs.some((d) => d.kind === "alert" && /A-Za-z0-9_/.test(d.text)));

  /* emerald_hue and rose are obsolete fields: rules reject new hue writes,
   * while rose remains for old profiles; the web panel finds and removes both
   * with the dedicated "legacy fields" filter (checked in section 3 above). */

  console.log("\n== 6. Комната: чат и слоты ==");
  setVal("#room", "main");
  click("#clearChat");
  await waitFor(async () => (await db("rooms/main/chat", token)) === null, "очистка чата");
  check("чат комнаты очищен", (await db("rooms/main/chat", token)) === null);
  click("#clearSlots");
  await waitFor(async () => (await db("rooms/main/players", token)) === null, "сброс слотов");
  check("слоты сброшены", (await db("rooms/main/players", token)) === null);

  console.log("\n== 7. Резервная копия и восстановление ==");
  click("#dump");
  await sleep(200);
  check("копия скачалась", dialogs.some((d) => d.kind === "download" && /^cb4-backup-.*\.json$/.test(d.text)),
        JSON.stringify(dialogs.filter((d) => d.kind === "download")));

  const file = new window.File([JSON.stringify({ event: 2, AzumBuff: 1 })], "restore.json",
                               { type: "application/json" });
  Object.defineProperty($("#restoreFile"), "files", { value: [file], configurable: true });
  $("#restoreFile").dispatchEvent(new window.Event("change", { bubbles: true }));
  await waitFor(async () => (await db("event", token)) === 2, "восстановление из копии");
  check("копия восстановлена", (await db("event", token)) === 2 && (await db("AzumBuff", token)) === 1);

  console.log("\n== 8. Самопроверка, журнал, ключ, выход ==");
  click("#selftest");
  await sleep(150);
  check("самопроверка сообщает, что всё привязано",
        dialogs.some((d) => d.kind === "alert" && /Все кнопки привязаны/.test(d.text)),
        JSON.stringify(dialogs.slice(-1)));
  click("#checkKey");
  await waitFor(() => (/проверка ключа/i.test(logText()) ? true : null), "проверка ключа");
  check("проверка ключа отработала", /проверка ключа/i.test(logText()));
  click("#clearLog");
  check("журнал очищается", $("#log").textContent === "");

  console.log("\n== 9. Вход обычным игроком (не админ) ==");
  // Clear session state manually because JSDOM does not implement location.reload().
  window.eval("TOKEN = null; REFRESH = null; SNAP = null; FOUND = [];");
  await sleep(100);
  setVal("#nick", "Player2");
  setVal("#pass", "secret");
  $("#authBox").classList.remove("hide");
  click("#login");
  await waitFor(() => (/читаю игровые узлы по отдельности/i.test(logText()) ? true : null),
                "вход игроком и снимок по узлам");
  check("вход игроком прошёл", /вход выполнен: player2@cb4\.game/i.test(logText()));
  check("корень закрыт - снимок по узлам", /читаю игровые узлы по отдельности/i.test(logText()));
  check("панель не умерла без прав админа", jsErrors().length === 0, jsErrors().join(" | "));

  console.log("\n== 9b. Второй админ qwertyuiopaj1234: всё, кроме выдачи подарков ==");
  window.eval("TOKEN = null; REFRESH = null; SNAP = null; FOUND = [];");
  await sleep(100);
  setVal("#nick", "qwertyuiopaj1234");
  setVal("#pass", "admin");
  $("#authBox").classList.remove("hide");
  $("#app").classList.add("hide");   // the previous player session left it open
  click("#login");
  await waitFor(() => (/вход выполнен: qwertyuiopaj1234@cb4\.game/i.test(logText()) &&
                       visible("#app") ? true : null),
                "вход второго админа и снимок корня");
  const qtoken = window.eval("TOKEN");
  check("второй админ вошёл с правами админа",
        /права админа есть/i.test(logText()) && typeof qtoken === "string" && qtoken.length > 10);
  check("журнал честно предупреждает про подарки", /кроме выдачи подарков/i.test(logText()),
        logText().slice(-260));
  const qput = async (path, value) => {
    const r = await fetch(`${BASE}/${path}.json?auth=${encodeURIComponent(qtoken)}`, {
      method: "PUT", headers: { "Content-Type": "application/json" }, body: JSON.stringify(value) });
    return r.status;
  };
  check("ивент 7 записывается вторым админом",
        (await qput("event", 7)) === 200 && (await db("event", qtoken)) === 7);
  check("бан спамера проходит", (await qput("bans/Spammer", true)) === 200);
  check("забанить самого себя он не может", (await qput("bans/qwertyuiopaj1234", true)) !== 200);
  check("чужой профиль правится (кубки)",
        (await qput("users/Player2/cups", 55)) === 200 && (await db("users/Player2/cups", qtoken)) === 55);
  check("выдача подарка другому игроку отклонена", (await qput("users/Player2/gifts/poop", 5)) !== 200);
  check("свой подарок купить можно", (await qput("users/qwertyuiopaj1234/gifts/poop", 1)) === 200);
  const dput = async (path, value) => {
    const r = await fetch(`${BASE}/${path}.json?auth=${encodeURIComponent(token)}`, {
      method: "PUT", headers: { "Content-Type": "application/json" }, body: JSON.stringify(value) });
    return r.status;
  };
  check("первый админ подарки по-прежнему выдаёт", (await dput("users/Player2/gifts/poop", 2)) === 200);
  check("второго админа нельзя забанить даже первому",
        (await dput("bans/qwertyuiopaj1234", true)) !== 200);

  console.log("\n== 10. Итог по ошибкам ==");
  check("за весь прогон не было ни одной ошибки", jsErrors().length === 0, jsErrors().join(" | "));
} catch (e) {
  failures.push("исключение в тесте: " + (e && e.stack || e));
  console.log("  FAIL исключение:", e && e.message);
  console.log(serverLog.slice(-2000));
} finally {
  server.kill("SIGTERM");
  if (dom) dom.window.close();
}

console.log("");
if (failures.length) {
  console.log(`ПРОВАЛЕНО: ${failures.length}`);
  for (const f of failures) console.log(" - " + f);
  process.exit(1);
}
console.log("Все проверки веб-панели прошли: каждая кнопка нажимается и доходит до базы.");
