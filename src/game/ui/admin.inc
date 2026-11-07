/* Draggable monochrome admin orb and control panel: Cheats, Gifts (root
 * admin only) and Admin commands (root admins, see admin_commands.inc). */

static double admin_a(double rgb, double a) {
    return (1 + floor(a * 254)) * 0x1000000 + rgb;
}

static double admin_in_battle(void) {
    return game_state == ST_SOLO || game_state == ST_ONLINE;
}

/* Admin panel layout metrics. */
static double admin_pad(void) { return 10; }
static double admin_gap(void) { return 6; }
static double admin_tab = 0;
static double admin_gift_choice = 0;
static double admin_gift_busy = 0;
static char admin_gift_target[24];
static const char *admin_gift_names[NET_GIFT_TYPE_COUNT] = {"Poop", "Cake", "Teddy", "Rocket", "Emerald"};

static double admin_header_h(void) { return clamp(screen_h * 0.03, 36, 56); }

static double admin_panel_w(void) {
    double lim = screen_w - 24;
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

static double admin_row_h(void) { return clamp(screen_h * 0.066, 36, 76); }

#define ADMIN_CHEATS 9
static double admin_graph_rows(void) { return (ADMIN_CHEATS + 1) / 2; }
static double admin_grid_h(void) { return admin_graph_rows() * admin_row_h() + (admin_graph_rows() - 1) * admin_gap(); }

static double admin_content_h(void) {
    if (admin_tab == 2 && admin_commands_allowed() == 1) {
        return admin_row_h() + admin_gap() + admin_cmd_content_h();
    }
    if (admin_tab == 1 && admin_gifts_allowed() == 1) {
        return 9 * admin_row_h() + 8 * admin_gap();
    }
    return admin_grid_h() + admin_row_h() + admin_gap();
}

static double admin_cheat_row_h(void) {
    double h = (admin_content_h() - 2 * admin_gap()) / 3;
    double cap = admin_row_h();
    return h > cap ? cap : h;
}

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

static double admin_grid_top(void) { return admin_content_top() + admin_row_h() + admin_gap(); }

static double admin_ball_r(void) { return clamp(screen_h / 34.0, 15, 26); }

/* Monochrome admin palette. */
static double admin_body(void) { return 0x000000; }

static double admin_header(void) { return 0x0D0D0D; }

static double admin_frame(void) { return 0xFFFFFF; }

static double admin_on(void) { return 0x333333; }

static double admin_off(void) { return 0x000000; }

/* Flat opaque controls: no bright perimeter around every button. */
static double admin_btn_r(void) { return 12; }

static void admin_btn(const char *label, double x, double y, double w, double h, double on, double scale) {
    roundrect(x, y, w, h, admin_btn_r(), on == 1 ? admin_on() : admin_off());
    text_in_box(label, x, y, w, h, admin_frame(), scale);
}

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
        if (idx == 6) {
            return "Отскоки буКа";
        }
        if (idx == 7) {
            return "Авто-блок";
        }
        return "Взлом H4c4k0r";
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
    if (idx == 6) {
        return "ebuC's bounces";
    }
    if (idx == 7) {
        return "Auto block";
    }
    return "H4c4k0r raid";
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
    if (idx == 6) {
        return cheat_ball_path;
    }
    if (idx == 7) {
        return cheat_autoblock;
    }
    return cheat_hack;
}

/* Return 1 while any admin cheat switch is active. */
static double admin_any_cheat_on(void) {
    if (admin_modifiers_allowed() == 0) {
        return 0;
    }
    if (cheat_god == 1 || cheat_damage == 1 || cheat_speed == 1 || cheat_nocd == 1 || cheat_cc == 1 ||
        cheat_estun == 1 || cheat_ball_path == 1 || cheat_autoblock == 1 || cheat_hack == 1) {
        return 1;
    }
    return 0;
}

/* Initialize battle cheat-stain flag at match start. */
static void admin_battle_stain_reset(void) {
    cheat_stain = admin_any_cheat_on();
    cheat_estun_mark = 0;
}

/* Return 0 when active or prior cheat use disables battle cup payout. */
static double admin_cups_allowed(void) {
    if (admin_modifiers_allowed() == 0) {
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
        /* Mark active battle stained so cups are unchanged at match end. */
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
        cheat_ball_path = v;
        if (v == 1) {
            ds_log("admin: ebuC's bounce path on");
        }
    }
    if (idx == 7) {
        cheat_autoblock = v;
        ds_log(v == 1 ? "admin: auto block on" : "admin: auto block off");
    }
    if (idx == 8) {
        cheat_hack = v;
        ds_log(v == 1 ? "admin: H4c4k0r raid replay on" : "admin: H4c4k0r raid replay off");
    }
}

static void admin_press(double idx) {
    play_button_click();
    admin_press_cheat(idx);
}

/* Cheats read by the fight code every frame. */
static double admin_cheat_god(void) {
    return admin_modifiers_allowed() == 1 && cheat_god == 1 ? 1 : 0;
}

static double admin_cheat_damage_mult(void) {
    if (admin_modifiers_allowed() == 1 && cheat_damage == 1) {
        return 5;
    }
    return 1;
}

static double admin_cheat_speed_mult(void) {
    if (admin_modifiers_allowed() == 1 && cheat_speed == 1) {
        return 2;
    }
    return 1;
}

static double admin_cheat_nocd(void) {
    return admin_modifiers_allowed() == 1 && cheat_nocd == 1 ? 1 : 0;
}

static double admin_cheat_ball_path(void) {
    return admin_modifiers_allowed() == 1 && cheat_ball_path == 1 ? 1 : 0;
}

static double admin_cheat_autoblock(void) {
    return admin_modifiers_allowed() == 1 && cheat_autoblock == 1 ? 1 : 0;
}

/* Local replay switch for the H4c4k0r raid scene (event 7). */
static double admin_cheat_hack(void) {
    return admin_modifiers_allowed() == 1 && cheat_hack == 1 ? 1 : 0;
}

/* Visible tab count and slot->tab mapping: Cheats always, Gifts for the root
 * admin, Admin commands for both root admins. Slots stay in that order. */
static double admin_tab_slots(void) {
    return 1 + (admin_gifts_allowed() == 1 ? 1 : 0) + (admin_commands_allowed() == 1 ? 1 : 0);
}

static double admin_tab_at_slot(double slot) {
    if (slot == 0) {
        return 0;
    }
    if (admin_gifts_allowed() == 1) {
        return slot == 1 ? 1 : 2;
    }
    return 2;
}

static const char *admin_tab_label(double tab) {
    if (tab == 1) {
        return language == 1 ? "Подарки" : "Gifts";
    }
    if (tab == 2) {
        return language == 1 ? "Админ команды" : "Admin commands";
    }
    return language == 1 ? "Читы" : "Cheats";
}

static void admin_draw_tabs(void) {
    double px = admin_panel_x() + admin_pad();
    double y = admin_content_top();
    double h = admin_row_h();
    double full = admin_panel_w() - 2 * admin_pad();
    double n = admin_tab_slots();
    if (n == 1) {
        admin_btn(language == 1 ? "Модификаторы" : "Modifiers", px, y, full, h, 1, 0.55);
        return;
    }
    double w = (full - (n - 1) * admin_gap()) / n;
    double slot = 0;
    while (slot < n) {
        double tab = admin_tab_at_slot(slot);
        const char *label = admin_tab_label(tab);
        admin_btn(label, px + slot * (w + admin_gap()), y, w, h, admin_tab == tab,
                  fit_text_scale(label, w - 10, 0.55));
        slot = slot + 1;
    }
}
