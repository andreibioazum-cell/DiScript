/* Lobby NPC cube and UTF-8 typewriter dialogue panel. */

static const char *helpy_dialogue_text(void) {
    if (language == 1) {
        if (cube_npc_dialogue_line == 0) return "Привет! Я Хелпи.";
        if (cube_npc_dialogue_line == 1) return "Я помогу тебе освоиться в лобби.";
        if (cube_npc_dialogue_line == 2) return "Выбирай класс и отправляйся в бой!";
        if (cube_npc_dialogue_line == 3) return "Не забудь потренировать блок.";
        return "Увидимся на арене!";
    }
    if (cube_npc_dialogue_line == 0) return "Hi! I'm Helpy.";
    if (cube_npc_dialogue_line == 1) return "I'll help you around the lobby.";
    if (cube_npc_dialogue_line == 2) return "Pick a class and jump into battle!";
    if (cube_npc_dialogue_line == 3) return "Remember to practice your block.";
    return "See you in the arena!";
}

static const char *cube_npc_dialogue_text(void) {
    if (cube_npc_speaker == 1) return helpy_dialogue_text();
    if (cube_npc_dialogue_line == 0) return cube_npc_d0;
    if (cube_npc_dialogue_line == 1) return cube_npc_d1;
    if (cube_npc_dialogue_line == 2) return cube_npc_d2;
    if (cube_npc_dialogue_line == 3) return cube_npc_d3;
    if (cube_npc_dialogue_line == 4) return cube_npc_d4;
    return "";
}

/* Count UTF-8 codepoints in string. */
static double utf8_len(const char *s) {
    double n = 0;
    while (*s != 0) {
        if (((unsigned char)*s & 0xC0) != 0x80) {
            n = n + 1;
        }
        s = s + 1;
    }
    return n;
}

static const char *utf8_prefix(const char *s, double chars) {
    static char buf[512];
    double seen = 0;
    size_t n = 0;
    const char *p = s;
    if (chars <= 0) {
        buf[0] = 0;
        return buf;
    }
    while (*p != 0 && seen < chars) {
        unsigned char c = (unsigned char)*p;
        p = p + 1;
        n = n + 1;
        if ((c & 0xC0) != 0x80) {
            seen = seen + 1;
            /* Include trailing UTF-8 continuation bytes in current codepoint. */
            while (((unsigned char)*p & 0xC0) == 0x80) {
                p = p + 1;
                n = n + 1;
            }
        }
    }
    if (n > 511) {
        n = 511;
    }
    memcpy(buf, s, n);
    buf[n] = 0;
    return buf;
}

/* Copy substring byte slice into static scratch buffer. */
static const char *dial_slice(const char *s, double off, double len) {
    static char buf[512];
    size_t total = strlen(s);
    size_t start = (size_t)off;
    size_t want = (size_t)len;
    if (start > total) {
        start = total;
    }
    if (want > total - start) {
        want = total - start;
    }
    if (want > 511) {
        want = 511;
    }
    memcpy(buf, s + start, want);
    buf[want] = 0;
    return buf;
}

static double helpy_npc_draw_sz = 512;
static double helpy_npc_x;
static double helpy_npc_y;
static double helpy_npc_hit_r = 256;

/* The dialogue window: big portrait, big text, kept low. */
static double dial_panel_h = 150;
static double dial_panel_margin = 14;
static double dial_text_pad = 20;
static double dial_portrait_sz = 96;
static double dial_text_sc = 1.05;
static double dial_anim;
static double dial_anim_time = 0.18;

static void cube_npc_tap(double speaker) {
    if (cube_npc_dialogue_active == 0 || speaker != cube_npc_speaker) {
        cube_npc_speaker = speaker;
        cube_npc_dialogue_active = 1;
        cube_npc_dialogue_line = 0;
        cube_npc_dialogue_chars = 0;
        return;
    }
    double max_chars = utf8_len(cube_npc_dialogue_text());
    if (cube_npc_dialogue_chars < max_chars) {
        cube_npc_dialogue_chars = max_chars;
        return;
    }
    double next = cube_npc_dialogue_line + 1;
    if (next >= cube_npc_dialogue_lines) {
        /* Preserve last line during fade-out; cube_npc_tick() rewinds after close. */
        cube_npc_dialogue_active = 0;
        return;
    }
    cube_npc_dialogue_line = next;
    cube_npc_dialogue_chars = 0;
}

/* Panel slide/fade, once per lobby frame. */
static void cube_npc_tick(void) {
    double target = 1;
    double step = dt / dial_anim_time;
    if (cube_npc_dialogue_active != 1) {
        target = 0;
    }
    if (dial_anim < target) {
        dial_anim = dial_anim + step;
        if (dial_anim > target) {
            dial_anim = target;
        }
    } else if (dial_anim > target) {
        dial_anim = dial_anim - step;
        if (dial_anim < target) {
            dial_anim = target;
        }
    }
    if (cube_npc_dialogue_active != 1 && dial_anim <= 0) {
        /* Fully closed: rewind the typewriter for the next greeting. */
        cube_npc_dialogue_line = 0;
        cube_npc_dialogue_chars = 0;
    }
}

/* Wrap text on ASCII space boundaries into up to `max_lines` spans. */
static double dial_wrap(const char *vis, double max_w, double sc, double *starts, double *lens,
                        double max_lines) {
    double total = str_len(vis);
    double n = 0;
    double line_start = 0;
    double i = 0;
    while (i < total && n < max_lines) {
        double wend = i;
        while (wend < total && vis[(int)wend] != ' ') {
            wend = wend + 1;
        }
        double w = text_ink_width(dial_slice(vis, line_start, wend - line_start)) * sc;
        if (w > max_w && wend > i && i > line_start) {
            /* The next word does not fit: close the line before it. */
            starts[(int)n] = line_start;
            lens[(int)n] = i - 1 - line_start;
            n = n + 1;
            line_start = i;
        } else if (w > max_w && wend > i) {
            /* One word alone is wider than the panel: give it its own line. */
            starts[(int)n] = line_start;
            lens[(int)n] = wend - line_start;
            n = n + 1;
            i = wend + 1;
            line_start = i;
        } else {
            i = wend + 1;
        }
    }
    if (n < max_lines && total > line_start) {
        double lend = total;
        while (lend > line_start && vis[(int)lend - 1] == ' ') {
            lend = lend - 1;
        }
        if (lend > line_start) {
            starts[(int)n] = line_start;
            lens[(int)n] = lend - line_start;
            n = n + 1;
        }
    }
    return n;
}

static void draw_lobby_npc_sprite(void) {
    /* Replace the old left-side cube NPC with Helpy's full 512px sprite. */
    helpy_npc_draw_sz = 512;
    double fit_w = screen_w - 20;
    double fit_h = screen_h - dial_panel_h - 28;
    if (helpy_npc_draw_sz > fit_w) helpy_npc_draw_sz = fit_w;
    if (helpy_npc_draw_sz > fit_h) helpy_npc_draw_sz = fit_h;
    if (helpy_npc_draw_sz < 1) helpy_npc_draw_sz = 1;
    helpy_npc_hit_r = helpy_npc_draw_sz / 2;
    helpy_npc_x = 20;
    helpy_npc_y = screen_h - helpy_npc_draw_sz - dial_panel_h - 14;
    double helpy_bob = sin(cube_npc_bob_t * 2.0) * 3;
    if (helpy_npc_tex_ok == 1) {
        tex(helpy_npc_x, helpy_npc_y + helpy_bob, HELPY_NPC_TEX, 0, helpy_npc_draw_sz / 512.0);
    } else {
        roundrect(helpy_npc_x, helpy_npc_y + helpy_bob, helpy_npc_draw_sz, helpy_npc_draw_sz, 9, 0xFF383838);
    }
    draw_overhead_nick(language == 1 ? "Хелпи" : "Helpy", helpy_npc_x + helpy_npc_draw_sz / 2,
                       helpy_npc_y + helpy_bob, 0xFFDDDDDD);
}

static void draw_lobby_npc_dialogue(void) {
    /* === Helpy dialogue panel: slides up and fades in from below === */
    double e = dial_anim;
    if (cube_npc_dialogue_active != 1 && e <= 0) {
        return;
    }
    double es = e * e * (3 - 2 * e);
    double slide = (1 - es) * 46;
    double alpha = es;
    double pm = dial_panel_margin;
    double pw = screen_w - 2 * pm;
    double ph = dial_panel_h;
    double px = pm;
    double py = screen_h - ph - pm + slide;
    double a8 = floor(alpha * 255);
    /* Soft drop shadow under the panel */
    roundrect(px + 3, py + 9, pw, ph, 12, floor(alpha * 110) * 16777216);
    /* Panel background - dark gray */
    roundrect(px, py, pw, ph, 12, a8 * 16777216 + 0x1A1A1A);
    roundrect_outline(px, py, pw, ph, 12, 2, a8 * 16777216 + 0xCCCCCC);
    /* Helpy portrait on the left */
    double ptm = (ph - dial_portrait_sz) / 2;
    double ptx = px + 12;
    double pty = py + ptm;
    const char *portrait_tex = cube_npc_speaker == 1 ? HELPY_NPC_TEX : CUBE_NPC_TEX;
    double portrait_loaded = cube_npc_speaker == 1 ? helpy_npc_tex_ok : cube_npc_tex_ok;
    if (portrait_loaded == 1) {
        tex_alpha(ptx, pty, portrait_tex, 0, dial_portrait_sz / 512.0, alpha);
    } else {
        roundrect(ptx, pty, dial_portrait_sz, dial_portrait_sz, 8, a8 * 16777216 + 0x8040FF);
    }
    /* Typewriter text - right of the portrait, big and word-wrapped. */
    const char *full = cube_npc_dialogue_text();
    double shown = floor(cube_npc_dialogue_chars);
    double len = utf8_len(full);
    if (shown > len) {
        shown = len;
    }
    const char *visible = utf8_prefix(full, shown);
    double sc = dial_text_sc;
    double text_area_x = ptx + dial_portrait_sz + 14;
    double text_area_w = px + pw - text_area_x - dial_text_pad;
    double starts[3];
    double lens[3];
    double nlines = dial_wrap(visible, text_area_w, sc, starts, lens, 3);
    if (nlines <= 0) {
        return;
    }
    double line_step = text_ink_height("Ag") * sc * 1.35;
    const char *line0 = dial_slice(visible, starts[0], lens[0]);
    double th = text_ink_height(line0) * sc;
    double tt = text_ink_top(line0) * sc;
    double block_h = (nlines - 1) * line_step + th;
    double y0 = py + (ph - block_h) / 2 - tt;
    double text_c = a8 * 16777216 + 0xFFFFFF;
    double k = 0;
    while (k < nlines) {
        text_scaled(dial_slice(visible, starts[(int)k], lens[(int)k]), (float)text_area_x,
                    (float)(y0 + k * line_step), (uint32_t)text_c, (float)sc);
        k = k + 1;
    }
}

static double hit_npc_helpy(double px, double py) {
    return hit_rect(px, py, helpy_npc_x, helpy_npc_y, helpy_npc_draw_sz, helpy_npc_draw_sz);
}
