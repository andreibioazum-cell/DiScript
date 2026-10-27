static const char *login_error_text(void) {
    if (login_status == 4) {
        return tr_no_connection();
    }
    if (login_status == 5) {
        return tr_bad_nick();
    }
    if (login_status == 6) {
        return tr_wrong_pass();
    }
    if (login_status == 7) {
        return tr_bad_pass();
    }
    if (login_status == 8) {
        return tr_consent_needed();
    }
    if (login_status == 9) {
        return tr_bad_nick_rules();
    }
    return "";
}

static void draw_login_field(const char *label, const char *value, const char *placeholder, double y, double focused) {
    text(label, login_x() + login_lab_ox, y - login_lab, text_color);
    roundrect(login_x(), y, login_w, login_fh, field_radius, card_bg);
    double caret = 0;
    if (focused == 1 && keyboard_visible() == 1) {
        caret = 1;
    }
    const char *shown = value;
    if (str_len(value) == 0 && caret == 0) {
        shown = placeholder;
    }
    if (caret == 1) {
        shown = ds_concat(value, "|");
    }
    text_in_field(shown, login_x() + field_text_pad, y, login_fh, text_color, 0.6);
}

static void draw_login(void) {
    screen_fill();
    draw_login_field(tr_nick(), login_nick, tr_nick_ph(), login_nick_y(), login_field == 1);
    draw_login_field(tr_pass(), login_pass, tr_pass_ph(), login_pass_y(), login_field == 2);
    button_scaled(tr_login_btn(), login_btn_x(), login_btn_y(), login_btn_w, btn_h, 0.65);
    const char *err = login_error_text();
    if (str_len(err) > 0) {
        ctext(err, login_msg_y(), text_color);
    }
    draw_login_consent();
    draw_back();
}

static void login_sync_field(void) {
    if (keyboard_visible() == 0) {
        return;
    }
    if (login_field == 1) {
        keyboard_get_text_to(login_nick_buf, sizeof(login_nick_buf));
        login_nick = login_nick_buf;
    } else {
        keyboard_get_text_to(login_pass_buf, sizeof(login_pass_buf));
        login_pass = login_pass_buf;
    }
}

static void login_focus(double field, const char *value) {
    login_sync_field();
    login_field = field;
    keyboard_clear();
    if (str_len(value) > 0) {
        keyboard_type(value);
    }
    keyboard_show();
}

static void login_focus_nick(void) {
    login_focus(1, login_nick);
}

static void login_focus_pass(void) {
    login_focus(2, login_pass);
}

static void login_open(void) {
    login_field = 1;
    login_status = 0;
    login_nick = "";
    login_pass = "";
        /* Reset consent checkbox on every login screen entry. */
        login_consent = 0;
    keyboard_clear();
    keyboard_hide();
}

static void login_do(void) {
    login_sync_field();
    if (str_len(login_nick) == 0) {
        login_status = 5;
        keyboard_hide();
        return;
    }
    if (str_len(login_pass) == 0) {
        login_status = 7;
        keyboard_hide();
        return;
    }
    if (nick_blocked(login_nick) == 1) {
        login_status = 9;
        keyboard_hide();
        return;
    }
    if (login_consent != 1) {
        login_status = 8;
        keyboard_hide();
        return;
    }
    double res = net_auth(FIREBASE, login_nick, login_pass);
    if (res != 2) {
        login_status = res;
        return;
    }
    login_status = 0;
    keyboard_hide();
    keyboard_clear();
    consent_give();
    progress_from_storage();
    settings_from_storage();
    start_transition(login_after);
}

static void update_login(void) {
    if (t_dir != 0) {
        return;
    }
    if (keyboard_visible() == 1) {
        login_sync_field();
    }
    if (keyboard_enter_pressed() == 1) {
        if (login_field == 1) {
            login_focus_pass();
        } else {
            login_do();
        }
    }
}

static double studio_rgb(double rgb) {
    return (1 + floor(studio_a * 254)) * 0x1000000 + rgb;
}

static double studio_bg_rgb(double rgb) {
    return (1 + floor(studio_bg_a * 254)) * 0x1000000 + rgb;
}

static void draw_studio(void) {
    rect(0, 0, screen_w, screen_h, studio_bg_rgb(0));
    uint32_t body = studio_rgb(text_color);
    double y = screen_h / 2 - 30;
    ctext_scaled(STUDIO_NAME, y, body, 1.1);
    double lw = fit_w(320);
    rect(center_x(lw), y + 34, lw, 2, body);
}

static void open_online(void) {
    if (net_login_status() != 2) {
        login_after = ST_ROOMS;
        start_transition(ST_LOGIN);
        return;
    }
        /* Online play and chat process data: no entry without consent. */
    if (consent_ok() == 0) {
        docs_transition(LEGAL_PAGE_CONSENT, 1);
        return;
    }
    if (net_is_banned(net_login_nick()) == 1) {
        banned_msg = tr_banned();
        start_transition(ST_LOBBY);
        return;
    }
    /* Online is a room list now, not one shared room for everyone. */
    start_transition(ST_ROOMS);
}

static void open_leaderboard(void) {
    lb_wait = 0;
    net_leaderboard_fetch(FIREBASE);
    start_transition(ST_LEADERBOARD);
}

static void touch_lobby(double x, double y, double action) {
    if (action != 0) {
        return;
    }
    /* NPC cube tap or dialogue panel tap (the cube lives in the new theme) */
    if (new_theme == 1 && (hit_npc_cube(x, y) == 1 || hit_npc_panel(x, y) == 1)) {
        play_button_click();
        cube_npc_tap();
        return;
    }
    /* Advance or close active NPC dialogue without triggering underlying menu rows. */
    if (new_theme == 1 && cube_npc_dialogue_active == 1) {
        play_button_click();
        cube_npc_tap();
        return;
    }
    if (hit_menu_row(x, y, 0) == 1) {
        start_transition(ST_MODES);
        return;
    }
    if (hit_menu_row(x, y, 1) == 1) {
        start_transition(ST_CLASSES);
        return;
    }
    if (hit_menu_row(x, y, 2) == 1) {
        start_transition(ST_SETTINGS);
        return;
    }
    if (hit_menu_row(x, y, 3) == 1) {
                /* Open account panel when signed in, or login screen otherwise. */
                if (net_login_status() == 2) {
                    if (t_dir == 0) {
                acct_pending = 1;
                start_transition(ST_LOBBY);
            }
            return;
        }
        login_after = ST_LOBBY;
        start_transition(ST_LOGIN);
        return;
    }
    if (hit_menu_row(x, y, 4) == 1) {
        start_transition(ST_ACHIEVEMENTS);
    }
    if (bpass_allowed() == 1 && hit_bpass_corner(x, y) == 1) {
        start_transition(ST_BPASS);
    }
}

static void touch_modes(double x, double y) {
    if (hit_promo_corner(x, y) == 1) {
        start_transition(ST_PROMO);
        return;
    }
    if (hit_modes_row(x, y, 4) == 1) {
        if (seasonal_admin_allowed() == 0) {
            return;
        }
        winter_theme = 1 - winter_theme;
        apply_winter_theme();
        save_settings();
        return;
    }
    if (hit_modes_row(x, y, 0) == 1) {
        start_transition(ST_LOBBY);
        return;
    }
    if (hit_modes_row(x, y, 1) == 1) {
        start_transition(ST_SOLO);
        return;
    }
    if (hit_modes_row(x, y, 2) == 1) {
        open_online();
        return;
    }
    if (hit_modes_row(x, y, 3) == 1) {
        /* The published rules open the users node to signed-in players only:
         * ask for a login instead of showing an empty top. */
        if (net_login_status() != 2) {
            login_after = ST_LEADERBOARD;
            start_transition(ST_LOGIN);
            return;
        }
        open_leaderboard();
        return;
    }
}

static void touch_settings(double x, double y) {
    if (hit_settings_row(x, y, 0) == 1) {
        start_transition(ST_LOBBY);
        return;
    }
    if (hit_settings_row(x, y, 1) == 1) {
        language = 0;
        save_settings();
        return;
    }
    if (hit_settings_row(x, y, 2) == 1) {
        language = 1;
        save_settings();
        return;
    }
    if (hit_settings_row(x, y, 3) == 1) {
        show_hitboxes = 1 - show_hitboxes;
        save_settings();
        return;
    }
    if (hit_settings_row(x, y, 4) == 1) {
        new_theme = 1 - new_theme;
        apply_theme();
        save_settings();
        return;
    }
    if (hit_settings_row(x, y, 5) == 1) {
        docs_transition(0, 0);
        return;
    }
}

static void touch_levels(double x, double y, double action) {
    if (action != 0) {
        return;
    }
    if (back_hit(x, y) == 1) {
        start_transition(ST_CLASSES);
        return;
    }
    double n = 1;
    while (n <= max_level) {
        if (hit_level_row(x, y, n) == 1) {
            level_action(player_class, n);
            return;
        }
        n = n + 1;
    }
}

static void touch_skins(double x, double y, double action) {
    if (action != 0) {
        return;
    }
    if (back_hit(x, y) == 1) {
        start_transition(ST_CLASSES);
        return;
    }
    double skin = 0;
    while (skin < SKIN_COUNT) {
        if (hit_skin_card(x, y, skin) == 1) {
            pick_skin(skin);
            return;
        }
        skin = skin + 1;
    }
}

static void touch_leaderboard(double x, double y, double action) {
    if (action != 0) {
        return;
    }
    if (back_hit(x, y) == 1) {
        start_transition(ST_MODES);
        return;
    }
    /* No token and no saved password: the only way to the top is a login. */
    if (net_leaderboard_status() == 4 && net_cloud_ready() == 0 &&
        hit_button(x, y, 0, screen_h / 2 - 40, screen_w, 80) == 1) {
        login_after = ST_LEADERBOARD;
        start_transition(ST_LOGIN);
    }
}

static void touch_login(double x, double y, double action) {
    if (action != 0) {
        return;
    }
    if (hit_login_field(x, y, login_nick_y()) == 1) {
        login_focus_nick();
        return;
    }
    if (hit_login_field(x, y, login_pass_y()) == 1) {
        login_focus_pass();
        return;
    }
    if (hit_login_btn(x, y) == 1) {
        login_do();
        keyboard_hide();
        return;
    }
    if (hit_login_more(x, y) == 1) {
        login_sync_field();
        docs_transition(LEGAL_PAGE_CONSENT - 2, 0);
        return;
    }
    if (hit_login_consent(x, y) == 1) {
        login_consent = 1 - login_consent;
        if (login_status == 8) {
            login_status = 0;
        }
        return;
    }
    login_sync_field();
    keyboard_hide();
    if (back_hit(x, y) == 1) {
        keyboard_clear();
        if (login_after == ST_ONLINE) {
            start_transition(ST_MODES);
        } else {
            start_transition(ST_LOBBY);
        }
    }
}
