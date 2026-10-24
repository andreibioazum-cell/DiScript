/* Battle screen FX: edge vignette, damage flash, and motion-blur tracking. */

/* Draw soft-edged rectangular band using multi-tap blur offsets. */
static void vignette_band(double x, double y, double w, double h, double a, double blur) {
    const double kw[3] = {0.25, 0.09, 0.06};
    const double kr[3] = {0.0, 0.55, 1.0};
    int ring = 0;
    while (ring < 3) {
        double rr = blur * kr[ring];
        unsigned ai = (unsigned)(int)(a * kw[ring] * 255.0 + 0.5);
        if (ai >= 1) {
            uint32_t c = ((uint32_t)ai) << 24;
            if (ring == 0) {
                rect(x, y, w, h, c);
            } else {
                double i = 0;
                while (i < 6) {
                    double ang = i * pi / 3;
                    rect(x + cos(ang) * rr, y + sin(ang) * rr, w, h, c);
                    i = i + 1;
                }
            }
        }
        ring = ring + 1;
    }
}

/* Draw dark edge vignette during active ultimates. */
static void draw_vignette(void) {
    int steps = 8;
    double sx = screen_w * 0.08 / steps;
    double sy = screen_h * 0.08 / steps;
    double tw = sx * 1.4;
    double th = sy * 1.4;
    int i = 0;
    while (i < steps) {
        double k = (steps - 1 - i) / (double)(steps - 1);
        double a = 0.26 * k * k;
        if (a > 0.002) {
            double x = i * sx;
            double y = i * sy;
            double w = screen_w - 2 * x;
            double h = screen_h - 2 * y;
            vignette_band(x, y, w, th, a, sy * 1.8);
            vignette_band(x, y + h - th, w, th, a, sy * 1.8);
            vignette_band(x, y, tw, h, a, sx * 1.8);
            vignette_band(x + w - tw, y, tw, h, a, sx * 1.8);
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
