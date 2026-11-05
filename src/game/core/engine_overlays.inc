/* Startup notices, achievement toast, and battle FPS counter. */

static void update_warning(void) {
    if (warn_open != 1) {
        return;
    }
    if (warn_closing == 1) {
        warn_close_t = clamp(warn_close_t + dt / warn_fade, 0, 1);
        double p = warn_close_t;
        warn_a = 1 - p * p * (3 - 2 * p);
        if (warn_close_t >= 1) {
            warn_a = 0;
            warn_open = 0;
            warn_closing = 0;
        }
        return;
    }
    warn_a = 1;
    warn_t = warn_t + dt;
    warn_ready = clamp(warn_t / warn_wait, 0, 1);
}

static void update_studio(void) {
    if (studio_open != 1) {
        return;
    }
    studio_t = studio_t + dt;
    if (studio_t < studio_in) {
        studio_a = studio_t / studio_in;
        return;
    }
    if (studio_t <= studio_in + studio_hold) {
        studio_a = 1;
        studio_bg_a = 1;
        return;
    }
    studio_a = 1 - (studio_t - studio_in - studio_hold) / studio_fade;
    studio_bg_a = studio_a;
    if (studio_a <= 0) {
        studio_a = 0;
        studio_bg_a = 0;
        studio_open = 0;
    }
}

static void tick_achievement_toast(void) {
    ach_toast_t = tick_down(ach_toast_t);
}

static const char *achievement_toast_text(void) {
    if (ach_toast_bit == ACH_FIRST_WIN) {
        return tr_achievement_first_win();
    }
    if (ach_toast_bit == ACH_ALL_CHARACTERS) {
        return tr_achievement_grass();
    }
    if (ach_toast_bit == ACH_LAST_HP) {
        return tr_achievement_last_hp();
    }
    if (ach_toast_bit == ACH_EAX229) {
        return tr_achievement_eax229();
    }
    if (ach_toast_bit == ACH_BUK_GIFT) {
        return tr_achievement_buk();
    }
    return tr_achievement_legends();
}

static void draw_achievement_toast(void) {
    if (ach_toast_bit <= 0 || ach_toast_t <= 0) {
        return;
    }
    double elapsed = ACH_TOAST_DURATION - ach_toast_t;
    double w = fit_w(460);
    double h = 70;
    double x = center_x(w);
    double home_y = 64;
    double off = home_y + h + 8;
    double y = home_y;
    if (elapsed < ACH_TOAST_FADE_IN) {
        double k = elapsed / ACH_TOAST_FADE_IN;
        double ease = 1 - (1 - k) * (1 - k);
        y = home_y - off * (1 - ease);
    } else if (elapsed > ACH_TOAST_FADE_IN + ACH_TOAST_HOLD) {
        double k2 = (elapsed - ACH_TOAST_FADE_IN - ACH_TOAST_HOLD) / ACH_TOAST_EXIT;
        y = home_y - off * clamp(k2, 0, 1) * clamp(k2, 0, 1);
    }
    if (y + h <= 0) {
        return;
    }
    roundrect(x, y, w, h, btn_radius, ui_purple);
    const char *head = tr_achievement_toast_title();
    const char *body = achievement_toast_text();
    double hs = 0.42;
    double bs = 0.62;
    double gap = 6;
    double hh = text_ink_height(head) * hs;
    double ht = text_ink_top(head) * hs;
    double bh = text_ink_height(body) * bs;
    double bt = text_ink_top(body) * bs;
    double top = y + (h - hh - gap - bh) / 2;
    ctext_scaled(head, top - ht, text_color, hs);
    ctext_scaled(body, top + hh + gap - bt, text_color, bs);
}


/* Battle FPS counter sampled from unclamped frame durations over 0.5 s windows. */
#define FPS_WINDOW 0.5
#define FPS_GAP_MAX 0.5
#define FPS_SCALE 0.45
#define FPS_MARGIN 12

static double fps_acc;
static double fps_frames;
static double fps_shown;

static void tick_fps_meter(void) {
    double frame = ds_frame_seconds();
    if (!(frame > 0) || frame > FPS_GAP_MAX) {
        fps_acc = 0;
        fps_frames = 0;
        return;
    }
    fps_acc = fps_acc + frame;
    fps_frames = fps_frames + 1;
    if (fps_acc >= FPS_WINDOW) {
        fps_shown = floor(fps_frames / fps_acc + 0.5);
        fps_acc = 0;
        fps_frames = 0;
    }
}

static void draw_fps_meter(void) {
    tick_fps_meter();
    if (fps_shown <= 0) {
        return;
    }
    const char *label = ds_concat(ds_num_to_string(fps_shown), " FPS");
    double tw = text_ink_width(label) * FPS_SCALE;
    double th = text_ink_height(label) * FPS_SCALE;
    double tt = text_ink_top(label) * FPS_SCALE;
    double x = screen_w - FPS_MARGIN - tw;
    double y = screen_h - FPS_MARGIN - th - tt;
        /* Draw 1 px shadow behind FPS readout. */
    text_scaled(label, x + 1, y + 1, 0xB0000000, FPS_SCALE);
    text_scaled(label, x, y, RGB(255,255,255), FPS_SCALE);
}
