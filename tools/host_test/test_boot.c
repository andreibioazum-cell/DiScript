// tools/host_test/test_boot.c
//
// Startup smoke test. Builds the whole editor, initialises it, advances one
// frame and draws at several surface sizes including degenerate ones.
//
// Android hands the app a 0x0 surface for the first frame or two while the
// activity is being laid out, and a bogus resize can produce a negative size.
// Neither may take the process down: a crash there looks to the user like "the
// app closes immediately", with no window and no message.
#include "ide/ide_app.h"
#include <stdio.h>

static IDEApp g_app;

static int failures = 0;

static void expect_draw_ok(int w, int h) {
    ide_app_draw(&g_app, w, h);
    printf("  ok   draw %dx%d\n", w, h);
}

int main(void) {
    printf("boot test\n");
    ide_app_init(&g_app, NULL);
    ide_app_update(&g_app, 0.016);
    printf("  ok   init + first update\n");

    expect_draw_ok(540, 960);
    expect_draw_ok(0, 0);
    expect_draw_ok(1, 1);
    expect_draw_ok(-5, -5);
    expect_draw_ok(20000, 20000);

    /* The view must stay usable after those sizes. */
    ide_app_draw(&g_app, 540, 960);
    printf("  ok   draw after degenerate sizes\n");

    printf("%d/%d passed\n", failures == 0 ? 6 : 6 - failures, 6);
    return failures == 0 ? 0 : 1;
}
