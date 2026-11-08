# Сторонние компоненты / Third-party notices

Игра «Cubic Battle 4» содержит перечисленные ниже сторонние компоненты. Они
распространяются на условиях собственных лицензий, которые имеют приоритет над
лицензией проекта в части этих компонентов. / The game bundles the components
below. Each one keeps its own license, which prevails over the project license
for that component.

## 1. stb_image

- Файл: `third_party/stb_image.h`
- Автор: Sean Barrett и участники проекта stb
- Лицензия: public domain (или MIT на выбор пользователя)
- Назначение: декодирование PNG-текстур и иконок

## 2. Шрифт Comic Relief

- Файлы: `assets/fonts/ComicRelief-Regular.ttf`, `ComicRelief-OFL.txt`,
  `ComicRelief-FONTLOG.txt`
- Лицензия: SIL Open Font License 1.1 (полный текст лежит рядом со шрифтом)
- Назначение: весь текст интерфейса игры
- Условия OFL соблюдены: шрифт не продаётся отдельно, имя файла шрифта не
  изменено, лицензия поставляется вместе со шрифтом

## 3. Музыка и звуки

- Файлы: `assets/audio/tracks/*.wav` (темы и диско-луп) и
  `assets/audio/sfx/*.wav` (короткие звуки)
- Автор: правообладатель проекта, `Дима Густенёв (Dimasi4ek229)`
- Как сделано: всё синтезировано генераторами `tools/generate_music.py`,
  `tools/generate_disco_loops.py` и `tools/gen_ability_sounds.py`; сэмплов,
  записей и чужих мелодий нет ни в одном файле
- Лицензия: проприетарная, та же, что у проекта (см. `LICENSE`); права на
  музыку и звуки принадлежат правообладателю
- Уведомление об авторских правах встроено в каждый WAV-файл: чанк RIFF
  `LIST/INFO` с полями `IART`, `ICOP`, `ICMT` (см. `tools/audio_rights.py`),
  поэтому принадлежность файла видна даже вне игры
- Сторонних музыкальных произведений в проекте нет: трек «Disco Medusae»
  (Kevin MacLeod, CC BY 4.0) из основной ветки в этот набор не входит

## 4. Платформенные библиотеки

- Android NDK (libandroid, liblog, libEGL, libvulkan): условия Android NDK
- Java/Kotlin-код платформы: стандартная библиотека Android

## 5. Сервисы

- Google Firebase Realtime Database и Firebase Authentication используются как
  облачное хранилище и сервис входа; обработка данных описана в
  [privacy_policy.md](docs/legal/privacy_policy.md)

## Требования о нарушении прав

Если вы считаете, что какой-либо материал нарушает ваши права, напишите в
обращения проекта: github.com/andreibioazum-cell/C-Larp. Материал будет снят
после проверки.
