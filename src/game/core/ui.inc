static double shade(double rgb, double k) {
    double a = floor(rgb / 16777216);
    double body = rgb - a * 16777216;
    double r = floor(body / 65536);
    double g = floor(body / 256) - r * 256;
    double b = body - floor(body / 256) * 256;
    return a * 16777216 + floor(r * k) * 65536 + floor(g * k) * 256 + floor(b * k);
}

static double blend_color(double a, double b, double t) {
    double aa = floor(a / 16777216);
    double ar = floor((a - aa * 16777216) / 65536);
    double ag = floor((a - aa * 16777216) / 256) - ar * 256;
    double ab = (a - aa * 16777216) - floor((a - aa * 16777216) / 256) * 256;
    double ba = floor(b / 16777216);
    double br = floor((b - ba * 16777216) / 65536);
    double bg = floor((b - ba * 16777216) / 256) - br * 256;
    double bb = (b - ba * 16777216) - floor((b - ba * 16777216) / 256) * 256;
    double ra = aa + (ba - aa) * t;
    double rr = ar + (br - ar) * t;
    double rg = ag + (bg - ag) * t;
    double rb = ab + (bb - ab) * t;
    return floor(ra) * 16777216 + floor(rr) * 65536 + floor(rg) * 256 + floor(rb);
}

static double locked_bg(void) { return shade(ui_purple, locked_shade); }

static double locked_text(void) { return locked_text_alpha * 16777216 + text_color; }

static double fade_color(double rgb, double t, double fade) {
    double a = clamp(t / fade, 0, 1);
    /* Smoothstep: message text eases in and out instead of ramping linearly. */
    a = a * a * (3 - 2 * a);
    return (1 + floor(a * 254)) * 0x1000000 + rgb;
}

static double hit_rect(double px, double py, double x, double y, double w, double h) {
    if (px >= x && px <= x + w && py >= y && py <= y + h) {
        return 1;
    }
    return 0;
}

static void ctext(const char *s, double y, uint32_t c) { text(s, (screen_w - text_ink_width(s)) / 2, y, c); }

static void ctext_scaled(const char *s, double y, uint32_t c, double scale) {
    text_scaled(s, (screen_w - text_ink_width(s) * scale) / 2, y, c, scale);
}

static void ctext_fade(const char *s, double y, uint32_t rgb, double t, double fade) {
    ctext(s, y, fade_color(rgb, t, fade));
}

static void text_in_box(const char *label, double x, double y, double w, double h, uint32_t c, double scale) {
    double tw = text_ink_width(label) * scale;
    double th = text_ink_height(label) * scale;
    double tt = text_ink_top(label) * scale;
    text_scaled(label, x + (w - tw) / 2, y + (h - th) / 2 - tt + 1.5, c, scale);
}

static void text_in_field(const char *label, double x, double y, double h, uint32_t c, double scale) {
    double th = text_ink_height(label) * scale;
    double tt = text_ink_top(label) * scale;
    text_scaled(label, x, y + (h - th) / 2 - tt + 1.5, c, scale);
}

static double fit_text_scale(const char *s, double w, double max_scale) {
    double tw = text_ink_width(s);
    if (tw <= 0 || w <= 0) {
        return max_scale;
    }
    double sc = w / tw;
    if (sc > max_scale) {
        sc = max_scale;
    }
    return sc;
}

static void centered_text(const char *label, double x, double w, double y, uint32_t c, double scale) {
    double tw = text_ink_width(label) * scale;
    text_scaled(label, x + (w - tw) / 2, y, c, scale);
}

/* Draw cups and candies counters side by side. */
static void money_pair(const char *left, const char *right, double x, double y, double scale, double gap) {
    double lw = text_ink_width(left) * scale;
    text_scaled(left, x, y, text_color, scale);
    text_scaled(right, x + lw + gap, y, text_color, scale);
}

static void draw_styled_nick(const char *nick, double x, double y, uint32_t base, double scale) {
    uint32_t col = admin_color_of(nick, base);
    if (nick_shimmers(nick) == 1) {
        text_shimmer(nick, x, y, col, scale, gift_shimmer_time());
        return;
    }
    text_scaled(nick, x, y, col, scale);
}

static void draw_nick_msg(const char *nick, const char *rest, double x, double y, uint32_t base, double scale) {
    double nw = text_ink_width(nick) * scale;
    draw_styled_nick(nick, x, y, base, scale);
    text_scaled(rest, x + nw, y, base, scale);
}

/* Draw button fill: gradient in new theme, flat rounded rect in classic theme. */
static void button_face(double x, double y, double w, double h, double col) {
    if (new_theme == 1) {
        cb_button_gradient(x, y, w, h, btn_radius, col);
        return;
    }
    roundrect(x, y, w, h, btn_radius, col);
}

static void button(const char *label, double x, double y, double w, double h) {
    button_fixed(label, x, y, w, h, ui_purple);
}

/* Draw button with custom face color. */
static void button_fixed(const char *label, double x, double y, double w, double h, double face) {
    double p = button_press_k(x, y, w, h);
    double s = 1 - PRESS_SQUEEZE * p;
    double bx = x + w * (1 - s) / 2;
    double by = y + h * (1 - s) / 2;
    double bw = w * s;
    double bh = h * s;
    double col = shade(face, 1 - PRESS_SHADE * p);
    button_face(bx, by, bw, bh, col);
    text_in_box(label, bx, by, bw, bh, shade(text_color, 1 - PRESS_SHADE * p), s);
}

/* Draw disabled button without press animation. */
static void button_disabled_scaled(const char *label, double x, double y, double w, double h, double scale) {
    button_face(x, y, w, h, 0xFF6E6E76);
    text_in_box(label, x, y, w, h, 0xFFB4B4BC, scale);
}

static void button_scaled(const char *label, double x, double y, double w, double h, double scale) {
    button_scaled_color(label, x, y, w, h, scale, text_color);
}

static void button_scaled_color(const char *label, double x, double y, double w, double h, double scale, uint32_t color) {
    double p = button_press_k(x, y, w, h);
    double s = 1 - PRESS_SQUEEZE * p;
    double bx = x + w * (1 - s) / 2;
    double by = y + h * (1 - s) / 2;
    double bw = w * s;
    double bh = h * s;
    double col = shade(ui_purple, 1 - PRESS_SHADE * p);
    button_face(bx, by, bw, bh, col);
    text_in_box(label, bx, by, bw, bh, shade(color, 1 - PRESS_SHADE * p), scale * s);
}

/* Draw scaled button with custom face color. */
static void button_scaled_face(const char *label, double x, double y, double w, double h, double scale, uint32_t face) {
    double p = button_press_k(x, y, w, h);
    double s = 1 - PRESS_SQUEEZE * p;
    double bx = x + w * (1 - s) / 2;
    double by = y + h * (1 - s) / 2;
    double bw = w * s;
    double bh = h * s;
    double col = shade(face, 1 - PRESS_SHADE * p);
    button_face(bx, by, bw, bh, col);
    text_in_box(label, bx, by, bw, bh, shade(text_color, 1 - PRESS_SHADE * p), scale * s);
}

/* Compute squeezed x/y coordinates around panel center. */
static double squeeze_x(double x, double w, double s) { return x + w * (1 - s) / 2; }

static double squeeze_y(double y, double top, double h, double s) { return top + h * (1 - s) / 2 + (y - top) * s; }

static void roundrect_outline(double x, double y, double w, double h, double rad, double t, uint32_t c) {
    double o = t / 2;
    double rr = rad + o;
    line(x + rad, y - o, x + w - rad, y - o, t, c);
    line(x + rad, y + h + o, x + w - rad, y + h + o, t, c);
    line(x - o, y + rad, x - o, y + h - rad, t, c);
    line(x + w + o, y + rad, x + w + o, y + h - rad, t, c);
    outline_arc(x + rad, y + rad, rr, t, c, -1, -1);
    outline_arc(x + w - rad, y + rad, rr, t, c, 1, -1);
    outline_arc(x + w - rad, y + h - rad, rr, t, c, 1, 1);
    outline_arc(x + rad, y + h - rad, rr, t, c, -1, 1);
}

static void outline_arc(double cx, double cy, double rr, double t, uint32_t c, double mx, double my) {
    line(cx + mx * rr, cy, cx + mx * 0.9239 * rr, cy + my * 0.3827 * rr, t, c);
    line(cx + mx * 0.9239 * rr, cy + my * 0.3827 * rr, cx + mx * 0.7071 * rr, cy + my * 0.7071 * rr, t, c);
    line(cx + mx * 0.7071 * rr, cy + my * 0.7071 * rr, cx + mx * 0.3827 * rr, cy + my * 0.9239 * rr, t, c);
    line(cx + mx * 0.3827 * rr, cy + my * 0.9239 * rr, cx, cy + my * rr, t, c);
}

/* Apply active UI color palette (new purple theme vs classic rose theme). */
static void apply_theme(void) {
    if (active_theme_mode() == 2) {
        /* Night: sleepy indigo with soft lavender cards. */
        ui_purple = 0xFF7D6BB8;
        bg_screen = 0x0C1230;
        bg_gradient_bottom = 0x2A2358;
        card_bg = 0xFF1B2150;
        card_ring = 0xFF7B86C8;
    } else if (active_theme_mode() == 1 && winter_theme_on() == 1) {
        /* New Year: frosty blue. */
        ui_purple = 0xFF3E7FB5;
        bg_screen = 0x102A40;
        bg_gradient_bottom = 0x2C5A78;
        card_bg = 0xFF1E415C;
        card_ring = 0xFF8FC8E8;
    } else if (new_theme == 1) {
        ui_purple = 0xFF5F10A0;
        bg_screen = 0x1A1A2E;
        bg_gradient_bottom = 0x4A2E82;
        card_bg = 0xFF2A2A40;
        card_ring = 0xFF5A5A78;
    } else {
        ui_purple = 0xFFD4508A;
        bg_screen = 0xFF3A1530;
        bg_gradient_bottom = 0x4A2E82;
        card_bg = 0xFF44203A;
        card_ring = 0xFFE07BAE;
    }
    transition_duration = 0.10;
}

static void draw_back(void) {
    button(tr_back(), (screen_w - btn_w) / 2, back_y, btn_w, btn_h);
}

static double back_hit(double x, double y) { return hit_button(x, y, (screen_w - btn_w) / 2, back_y, btn_w, btn_h); }

static double battle_back_x(void) { return (screen_w - btn_w) / 2; }

static double battle_back_y(void) { return back_y; }

#define RESULT_PURPLE 0xFF5F10A0
#define RESULT_ROSE 0xFFD4508A

/* Return result window face color for active theme. */
static double result_face_color(void) {
    if (new_theme == 0) {
        return RESULT_ROSE;
    }
    return RESULT_PURPLE;
}

static void draw_battle_back(void) {
    if (finished != 0) {
        button_fixed(tr_back(), battle_back_x(), battle_back_y(), btn_w, btn_h, result_face_color());
        return;
    }
    button(tr_back(), battle_back_x(), battle_back_y(), btn_w, btn_h);
}

static double battle_back_hit(double x, double y) { return hit_button(x, y, battle_back_x(), battle_back_y(), btn_w, btn_h); }

static void screen_fill(void) {
    if (new_theme == 0) {
        rect(0, 0, screen_w, screen_h, bg_screen);
    } else {
        double top = bg_screen;
        double bottom = bg_gradient_bottom;
        if (active_theme_mode() == 1 && christmas_active() == 1) {
            top = christmas_bg_top;
            bottom = christmas_bg_bottom;
        }
        rect(0, 0, screen_w, screen_h, top);
        draw_bottom_gradient(top, bottom);
    }
    /* Draw backdrop snow / falling stars beneath UI controls and modals. */
    draw_christmas_under_notices();
    draw_night_stars();
}

static void draw_bottom_gradient(double top, double bottom) {
    double h = bg_gradient_h;
    double bands = h / 2;
    double y0 = screen_h - h;
    /* Use 2 px horizontal bands for smooth gradient transitions. */
    if (bands < bg_gradient_bands) {
        bands = bg_gradient_bands;
    }
    if (bands > 400) {
        bands = 400;
    }
    double band_h = h / bands + 1;
    double i = 0;
    while (i < bands) {
        double t = (i + 1) / bands;
        double y = y0 + i * (h / bands);
        rect(0, y, screen_w, band_h, blend_color(top, bottom, t));
        i = i + 1;
    }
}

/* k = 1 is the resting look; a pressed card passes k < 1 to darken itself. */
static void card_state(double x, double y, double w, double h, double selected, double locked, double k) {
    if (selected == 1) {
        roundrect(x - card_ring_pad, y - card_ring_pad, w + 2 * card_ring_pad, h + 2 * card_ring_pad, btn_radius + 4,
                  card_ring);
    }
    if (locked == 1) {
        roundrect(x, y, w, h, btn_radius, shade(shade(card_bg, locked_shade), k));
        return;
    }
    roundrect(x, y, w, h, btn_radius, shade(card_bg, k));
}

static void panel(double x, double y, double w, double h, double locked) {
    if (locked == 1) {
        roundrect(x, y, w, h, btn_radius, locked_bg());
        return;
    }
    roundrect(x, y, w, h, btn_radius, ui_purple);
}

static double panel_text(double locked) {
    if (locked == 1) {
        return locked_text();
    }
    return text_color;
}

static void hud_bar(double bx, double by, double w, double h, double cur, double max, uint32_t c, double from_right) {
    roundrect(bx, by, w, h, h / 2, 0xDD171923);
    double shown = clamp(cur, 0, max);
    double pad = 4;
    if (h <= 12) {
        pad = 1;
    }
    double fh = h - 2 * pad;
    if (fh < 1) {
        fh = 1;
    }
    if (max <= 0) {
        return;
    }
    double fill_w = (w - 2 * pad) * shown / max;
    if (fill_w <= 0) {
        return;
    }
    if (from_right == 1) {
        roundrect(bx + w - pad - fill_w, by + pad, fill_w, fh, fh / 2, c);
    } else {
        roundrect(bx + pad, by + pad, fill_w, fh, fh / 2, c);
    }
}

static double tick_down(double t) {
    if (t <= 0) {
        return 0;
    }
    if (t - dt <= 0) {
        return 0;
    }
    return t - dt;
}

static void draw_msg(const char *label, double y, double t) {
    if (t > 0) {
        ctext_fade(label, y, text_color, t, msg_fade);
    }
}

/* Sliding-window touch velocity tracker immune to lift-off thumb roll and batched moves. */
static void scroll_track_reset(ScrollTrack *st) { memset(st, 0, sizeof(*st)); }

static void scroll_track_move(ScrollTrack *st, double d) {
    st->frame_d += d;
    st->frame_moved = 1;
}

static void scroll_track_push(ScrollTrack *st, double d, double frame_dt) {
    double step_dt = clamp(frame_dt > 0 ? frame_dt : 0.016, 0.004, 0.05);
    if (st->hist_n < 4) {
        st->hist_d[st->hist_n] = d;
        st->hist_dt[st->hist_n] = step_dt;
        st->hist_n++;
    } else {
        memmove(&st->hist_d[0], &st->hist_d[1], 3 * sizeof(double));
        memmove(&st->hist_dt[0], &st->hist_dt[1], 3 * sizeof(double));
        st->hist_d[3] = d;
        st->hist_dt[3] = step_dt;
    }
}

static void scroll_track_tick(ScrollTrack *st) {
    scroll_track_push(st, st->frame_d, dt);
    st->frame_d = 0;
    st->frame_moved = 0;
}

static double scroll_track_release(ScrollTrack *st, double max_vel) {
    if (st->frame_moved) {
        scroll_track_push(st, st->frame_d, dt);
        st->frame_d = 0;
        st->frame_moved = 0;
    }
    if (st->hist_n <= 0)
        return 0;
    int end = st->hist_n;
    double hold_d = 0, hold_dt = 0;
    for (int i = end - 1; i >= 0 && hold_dt < 0.032; i--) {
        hold_d += fabs(st->hist_d[i]);
        hold_dt += st->hist_dt[i];
    }
    if (hold_dt >= 0.028 && hold_d < 2.0) {
        scroll_track_reset(st);
        return 0;
    }
    if (end >= 2) {
        double prev = 0;
        for (int i = 0; i < end - 1; i++)
            prev += st->hist_d[i];
        double last = st->hist_d[end - 1];
        if (st->hist_d[end - 2] * last < 0 && prev * last < 0 && fabs(last) < fabs(prev) * 0.65)
            end--;
    }
    double sum_d = 0, sum_dt = 0, sign_ref = st->hist_d[end - 1];
    for (int i = end - 1; i >= 0 && sum_dt < 0.09; i--) {
        if (i < end - 1 && sign_ref * st->hist_d[i] < 0 && fabs(st->hist_d[i]) > 2.0)
            break;
        sum_d += st->hist_d[i];
        sum_dt += st->hist_dt[i];
    }
    scroll_track_reset(st);
    if (fabs(sum_d) < 3.0)
        return 0;
    return clamp((sum_d / fmax(sum_dt, 0.012)) * 0.72, -max_vel, max_vel);
}
