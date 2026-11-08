#ifndef CB_BACKSPACE_REPEAT_H
#define CB_BACKSPACE_REPEAT_H

/* Pure C99 held-Backspace timer. The platform supplies a monotonic millisecond
 * clock and a callback that deletes exactly one UTF-8 codepoint. No threads,
 * sleeps or OS autorepeat are used here: call _tick once per game loop. */

#include <stdint.h>

#define BACKSPACE_REPEAT_DELAY_MS 500u
#define BACKSPACE_REPEAT_INTERVAL_MS 50u

typedef void (*BackspaceEraseFn)(void *userdata);

typedef struct {
    int held;                 /* 1 between ACTION_DOWN and ACTION_UP/CANCEL */
    uint64_t next_ms;         /* monotonic deadline of the next deletion */
} BackspaceRepeat;

static void backspace_repeat_init(BackspaceRepeat *r) {
    r->held = 0;
    r->next_ms = 0;
}

/* ACTION_DOWN: delete one symbol immediately and arm the 500 ms delay.
 * Repeated DOWN events from the OS while already held are ignored. */
static void backspace_repeat_down(BackspaceRepeat *r, uint64_t now_ms,
                                  BackspaceEraseFn erase, void *userdata) {
    if (r->held) {
        return;
    }
    r->held = 1;
    r->next_ms = now_ms + BACKSPACE_REPEAT_DELAY_MS;
    if (erase) {
        erase(userdata);
    }
}

/* ACTION_UP / CANCEL: stop instantly; a later tick must not delete anything. */
static void backspace_repeat_up(BackspaceRepeat *r) {
    r->held = 0;
    r->next_ms = 0;
}

/* Call every frame. After the first 500 ms, delete at most one symbol per
 * tick and keep a 50 ms cadence. If the loop stalled, do not burst-delete all
 * missed intervals; restart the cadence from the current time. */
static void backspace_repeat_tick(BackspaceRepeat *r, uint64_t now_ms,
                                  BackspaceEraseFn erase, void *userdata) {
    if (!r->held || now_ms < r->next_ms) {
        return;
    }
    if (erase) {
        erase(userdata);
    }
    r->next_ms += BACKSPACE_REPEAT_INTERVAL_MS;
    if (r->next_ms <= now_ms) {
        r->next_ms = now_ms + BACKSPACE_REPEAT_INTERVAL_MS;
    }
}

#endif
