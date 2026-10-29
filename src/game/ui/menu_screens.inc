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

static void draw_lobby_night_environment(void) {
    if (night_theme_on() == 0) {
        return;
    }
    const double ground_h = 66;
    const double grass_tile = 64;
    rect(0, 0, screen_w, screen_h, 0xFF0B1030);
    rect(0, screen_h - ground_h, screen_w, ground_h, 0xFF15243A);
    if (tex_ready(GRASS) == 1) {
        double x = 0;
        while (x < screen_w) {
            tex_tint(x, screen_h - grass_tile, GRASS, 0, 0.25, 0xFF8BA56B);
            x = x + grass_tile;
        }
        rect(0, screen_h - 3, screen_w, 3, 0xFFB9C6FF);
    }
    double i = 0;
    while (i < 22) {
        double hx = hash01(i * 3 + 317);
        double hy = hash01(i * 7 + 491);
        double hs = hash01(i * 13 + 205);
        double fall = 3.8 + 3.5 * hs;
        double x = 12 + hx * (screen_w - 24) + 8 * sin(snow_t * 0.45 + i);
        double y = -12 + frac01(snow_t / fall + hy) * (screen_h + 24);
        double twinkle = 0.65 + 0.35 * sin(snow_t * 2.0 + i * 1.8);
        double radius = 2.2 + 2.1 * hs;
        double rotation = snow_t * (0.7 + hs * 1.4) + i * 0.6;
        uint32_t color = i - floor(i / 3) * 3 < 1 ? 0xDDF7C948 : 0xDDB9C6FF;
        draw_star_rotated(x, y, radius, color, rotation);
        circle(x, y, radius * 0.24 * twinkle, 0xFFFFFFD9);
        i = i + 1;
    }
}

static void draw_lobby(void) {
    check_achievements();
    screen_fill();
    draw_lobby_night_environment();
    text("Cubic Battle 4", lobby_label_x(text_ink_width("Cubic Battle 4")), lobby_title_y(), text_color);
    if (christmas_active() == 1) {
        const char *banner = tr_christmas_banner();
        text_scaled(banner, lobby_label_x(text_ink_width(banner) * 0.6), lobby_title_y() + 34,
                    christmas_gold + 0xFF000000, 0.6);
    }
    draw_menu_button(0, tr_play());
    draw_menu_button(1, tr_shop());
    draw_menu_button(2, tr_settings());
    draw_menu_button(3, tr_account());
    draw_menu_button(4, tr_achievements());
    if (admin_playing() == 1) {
        const char *wheel_label = language == 1 ? "Колесо" : "Fortune Wheel";
        const char *wheel_count = ds_concat(wheel_label, ds_concat(" (", ds_concat(ds_num_to_string((double)net_load_wheel_spins()), ")")));
        button_scaled(wheel_count, 16, screen_h - 58, 156, 42, 0.72);
    }
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
    /* The theme switch lives here, behind "Играть", where the old winter
     * toggle used to be. A locked New Year theme stays in the cycle but is
     * drawn grey: it is selected, yet the game keeps the ordinary look. */
    if (active_theme_mode() == 1 && winter_theme_on() == 0) {
        button_disabled_scaled(tr_theme_label(), menu_x(), modes_row(4), menu_btn_w(), btn_h,
                               fit_text_scale(tr_theme_label(), menu_btn_w() - 24, 0.8));
    } else {
        button_scaled(tr_theme_label(), menu_x(), modes_row(4), menu_btn_w(), btn_h,
                      fit_text_scale(tr_theme_label(), menu_btn_w() - 24, 0.8));
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
    button_scaled(tr_legal(), menu_x(), settings_row_y(4), menu_btn_w(), settings_btn_h(),
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
        /* The CodeCracker artwork has a larger device frame than other class renders.
         * Draw it smaller and centered so its frame matches the neighboring cards. */
        double render_px = target_px * (cls == CLASS_CODECRACKER ? 0.80 : 1.0);
        double inset = (target_px - render_px) / 2;
        tex_round(x + inset, y + inset, class_card_tex(cls, skin), render_px / CARD_RENDER_PX,
                  render_px * card_render_radius_frac);
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
    double y = levels_row_y(n);
    double selected = 0;
    if (player_level == n && n <= levels_unlocked) {
        selected = 1;
    }
    /* A level row is picked like a button, so it answers the touch like one. */
    double p = button_press_k(x, y, w, level_row_h);
    double s = 1 - PRESS_SQUEEZE * p;
    double bx = squeeze_x(x, w, s);
    double bw = w * s;
    uint32_t tc = shade(text_color, 1 - PRESS_SHADE * p);
    card_state(bx, squeeze_y(y, y, level_row_h, s), bw, level_row_h * s, selected, 0, 1 - PRESS_SHADE * p);
    centered_text(ds_concat(ds_concat(tr_level(), " "), ds_num_to_string((double)(n))), bx, bw,
                  squeeze_y(y + 10, y, level_row_h, s), tc, 0.62 * s);
    centered_text(tr_class_level_effect(player_class, n), bx, bw, squeeze_y(y + 34, y, level_row_h, s), tc, 0.46 * s);
    /* During the flash sale the unlock price is red, like class prices. */
    uint32_t status_c = tc;
    if (flash_sale_active == 1 && n == class_unlocked_of(player_class) + 1) {
        status_c = shade(0xFFFF3030, 1 - PRESS_SHADE * p);
    }
    centered_text(level_status_of(player_class, n), bx, bw, squeeze_y(y + 52, y, level_row_h, s), status_c, 0.42 * s);
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

static void draw_wheel(void) {
    if (admin_playing() == 0) {
        start_transition(ST_LOBBY);
        return;
    }
    screen_fill();
    ctext(language == 1 ? "Колесо фортуны" : "Fortune Wheel", title_y(), text_color);
    draw_back();

    double radius = wheel_ui_radius();
    double cx = screen_w / 2;
    double cy = wheel_ui_center_y();
    double rotation = wheel_rotation;
    if (wheel_anim_t > 0) {
        double progress = clamp(1 - wheel_anim_t / WHEEL_SPIN_DURATION, 0, 1);
        double eased = progress * progress * (3 - 2 * progress);
        rotation = wheel_spin_start + wheel_spin_delta * eased;
    }

    const uint32_t colors[6] = {0xFF3384D8, 0xFF37A783, 0xFFE0A52F, 0xFFD85C68, 0xFF8263C7, 0xFF33A7B5};
    const char *values[6] = {"+1", "+3", "+5", "+10", "+15", "ЗОЛОТО"};
    const char *units[6] = {"КУБ.", "КУБ.", "КУБ.", "КУБ.", "КУБ.", "СКИН"};
    if (language == 0) {
        units[0] = "CUPS";
        units[1] = "CUPS";
        units[2] = "CUPS";
        units[3] = "CUPS";
        units[4] = "CUPS";
        units[5] = "SKIN";
        values[5] = "GOLD";
    }

    circle(cx, cy, radius + 9, 0xFF172033);
    double slice = 0;
    while (slice < 6) {
        double center_deg = -90 + slice * 60 + rotation;
        sector(cx, cy, radius - 3, center_deg - 30, 60, colors[(int)slice]);
        slice = slice + 1;
    }
    slice = 0;
    while (slice < 6) {
        double a = (-120 + slice * 60 + rotation) * 3.141592653589793 / 180;
        line(cx, cy, cx + cos(a) * (radius - 4), cy + sin(a) * (radius - 4), 3, 0xFF182033);
        slice = slice + 1;
    }
    ring(cx, cy, radius, 6, 0xFFFFD166);

    slice = 0;
    while (slice < 6) {
        double a = (-90 + slice * 60 + rotation) * 3.141592653589793 / 180;
        double px = cx + cos(a) * radius * 0.58;
        double py = cy + sin(a) * radius * 0.58;
        double badge_w = radius * 0.46;
        double badge_h = radius * 0.32;
        double badge_x = px - badge_w * 0.5;
        double badge_y = py - badge_h * 0.5;
        double main_scale = fit_text_scale(values[(int)slice], badge_w - 12, 0.78);
        double unit_scale = fit_text_scale(units[(int)slice], badge_w - 12, 0.44);
        roundrect(badge_x, badge_y, badge_w, badge_h, badge_h * 0.34, 0xD9162032);
        text_in_field(values[(int)slice], px - text_ink_width(values[(int)slice]) * main_scale * 0.5,
                      badge_y + 1, badge_h * 0.54, 0xFFFFFFFF, main_scale);
        text_in_field(units[(int)slice], px - text_ink_width(units[(int)slice]) * unit_scale * 0.5,
                      badge_y + badge_h * 0.48, badge_h * 0.50, 0xFFF2F5FF, unit_scale);
        slice = slice + 1;
    }

    circle(cx, cy, radius * 0.23, 0xFF20283B);
    ring(cx, cy, radius * 0.23, 5, 0xFFFFD166);
    centered_text(language == 1 ? "УДАЧА" : "LUCK", cx - radius * 0.22, radius * 0.44, cy - 4,
                  0xFFFFD166, 0.46);
    line(cx, cy - radius - 20, cx, cy - radius + 5, 7, 0xFFFFD166);
    circle(cx, cy - radius - 18, 7, 0xFFFFFFFF);

    const char *progress = ds_concat(language == 1 ? "Побед: " : "Wins: ",
                                     ds_concat(ds_num_to_string((double)net_load_wheel_wins()), "/6"));
    ctext_scaled(progress, cy + radius + 24, text_color, 0.55);
    if (wheel_anim_t > 0) {
        ctext_scaled(language == 1 ? "Колесо крутится..." : "The Wheel Is Spinning...",
                     cy + radius + 48, text_color, 0.46);
    } else if (wheel_result_kind == 1) {
        ctext_scaled(language == 1 ? "Выпал золотой скин!" : "Gold Cube Skin Unlocked!",
                     cy + radius + 48, 0xFFFFD166, 0.50);
    } else if (wheel_result_kind == 2) {
        const char *won = ds_concat(language == 1 ? "Выигрыш: +" : "You won +",
                                    ds_concat(ds_num_to_string(wheel_result_cups), language == 1 ? " кубков" : " cups"));
        ctext_scaled(won, cy + radius + 48, 0xFFFFD166, 0.50);
    } else if (wheel_result_kind == 3) {
        ctext_scaled(language == 1 ? "Прокруток нет" : "No Spins Available",
                     cy + radius + 48, 0xFFFFFFFF, 0.50);
    }
    const char *spin_label = language == 1 ? "Крутить" : "Spin";
    const char *spin_count = ds_concat(spin_label, ds_concat(" (", ds_concat(ds_num_to_string((double)net_load_wheel_spins()), ")")));
    button_scaled(spin_count, center_x(btn_w), wheel_ui_button_y(), btn_w, btn_h, 0.8);
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

static void draw_lb_row(double i) {
    double x = lb_x();
    double y = lb_row_y(i);
    double w = lb_w();
    roundrect(x, y, w, lb_row_h, 12, lb_place_bg(i));
    const char *place = ds_concat("#", ds_num_to_string((double)((i + 1))));
    const char *nick = net_leaderboard_nick(i);
    const char *cup_s = ds_concat("", ds_num_to_string((double)(floor(net_leaderboard_cups(i) + 0.5))));
    double scale = 0.6;
    double th = text_ink_height(place) * scale;
    double tt = text_ink_top(place) * scale;
    double ty = y + (lb_row_h - th) / 2 - tt + 1;
    text_scaled(place, x + card_text_pad, ty, text_color, scale);
    draw_styled_nick(nick, x + 70, ty, text_color, scale);
    double cw = text_ink_width(cup_s) * scale;
    text_scaled(cup_s, x + w - card_text_pad - cw, ty, text_color, scale);
}

static void draw_leaderboard(void) {
    screen_fill();
    double st = net_leaderboard_status();
    if (st == 1) {
        lb_wait = lb_wait + dt;
        if (lb_wait > lb_wait_max) {
            ctext(tr_no_connection(), screen_h / 2, text_color);
        } else {
            ctext(tr_loading(), screen_h / 2, text_color);
        }
        draw_back();
        return;
    }
    if (st == 4) {
        ctext(tr_no_connection(), screen_h / 2, text_color);
        draw_back();
        return;
    }
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
