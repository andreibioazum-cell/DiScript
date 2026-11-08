## RU

Полная версия Cubic Battle 4 для Android. Игра написана на C, и немного на Java.

- Минимум: Android 8
- Цель: Android 15
- Требуется: Vulkan 1.0

## ПК-версия (Windows .exe)

Та же игра, что в APK, но для компьютера: окно, мышь и клавиатура.

* **Скачать:** артефакт `Cubic-Battle-4-PC-Windows` из GitHub Actions -
  `CubicBattle4.exe` плюс папка `assets/` рядом. Установка не нужна.
* **Собрать:** `pip install ziglang && python3 tools/build_pc.py --windows`
  (Zig собирает exe и на Linux, MSVC не нужен).
* **Управление:** `WASD`/стрелки - движение, `ЛКМ` - удар, `E` - блок
  (удерживать), `Q` - суперудар, мышь - меню и чат, `Esc` - назад, `F12` -
  скриншот.

Логика игры общая с Android (`src/game`, `src/engine`); ПК-слой - окно,
программный растеризатор вместо Vulkan, WinHTTP вместо JNI и перевод
клавиатуры в касания. Подробности: [docs/pc-version.md](docs/pc-version.md).

## Музыка

Три оригинальных трека: обычная и новогодняя темы лобби - по **70 секунд**,
новая тема боя Азума против Астры - **45 секунд**. Старая песня
`azum_vs_astra.wav` и исходный mp3 удалены; вся музыка собирается из нот
скриптом `python3 tools/generate_music.py`. Подробности: [docs/music.md](docs/music.md).

## Диско-лупы (10 треков по 30 секунд)

В папке `disco/` лежат десять **оригинальных** диско-лупов по 30 секунд,
сгенерированных этим репозиторием: без сэмплов, без сторонних библиотек, без
чужих мелодий. Каждый файл - бесшовная петля, стык попадает на первую долю.

* **Слушать:** открой `disco/index.html` (проигрыватель с повтор-режимом).
* **Пересобрать:** `python3 tools/generate_disco_loops.py` (только Python 3,
  около 90 секунд на все десять; `--list` покажет темпы и такты).
* **Проверить:** `python3 tools/test_disco_loops.py` (формат, длительность,
  целое число тактов, отсутствие щелчка на стыке, уровень).
* **Посмотреть:** `python3 tools/disco_preview.py` - осциллограммы всех десяти
  лупов в `disco/preview.png`.

Лупы намеренно лежат вне `assets/`, поэтому не попадают ни в APK, ни в
Windows-сборку. Список треков, тональности и устройство синтеза:
[docs/disco-loops.md](docs/disco-loops.md).

## Установка APK

APK лежит в релизе `latest-build`: `CubicBattle4.apk`, рядом файл
`CubicBattle4.apk.sha256`. Если телефон пишет «Пакет повреждён», значит файл
докачался не полностью: скачай заново и сверь контрольную сумму на компьютере

    python3 tools/check_apk.py CubicBattle4.apk

Скрипт проверяет целостность архива, обязательные файлы, подписи v1 и v2 и
печатает sha256. Строка из `.sha256` в релизе должна совпадать. Через кабель
ошибку установки видно точнее: `adb install -r CubicBattle4.apk`.

## Текстурпаки

В левом верхнем углу лобби есть кнопка **Паки**. Текстурпак - это `.zip` или
папка в `<данные игры>/packs/<имя>/`, где лежат файлы с теми же именами, что в
`assets`: `textures/`, `sounds/`, `fonts/`. Включённый пак подменяет любую
картинку, звук и шрифт игры без пересборки: выбор хранится в
`packs/active.txt` и применяется сразу, картинки перечитываются на месте.
Импорт идёт через файловый менеджер телефона (SAF); на ПК - через встроенный
проводник. Паки делает сам игрок, поэтому распространять в них материалы игры
нельзя (см. [LICENSE](LICENSE)).

## Telegram-бот

`bot/cb4_bot.py` - бот-компаньон: кубки раз в час, новости, премиум и
админ-команды. Токен берётся из `CB4_BOT_TOKEN`, база - локальная SQLite.

```
pip install -r bot/requirements.txt
export CB4_BOT_TOKEN="123456:AA..."
python3 bot/cb4_bot.py
```

У игрока `Sanch` ник золотой, поэтому премиум в боте у него уже есть - заявку он
не подаёт и звёзды не платит. Подробности и канон имени класса `буК` (род. `буКа`,
англ. `ebuC` / `ebuC's`) - в [bot/README.md](bot/README.md).

## Веб-панель админа

Кроме APK в репозитории есть веб-версия - [index.html](index.html): панель
админа для базы Firebase (вход по нику и паролю от игры, снимок базы, поиск и
удаление мусора, ивенты 0..5, баннер, флаги магазина, `Christmas` (снежки
для всех классов), баны, чистка чата и слотов, резервная копия). Сборка не нужна: открой файл в браузере.

Потренироваться без боевой базы можно так:

```
python3 test/fake_firebase.py 8080
```

затем открой <http://localhost:8080/>, нажми «Тестовый режим» и войди как
`Dimasi4ek229` с паролем `admin`. Подробности, список починенных поломок и
проверки - в [docs/admin-panel.md](docs/admin-panel.md).

Панель умеет чистить базу от **полей старых версий** - например,
`gifts/emerald_hue` и `gifts/rose`, которые игра уже не использует. Оттенок
`emerald_hue` уже удалён из Firebase rules; отдельный фильтр «Устаревшие поля»
удаляет такие остатки из профилей, не трогая живой прогресс.

## Правовая информация

Возрастная категория 12+. Пользовательское соглашение, политика обработки
персональных данных и согласие на обработку лежат в [docs/legal](docs/legal);
там же список того, что владелец должен сделать вне кода (уведомление
Роскомнадзора, локализация данных и т. д.). Правовые тексты также встроены в
игру: экран «Правовая информация» с лицензией, правами и согласием на
обработку данных. Сторонние компоненты и их лицензии перечислены в
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Лицензия

Copyright (C) 2026 Дима Густенёв

Все права защищены. Код и материалы проекта нельзя копировать, использовать,
изменять или распространять без письменного разрешения правообладателя.
Полные условия - в файле [LICENSE](LICENSE).

SPDX-License-Identifier: LicenseRef-Proprietary

## EN

Full version of Cubic Battle 4 for Android. The game is written in C, with a bit of Java.

- Minimum: Android 8
- Target: Android 15
- Requires: Vulkan 1.0

## PC version (Windows .exe)

The same game as in the APK, on a desktop: a window, the mouse and the keyboard.

* **Download:** the `Cubic-Battle-4-PC-Windows` artifact from GitHub Actions -
  `CubicBattle4.exe` with the `assets/` folder next to it. No installer.
* **Build:** `pip install ziglang && python3 tools/build_pc.py --windows`
  (Zig cross-builds the exe on Linux too; MSVC is not needed).
* **Controls:** `WASD`/arrows to move, `LMB` to punch, `E` to block (hold),
  `Q` for the class super, mouse for menus and chat, `Esc` back, `F12`
  screenshot.

The game logic is shared with Android (`src/game`, `src/engine`); the PC layer
is the window, a software rasterizer instead of Vulkan, WinHTTP instead of JNI
and a keyboard-to-touch bridge. Details: [docs/pc-version.md](docs/pc-version.md).

## Texture packs

The lobby has a small **Packs** button in the top-left corner. A texture pack is
a `.zip` or a folder in `<game data>/packs/<name>/` holding files named like the
ones in `assets`: `textures/`, `sounds/`, `fonts/`. The active pack replaces any
picture, sound or font of the game without a rebuild: the choice is kept in
`packs/active.txt` and applied at once, textures are reloaded in place. Import
goes through the phone file manager (SAF) on Android and through the built-in
file manager on PC. Packs are made by players, so they must not redistribute the
game materials (see [LICENSE](LICENSE)).

## Music

Three original tracks: the ordinary and New Year lobby themes are **70 seconds**
each, and the new Azum-vs-Astra duel theme is **45 seconds**. The old
`azum_vs_astra.wav` song and its source mp3 are gone; every track is rebuilt
from notes with `python3 tools/generate_music.py`. Details: [docs/music.md](docs/music.md).

## Web admin panel

Besides the APK the repository contains a web version - [index.html](index.html):
the Firebase admin panel (sign in with the game nick and password, database
snapshot, junk search and cleanup, events 0..5, banner, shop flags, `Christmas`
(snowballs for every class), bans, chat and slot cleanup, backup copy). No build step: open the file in a browser.

To practise without touching the live database run

```
python3 test/fake_firebase.py 8080
```

then open <http://localhost:8080/>, press the test-mode button and sign in as
`Dimasi4ek229` with the password `admin`. The panel also cleans obsolete fields
from older game versions (`gifts/emerald_hue`, `gifts/rose`) with the dedicated
legacy-field filter and button; `emerald_hue` is no longer accepted by Firebase
rules. Details in [docs/admin-panel.md](docs/admin-panel.md).

## Legal

Age rating 12+. The Terms of Use, the personal data policy and the consent
form (in Russian) are in [docs/legal](docs/legal).

## License

Copyright (C) 2026 Дима Густенёв

All rights reserved. The code and materials of this project may not be
copied, used, modified or distributed without the copyright holder's written
permission. Full terms in the [LICENSE](LICENSE) file.

SPDX-License-Identifier: LicenseRef-Proprietary
