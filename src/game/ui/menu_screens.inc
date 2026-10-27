static void draw_menu_button(double row, const char *label) {
    double w = menu_btn_w();
    /* Draw lobby button at uniform text scale. */
    button(label, menu_x(), menu_row(row), w, btn_h);
}

static void draw_lobby_nick(void) {
    if (net_login_status() != 2) {
        return;
    }
    const char *nick = net_login_nick();
    if (str_len(nick) == 0) {
        return;
    }
    const char *pre = tr_hello_pre();
    const char *post = tr_hello_post();
    double scale = 0.5;
    double pw = text_ink_width(pre) * scale;
    double nw = text_ink_width(nick) * scale;
    double qw = text_ink_width(post) * scale;
    double x = lobby_label_x(pw + nw + qw);
    double y = lobby_hello_y();
    text_scaled(pre, x, y, text_color, scale);
    draw_styled_nick(nick, x + pw, y, text_color, scale);
    text_scaled(post, x + pw + nw, y, text_color, scale);
}

static void draw_lobby_money(void) {
    const char *cup_s = ds_concat(ds_concat(tr_cups(), ": "), ds_num_to_string((double)(cups)));
    if (candies_on() == 0) {
        text_scaled(cup_s, lobby_label_x(text_ink_width(cup_s) * money_scale), lobby_money_y(), text_color,
                    money_scale);
        return;
    }
    const char *cd_s = ds_concat(ds_concat(tr_candies(), ": "), ds_num_to_string((double)(candies)));
    double lw = text_ink_width(cup_s) * money_scale;
    double rw = text_ink_width(cd_s) * money_scale;
    money_pair(cup_s, cd_s, lobby_label_x(lw + rw + money_gap), lobby_money_y(), money_scale, money_gap);
}

static void draw_lobby(void) {
    check_achievements();
    screen_fill();
    text("Cubic Battle 4", lobby_label_x(text_ink_width("Cubic Battle 4")), lobby_title_y(), text_color);
    if (newyear_active() == 1) {
        const char *banner = tr_newyear_banner();
        text_scaled(banner, lobby_label_x(text_ink_width(banner) * 0.6), lobby_title_y() + 34,
                    newyear_gold + 0xFF000000, 0.6);
    }
    draw_menu_button(0, tr_play());
    draw_menu_button(1, tr_shop());
    draw_menu_button(2, tr_settings());
    draw_menu_button(3, tr_account());
    draw_menu_button(4, tr_achievements());
    if (bpass_allowed() == 1) {
        button(language == 1 ? "Баттл-пасс" : "Battle Pass", bpass_corner_x(), bpass_corner_y(),
               bpass_corner_w(), btn_h);
    }
    draw_lobby_nick();
    draw_lobby_money();
    if (new_theme == 1) {
        draw_lobby_npc_cube();
    }
    if (str_len(banned_msg) > 0) {
        ctext(banned_msg, lobby_msg_y(), text_color);
    }
}

static void draw_modes(void) {
    screen_fill();
    button(tr_back(), menu_x(), modes_row(0), menu_btn_w(), btn_h);
    button(tr_solo(), menu_x(), modes_row(1), menu_btn_w(), btn_h);
    button(tr_online(), menu_x(), modes_row(2), menu_btn_w(), btn_h);
    button(tr_top(), menu_x(), modes_row(3), menu_btn_w(), btn_h);
    if (seasonal_admin_allowed() == 1) {
        button_scaled(tr_winter_toggle(), menu_x(), modes_row(4), menu_btn_w(), btn_h, 0.8);
    } else {
        /* Winter theme is available only to signed-in CCP moderators. */
        button_disabled_scaled(tr_winter_toggle(), menu_x(), modes_row(4), menu_btn_w(), btn_h, 0.8);
    }
    /* The promo code lives in the bottom-right corner, out of the column. */
    button(tr_promo(), promo_corner_x(), promo_corner_y(), promo_corner_w(), btn_h);
}

static void draw_settings(void) {
    screen_fill();
    button(tr_back(), menu_x(), settings_row_y(0), menu_btn_w(), settings_btn_h());
    button("English", menu_x(), settings_row_y(1), menu_btn_w(), settings_btn_h());
    button("Русский", menu_x(), settings_row_y(2), menu_btn_w(), settings_btn_h());
    button(tr_hitboxes_toggle(), menu_x(), settings_row_y(3), menu_btn_w(), settings_btn_h());
    button(tr_theme_label(), menu_x(), settings_row_y(4), menu_btn_w(), settings_btn_h());
    button_scaled(tr_legal(), menu_x(), settings_row_y(5), menu_btn_w(), settings_btn_h(),
                  fit_text_scale(tr_legal(), menu_btn_w() - 24, 0.8));
}

static void draw_card_look(double look, double x, double y, double scale) {
    if (look != LOOK_NONE) {
        tex(x, y, look_idle_tex(look), 0, scale);
        return;
    }
    double side = 50 * scale;
    rect(x, y, side, side, fighter_fallback_rgb);
}

/* Draw rounded class portrait, falling back to battle sprite until card art uploads. */
static void draw_class_render(double cls, double skin, double x, double y, double target_px) {
    if (class_card_render_ready(cls, skin) == 1) {
        tex_round(x, y, class_card_tex(cls, skin), target_px / CARD_RENDER_PX, target_px * card_render_radius_frac);
        return;
    }
    draw_card_look(card_look(cls, skin), x, y, target_px / 50);
}

/* Draw shop class card with unified press squeeze. */
static void draw_class_card(double vi) {
    double cls = visible_class_at(vi);
    double x = classes_card_x(vi);
    double w = classes_card_w();
    double top = classes_top();
    double selected = 0;
    if (player_class == cls && class_owned_of(cls) == 1) {
        selected = 1;
    }
    double p = button_press_k(x, top, w, class_card_h);
    double s = 1 - PRESS_SQUEEZE * p;
    double bx = squeeze_x(x, w, s);
    double bw = w * s;
    uint32_t tc = shade(text_color, 1 - PRESS_SHADE * p);
    card_state(bx, squeeze_y(top, top, class_card_h, s), bw, class_card_h * s, selected, 0, 1 - PRESS_SHADE * p);
    double sprite = class_render_px() * s;
    double sx = bx + (bw - sprite) / 2;
    /* Render base class appearance in shop regardless of equipped skin. */
    draw_class_render(cls, SKIN_NORMAL, sx, squeeze_y(class_sprite_y(), top, class_card_h, s), sprite);
    centered_text(class_name_of(cls), bx, bw, squeeze_y(class_title_y(), top, class_card_h, s),
                  cls == CLASS_AZUM && azum_buff_active == 1 ? shade(0xFFFF3030, 1 - PRESS_SHADE * p) : tc,
                  class_title_scale() * s);
    const char *motto = class_desc_of(cls);
    centered_text(motto, bx, bw, squeeze_y(class_desc_y(), top, class_card_h, s), tc,
                  fit_text_scale(motto, w - 16, 0.5) * s);
    centered_text(ds_concat(ds_concat(tr_level(), " "), ds_num_to_string((double)(class_level_of(cls)))), bx, bw,
                  squeeze_y(class_level_y(), top, class_card_h, s), tc, 0.45 * s);
    if (flash_sale_active == 1 && class_owned_of(cls) == 0) {
        /* A discounted price is red, so the sale is visible at a glance. */
        button_scaled_color(class_pick_label(cls), bx + class_pick_inset() * s,
                            squeeze_y(class_pick_y(), top, class_card_h, s), class_pick_w() * s, class_pick_h * s,
                            0.55 * s, 0xFFFF3030);
    } else {
        button_scaled(class_pick_label(cls), bx + class_pick_inset() * s,
                      squeeze_y(class_pick_y(), top, class_card_h, s), class_pick_w() * s, class_pick_h * s, 0.55 * s);
    }
}

static void draw_class_stats(void) {
    draw_imbalance_stars();
    const char *smotto = class_desc_of(player_class);
    ctext_scaled(smotto, stat_motto_y(), text_color, fit_text_scale(smotto, screen_w - 2 * screen_margin, 0.8));
}

static void draw_classes(void) {
    screen_fill();
    if (flash_sale_active == 1) {
        ctext_scaled(language == 1 ? "ФЛЕШ-РАСПРОДАЖА -10%" : "FLASH SALE -10%", classes_top() - 34, 0xFFFF3030, 0.6);
    }
    classes_scroll_clamp();
    if (classes_scroll_needed() == 1) {
        ds_gfx_scissor(screen_margin - 2, classes_top() - 8, classes_viewport_w() + 4, class_card_h + 16);
    }
    double vi = 0;
    double last = classes_visible_count();
    while (vi < last) {
        double cx = classes_card_x(vi);
        if (cx + classes_card_w() > 0 && cx < screen_w) {
            draw_class_card(vi);
        }
        vi = vi + 1;
    }
    if (classes_scroll_needed() == 1) {
        ds_gfx_scissor(0, 0, 0, 0);
    }
    if (classes_scroll_needed() == 1) {
        /* Draw proportional horizontal scrollbar when strip overflows viewport. */
        double track_x = screen_margin;
        double track_w = classes_viewport_w();
        double track_y = classes_top() - 14;
        roundrect(track_x, track_y, track_w, 5, 2, card_bg);
        double thumb_w = fmax(28, track_w * classes_viewport_w() / classes_strip_w());
        if (thumb_w > track_w) thumb_w = track_w;
        double thumb_x = track_x + (track_w - thumb_w) * classes_scroll / classes_scroll_max();
        roundrect(thumb_x, track_y, thumb_w, 5, 2, text_color);
    }
    button_scaled(tr_levels(), levels_btn_x(), class_actions_y(), class_action_w(), btn_h, 0.9);
    if (skins_btn_visible() == 1) {
        button_scaled(tr_skins(), skins_btn_x(), class_actions_y(), class_action_w(), btn_h, 0.9);
    }
    class_msg_t = tick_down(class_msg_t);
    draw_msg(class_msg_text(), class_msg_y(), class_msg_t);
    draw_class_stats();
    draw_back();
}

static void draw_level_row(double n) {
    double x = levels_x();
    double w = levels_w();
    double rh = levels_row_h_fit();
    double k = rh / 70.0;
    double y = levels_row_y(n);
    double selected = 0;
    if (player_level == n && n <= levels_unlocked) {
        selected = 1;
    }
    /* A level row is picked like a button, so it answers the touch like one. */
    double p = button_press_k(x, y, w, rh);
    double s = 1 - PRESS_SQUEEZE * p;
    double bx = squeeze_x(x, w, s);
    double bw = w * s;
    uint32_t tc = shade(text_color, 1 - PRESS_SHADE * p);
    /* The last, jackpot level is titled in gold so it stands out at once. */
    uint32_t title_c = tc;
    if (n == max_level) {
        title_c = shade(0xFFFFD54A, 1 - PRESS_SHADE * p);
    }
    card_state(bx, squeeze_y(y, y, rh, s), bw, rh * s, selected, 0, 1 - PRESS_SHADE * p);
    centered_text(ds_concat(ds_concat(tr_level(), " "), ds_num_to_string((double)(n))), bx, bw,
                  squeeze_y(y + 10 * k, y, rh, s), title_c, 0.62 * k * s);
    centered_text(tr_class_level_effect(player_class, n), bx, bw, squeeze_y(y + 34 * k, y, rh, s), tc, 0.46 * k * s);
    /* During the flash sale the unlock price is red, like class prices. */
    uint32_t status_c = tc;
    if (n == max_level) {
        status_c = shade(0xFFFFD54A, 1 - PRESS_SHADE * p);
    }
    if (flash_sale_active == 1 && n == class_unlocked_of(player_class) + 1) {
        status_c = shade(0xFFFF3030, 1 - PRESS_SHADE * p);
    }
    centered_text(level_status_of(player_class, n), bx, bw, squeeze_y(y + 52 * k, y, rh, s), status_c, 0.42 * k * s);
}

static void draw_levels(void) {
    screen_fill();
    double n = 1;
    while (n <= max_level) {
        draw_level_row(n);
        n = n + 1;
    }
    level_msg_t = tick_down(level_msg_t);
    draw_msg(level_msg_text(), levels_msg_y(), level_msg_t);
    draw_back();
}

static void draw_skin_card(double skin) {
    double x = skins_card_x(skin);
    double w = skin_card_w;
    double top = skins_top();
    double locked = 1 - skin_unlocked(skin);
    double selected = 0;
    if (current_skin() == skin) {
        selected = 1;
    }
    /* A skin card is picked like a button, so it answers the touch like one. */
    double p = button_press_k(x, top, w, skin_card_h);
    double s = 1 - PRESS_SQUEEZE * p;
    double bx = squeeze_x(x, w, s);
    double bw = w * s;
    card_state(bx, squeeze_y(top, top, skin_card_h, s), bw, skin_card_h * s, selected, locked, 1 - PRESS_SHADE * p);
    double sprite = 50 * skin_preview_scale() * s;
    double sx = bx + (bw - sprite) / 2;
    draw_class_render(player_class, skin, sx, squeeze_y(skin_preview_y(), top, skin_card_h, s), sprite);
    uint32_t c = shade(panel_text(locked), 1 - PRESS_SHADE * p);
    const char *status = skin_hint_for(player_class);
    if (locked == 0) {
        status = tr_class_pick();
        if (selected == 1) {
            status = tr_class_selected();
        }
    }
    centered_text(skin_name_of(skin), bx, bw, squeeze_y(skin_name_y(), top, skin_card_h, s), c, 0.8 * s);
    centered_text(status, bx, bw, squeeze_y(skin_status_y(), top, skin_card_h, s), c, 0.45 * s);
}

static void draw_skins(void) {
    screen_fill();
    double skin = 0;
    while (skin < SKIN_COUNT) {
        draw_skin_card(skin);
        skin = skin + 1;
    }
    skin_msg_t = tick_down(skin_msg_t);
    draw_msg(tr_skins_note_for(player_class), skins_msg_y(), skin_msg_t);
    draw_back();
}

static double lb_place_bg(double i) {
    if (i == 0) {
        return 0xFF3D3215;
    }
    if (i == 1) {
        return 0xFF2E2E30;
    }
    if (i == 2) {
        return 0xFF3A2414;
    }
    return card_bg;
}

/* Cup counts are whole numbers up to seven digits: %g would print "1e+07". */
static const char *lb_cups_text(double v) {
    char buf[32];
    if (v != v || v < 0)
        v = 0;
    snprintf(buf, sizeof(buf), "%.0f", floor(v + 0.5));
    return ds_concat("", buf);
}

static void draw_lb_row(double i) {
    double x = lb_x();
    double y = lb_row_y(i);
    double w = lb_w();
    roundrect(x, y, w, lb_row_h, 12, lb_place_bg(i));
    const char *place = ds_concat("#", ds_num_to_string((double)((i + 1))));
    const char *nick = net_leaderboard_nick(i);
    const char *cup_s = lb_cups_text(net_leaderboard_cups(i));
    double scale = 0.6;
    double th = text_ink_height(place) * scale;
    double tt = text_ink_top(place) * scale;
    double ty = y + (lb_row_h - th) / 2 - tt + 1;
    text_scaled(place, x + card_text_pad, ty, text_color, scale);
    draw_styled_nick(nick, x + 70, ty, text_color, scale);
    double cw = text_ink_width(cup_s) * scale;
    text_scaled(cup_s, x + w - card_text_pad - cw, ty, text_color, scale);
}

/* A failed top is retried by itself a few times: a VPN or a mobile link that
 * dropped for a moment must not leave the table empty until a manual restart. */
static double lb_retry_t;
static double lb_retry_left = 3;

static const char *lb_fail_text(void) {
    return net_cloud_ready() == 1 ? tr_no_connection() : tr_need_sign_in();
}

static void draw_leaderboard(void) {
    screen_fill();
    double st = net_leaderboard_status();
    if (st == 1) {
        lb_wait = lb_wait + dt;
        lb_retry_t = 0;
        if (lb_wait > lb_wait_max) {
            ctext(lb_fail_text(), screen_h / 2, text_color);
        } else {
            ctext(tr_loading(), screen_h / 2, text_color);
        }
        draw_back();
        return;
    }
    if (st == 4) {
        lb_retry_t = lb_retry_t + dt;
        if (lb_retry_t > 2.5 && lb_retry_left > 0) {
            lb_retry_t = 0;
            lb_retry_left = lb_retry_left - 1;
            net_leaderboard_fetch(FIREBASE);
        }
        ctext(lb_fail_text(), screen_h / 2, text_color);
        draw_back();
        return;
    }
    lb_retry_t = 0;
    lb_retry_left = 3;
    double count = net_leaderboard_count();
    if (count == 0) {
        ctext(tr_no_data(), screen_h / 2, text_color);
    } else {
        double i = 0;
        while (i < count) {
            draw_lb_row(i);
            i = i + 1;
        }
    }
    draw_back();
}
