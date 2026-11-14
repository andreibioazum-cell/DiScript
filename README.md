# DiScript

Редактор кода для языка DiScript — на ПК (Windows) и на Android.

Это не игра: приложение открывает `.ds`-файлы, редактирует их с подсветкой
синтаксиса и запускает прямо внутри себя, показывая результат в панели
вывода.

## Что умеет

* **Редактор**: вкладки, номера строк, подсветка синтаксиса DiScript
  (ключевые слова, встроенные функции, числа, строки, комментарии, имена
  функций и классов), отмена/повтор, автоотступ после `:`, Tab, поиск
  слова курсором (`Ctrl+←/→`).
* **Запуск**: скрипт исполняется виртуальной машиной DiScript (JIT), а всё,
  что печатает `print`, попадает в панель вывода вместе с ошибками
  компиляции и их номерами строк.
* **Файлы**: открытие и сохранение через системный диалог.
* **3D как в Studio**: скрипт строит мир из деталей (`Part`), моделей и
  групп, раскрашивает их, вкладывает друг в друга и анимирует. Сцена
  рисуется рядом с кодом — камера наводится сама, мышью вращается и
  приближается. Подробности и примеры: [docs/studio.md](docs/studio.md).
* **2D-игры из картинок**: проект — обычная папка, кнопка «+» добавляет
  скрипт или картинку (галерея телефона), скрипт с `update`/`draw` работает
  каждый кадр. Готовый пример: [examples/lobby](examples/lobby). Как всё
  устроено: [docs/sprites.md](docs/sprites.md).

## Скачать

* **Android**: `DiScript.apk` из раздела releases (тег `latest-build`).
  Минимум Android 8, нужен Vulkan 1.0.
* **Windows**: `DiScript.exe` — один самодостаточный файл, шрифт встроен
  внутрь, установка не нужна. Рядом лежит `DiScript-Windows.zip` с папкой
  ресурсов.

## Горячие клавиши (ПК)

| Клавиши | Действие |
| --- | --- |
| `Ctrl+S` / `Ctrl+Shift+S` | сохранить / сохранить как |
| `Ctrl+O` | открыть |
| `Ctrl+N` | новый файл |
| `Ctrl+R` или `F5` | запустить |
| `Ctrl+Z` / `Ctrl+Y` | отменить / повторить |
| `Ctrl+←` / `Ctrl+→` | по словам |
| `Ctrl+Home` / `Ctrl+End` | начало / конец файла |
| `Tab` | отступ (с выделением — сдвиг блока) |
| колесо мыши | прокрутка |

На Android то же самое через системную клавиатуру, а файлы открываются и
сохраняются через системный диалог.

## Сцена за пять минут

```ds
base = scene.part("Base", "block")
base.size(20, 1, 20)
base.color(110, 130, 100)

house = scene.model("House")
wall = scene.part("Wall", "block")
wall.parent(house)
wall.size(4, 3, 4)

roof = scene.part("Roof", "cone")
roof.parent(house)
roof.move(0, 2.4, 0)
roof.spin(45)          # вертушка
```

Деталь умеет `move`, `rotate`, `size`, `color`, `transparency`, `visible`,
`parent`, `remove`, `animate` и `spin`; `position()` отдаёт мировые
координаты с учётом родителей. Готовый пример —
[examples/studio_house.ds](examples/studio_house.ds).

## Сборка

**Windows** (Zig собирает exe даже на Linux, MSVC не нужен):

```bash
pip install ziglang
python3 tools/build_pc.py --windows
```

**Проверка без окна** (headless-харнесс: набирает текст, запускает скрипт,
печатает содержимое буфера и панели вывода):

```bash
python3 tools/build_pc.py --linux
./build/pc/discript_headless --headless --assets build/pc/assets \
  --type "4:print('hi')" --run 20 --report 25
```

**Android**: CI (`.github/workflows/main.yml`) собирает APK через NDK.
Локально — `python3 stage_assets.py` плюс CMake с Android-тулчейном.

## Устройство

```
src/ide/        редактор: буфер, подсветка, отрисовка, логика приложения
src/discript/   язык: лексер, парсер, компилятор, VM, GC, стандартная библиотека
src/engine/     движок: Vulkan/software-рендер, шрифты, звук
src/platform/   платформенные слои: pc/ (окно Windows) и android/ (native activity)
assets/         только шрифт DejaVuSansMono
```

Игровой режим «Кубик Батл 4», из которого вырос этот репозиторий, больше не
входит в `main`; он сохранён тегом `cb4-final`, если понадобится.
