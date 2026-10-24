/* New Year battle pass: 7 tiers. Winning battles earns XP (a win is
 * 10 XP, see award_battle_result); reaching a tier's XP unlocks it, and
 * opening (claiming) it costs candies. Tiers are claimed in order:
 *   1:  10 XP,  12 candies -> +10 cups
 *   2:  30 XP,  20 candies -> +20 cups
 *   3:  60 XP,  35 candies -> +30 cups
  *   4: 100 XP,  50 candies -> Elf ebuc skin
 *   5: 150 XP,  70 candies -> +60 cups
 *   6: 220 XP,  95 candies -> +120 candies
  *   7: 300 XP, 120 candies -> Santa class
 * XP is the cloud profile field "bpx"; claimed tiers ride the existing
 * "bp" profile field (bp_claimed). */

#define BP_TIERS 7

static double bp_msg_t;
static char bp_msg_buf[160];
static const char *bp_msg;

static double bp_xp_req(double t) {
    if (t <= 1) return 10;
    if (t == 2) return 30;
    if (t == 3) return 60;
    if (t == 4) return 100;
    if (t == 5) return 150;
    if (t == 6) return 220;
    return 300;
}

static double bp_cost(double t) {
    if (t <= 1) return 12;
    if (t == 2) return 20;
    if (t == 3) return 35;
    if (t == 4) return 50;
    if (t == 5) return 70;
    if (t == 6) return 95;
    return 120;
}

static void bp_set_msg(const char *text) {
    double i = 0;
    if (!text) {
        text = "";
    }
    while (text[(int)i] != 0 && i < (double)(sizeof(bp_msg_buf) - 1)) {
        bp_msg_buf[(int)i] = text[(int)i];
        i = i + 1;
    }
    bp_msg_buf[(int)i] = 0;
    bp_msg = bp_msg_buf;
    bp_msg_t = msg_time;
}

static const char *bp_reward_text(double t) {
    if (t == 1) return ds_concat("+10 ", tr_cups_unit());
    if (t == 2) return ds_concat("+20 ", tr_cups_unit());
    if (t == 3) return ds_concat("+30 ", tr_cups_unit());
    if (t == 4) return language == 1 ? "Скин: Эльф-буК" : "Skin: Elf ebuC";
    if (t == 5) return ds_concat("+60 ", tr_cups_unit());
    if (t == 6) return ds_concat("+120 ", tr_candies_unit());
    return language == 1 ? "Класс: Дед Мороз" : "Class: Santa";
}

static void bp_grant(double t) {
    if (t == 1) {
        cups = cups + 10;
    } else if (t == 2) {
        cups = cups + 20;
    } else if (t == 3) {
        cups = cups + 30;
    } else if (t == 5) {
        cups = cups + 60;
    } else if (t == 6) {
        candies = candies + 120;
    } else if (t == 7) {
        set_class_owned(CLASS_SANTA, 1);
    }
        /* tier 4 is the Elf ebuc skin itself: skin_unlocked_for() opens it
     * once bp_claimed reaches 4, nothing else to grant. */
}

static void bp_claim(double t) {
    if (is_admin() != 1) {
        return;
    }
    if (t <= bp_claimed) {
        return;
    }
    if (t > bp_claimed + 1) {
        bp_set_msg(language == 1 ? "Сначала открой прошлый уровень" : "Open the previous tier first");
        return;
    }
    if (bp_xp < bp_xp_req(t)) {
        bp_set_msg(language == 1 ? "Не хватает XP - выигрывай бои" : "Not enough XP - win battles");
        return;
    }
    if (candies < bp_cost(t)) {
        bp_set_msg(language == 1 ? "Не хватает конфет" : "Not enough candies");
        return;
    }
    candies = candies - bp_cost(t);
    bp_claimed = t;
    bp_grant(t);
    save_progress();
    check_achievements();
    bp_set_msg(ds_concat(language == 1 ? "Получено: " : "Claimed: ", bp_reward_text(t)));
    ds_log("battle pass: tier %g claimed, xp %g", t, bp_xp);
}

/* Rows are ordinary game buttons now: same purple face, same press squeeze,
 * same disabled grey. A claimed tier is green instead of grey, so what is
 * already yours reads at a glance. They shrink so all seven tiers fit above
 * the status line on short screens instead of running off the bottom.
 * The whole pass is admin-only: the lobby does not draw its button for
 * anyone else, and this screen turns non-admins away (see touch_bpass). */
static double bp_rows_top(void) { return content_top() + 88; }
static double bp_row_h(void) {
    double stride = (screen_h - bp_rows_top() - 70) / BP_TIERS;
    return clamp(stride - 6, 22, 58);
}
static double bp_row_y(double t) { return bp_rows_top() + (t - 1) * (bp_row_h() + 6); }
static double bp_row_x(void) { return center_x(fit_w(620)); }
static double bp_row_w(void) { return fit_w(620); }

static const char *bp_status_text(double t) {
    if (t <= bp_claimed) {
        return language == 1 ? "получено" : "claimed";
    }
    if (bp_xp < bp_xp_req(t)) {
        return ds_concat(ds_concat(language == 1 ? "нужно " : "needs ", ds_num_to_string(bp_xp_req(t))), " XP");
    }
    if (t > bp_claimed + 1) {
        return language == 1 ? "сначала прошлый" : "previous first";
    }
    return ds_concat(ds_concat(ds_num_to_string(bp_cost(t)), ds_concat(" ", tr_candies_unit())),
                     language == 1 ? " · открыть" : " · open");
}

/* One tier as a normal button: "3. +30 cups, 35 candies, unlock". The
 * row is dead grey until it can actually be claimed. */
static void bp_draw_row(double t) {
    double x = bp_row_x(), y = bp_row_y(t), w = bp_row_w(), h = bp_row_h();
    const char *title = ds_concat(ds_concat(ds_num_to_string(t), ". "), bp_reward_text(t));
    const char *label = ds_concat(title, ds_concat(" · ", bp_status_text(t)));
    double sc = fit_text_scale(label, w - 24, 0.58);
    double ready = t == bp_claimed + 1 && bp_xp >= bp_xp_req(t);
    if (t <= bp_claimed) {
        button_scaled_face(label, x, y, w, h, sc, 0xFF2F9E44);
    } else if (ready == 1) {
        button_scaled(label, x, y, w, h, sc);
    } else {
        button_disabled_scaled(label, x, y, w, h, sc);
    }
}

static void draw_bpass(void) {
    if (is_admin() != 1) {
        screen_fill();
        draw_back();
        return;
    }
    screen_fill();
    ctext_scaled(language == 1 ? "Новогодний баттл-пасс" : "New Year Battle Pass", content_top() + 6, newyear_gold + 0xFF000000,
                 fit_text_scale(language == 1 ? "Новогодний баттл-пасс" : "New Year Battle Pass", screen_w - 32, 0.85));
    double next = bp_claimed + 1;
    const char *xp_line;
    if (next > BP_TIERS) {
        xp_line = ds_concat(language == 1 ? "XP: " : "XP: ", ds_num_to_string((double)(int)bp_xp));
    } else {
        xp_line = ds_concat(
            ds_concat(ds_concat(language == 1 ? "XP: " : "XP: ", ds_num_to_string((double)(int)bp_xp)), " / "),
            ds_num_to_string(bp_xp_req(next)));
    }
    ctext_scaled(ds_concat(xp_line, language == 1 ? " (победа = 10 XP)" : " (a win = 10 XP)"), content_top() + 44,
                 text_color, 0.55);
    double t = 1;
    while (t <= BP_TIERS) {
        bp_draw_row(t);
        t = t + 1;
    }
    bp_msg_t = tick_down(bp_msg_t);
    draw_msg(bp_msg, bp_row_y(BP_TIERS) + bp_row_h() + 16, bp_msg_t);
    draw_back();
}

static void touch_bpass(double x, double y, double action) {
    if (is_admin() != 1) {
        start_transition(ST_LOBBY);
        return;
    }
    if (action != 0) {
        return;
    }
    if (back_hit(x, y) == 1) {
        start_transition(ST_LOBBY);
        return;
    }
    double t = 1;
    while (t <= BP_TIERS) {
        if (hit_button(x, y, bp_row_x(), bp_row_y(t), bp_row_w(), bp_row_h()) == 1) {
            bp_claim(t);
            return;
        }
        t = t + 1;
    }
}

static double bpass_any_hit(double x, double y) {
    if (back_hit(x, y) == 1) {
        return 1;
    }
    if (is_admin() != 1) {
        return 0;
    }
    double t = 1;
    while (t <= BP_TIERS) {
        if (hit_button(x, y, bp_row_x(), bp_row_y(t), bp_row_w(), bp_row_h()) == 1) {
            return 1;
        }
        t = t + 1;
    }
    return 0;
}
