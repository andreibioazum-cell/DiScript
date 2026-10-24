/* Draw five-point star using sub-pixel horizontal scanline strokes. */
static void draw_star(double cx, double cy, double r, uint32_t c) {
    double px[10], py[10], xs[10];
    double step = r / 48;
    double y = -r;
    int i = 0;
    if (r <= 0) {
        return;
    }
    if (step < 0.2) {
        step = 0.2;
    }
    if (step > 0.75) {
        step = 0.75;
    }
    while (i < 10) {
        double a = -pi / 2 + i * pi / 5;
        double point_r = i % 2 == 0 ? r : r * 0.45;
        px[i] = cos(a) * point_r;
        py[i] = sin(a) * point_r;
        i++;
    }
    while (y <= r) {
        int count = 0;
        i = 0;
        while (i < 10) {
            int j = (i + 1) % 10;
            double y1 = py[i];
            double y2 = py[j];
            if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                double x = px[i] + (px[j] - px[i]) * (y - y1) / (y2 - y1);
                int pos = count;
                while (pos > 0 && xs[pos - 1] > x) {
                    xs[pos] = xs[pos - 1];
                    pos--;
                }
                xs[pos] = x;
                count++;
            }
            i++;
        }
        i = 0;
        while (i + 1 < count) {
            line(cx + xs[i], cy + y, cx + xs[i + 1], cy + y, step + 0.35, c);
            i = i + 2;
        }
        y = y + step;
    }
}

static double class_imbalance_of(double cls) {
    if (cls == CLASS_CODECRACKER) {
        return 5;
    }
    if (cls == CLASS_ASTRA) {
        return 4;
    }
    if (cls == CLASS_AZUM || cls == CLASS_EBUC) {
        return 3;
    }
    if (cls == CLASS_SANTA) {
        return 2;
    }
    return 1;
}

static double stat_bar_w(void) { return fit_w(520); }
static double stat_star_r(void) { return clamp(stat_bar_w() / 13.6, 4, 26); }

static void draw_imbalance_stars(void) {
    double rating = class_imbalance_of(player_class);
    double r = stat_star_r();
    double w = stat_bar_w();
    double gap = r * 0.9;
    double sx = center_x(w) + (w - (5 * r * 2 + 4 * gap)) / 2 + r;
    double cy = stat_motto_y() - 48;
    double i = 0;
    while (i < 5) {
        double cx = sx + i * (r * 2 + gap);
        if (i < rating) {
            draw_star(cx, cy, r * 1.1, 0xFFF7C948);
        } else {
            draw_star(cx, cy, r, 0xFF0B0B12);
        }
        i = i + 1;
    }
}

