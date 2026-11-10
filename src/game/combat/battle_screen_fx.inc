/* Battle screen FX: edge vignette, damage flash, and motion-blur tracking. */

/* Draw non-overlapping concentric edge vignette bands around the arena. */
static void draw_vignette(void) {
    /* New Year keeps the same bands but in white snow light instead of black. */
    double winter = winter_theme_on();
    double peak = winter == 1 ? 0.46 : 0.24;
    uint32_t rgb = winter == 1 ? 0xFFFFFFu : 0x000000u;
    int steps = 10;
    double sx = screen_w * 0.085 / steps;
    double sy = screen_h * 0.085 / steps;
    int i = 0;
    while (i < steps) {
        double k = (steps - i) / (double)steps;
        unsigned ai = (unsigned)(int)(peak * k * k * 255.0 + 0.5);
        if (ai >= 1) {
            uint32_t c = (((uint32_t)ai) << 24) | rgb;
            double x = i * sx;
            double y = i * sy;
            double w = screen_w - 2 * x;
            double h = screen_h - 2 * y;
            double ih = h - 2 * sy;
            rect(x, y, w, sy, c);
            rect(x, y + h - sy, w, sy, c);
            if (ih > 0) {
                rect(x, y + sy, sx, ih, c);
                rect(x + w - sx, y + sy, sx, ih, c);
            }
        }
        i = i + 1;
    }
}

/* Draw pre-HUD screen FX. */
static void draw_screen_fx(void) {
    draw_vignette();
}

/* Record per-frame fighter displacement for motion-blur trails. */
static void motion_blur_track(void) {
    if (mb_ok == 0) {
        mb_px = player->x;
        mb_py = player->y;
        mb_ex = enemy->x;
        mb_ey = enemy->y;
        mb_dx = 0;
        mb_dy = 0;
        mb_edx = 0;
        mb_edy = 0;
        mb_ok = 1;
        return;
    }
    mb_dx = player->x - mb_px;
    mb_dy = player->y - mb_py;
    mb_edx = enemy->x - mb_ex;
    mb_edy = enemy->y - mb_ey;
    mb_px = player->x;
    mb_py = player->y;
    mb_ex = enemy->x;
    mb_ey = enemy->y;
}
