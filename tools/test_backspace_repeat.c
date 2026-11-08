/* Host test for the pure C held-Backspace timer (500 ms delay, 50 ms repeat). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "engine/input/backspace_repeat.h"

static char text[64];
static int deletes;

static void erase_one(void *userdata) {
    size_t n;
    (void)userdata;
    deletes++;
    n = strlen(text);
    if (n > 0) {
        /* The test string is ASCII; the platform callback deletes UTF-8. */
        text[n - 1] = '\0';
    }
}

int main(void) {
    BackspaceRepeat r;
    backspace_repeat_init(&r);
    strcpy(text, "abcdefghij");
    deletes = 0;

    /* ACTION_DOWN deletes exactly one immediately. */
    backspace_repeat_down(&r, 1000, erase_one, NULL);
    assert(deletes == 1 && strcmp(text, "abcdefghi") == 0);
    /* OS autorepeat DOWN events while held must not double-delete. */
    backspace_repeat_down(&r, 1100, erase_one, NULL);
    assert(deletes == 1);

    backspace_repeat_tick(&r, 1499, erase_one, NULL);
    assert(deletes == 1); /* 499 ms held: still waiting */
    backspace_repeat_tick(&r, 1500, erase_one, NULL);
    assert(deletes == 2 && strcmp(text, "abcdefgh") == 0);
    backspace_repeat_tick(&r, 1549, erase_one, NULL);
    assert(deletes == 2);
    backspace_repeat_tick(&r, 1550, erase_one, NULL);
    assert(deletes == 3 && strcmp(text, "abcdefg") == 0);
    backspace_repeat_tick(&r, 1600, erase_one, NULL);
    assert(deletes == 4);

    /* ACTION_UP stops instantly; later ticks are silent. */
    backspace_repeat_up(&r);
    backspace_repeat_tick(&r, 5000, erase_one, NULL);
    assert(deletes == 4 && r.held == 0);

    /* A stalled loop must not burst-delete all missed 50 ms slots. */
    backspace_repeat_down(&r, 6000, erase_one, NULL);
    assert(deletes == 5);
    backspace_repeat_tick(&r, 9000, erase_one, NULL);
    assert(deletes == 6); /* one catch-up delete only */
    backspace_repeat_tick(&r, 9049, erase_one, NULL);
    assert(deletes == 6);
    backspace_repeat_tick(&r, 9050, erase_one, NULL);
    assert(deletes == 7);

    /* Cancel behaves like UP (focus loss / keyboard hidden). */
    backspace_repeat_up(&r);
    backspace_repeat_tick(&r, 99999, erase_one, NULL);
    assert(deletes == 7);
    puts("backspace repeat: immediate delete, 500 ms delay, 50 ms cadence, instant stop OK");
    return 0;
}
