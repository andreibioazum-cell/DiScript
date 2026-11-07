/* Rendering of the H4c4k0r raid: a silent glitch intro, compact draggable
 * cmd.exe-style consoles with a green title bar whose lines type slowly, as
 * if a real cheat and a real admin sat at two keyboards. Avatars are the game
 * textures: ordinary.png for the hacker's cube, azum.png for Azum's cube.
 * His "launched" events stay switched off - only one static cloud hangs in
 * the corner - and the ban arrives as a full blackout followed by a console
 * window that types the verdict before everything explodes. */

#define HACK_TERM_BAR 0x1E8E3Eu
#define HACK_TERM_BAR_INK 0xF2FFF2u
#define HACK_TERM_BODY 0x000000u
#define HACK_TERM_EDGE 0x6F6F6Fu
#define HACK_GLYPH 0x0B3A12u
#define HACK_INK_WARN 0xFF6B6Bu
#define HACK_INK_HACK 0x7CFC7Cu
#define HACK_INK_AZUM 0xE8E8E8u

#define HACK_WRAP_MAX 8

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
    return hack_line_text(i)[0] == '/' ? HACK_CMD_CPS : HACK_TYPE_CPS;
}

/* A line waits for the previous line of its window to finish plus a short
 * breath, so the conversation reads like two people typing in turn. */
static double hack_line_start(double i) {
    int k = (int)i;
    int j = 0;
    double win = hack_script[k].win;
    double start = hack_script[k].at;
    double prev_end = -1;
    while (j < k) {
        if (hack_script[j].win == win) {
            double s = hack_script[j].at;
            if (prev_end > s) {
                s = prev_end;
            }
            prev_end = s + utf8_len(language == 1 ? hack_script[j].ru : hack_script[j].en) /
                           hack_line_cps((double)j) + 0.35;
        }
        j = j + 1;
    }
    if (prev_end > start) {
        start = prev_end;
    }
    return start;
}

/* Codepoints of this line already on screen. */
static double hack_typed_chars(double i) {
    double n = (hack_t - hack_line_start(i)) * hack_line_cps(i);
    double full = utf8_len(hack_line_text(i));
    if (n < 0) {
        n = 0;
    }
    if (n > full) {
        n = full;
    }
    return floor(n);
}

/* Avatar cube in front of a line: the hacker is the ordinary cube sprite,
 * Azum's is his own sprite; the warning line gets a red exclamation. */
static void hack_avatar(double x, double y, double s, double who) {
    if (who == HACK_WHO_WARN) {
        text_scaled("!", x + s * 0.30, y, 0xFF000000u | HACK_INK_WARN, s / 18.0);
        return;
    }
    tex_alpha(x, y, who == HACK_WHO_AZUM ? AZUM_TEX : ORDINARY_TEX, 0, s / 50.0, 1);
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

/* cmd.exe chrome: green title bar, path title and the three window glyphs. */
static void hack_term_chrome(double x, double y, double w, double head, double win,
                             const char *custom) {
    roundrect(x, y, w, head, 4, HACK_TERM_BAR);
    char title[44];
    if (custom != 0) {
        snprintf(title, sizeof(title), "%s", custom);
    } else if (hack_platform == HACK_PLAT_PC) {
        snprintf(title, sizeof(title), "C:\\Windows\\system32\\cmd_0%d.exe", (int)win + 1);
    } else {
        snprintf(title, sizeof(title), "shell@kb4:/ # cmd_0%d", (int)win + 1);
    }
    double ts = clamp(head / 46.0, 0.24, 0.34);
    text_scaled(title, x + 6, y + (head - text_ink_height(title) * ts) / 2, HACK_TERM_BAR_INK, ts);
    double gx = x + w - 34;
    double my = y + head / 2;
    rect(gx, my + 2, 8, 2, HACK_GLYPH);
    roundrect_outline(gx + 12, my - 4, 8, 8, 1, 2, HACK_GLYPH);
    line(gx + 25, my - 4, gx + 32, my + 4, 2, HACK_GLYPH);
    line(gx + 32, my - 4, gx + 25, my + 4, 2, HACK_GLYPH);
}

/* One compact terminal; returns the drawn height. Erased = wiped by the ban. */
static double hack_draw_term(double win, double x, double y, double erased) {
    double sc = hack_text_sc();
    double w = hack_term_w();
    double head = hack_term_head();
    double pad = 6;
    double step = hack_line_step(sc);
    double icon = step * 0.95;
    double text_w = w - 2 * pad - icon - 6;
    double max_h = screen_h * 0.30;
    double cap_lines = floor((max_h - head - 2 * pad) / step);
    double wraps[24];
    double total = 0;
    double i = 0;
    while (i < HACK_LINES) {
        wraps[(int)i] = 0;
        if (hack_script[(int)i].win == win && hack_line_start(i) <= hack_t) {
            const char *vis = utf8_prefix(hack_line_text(i), hack_typed_chars(i));
            double wn = hack_wrap_n(vis, text_w, sc);
            wraps[(int)i] = wn < 1 ? 1 : wn;
            total = total + wraps[(int)i];
        }
        i = i + 1;
    }
    if (erased == 1) {
        /* Wiped console: empty black body with a lone blinking cursor. */
        double h = head + 2 * pad + step;
        roundrect(x + 3, y + 5, w, h, 6, 0x60000000u);
        roundrect(x, y, w, h, 6, 0xFF000000u | HACK_TERM_BODY);
        roundrect_outline(x, y, w, h, 6, 2, HACK_TERM_EDGE);
        hack_term_chrome(x, y, w, head, win, 0);
        if (fmod(event_t * 3, 2) < 1) {
            rect(x + pad + 1, y + head + pad, 5 * sc + 2, step * 0.72, 0xFF000000u | HACK_INK_HACK);
        }
        return h;
    }
    if (total <= 0) {
        return 0;
    }
    double skip = total > cap_lines ? total - cap_lines : 0;
    double shown = total - skip;
    double h = head + 2 * pad + shown * step;
    roundrect(x + 3, y + 5, w, h, 6, 0x60000000u);
    roundrect(x, y, w, h, 6, 0xFF000000u | HACK_TERM_BODY);
    roundrect_outline(x, y, w, h, 6, 2, HACK_TERM_EDGE);
    hack_term_chrome(x, y, w, head, win, 0);
    double cy = y + head + pad;
    double left = skip;
    i = 0;
    while (i < HACK_LINES) {
        double li = i;
        i = i + 1;
        if (hack_script[(int)li].win != win || hack_line_start(li) > hack_t) {
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
        if (left >= wn) {
            left = left - wn;
            continue;
        }
        double first = left;
        left = 0;
        double who = hack_line_who(li);
        uint32_t ink = 0xFF000000u | (unsigned)hack_ink_of(who);
        hack_avatar(x + pad, cy + 1, icon, who);
        double k = first;
        while (k < got) {
            const char *span = dial_slice(vis, starts[(int)k], lens[(int)k]);
            text_scaled(span, x + pad + icon + 6, cy, ink, sc);
            cy = cy + step;
            k = k + 1;
        }
        if (typed < utf8_len(full) && fmod(event_t * 3, 2) < 1) {
            const char *last = dial_slice(vis, starts[(int)(got - 1)], lens[(int)(got - 1)]);
            double cw = text_ink_width(last) * sc;
            rect(x + pad + icon + 6 + cw + 1, cy - step, 5 * sc + 2, step * 0.72, ink);
        }
    }
    return h;
}

/* Full-screen interference while the hacker's connection boots. Silent: no
 * captions, no complaints - just the static. */
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
        uint32_t c = i % 3 == 0 ? 0x0044FFu : (i % 3 == 1 ? 0x00FFC8u : 0x2244FFu);
        rect(dx, y, screen_w, h, ((unsigned)(int)(k * 120)) << 24 | c);
        rect(0 - dx, y + h, screen_w, 2, ((unsigned)(int)(k * 90)) << 24 | 0xFFFFFFu);
        i = i + 1;
    }
}

/* The one cloud of his "storm": hangs in the corner, does absolutely nothing. */
static void draw_hacker_cloud(void) {
    if (hack_cloud_on != 1) {
        return;
    }
    double x = hack_cloud_x;
    double y = hack_cloud_y;
    uint32_t c = 0xE8F4FFu;
    circle(x - 14, y + 4, 10, c);
    circle(x, y - 4, 13, c);
    circle(x + 14, y + 4, 10, c);
    roundrect(x - 22, y + 2, 44, 12, 6, c);
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

/* The ban: first a full blackout as if the game died, then a console window
 * types Azum's verdict while the world comes back. */
static void draw_hacker_ban(void) {
    if (hack_ban_t <= 0 || hack_t >= HACK_RAIN_AT || hack_ban_t < HACK_BLACK_DUR) {
        return;
    }
    const char *l1 = language == 1 ? "[CONSOLE]: H4c4k0r забанен Azum's." : "[CONSOLE]: H4c4k0r banned by Azum's.";
    const char *l2 = language == 1 ? "[CONSOLE]: Причина: Не умеешь хакать - не берись."
                                   : "[CONSOLE]: Reason: can't hack - don't try.";
    double sc = hack_text_sc();
    double step = hack_line_step(sc);
    double head = hack_term_head();
    double pad = 6;
    double icon = step * 0.95;
    double w = clamp(fmax(text_ink_width(l1), text_ink_width(l2)) * sc + 2 * pad + icon + 12,
                     220, screen_w - 16);
    double h = head + 2 * pad + 2 * step;
    double x = (screen_w - w) / 2;
    double y = screen_h * 0.30;
    roundrect(x + 3, y + 5, w, h, 6, 0x60000000u);
    roundrect(x, y, w, h, 6, 0xFF000000u | HACK_TERM_BODY);
    roundrect_outline(x, y, w, h, 6, 2, HACK_TERM_EDGE);
    hack_term_chrome(x, y, w, head, 0,
                     hack_platform == HACK_PLAT_PC ? "C:\\Azum\\ban.exe" : "shell@kb4:/ # ban");
    double t0 = HACK_BAN_AT + HACK_BLACK_DUR;
    double n1 = clamp((hack_t - t0) * HACK_CMD_CPS, 0, utf8_len(l1));
    double t1 = t0 + utf8_len(l1) / HACK_CMD_CPS + 0.25;
    double n2 = clamp((hack_t - t1) * HACK_CMD_CPS, 0, utf8_len(l2));
    double k = 0;
    while (k < 2) {
        const char *line = k == 0 ? l1 : l2;
        double n = k == 0 ? n1 : n2;
        const char *vis = utf8_prefix(line, n);
        double yy = y + head + pad + k * step;
        hack_avatar(x + pad, yy + 1, icon, HACK_WHO_AZUM);
        text_scaled(vis, x + pad + icon + 6, yy, 0xFFFFFFFFu, sc);
        if (n < utf8_len(line) && n > 0 && fmod(event_t * 3, 2) < 1) {
            double cw = text_ink_width(vis) * sc;
            rect(x + pad + icon + 6 + cw + 1, yy, 5 * sc + 2, step * 0.72, 0xFFFFFFFFu);
        }
        k = k + 1;
    }
}

/* The crash look: pure black over everything, HUD included. */
static void draw_hacker_blackout(void) {
    if (hack_ban_t <= 0 || hack_ban_t >= HACK_BLACK_DUR || hack_t >= HACK_RAIN_AT) {
        return;
    }
    rect(0, 0, screen_w, screen_h, 0xFF000000u);
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

/* World layer: the cloud, the junk pile and the canes under the HUD. */
static void draw_hacker_world(void) {
    if (hack_active == 0) {
        return;
    }
    draw_hacker_cloud();
    draw_hacker_buttons();
    draw_hacker_canes();
}

/* Overlay layer: glitch, the draggable consoles, the ban and the blackout. */
static void draw_hacker_event(void) {
    if (hack_active == 0) {
        return;
    }
    draw_hacker_glitch();
    if (hack_t < HACK_RAIN_AT) {
        double erased = hack_ban_t > 0 ? 1 : 0;
        double wi = 0;
        while (wi < HACK_WINS) {
            double x = clamp(hack_term_x[(int)wi], 4, screen_w - hack_term_w() - 4);
            double y = clamp(hack_term_y[(int)wi], 4, screen_h - hack_term_head() - 40);
            hack_draw_term(wi, x, y, erased);
            wi = wi + 1;
        }
        draw_hacker_ban();
    }
    draw_hacker_cane_hud();
    draw_hacker_blackout();
}
