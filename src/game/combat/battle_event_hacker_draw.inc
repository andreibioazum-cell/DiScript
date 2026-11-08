/* Rendering of the H4c4k0r raid: hacker console first, Azum's console later,
 * persistent inert event props, then a long blackout and an animated ban /
 * terminal close. All of it is local theatre: no real code is executed. */

#define HACK_TERM_BAR 0x1E8E3Eu
#define HACK_TERM_BAR_AZUM 0x244A73u
#define HACK_TERM_BAR_INK 0xF2FFF2u
#define HACK_TERM_BODY 0x000000u
#define HACK_TERM_EDGE 0x3A3A3Au
#define HACK_GLYPH 0x0B3A12u
#define HACK_INK_WARN 0xFF6B6Bu
#define HACK_INK_HACK 0x7CFC7Cu
#define HACK_INK_AZUM 0xE8E8E8u

#define HACK_WRAP_MAX 8
#define HACK_LINE_SLOTS 24

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

static const char *hack_line_label(double who) {
    if (who == HACK_WHO_WARN) {
        return "";
    }
    if (who == HACK_WHO_AZUM) {
        return "Azum:";
    }
    return "Hacker:";
}

static double hack_line_cps(double i) {
    return hack_line_text(i)[0] == '/' ? HACK_CMD_CPS : HACK_TYPE_CPS;
}

/* Lines wait for the previous line in the same window plus a short breath. */
static double hack_line_start(double i) {
    int k = (int)i;
    int j = 0;
    double win = hack_line_win(i);
    double start = hack_script[k].at;
    double prev_end = -1;
    while (j < k) {
        if (hack_line_win((double)j) == win) {
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
    if (win == 1 && start < HACK_SECOND_AT + 0.5) {
        start = HACK_SECOND_AT + 0.5;
    }
    return start;
}

static double hack_typed_chars(double i) {
    double n = (hack_t - hack_line_start(i)) * hack_line_cps(i);
    double full = utf8_len(hack_line_text(i));
    if (n < 0) {
        n = 0;
    }
    if (n > full) {
        n = full;
    }
    double chars = floor(n);
    if (chars > 0 && n < full && fmod(hack_t * 10, 1.0) < 0.05) {
        chars = chars - 1;
    }
    if (chars < 0) {
        chars = 0;
    }
    if (chars > full) {
        chars = full;
    }
    return chars;
}

/* ---- ban terminal timing (shared with the update / input lock) ---------- */
static const char *hack_ban_l1(void) {
    return language == 1 ? "[CONSOLE]: H4c4k0r забанен Azum's." : "[CONSOLE]: H4c4k0r banned by Azum's.";
}
static const char *hack_ban_l2(void) {
    return language == 1 ? "[CONSOLE]: Причина: не умеешь хакать - не берись."
                         : "[CONSOLE]: Reason: can't hack - don't try.";
}
static const char *hack_ban_reconnect(void) {
    return "Hacker: /reconnect --force";
}
static const char *hack_ban_denied(void) {
    return language == 1 ? "Azum: ACCESS DENIED. Сессия отозвана." : "Azum: ACCESS DENIED. Session revoked.";
}
static const char *hack_ban_exit(void) {
    return "Azum: exit";
}
static double hack_ban_verdict_end(void) {
    return HACK_BLACK_DUR + (utf8_len(hack_ban_l1()) + utf8_len(hack_ban_l2())) / HACK_BAN_CPS + 2.2;
}
static double hack_ban_reply_end(void) {
    return hack_ban_verdict_end() + utf8_len(hack_ban_reconnect()) / HACK_CMD_CPS + 0.35 +
           utf8_len(hack_ban_denied()) / HACK_BAN_CPS + 0.8;
}
static double hack_ban_exit_end(void) {
    return hack_ban_reply_end() + utf8_len(hack_ban_exit()) / HACK_BAN_CPS + 0.5;
}
static double hack_ban_click_at(void) {
    return hack_ban_exit_end() + 0.75;
}
static double hack_ban_closed_at(void) {
    return hack_ban_click_at() + 0.45;
}
static double hack_ban_done_at(void) {
    return hack_ban_closed_at() + 1.1;
}
static double hack_ban_scene_at(void) {
    double last = HACK_LINES - 1;
    double end = hack_line_start(last) + utf8_len(hack_line_text(last)) / hack_line_cps(last) + 0.8;
    return fmax(HACK_BAN_AT, end);
}
static double hack_rain_at(void) {
    return hack_ban_scene_at() + hack_ban_done_at() + 0.8;
}

/* cmd.exe chrome. Hacker is green; Azum's delayed console is navy. */
static void hack_term_chrome(double x, double y, double w, double head, double win,
                             const char *custom) {
    uint32_t bar = win == 1 ? HACK_TERM_BAR_AZUM : HACK_TERM_BAR;
    roundrect(x, y, w, head, 4, bar);
    char title[64];
    if (custom != 0) {
        snprintf(title, sizeof(title), "%s", custom);
    } else if (win == 1) {
        snprintf(title, sizeof(title), "Azum's admin console");
    } else if (hack_platform == HACK_PLAT_PC) {
        snprintf(title, sizeof(title), "C:\\Windows\\system32\\cmd_01.exe");
    } else {
        snprintf(title, sizeof(title), "shell@kb4:/ # cmd_01");
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

static void hack_term_frame(double x, double y, double w, double h, double head, double win,
                            const char *custom) {
    roundrect(x + 3, y + 5, w, h, 6, 0x60000000u);
    roundrect(x, y, w, h, 6, 0xFF000000u | HACK_TERM_BODY);
    roundrect_outline(x, y, w, h, 6, 2, HACK_TERM_EDGE);
    hack_term_chrome(x, y, w, head, win, custom);
}

/* One console window. Only lines belonging to this window are drawn. */
static double hack_draw_term(double win, double x, double y, double erased) {
    if (win == 1 && hack_second_open() == 0) {
        return 0;
    }
    double sc = hack_text_sc();
    double w = hack_term_w();
    double head = hack_term_head();
    double pad = 6;
    double step = hack_line_step(sc);
    double max_h = screen_h * (win == 1 ? 0.24 : 0.28);
    double cap_lines = floor((max_h - head - 2 * pad) / step);
    if (cap_lines < 1) {
        cap_lines = 1;
    }
    if (erased == 1) {
        double h = head + 2 * pad + step;
        hack_term_frame(x, y, w, h, head, win, 0);
        if (fmod(event_t * 3, 2) < 1) {
            rect(x + pad + 1, y + head + pad, 5 * sc + 2, step * 0.72, 0xFF000000u | HACK_INK_HACK);
        }
        return h;
    }
    double wraps[HACK_LINE_SLOTS];
    double lab_w[HACK_LINE_SLOTS];
    double starts[HACK_LINE_SLOTS][HACK_WRAP_MAX];
    double lens[HACK_LINE_SLOTS][HACK_WRAP_MAX];
    double total = 0;
    int i = 0;
    while (i < (int)HACK_LINES && i < HACK_LINE_SLOTS) {
        wraps[i] = 0;
        lab_w[i] = 0;
        if (hack_line_win((double)i) == win && hack_line_start((double)i) <= hack_t) {
            const char *vis = utf8_prefix(hack_line_text((double)i), hack_typed_chars((double)i));
            lab_w[i] = text_ink_width(hack_line_label(hack_line_who((double)i))) * sc * 0.8 + 6;
            double got = dial_wrap(vis, w - 2 * pad - lab_w[i], sc, starts[i], lens[i], HACK_WRAP_MAX);
            if (got < 1) {
                got = 1;
                starts[i][0] = 0;
                lens[i][0] = str_len(vis);
            }
            wraps[i] = got;
            total = total + got;
        }
        i = i + 1;
    }
    if (total <= 0) {
        return 0;
    }
    double skip = total > cap_lines ? total - cap_lines : 0;
    double shown = total - skip;
    double h = head + 2 * pad + shown * step;
    hack_term_frame(x, y, w, h, head, win, 0);
    double cy = y + head + pad;
    double left = skip;
    i = 0;
    while (i < (int)HACK_LINES && i < HACK_LINE_SLOTS) {
        int li = i;
        i = i + 1;
        double got = wraps[li];
        if (got <= 0) {
            continue;
        }
        if (left >= got) {
            left = left - got;
            continue;
        }
        double first = left;
        left = 0;
        const char *full = hack_line_text((double)li);
        double typed = hack_typed_chars((double)li);
        const char *vis = utf8_prefix(full, typed);
        double who = hack_line_who((double)li);
        uint32_t ink = 0xFF000000u | (unsigned)(who == HACK_WHO_WARN ? HACK_INK_WARN :
                                              who == HACK_WHO_AZUM ? HACK_INK_AZUM : HACK_INK_HACK);
        const char *label = hack_line_label(who);
        text_scaled(label, x + pad, cy, ink, sc * 0.8);
        double k = first;
        while (k < got) {
            const char *span = dial_slice(vis, starts[li][(int)k], lens[li][(int)k]);
            text_scaled(span, x + pad + lab_w[li], cy, ink, sc);
            cy = cy + step;
            k = k + 1;
        }
        if (typed < utf8_len(full) && fmod(event_t * 3, 2) < 1) {
            const char *last = dial_slice(vis, starts[li][(int)(got - 1)], lens[li][(int)(got - 1)]);
            double cw = text_ink_width(last) * sc;
            rect(x + pad + lab_w[li] + cw + 1, cy - step, 5 * sc + 2, step * 0.72, ink);
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
        uint32_t c = i % 3 == 0 ? 0x0044FFu : (i % 3 == 1 ? 0x88BBFFu : 0x2244FFu);
        rect(dx, y, screen_w, h, ((unsigned)(int)(k * 120)) << 24 | c);
        rect(0 - dx, y + h, screen_w, 2, ((unsigned)(int)(k * 90)) << 24 | 0xFFFFFFu);
        i = i + 1;
    }
}

/* ---- inert event props ------------------------------------------------- */
static double hack_prop_pop(double born) {
    double k = clamp((hack_t - born) / 0.35, 0, 1);
    return 1 - (1 - k) * (1 - k);
}

static void hack_dead_plate(double cx, double cy, double pop) {
    double half = plates_size / 2 * pop;
    if (half < 2) {
        return;
    }
    roundrect(cx - half - 4, cy - half - 4, half * 2 + 8, half * 2 + 8, 22 * pop, 0xC03A3F46u);
    roundrect(cx - half, cy - half, half * 2, half * 2, 20 * pop, 0xD05A6068u);
    ring(cx, cy, half * 0.42, 3 * pop, 0xB08A9098u);
}

static void draw_hacker_props(void) {
    if (hack_stub_on[0] == 1) {
        double pop = hack_prop_pop(hack_stub_t[0]);
        double x = screen_w * 0.5;
        double y = screen_h * 0.36;
        double r = clamp(screen_w * 0.035, 16, 28) * pop;
        line(x, y - r - 26 * pop, x, y - r, 2, 0xB0B8C0C8u);
        circle(x, y, r, 0xD08A9098u);
        line(x - r * 0.7, y, x + r * 0.7, y, 2, 0xD0B8C0C8u);
        line(x, y - r * 0.7, x, y + r * 0.7, 2, 0xD0B8C0C8u);
    }
    if (hack_stub_on[1] == 1 || hack_stub_on[5] == 1) {
        double born = hack_stub_on[1] == 1 ? hack_stub_t[1] : hack_stub_t[5];
        double pop = hack_prop_pop(born);
        hack_dead_plate(plate_ax(), plate_ay(), pop);
        hack_dead_plate(plate_bx(), plate_by(), pop);
    }
    if (hack_stub_on[2] == 1) {
        double pop = hack_prop_pop(hack_stub_t[2]);
        double half = 25 * cube_scale * player_scale * 0.55 * pop;
        double x = screen_w * 0.30;
        double y = screen_h * 0.62;
        if (tex_ready(SANTA_TEX) == 1) {
            tex_alpha(x - half, y - half, SANTA_TEX, 0, half / 250.0, 0.55 * pop);
        } else {
            roundrect(x - half, y - half, half * 2, half * 2, 8, 0x905A6068u);
        }
    }
    if (hack_stub_on[3] == 1) {
        double pop = hack_prop_pop(hack_stub_t[3]);
        double half = clamp(screen_w * 0.045, 20, 36) * pop;
        double x = screen_w * 0.50;
        double y = screen_h * 0.68;
        if (tex_ready(ORDINARY_CARD_TEX) == 1) {
            tex_alpha(x - half, y - half, ORDINARY_CARD_TEX, 0, half / 250.0, 0.55 * pop);
        } else {
            roundrect(x - half, y - half, half * 2, half * 2, 8, 0x905A6068u);
        }
    }
    if (hack_stub_on[4] == 1) {
        double pop = hack_prop_pop(hack_stub_t[4]);
        double half = columns_size / 2 * 0.55 * pop;
        double x = screen_w * 0.70;
        double y = screen_h * 0.62;
        if (tex_ready(EVENT_COLUMNS_TEX) == 1) {
            tex_alpha(x - half, y - half, EVENT_COLUMNS_TEX, 0, half / 250.0, 0.55 * pop);
        } else {
            roundrect(x - half, y - half, half * 2, half * 2, 8, 0x905A6068u);
        }
    }
}

/* /event 11: one static cloud, placed where neither console hides it. */
static void draw_hacker_cloud(void) {
    if (hack_cloud_on != 1) {
        return;
    }
    double pop = hack_prop_pop(hack_t - hack_cloud_t);
    double x = hack_cloud_x;
    double y = hack_cloud_y;
    uint32_t c = 0xE8F4FFu;
    circle(x - 14 * pop, y + 4 * pop, 10 * pop, c);
    circle(x, y - 4 * pop, 13 * pop, c);
    circle(x + 14 * pop, y + 4 * pop, 10 * pop, c);
    roundrect(x - 22 * pop, y + 2 * pop, 44 * pop, 12 * pop, 6 * pop, c);
}

/* /event 10: a tornado silhouette that never spins, moves or pushes anything. */
static void draw_hacker_tornado(void) {
    if (hack_tornado_on != 1) {
        return;
    }
    double pop = hack_prop_pop(hack_tornado_t);
    double x = screen_w - 68;
    double top = screen_h * 0.48;
    double height = clamp(screen_h * 0.16, 70, 140) * pop;
    int i = 0;
    while (i < 7) {
        double k = (double)i / 6.0;
        double w = lerp(56, 10, k) * pop;
        double y = top + height * k;
        double dx = sin(k * 3.0) * 3.0 * pop;
        roundrect(x - w / 2 + dx, y, w, 5 * pop + 1, 3, 0xB09AA4AEu);
        i = i + 1;
    }
    line(x - 28 * pop, top, x - 5 * pop, top + height, 2, 0x908A949Eu);
    line(x + 28 * pop, top, x + 5 * pop, top + height, 2, 0x908A949Eu);
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

/* ---- the long blackout and Azum's animated terminal shutdown ----------- */
static void hack_ban_type(const char *line, double start, double cps, uint32_t ink,
                          double x, double y, double sc, double step) {
    double n = clamp((hack_ban_t - start) * cps, 0, utf8_len(line));
    const char *vis = utf8_prefix(line, n);
    text_scaled(vis, x, y, ink, sc);
    if (n > 0 && n < utf8_len(line) && fmod(event_t * 3, 2) < 1) {
        double cw = text_ink_width(vis) * sc;
        rect(x + cw + 1, y, 5 * sc + 2, step * 0.72, ink);
    }
}

static void draw_hacker_ban(void) {
    if (hack_ban_t <= 0 || hack_ban_t >= hack_ban_done_at() || hack_ban_t < HACK_BLACK_DUR) {
        return;
    }
    double u = hack_ban_t;
    double sc = hack_text_sc();
    double step = hack_line_step(sc);
    double head = hack_term_head();
    double pad = 8;
    double w = clamp(fmax(text_ink_width(hack_ban_l1()), text_ink_width(hack_ban_denied())) * sc + 2 * pad + 16,
                     240, screen_w - 16);
    double body = 5 * step + 2 * pad;
    double x = (screen_w - w) / 2;
    double y = screen_h * 0.28;
    double click = hack_ban_click_at();
    if (u >= click) {
        /* Azum clicks X: content vanishes, the frame collapses and fades. */
        double k = clamp((u - click) / 0.45, 0, 1);
        double h = head * (1 - k) + 2;
        unsigned a = (unsigned)(int)(255 * (1 - k));
        if (a < 1) {
            return;
        }
        roundrect(x, y, w, h, 4, (a << 24) | HACK_TERM_BAR_AZUM);
        hack_term_chrome(x, y, w, fmax(head * (1 - k), 2), 1,
                         hack_platform == HACK_PLAT_PC ? "C:\\Azum\\ban.exe" : "shell@kb4:/ # ban");
        return;
    }
    hack_term_frame(x, y, w, head + body, head, 1,
                    hack_platform == HACK_PLAT_PC ? "C:\\Azum\\ban.exe" : "shell@kb4:/ # ban");
    double cy = y + head + pad;
    double t1 = HACK_BLACK_DUR;
    double t2 = t1 + utf8_len(hack_ban_l1()) / HACK_BAN_CPS + 0.25;
    double t3 = hack_ban_verdict_end();
    double t4 = t3 + utf8_len(hack_ban_reconnect()) / HACK_CMD_CPS + 0.35;
    double t5 = hack_ban_reply_end();
    hack_ban_type(hack_ban_l1(), t1, HACK_BAN_CPS, 0xFFFFFFFFu, x + pad, cy, sc, step);
    hack_ban_type(hack_ban_l2(), t2, HACK_BAN_CPS, 0xFFB8C0C8u, x + pad, cy + step, sc, step);
    hack_ban_type(hack_ban_reconnect(), t3, HACK_CMD_CPS, 0xFFFF6B6Bu, x + pad, cy + 2 * step, sc, step);
    hack_ban_type(hack_ban_denied(), t4, HACK_BAN_CPS, 0xFFFFD54Au, x + pad, cy + 3 * step, sc, step);
    hack_ban_type(hack_ban_exit(), t5, HACK_BAN_CPS, 0xFF7CFC7Cu, x + pad, cy + 4 * step, sc, step);
    /* After "exit", a small cursor travels to the close glyph and clicks it. */
    double exit_end = hack_ban_exit_end();
    if (u >= exit_end) {
        double k = clamp((u - exit_end) / 0.75, 0, 1);
        double sx = x + pad + text_ink_width(hack_ban_exit()) * sc + 8;
        double sy = cy + 4 * step + step * 0.4;
        double tx = x + w - 5;
        double ty = y + head / 2;
        double cx = lerp(sx, tx, k);
        double cy2 = lerp(sy, ty, k);
        if (u >= click - 0.12) {
            roundrect(x + w - 18, y + 2, 16, head - 4, 3, 0xFFE04848u);
        }
        circle(cx, cy2, 3, 0xFFFFFFFFu);
        line(cx, cy2, cx - 5, cy2 + 7, 2, 0xFFFFFFFFu);
    }
}

/* Pure black for the whole long hold; only the last second fades back. */
static void draw_hacker_blackout(void) {
    if (hack_ban_t <= 0 || hack_ban_t >= hack_ban_done_at()) {
        return;
    }
    unsigned a = 255;
    if (hack_ban_t >= hack_ban_closed_at()) {
        double k = clamp((hack_ban_t - hack_ban_closed_at()) / 1.1, 0, 1);
        a = (unsigned)(int)(255 * (1 - k));
    }
    if (a > 0) {
        rect(0, 0, screen_w, screen_h, (a << 24));
    }
}

static void draw_hacker_cane_hud(void) {
    if (hack_t < hack_rain_at() || hack_cane_made <= 0) {
        return;
    }
    const char *label = language == 1 ? "Трости Деда Мороза: " : "Ded Moroz canes: ";
    const char *got = ds_num_to_string(hack_cane_got);
    const char *of = ds_num_to_string(hack_cane_total);
    ctext_scaled(ds_concat(ds_concat(ds_concat(label, got), "/"), of), back_y + btn_h + 6,
                 0xFFD54Au, 0.5);
}

static void draw_hacker_world(void) {
    if (hack_active == 0) {
        return;
    }
    draw_hacker_props();
    draw_hacker_tornado();
    draw_hacker_cloud();
    draw_hacker_buttons();
    draw_hacker_canes();
}

static void draw_hacker_event(void) {
    if (hack_active == 0) {
        return;
    }
    draw_hacker_glitch();
    if (hack_cinematic_lock() == 1) {
        draw_hacker_blackout();
        draw_hacker_ban();
        return;
    }
    if (hack_t < hack_rain_at()) {
        double x0 = clamp(hack_term_x[0], 4, screen_w - hack_term_w() - 4);
        double y0 = clamp(hack_term_y[0], 4, screen_h - hack_term_head() - 40);
        hack_draw_term(0, x0, y0, 0);
        if (hack_second_open() == 1) {
            double x1 = clamp(hack_term_x[1], 4, screen_w - hack_term_w() - 4);
            double y1 = clamp(hack_term_y[1], 4, screen_h - hack_term_head() - 40);
            hack_draw_term(1, x1, y1, 0);
        }
    }
    draw_hacker_cane_hud();
}
