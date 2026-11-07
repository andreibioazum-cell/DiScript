/* Rendering of the H4c4k0r raid: a silent glitch intro, compact draggable
 * cmd.exe-style consoles with a green title bar whose lines type slowly, as
 * if a real cheat and a real admin sat at two keyboards. No avatars, no icons,
 * just text labels (Hacker:, Azum:, etc.). His launched events stay switched off -
 * only one static cloud hangs in the corner - and the ban arrives as a full
 * blackout (longer duration) followed by a console window that types the verdict
 * before everything explodes. */

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
    return hack_line_text(i)[0] == '/' ? HACK_CMD_CPS * 0.35 : HACK_TYPE_CPS * 0.35;
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
    
    /* Simulate typos: occasionally replace one character with another, then fix almost immediately */
    double chars = floor(n);
    if (chars > 0 && fmod(hack_t * 10, 1.0) < 0.05) {
        /* Typo occurs */
        chars = chars - 1;
    } else if (fmod(hack_t * 10, 1.0) < 0.15) {
        /* Fix typo */
        chars = floor(n);
    }
    
    if (chars < 0) {
        chars = 0;
    }
    if (chars > full) {
        chars = full;
    }
    
    return chars;
}

/* Avatar cube in front of a line: the hacker is the ordinary cube sprite,
 * Azum's is his own sprite; the warning line gets a red exclamation. */
static void hack_avatar(double x, double y, double s, double who) {
    if (who == HACK_WHO_WARN) {
        text_scaled("!", x + s * 0.30, y, 0xFF000000u | HACK_INK_WARN, s / 18.0);
        return;
    }
    /* No avatar cube or glyph icons, just text labels */
}

static double hack_ink_of(double who) {
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
    
    /* ALL CHATS IN ONE WINDOW (TOP LEFT) */
    double cy = head + pad; /* Start at top of window */
    double left = 0;
    double i = 0;
    double win_count = 0; /* Only one window now */
    
    /* Combine all lines from all windows into one list for the single chat */
    double combined_lines[HACK_LINES];
    int combined_n = 0;
    while (i < HACK_LINES) {
        if (hack_script[(int)i].win == win || win == 0) { /* win=0 used for combined */
            combined_lines[combined_n] = i;
            combined_n = combined_n + 1;
        }
        i = i + 1;
    }
    
    double shown_total = 0;
    int idx = 0;
    while (idx < combined_n) {
        double li = combined_lines[idx];
        idx = idx + 1;
        if (hack_line_start(li) > hack_t) {
            continue;
        }
        double wn = wraps[(int)li]; /* wraps calculated earlier - need to recalculate for single window */
        /* Simplified: recalculate wrap for single window width */
        const char *vis = utf8_prefix(hack_line_text(li), hack_typed_chars(li));
        double starts[HACK_WRAP_MAX];
        double lens[HACK_WRAP_MAX];
        double got = dial_wrap(vis, text_w, sc, starts, lens, HACK_WRAP_MAX);
        if (got < 1) {
            got = 1;
            starts[0] = 0;
            lens[0] = str_len(vis);
        }
        shown_total = shown_total + got;
    }
    
    /* Redraw logic for single top-left window */
    double final_h = head + 2 * pad + (shown_total > cap_lines ? cap_lines : shown_total) * step;
    roundrect(x + 3, y + 5, w, final_h, 6, 0x60000000u);
    roundrect(x, y, w, final_h, 6, 0xFF000000u | HACK_TERM_BODY);
    roundrect_outline(x, y, w, final_h, 6, 2, HACK_TERM_EDGE);
    hack_term_chrome(x, y, w, head, win, 0);
    
    double cy = head + pad;
    double skip = shown_total > cap_lines ? shown_total - cap_lines : 0;
    double shown = shown_total - skip;
    i = 0;
    double current_shown = 0;
    while (i < HACK_LINES && current_shown < shown) {
        double li = i;
        i = i + 1;
        if (hack_line_start(li) > hack_t) {
            continue;
        }
        const char *full = hack_line_text(li);
        double typed = hack_typed_chars(li);
        const char *vis = utf8_prefix(full, typed);
        double starts[HACK_WRAP_MAX];
        double lens[HACK_WRAP_MAX];
        double got = dial_wrap(vis, text_w, sc, starts, lens, HACK_WRAP_MAX);
        if (got < 1) {
            got = 1;
            starts[0] = 0;
            lens[0] = str_len(vis);
        }
        if (left >= got) {
            left = left - got;
            continue;
        }
        double first = left;
        left = 0;
        double who = hack_line_who(li);
        uint32_t ink = 0xFF000000u | (unsigned)hack_ink_of(who);
        
        /* Draw label instead of icon, ALL IN ONE CHAT (TOP LEFT) */
        const char *label = "Hacker:";
        if (who == HACK_WHO_WARN) {
            label = "[Warning]:";
        } else if (who == HACK_WHO_AZUM) {
            label = "Azum:";
        }
        text_scaled(label, x + pad, cy, ink, sc * 0.8);
        
        /* Prevent text from going outside the panel (w) */
        double max_text_w = w - pad * 2 - text_ink_width(label) * sc * 0.8 - 12;
        if (text_w > max_text_w) {
            text_w = max_text_w;
        }
        
        double k = first;
        while (k < got && current_shown < shown) {
            const char *span = dial_slice(vis, starts[(int)k], lens[(int)k]);
            text_scaled(span, x + pad + text_ink_width(label) * sc * 0.8 + 6, cy, ink, sc);
            cy = cy + step;
            k = k + 1;
            current_shown = current_shown + 1;
        }
        if (typed < utf8_len(full) && fmod(event_t * 3, 2) < 1 && current_shown < shown) {
            const char *last = dial_slice(vis, starts[(int)(got - 1)], lens[(int)(got - 1)]);
            double cw = text_ink_width(last) * sc;
            rect(x + pad + text_ink_width(label) * sc * 0.8 + 6 + cw + 1, cy - step, 5 * sc + 2, step * 0.72, ink);
        }
    }
    return final_h;
}