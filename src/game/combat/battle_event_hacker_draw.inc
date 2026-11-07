/* Rendering of the H4c4k0r raid: glitch intro, command-line terminals whose
 * lines are typed live behind little cube avatars, the hacker's broken event
 * stubs, the menu-button pile and the New Year cane rain. Both avatar colours
 * are painted right here in C: flat red for the hacker, and the dominant
 * colour of assets/textures/azum.png for Azum's cube. */

#define HACK_CUBE_HACK 0xE2232Du
/* Dominant opaque colour of the Azum sprite, sampled from azum.png. */
#define HACK_CUBE_AZUM 0x1E374Bu
#define HACK_CUBE_AZUM_LIT 0x3C5569u
#define HACK_CUBE_AZUM_EYES 0xE169D2u
#define HACK_TERM_HEAD 0x24242Eu
#define HACK_TERM_BODY 0x0B0B10u
#define HACK_TERM_EDGE 0x6E6E82u
#define HACK_INK_WARN 0xFF5252u
#define HACK_INK_HACK 0x7CFC7Cu
#define HACK_INK_AZUM 0xE8F4FFu
#define HACK_BAN_RED 0xD8141Eu

#define HACK_WRAP_MAX 8

static double hack_term_w(void) {
#ifdef CB_PC
    return clamp(screen_w * 0.40, 340, 660);
#else
    return screen_w - 20;
#endif
}

static double hack_text_sc(void) {
    return clamp(screen_w / 2400.0, 0.34, 0.5);
}

static double hack_line_step(double sc) {
    return text_ink_height("Ag") * sc * 1.3;
}

static const char *hack_line_text(double i) {
    int k = (int)i;
    if (k < 0 || k >= (int)HACK_LINES) {
        return "";
    }
    return language == 1 ? hack_script[k].ru : hack_script[k].en;
}

static double hack_line_who(double i) {
    int k = (int)i;
    if (k < 0 || k >= (int)HACK_LINES) {
        return HACK_WHO_HACK;
    }
    return hack_script[k].who;
}

static double hack_line_cps(double i) {
    const char *t = hack_line_text(i);
    return t[0] == '/' ? HACK_CMD_CPS : HACK_TYPE_CPS;
}

/* Codepoints of this line already on screen. */
static double hack_typed_chars(double i) {
    int k = (int)i;
    double at = hack_script[k].at;
    double n = (hack_t - at) * hack_line_cps(i);
    double full = utf8_len(hack_line_text(i));
    if (n < 0) {
        n = 0;
    }
    if (n > full) {
        n = full;
    }
    return floor(n);
}

/* Small square avatar in front of a terminal line, painted by hand. */
static void hack_cube_icon(double x, double y, double s, double who) {
    if (who == HACK_WHO_AZUM) {
        rect(x, y, s, s, HACK_CUBE_AZUM);
        rect(x + 1, y + 1, s - 2, 2, HACK_CUBE_AZUM_LIT);
        rect(x + s * 0.22, y + s * 0.42, 2, 2, HACK_CUBE_AZUM_EYES);
        rect(x + s * 0.60, y + s * 0.42, 2, 2, HACK_CUBE_AZUM_EYES);
        return;
    }
    rect(x, y, s, s, HACK_CUBE_HACK);
    rect(x, y + s - 2, s, 2, 0x7A0E14u);
    rect(x + 2, y + 2, 2, 2, 0xFF8A8Au);
    if (who == HACK_WHO_WARN) {
        rect(x + s / 2 - 1, y + 3, 2, s - 8, 0xFFFFFFu);
        rect(x + s / 2 - 1, y + s - 5, 2, 2, 0xFFFFFFu);
    }
}

static double hack_ink_of(double who) {
    if (who == HACK_WHO_WARN) {
        return HACK_INK_WARN;
    }
    if (who == HACK_WHO_AZUM) {
        return HACK_INK_AZUM;
    }
    return HACK_INK_HACK;
}

/* Wrapped line count of one script line (pass 1, heights only). */
static double hack_wrap_n(const char *vis, double maxw, double sc) {
    double starts[HACK_WRAP_MAX];
    double lens[HACK_WRAP_MAX];
    return dial_wrap(vis, maxw, sc, starts, lens, HACK_WRAP_MAX);
}

static void hack_term_chrome(double x, double y, double w, double head, double win, double ban) {
    if (ban == 1) {
        roundrect(x, y, w, head, 6, 0x6E0A10u);
        return;
    }
    roundrect(x, y, w, head, 6, HACK_TERM_HEAD);
#ifdef CB_PC
    /* Desktop console chrome: title plus the three window glyphs. */
    char title[40];
    snprintf(title, sizeof(title), "C:\\H4CK\\term_0%d.exe", (int)win + 1);
    text_scaled(title, x + 8, y + (head - text_ink_height(title) * 0.34) / 2, 0xB9B9C6u, 0.34);
    double gx = x + w - 46;
    double i = 0;
    while (i < 3) {
        rect(gx + i * 15, y + head / 2 - 1, 9, 2, 0x9A9AA8u);
        i = i + 1;
    }
    rect(x + w - 31, y + head / 2 - 4, 2, 8, 0x9A9AA8u);
    rect(x + w - 31 + 7, y + head / 2 - 4, 2, 8, 0x9A9AA8u);
#else
    char title[24];
    snprintf(title, sizeof(title), "term_0%d", (int)win + 1);
    text_scaled(title, x + 8, y + (head - text_ink_height(title) * 0.34) / 2, 0xB9B9C6u, 0.34);
    rect(x + w - 20, y + head / 2 - 1, 12, 2, 0x9A9AA8u);
#endif
}

/* One terminal window with its typed lines; returns the drawn height. */
static double hack_draw_term(double win, double x, double y, double ban) {
    double sc = hack_text_sc();
    double w = hack_term_w();
    double head = clamp(screen_h * 0.035, 20, 30);
    double pad = 8;
    double icon = 12;
    double step = hack_line_step(sc);
    double text_w = w - 2 * pad - icon - 6;
    double max_h = screen_h * 0.44;
    double cap_lines = floor((max_h - head - 2 * pad) / step);
    double wraps[24];
    double total = 0;
    double i = 0;
    while (i < HACK_LINES) {
        wraps[(int)i] = 0;
        if (hack_script[(int)i].win == win && hack_script[(int)i].at <= hack_t) {
            const char *vis = utf8_prefix(hack_line_text(i), hack_typed_chars(i));
            double wn = hack_wrap_n(vis, text_w, sc);
            if (wn < 1) {
                wn = 1;
            }
            wraps[(int)i] = wn;
            total = total + wn;
        }
        i = i + 1;
    }
    if (total <= 0) {
        return 0;
    }
    /* Scroll: when the log outgrows the window, the oldest lines roll out. */
    double skip = 0;
    if (total > cap_lines) {
        skip = total - cap_lines;
    }
    double shown = total - skip;
    double h = head + 2 * pad + shown * step;
    if (ban == 1) {
        /* Wiped terminal: solid alarm red, no text at all. */
        roundrect(x, y, w, h, 8, HACK_BAN_RED);
        roundrect_outline(x, y, w, h, 8, 2, 0xFF6E63u);
        hack_term_chrome(x, y, w, head, win, 1);
        return h;
    }
    roundrect(x + 3, y + 5, w, h, 8, 0x60000000u);
    roundrect(x, y, w, h, 8, 0xF2000000u | HACK_TERM_BODY);
    roundrect_outline(x, y, w, h, 8, 2, HACK_TERM_EDGE);
    hack_term_chrome(x, y, w, head, win, 0);
    double cy = y + head + pad;
    double left = skip;
    i = 0;
    while (i < HACK_LINES) {
        double li = i;
        i = i + 1;
        if (hack_script[(int)li].win != win || hack_script[(int)li].at > hack_t) {
            continue;
        }
        double wn = wraps[(int)li];
        if (wn <= 0) {
            continue;
        }
        double typed = hack_typed_chars(li);
        const char *full = hack_line_text(li);
        const char *vis = utf8_prefix(full, typed);
        double starts[HACK_WRAP_MAX];
        double lens[HACK_WRAP_MAX];
        double got = dial_wrap(vis, text_w, sc, starts, lens, HACK_WRAP_MAX);
        if (got < 1) {
            got = 1;
            starts[0] = 0;
            lens[0] = str_len(vis);
        }
        double used = wn;
        double k = 0;
        if (left >= used) {
            left = left - used;
            continue;
        }
        double first = left;
        left = 0;
        double who = hack_line_who(li);
        double ink = 0xFF000000u | (unsigned)hack_ink_of(who);
        hack_cube_icon(x + pad, cy + 1, icon, who);
        k = first;
        while (k < got) {
            const char *span = dial_slice(vis, starts[(int)k], lens[(int)k]);
            text_scaled(span, x + pad + icon + 6, cy, ink, sc);
            cy = cy + step;
            k = k + 1;
        }
        /* Blinking block cursor while this line is still being typed. */
        if (typed < utf8_len(full) && fmod(event_t * 3, 2) < 1) {
            const char *last = dial_slice(vis, starts[(int)(got - 1)], lens[(int)(got - 1)]);
            double cw = text_ink_width(last) * sc;
            rect(x + pad + icon + 6 + cw + 1, cy - step, 5 * sc + 2, step * 0.72, ink);
        }
    }
    return h;
}

/* Full-screen interference while the hacker's connection boots. */
static void draw_hacker_glitch(void) {
    if (hack_glitch <= 0) {
        return;
    }
    double k = clamp(hack_glitch / HACK_GLITCH_TIME, 0, 1);
    rect(0, 0, screen_w, screen_h, ((unsigned)(int)(k * 70)) << 24 | 0x05060Au);
    int i = 0;
    while (i < 9) {
        double y = (double)(rand() % (screen_h > 1 ? screen_h : 1));
        double h = 3 + (double)(rand() % 16);
        double dx = (double)(rand() % 44) - 22;
        uint32_t c = i % 3 == 0 ? 0xFF0044u : (i % 3 == 1 ? 0x00FFC8u : 0x2244FFu);
        rect(dx, y, screen_w, h, ((unsigned)(int)(k * 120)) << 24 | c);
        rect(0 - dx, y + h, screen_w, 2, ((unsigned)(int)(k * 90)) << 24 | 0xFFFFFFu);
        i = i + 1;
    }
    const char *msg = language == 1 ? "ПОМЕХИ..." : "INTERFERENCE...";
    ctext_scaled(msg, screen_h * 0.46, ((unsigned)(int)(k * 230)) << 24 | 0xFF5252u, 0.8);
}

/* His "global" events: tiny dead stubs and the single surviving cloud. */
static void draw_hacker_stubs(void) {
    if (hack_stub_disco_t > 0) {
        unsigned a = (unsigned)(int)(clamp(hack_stub_disco_t, 0, 1) * 200) << 24;
        double x = 10;
        double y = screen_h - 118;
        rect(x, y, 92, 66, a | (unsigned)disco_color(hack_t * 9));
        roundrect_outline(x, y, 92, 66, 6, 2, a | 0xFFFFFFu);
        const char *tag = language == 1 ? "диско: СЛОМАНО" : "disco: BROKEN";
        text_scaled(tag, x, y + 70, a | 0xFF8A8Au, 0.34);
    }
    if (hack_stub_plates_t > 0) {
        unsigned a = (unsigned)(int)(clamp(hack_stub_plates_t, 0, 1) * 200) << 24;
        double x = screen_w - 108;
        double y = screen_h - 112;
        roundrect(x, y, 40, 40, 8, a | 0x7F1414u);
        roundrect(x + 52, y, 40, 40, 8, a | 0x7F1414u);
        const char *tag = language == 1 ? "плиты: СЛОМАНО" : "plates: BROKEN";
        text_scaled(tag, x - 6, y + 46, a | 0xFF8A8Au, 0.34);
    }
    if (hack_cloud_on == 1) {
        double bob = 3 * sin(hack_cloud_t * 2);
        double x = hack_cloud_x;
        double y = hack_cloud_y + bob;
        uint32_t c = 0xE8F4FFu;
        circle(x - 14, y + 4, 10, c);
        circle(x, y - 4, 13, c);
        circle(x + 14, y + 4, 10, c);
        roundrect(x - 22, y + 2, 44, 12, 6, c);
        const char *tag = language == 1 ? "облачко (пустышка)" : "cloud (dummy)";
        double tw = text_ink_width(tag) * 0.32;
        text_scaled(tag, x - tw / 2, y + 20, 0xB9D9FFu, 0.32);
    }
}

/* Menu junk falling as solid cover; pressed buttons squeeze and click. */
static void draw_hacker_buttons(void) {
    double i = 0;
    while (i < HACK_BTN_MAX) {
        double b = i * HB_FIELDS;
        if (arr_get(hack_buttons, b + HB_ON) != 1) {
            i = i + 1;
            continue;
        }
        double x = arr_get(hack_buttons, b + HB_X);
        double y = arr_get(hack_buttons, b + HB_Y);
        double w = arr_get(hack_buttons, b + HB_W);
        double h = arr_get(hack_buttons, b + HB_H);
        double k = clamp(arr_get(hack_buttons, b + HB_PRESS) / 0.16, 0, 1);
        double s = 1 - PRESS_SQUEEZE * k;
        double bx = x + w * (1 - s) / 2;
        double by = y + h * (1 - s) / 2;
        double bw = w * s;
        double bh = h * s;
        double face = shade(ui_purple, 1 - PRESS_SHADE * k);
        roundrect(bx + 2, by + 4, bw, bh, btn_radius, 0x70000000u);
        button_face(bx, by, bw, bh, face);
        const char *label = hack_junk_label(arr_get(hack_buttons, b + HB_LABEL));
        double sc = fit_text_scale(label, bw - 16, clamp(bh / 60.0, 0.4, 0.72));
        text_in_box(label, bx, by, bw, bh, shade(text_color, 1 - PRESS_SHADE * k), sc);
        i = i + 1;
    }
}

/* Ded Moroz canes sliding down at exactly 45 degrees. */
static void draw_hacker_canes(void) {
    double i = 0;
    while (i < HACK_CANE_MAX) {
        double b = i * HC_FIELDS;
        if (arr_get(hack_canes, b + HC_ON) != 1) {
            i = i + 1;
            continue;
        }
        double x = arr_get(hack_canes, b + HC_X);
        double y = arr_get(hack_canes, b + HC_Y);
        double dx = arr_get(hack_canes, b + HC_DX);
        double land = arr_get(hack_canes, b + HC_LAND);
        double pop = arr_get(hack_canes, b + HC_POP);
        double v = 1;
        double ang = 0;
        if (pop > 0) {
            v = clamp(1 - pop / 0.5, 0, 1);
        } else if (land == 0) {
            ang = atan2(dx, HACK_CANE_SPEED * 0.70710678);
        } else {
            y = y + 3 * sin(event_t * 4 + i);
        }
        double sc = 0.85 * v;
        if (sc <= 0.02) {
            i = i + 1;
            continue;
        }
        double hw = 25 * sc;
        if (candy_tex_ok == 1) {
            tex_alpha(x - hw, y - hw, CANDY_TEX, ang, sc, v);
        } else {
            circle(x, y, hw, (((unsigned)(int)(v * 254) + 1u) << 24) | 0x00FF4081u);
        }
        i = i + 1;
    }
}

/* Azum's final ban over the wiped terminals. */
static void draw_hacker_ban(void) {
    if (hack_ban_t <= 0 || hack_t >= HACK_RAIN_AT) {
        return;
    }
    double p = clamp(hack_ban_t / 0.25, 0, 1);
    double pop = 1 - (1 - p) * (1 - p);
    rect(0, 0, screen_w, screen_h, ((unsigned)(int)(pop * 120)) << 24 | 0x000000u);
    double sc = 0.85 * pop;
    if (sc <= 0.02) {
        return;
    }
    const char *l1 = language == 1 ? "[CONSOLE]: H4c4k0r забанен Azum's." : "[CONSOLE]: H4c4k0r banned by Azum's.";
    const char *l2 = language == 1 ? "[CONSOLE]: Причина: Не умеешь хакать - не берись."
                                   : "[CONSOLE]: Reason: can't hack - don't try.";
    double step = hack_line_step(sc) * 1.5;
    double w1 = text_ink_width(l1) * sc;
    double w2 = text_ink_width(l2) * sc;
    double wide = w1 > w2 ? w1 : w2;
    double icon = 20 * pop;
    double x0 = (screen_w - wide - icon - 8) / 2;
    double y0 = screen_h / 2 - step;
    double k = 0;
    while (k < 2) {
        const char *line = k == 0 ? l1 : l2;
        double yy = y0 + k * step;
        hack_cube_icon(x0, yy + 2, icon, HACK_WHO_AZUM);
        text_scaled(line, x0 + icon + 8, yy, 0xFFFFFFFFu, sc);
        k = k + 1;
    }
}

static void draw_hacker_cane_hud(void) {
    if (hack_t < HACK_RAIN_AT || hack_cane_made <= 0) {
        return;
    }
    const char *label = language == 1 ? "Трости Деда Мороза: " : "Ded Moroz canes: ";
    const char *got = ds_num_to_string(hack_cane_got);
    const char *of = ds_num_to_string(hack_cane_total);
    ctext_scaled(ds_concat(ds_concat(ds_concat(label, got), "/"), of), back_y + btn_h + 6,
                 0xFFD54Au, 0.5);
}

/* World layer: stubs, cloud, junk pile and canes under the HUD. */
static void draw_hacker_world(void) {
    if (hack_active == 0) {
        return;
    }
    draw_hacker_stubs();
    draw_hacker_buttons();
    draw_hacker_canes();
}

/* Overlay layer: glitch, terminals and the ban, above the HUD. */
static void draw_hacker_event(void) {
    if (hack_active == 0) {
        return;
    }
    draw_hacker_glitch();
    if (hack_t < HACK_RAIN_AT) {
        double ban = hack_ban_t > 0 ? 1 : 0;
#ifdef CB_PC
        double w = hack_term_w();
        hack_draw_term(0, 10, clamp(screen_h * 0.06, 30, 60), ban);
        hack_draw_term(1, screen_w - w - 10, clamp(screen_h * 0.10, 54, 90), ban);
        hack_draw_term(2, 10, screen_h * 0.52, ban);
#else
        /* Phones stack the windows and keep every one of them on screen. */
        double h0 = hack_draw_term(0, 10, clamp(screen_h * 0.05, 26, 44), ban);
        double y1 = clamp(screen_h * 0.46, 26 + h0 + 6, screen_h - 40);
        double h1 = hack_draw_term(1, 10, y1, ban);
        double y2 = y1 + h1 + 6;
        if (y2 > screen_h - 120) {
            y2 = screen_h - 120;
        }
        hack_draw_term(2, 10, y2, ban);
#endif
        draw_hacker_ban();
    }
    draw_hacker_cane_hud();
}
