# Удержание Backspace в NativeActivity

Родной ввод игры обрабатывает `ACTION_DOWN` один раз, поэтому долгое нажатие
Backspace стирало только один символ. Теперь повторение делается не потоком и
не `sleep`, а маленьким таймером в игровом цикле.

Чистая C99-структура лежит в `src/engine/input/backspace_repeat.h`:

```c
BackspaceRepeat r = {0, 0};

/* ACTION_DOWN: стереть один символ сразу и взвести задержку. */
backspace_repeat_down(&r, now_ms, erase_one_utf8, userdata);

/* Каждый кадр игрового цикла, now_ms из CLOCK_MONOTONIC. */
backspace_repeat_tick(&r, now_ms, erase_one_utf8, userdata);

/* ACTION_UP / ACTION_CANCEL / потеря фокуса: остановиться мгновенно. */
backspace_repeat_up(&r);
```

Внутри нужны только два поля:

- `held` — клавиша ещё зажата;
- `next_ms` — монотонный дедлайн следующего удаления.

Правила:

1. Первый `ACTION_DOWN` удаляет один символ и ставит `next_ms = now + 500`.
2. Пока `held == 1`, каждый кадр проверяет `now >= next_ms`; если да — удаляет
   ещё один символ и ставит `next_ms += 50`.
3. Если цикл надолго завис, пропущенные интервалы не удаляются залпом:
   `next_ms` переносится от текущего времени, чтобы не было burst-delete.
4. `ACTION_UP` сразу сбрасывает `held` и `next_ms`; повторные `ACTION_DOWN` от
   ОС, пока клавиша уже зажата, игнорируются.

Android-слой (`src/platform/android/keyboard.inc`) вызывает этот таймер из
`keyboard_tick()` раз в кадр `android_main.c`. `keyboard_hide`, `keyboard_clear`
и JNI `nativeKeyboardHidden` дополнительно отменяют повторение. Java-мост
(`GameActivity`) отдаёт KeyEvent-ы Backspace в C и потребляет их, чтобы EditText
не удалял символ второй раз; IME, которые удаляют через `InputConnection`,
продолжают работать средствами EditText. Старый «костыль» очистки всей строки
после шести быстрых удалений убран.

PC-слой использует тот же таймер (`keyboard_pc.inc`, `WM_KEYDOWN/WM_KEYUP`,
`WM_KILLFOCUS`). Тест: `CC=gcc python3 tools/test_backspace_repeat.py`.
