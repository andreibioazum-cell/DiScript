/* Admin panel: a small black-and-white "CCP" orb that only the admin
 * account sees. Drag it anywhere, tap it to open a compact monochrome panel
 * with two tabs - cheats and players. The panel is drawn above every screen,
 * above the transition curtain and above the epilepsy warning, so it never
 * hides itself, and nothing here prints a status line under the panel.
 * Everything is session-local and gated behind is_admin(), the same check the
 * admin chat commands use, so a copied progress file cannot summon the orb. */

static double admin_a(double rgb, double a) {
    return (1 + floor(a * 254)) * 0x1000000 + rgb;
}

static double admin_in_battle(void) {
    return game_state == ST_SOLO || game_state == ST_ONLINE;
}

/* Compact panel: a bit over half of the screen wide and only as tall as its
 * content needs, so it never stretches over the whole display. Every button in
 * it shares one flat black-and-white style. */
static double admin_pad(void) { return 10; }
static double admin_gap(void) { return 6; }
static double admin_tab = 0;
static double admin_gift_choice = 0;
/* The emerald hue given to the player: 0 is the plain rainbow, 1..7 a custom one. */
static double admin_gift_hue = 0;
static double admin_gift_busy = 0;
static char admin_gift_target[24];
static const char *admin_gift_names[5] = {"Poop", "Cake", "Teddy", "Rocket", "Emerald"};

/* Chrome heights are measured from the screen, never from the panel, so the
 * panel height stays a plain function of the content. */
static double admin_header_h(void) { return clamp(screen_h * 0.03, 36, 56); }

static double admin_panel_w(void) {
    double lim = screen_w - 24;
        /* The panel is 20% wider than before. */
    double w = screen_w * 0.70;
    double cap = screen_h * 1.1;
    if (w > cap) {
        w = cap;
    }
    if (w > lim) {
        w = lim;
    }
    if (w < 180) {
        w = 180;
    }
    return w;
}

/* Row height: one cheat button. 20% more than before. */
static double admin_row_h(void) { return clamp(screen_h * 0.066, 36, 76); }

/* Seven cheats: three rows of two and one button on the fourth row. */
#define ADMIN_CHEATS 7
static double admin_graph_rows(void) { return (ADMIN_CHEATS + 1) / 2; }
static double admin_grid_h(void) { return admin_graph_rows() * admin_row_h() + (admin_graph_rows() - 1) * admin_gap(); }

/* No tabs any more: the panel content is just the cheat grid. */
static double admin_content_h(void) { return admin_tab == 0 ? admin_grid_h() + admin_row_h() + admin_gap() : 9 * admin_row_h() + 8 * admin_gap(); }

static double admin_cheat_row_h(void) {
    double h = (admin_content_h() - 2 * admin_gap()) / 3;
    double cap = admin_row_h();
    return h > cap ? cap : h;
}

/* The header and the cheat grid set the panel height - there are no tabs. */
static double admin_panel_h(void) {
    double lim = screen_h - 32;
    double h = admin_header_h() + 2 * admin_gap() + admin_content_h() + admin_pad();
    if (h > lim) {
        h = lim;
    }
    if (h < 180) {
        h = 180;
    }
    if (h > lim) {
        h = lim;
    }
    return h;
}

static double admin_panel_x(void) { return (screen_w - admin_panel_w()) / 2; }

static double admin_panel_y(void) { return (screen_h - admin_panel_h()) / 2; }

static double admin_content_top(void) { return admin_panel_y() + admin_header_h() + 2 * admin_gap(); }

/* The cheat grid fills the whole panel content. */
static double admin_grid_top(void) { return admin_content_top() + admin_row_h() + admin_gap(); }

static double admin_ball_r(void) { return clamp(screen_h / 34.0, 15, 26); }

/* Monochrome palette: black surfaces, white outlines, grey for "on". */
static double admin_body(void) { return 0x000000; }

static double admin_header(void) { return 0x0D0D0D; }

static double admin_frame(void) { return 0xFFFFFF; }

/* Switched on / selected: the fill only turns grey. Nothing turns white, so
 * flipping a switch is barely noticeable at a glance. */
static double admin_on(void) { return 0x333333; }

static double admin_off(void) { return 0x000000; }

/* One flat style for every button of the panel: same radius, same 2 px white
 * outline, fully opaque fills and white text in both states. Off and on differ
 * in nothing but the fill colour - black against grey - so a toggle or a tab
 * has no flash, fade or highlight to look at. */
static double admin_btn_r(void) { return 9; }

static void admin_btn(const char *label, double x, double y, double w, double h, double on, double scale) {
    roundrect(x, y, w, h, admin_btn_r(), on == 1 ? admin_on() : admin_off());
    roundrect_outline(x, y, w, h, admin_btn_r(), 2, admin_frame());
    text_in_box(label, x, y, w, h, admin_frame(), scale);
}

/* The orb wears the short name of the panel. */
static const char *admin_orb_label(void) { return "CCP"; }

static const char *admin_cheat_label(double idx) {
    if (language == 1) {
        if (idx == 0) {
            return "Бессмертие";
        }
        if (idx == 1) {
            return "Урон x5";
        }
        if (idx == 2) {
            return "Скорость x2";
        }
        if (idx == 3) {
            return "Без КД";
        }
        if (idx == 4) {
            return "Анти-стан";
        }
        if (idx == 5) {
            return "Стан врага";
        }
        return "Шедевро-хитбоксы";
    }
    if (idx == 0) {
        return "God mode";
    }
    if (idx == 1) {
        return "Damage x5";
    }
    if (idx == 2) {
        return "Speed x2";
    }
    if (idx == 3) {
        return "No cooldown";
    }
    if (idx == 4) {
        return "Anti-stun";
    }
    if (idx == 5) {
        return "Stun enemy";
    }
    return "Masterpiece hitboxes";
}

static double admin_cheat_value(double idx) {
    if (idx == 0) {
        return cheat_god;
    }
    if (idx == 1) {
        return cheat_damage;
    }
    if (idx == 2) {
        return cheat_speed;
    }
    if (idx == 3) {
        return cheat_nocd;
    }
    if (idx == 4) {
        return cheat_cc;
    }
    if (idx == 5) {
        return cheat_estun;
    }
    return cheat_chaos;
}

/* True while at least one admin panel cheat switch is on: if anything is on,
 * cups must neither be awarded on a win nor deducted on a loss. */
static double admin_any_cheat_on(void) {
    if (is_admin() == 0) {
        return 0;
    }
    if (cheat_god == 1 || cheat_damage == 1 || cheat_speed == 1 || cheat_nocd == 1 || cheat_cc == 1 ||
        cheat_estun == 1 || cheat_chaos == 1) {
        return 1;
    }
    return 0;
}

/* Called when a battle starts: entering a fight with a cheat already on marks
 * it just like flipping one on during the fight does. */
static void admin_battle_stain_reset(void) {
    cheat_stain = admin_any_cheat_on();
    /* New fight: the previous admin state is forgotten, the ring makes sense again. */
    cheat_estun_mark = 0;
}

/* A stained battle neither pays Cups on a win nor deducts Cups on a loss. */
static double admin_cups_allowed(void) {
    if (is_admin() == 0) {
        return 1;
    }
    if (cheat_stain != 0) {
        return 0;
    }
    if (finished == 0 && admin_any_cheat_on() == 1) {
        return 0;
    }
    return 1;
}

static void admin_press_cheat(double idx) {
    double v = admin_cheat_value(idx);
    if (v == 1) {
        v = 0;
    } else {
        v = 1;
    }
    if (v == 1 && finished == 0) {
        /* Switching any cheat on leaves a mark on the current battle: a win
         * then pays no Cups and a loss takes none, even if flipped back off. */
        cheat_stain = 1;
    }
    if (idx == 0) {
        cheat_god = v;
    }
    if (idx == 1) {
        cheat_damage = v;
    }
    if (idx == 2) {
        cheat_speed = v;
    }
    if (idx == 3) {
        cheat_nocd = v;
    }
    if (idx == 4) {
        cheat_cc = v;
    }
    if (idx == 5) {
        cheat_estun = v;
    }
    if (idx == 6) {
        cheat_chaos = v;
        if (v == 1) {
            ds_log("admin: masterpiece hitboxes on");
        }
    }
}

static void admin_press(double idx) {
    play_button_click();
    admin_press_cheat(idx);
}

/* Cheats read by the fight code every frame. */
static double admin_cheat_god(void) {
    return is_admin() == 1 && cheat_god == 1 ? 1 : 0;
}

static double admin_cheat_damage_mult(void) {
    if (is_admin() == 1 && cheat_damage == 1) {
        return 5;
    }
    return 1;
}

static double admin_cheat_speed_mult(void) {
    if (is_admin() == 1 && cheat_speed == 1) {
        return 2;
    }
    return 1;
}

static double admin_cheat_nocd(void) {
    return is_admin() == 1 && cheat_nocd == 1 ? 1 : 0;
}

/* Joke mode switched on right here, without the web panel flag. */
static double admin_cheat_chaos(void) {
    return is_admin() == 1 && cheat_chaos == 1 ? 1 : 0;
}
