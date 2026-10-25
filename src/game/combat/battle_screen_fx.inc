/* Battle screen FX: edge vignette, damage flash, and motion-blur tracking. */

/* Draw non-overlapping concentric edge vignette bands around the arena. */
static void draw_vignette(void) {
    int steps = 10;
    double sx = screen_w * 0.085 / steps;
    double sy = screen_h * 0.085 / steps;
    int i = 0;
    while (i < steps) {
        double k = (steps - i) / (double)steps;
        unsigned ai = (unsigned)(int)(0.24 * k * k * 255.0 + 0.5);
        if (ai >= 1) {
            uint32_t c = ((uint32_t)ai) << 24;
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

/* Peak red damage-flash opacity; skip zero alpha since engine treats 0 as opaque. */
#define HURT_FLASH_ALPHA 0.07
#define HURT_FLASH_RGB 0xB04848

static void draw_hurt_flash(void) {
    unsigned a = (unsigned)(int)(HURT_FLASH_ALPHA * player_hurt_a * 255.0 + 0.5);
    if (a < 1) {
        return;
    }
    rect(0, 0, screen_w, screen_h, (((uint32_t)a) << 24) | HURT_FLASH_RGB);
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
