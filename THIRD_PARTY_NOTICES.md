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

- Оригинальные темы: `assets/audio/lobbymusic.wav`, `winter_jingle.wav`,
  `duel_theme.wav` и короткие звуки эффектов — созданы генераторами проекта;
  лицензия проекта (`LICENSE`).
- **Диско:** `assets/audio/disco_medusae_kevin_macleod.mp3` и исходник
  `third_party/music/Disco Medusae - Kevin MacLeod.mp3` —
  **«Disco Medusae», Kevin MacLeod (incompetech.com)**, ISRC USUAN1500041.
  Лицензия **CC BY 4.0**: https://creativecommons.org/licenses/by/4.0/
  Официальный источник:
  https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1500041
  Изменения: полная запись перекодирована в stereo MP3 32 kbps, 22050 Гц
  (885 568 байт); предыдущая обработка зеркала и контрольные суммы описаны в
  `third_party/music/README.md`. Это опубликованная запись, не генерация ИИ.
  Атрибуция: `assets/audio/Disco-Medusae-NOTICE.txt`; полный текст лицензии:
  `assets/audio/licenses/Disco-Medusae-CC-BY-4.0.txt`. Автор не подразумевает
  endorsement проекта. Ограничения проприетарной лицензии проекта не заменяют
  и не отменяют разрешения CC BY 4.0 для этой записи.

## 4. minimp3

- Файлы: `third_party/minimp3.h`, `third_party/minimp3-LICENSE.txt`
- Автор: lieff и участники проекта minimp3
- Лицензия: CC0 1.0 / public domain (текст рядом с заголовочным файлом)
- Назначение: декодирование сжатого MP3-трека диско в PCM при загрузке
- Источник: https://github.com/lieff/minimp3, commit
  `ea99364f61c14656440e8d77e9c233ccf3124633`

## 5. Платформенные библиотеки

- Android NDK (libandroid, liblog, libEGL, libvulkan): условия Android NDK
- Java/Kotlin-код платформы: стандартная библиотека Android

## 6. Сервисы

- Google Firebase Realtime Database и Firebase Authentication используются как
  облачное хранилище и сервис входа; обработка данных описана в
  [privacy_policy.md](docs/legal/privacy_policy.md)

## Требования о нарушении прав

Если вы считаете, что какой-либо материал нарушает ваши права, напишите в
обращения проекта: github.com/andreibioazum-cell/C-Larp. Материал будет снят
после проверки.
