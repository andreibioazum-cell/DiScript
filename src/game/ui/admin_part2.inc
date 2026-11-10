/* Admin panel input handling, orb rendering, and continuous cheat ticks. */

static double admin_handle(const char *msg) {
    if (is_admin() == 0) {
        return 0;
    }
    const char *by = admin_actor_nick();
    if (str_eq(msg, "event 0") == 1) {
        double cur = net_event();
        if (cur == 0) {
            net_event_set(1);
            net_chat_send(ds_concat(ds_concat("ДИСКО-СОБЫТИЕ от ", by), "!"));
            ds_log("admin: event 0 -> disco ON");
        } else {
            net_event_set(0);
            net_chat_send(ds_concat("СОБЫТИЕ ВЫКЛЮЧИЛ ", by));
            ds_log("admin: event 0 -> OFF");
        }
        return 1;
    }
    if (str_eq(msg, "event 1") == 1) {
        net_event_set(1);
        net_chat_send(ds_concat(ds_concat("ДИСКО-СОБЫТИЕ от ", by), "!"));
        ds_log("admin: event 1 disco");
        return 1;
    }
    if (str_eq(msg, "event 2") == 1) {
        net_event_set(2);
        net_chat_send(ds_concat("СОБЫТИЕ «ПЛИТЫ» от ", by));
        ds_log("admin: event 2 plates");
        return 1;
    }
    if (str_eq(msg, "event 3") == 1) {
        net_event_set(3);
        net_chat_send(ds_concat("СОБЫТИЕ «ДЕД МОРОЗ» от ", by));
        ds_log("admin: event 3 santa");
        return 1;
    }
    if (str_eq(msg, "event 4") == 1) {
        net_event_set(4);
        net_chat_send(ds_concat(ds_concat("СОБЫТИЕ «КУБИК» от ", by), "!"));
        ds_log("admin: event 4 dice");
        return 1;
    }
    if (str_eq(msg, "event 5") == 1) {
        net_event_set(5);
        net_chat_send(ds_concat(ds_concat("СОБЫТИЕ «КОЛОНКИ» от ", by), "!"));
        ds_log("admin: event 5 columns");
        return 1;
    }
    if (str_eq(msg, "event 6") == 1) {
        net_event_set(6);
        net_chat_send(ds_concat(ds_concat("СОБЫТИЕ «СЛАБЫЕ УДАРЫ» от ", by), "! Встаньте на плиту!"));
        ds_log("admin: event 6 weak-hit plates");
        return 1;
    }
    if (str_eq(msg, "event 7") == 1) {
        if (admin_hack_event_allowed() == 0) {
            net_chat_send(tr_hack_root_only());
            ds_log("admin: event 7 refused, root admin only");
            return 1;
        }
        net_event_set(7);
        net_chat_send(ds_concat(ds_concat("СОБЫТИЕ «ВЗЛОМ H4c4k0r» от ", by), "! Он уже ломает ивенты..."));
        ds_log("admin: event 7 hacker raid");
        return 1;
    }
    if (net_chat_is_passwd(msg) == 1) {
        const char *t1 = net_chat_passwd_target(msg);
        const char *np = net_chat_passwd_new(msg);
        if (str_len(t1) > 0 && str_len(np) > 0) {
            if (admin_slot(t1) != 0) {
                net_chat_send(tr_admin_ban_protected());
                ds_log("admin passwd blocked for protected account %s", t1);
                return 1;
            }
            double i = 0;
            while (t1[(int)i] != 0 && i < (double)(sizeof(admin_passwd_target_buf) - 1)) { admin_passwd_target_buf[(int)i] = t1[(int)i]; i = i + 1; }
            admin_passwd_target_buf[(int)i] = 0;
            net_admin_reset_password(t1, np);
            net_chat_send(language == 1 ? "Меняю пароль..." : "Changing password...");
            ds_log("admin passwd reset requested for %s", t1);
            return 1;
        }
    }
    if (net_chat_is_ban(msg) == 1) {
        const char *t1 = net_chat_ban_target(msg);
        if (str_len(t1) > 0) {
            if (admin_slot(t1) != 0) {
                net_chat_send(tr_admin_ban_protected());
                ds_log("admin ban blocked for protected account %s", t1);
                return 1;
            }
            net_ban_set(t1, 1);
            net_chat_send(ds_concat(ds_concat(t1, " ЗАБЛОКИРОВАН, админ "), by));
            ds_log("admin ban %s", t1);
            return 1;
        }
    }
    if (net_chat_is_unban(msg) == 1) {
        const char *t2 = net_chat_unban_target(msg);
        if (str_len(t2) > 0) {
            net_ban_set(t2, 0);
            net_chat_send(ds_concat(ds_concat(t2, " РАЗБЛОКИРОВАН, админ "), by));
            ds_log("admin unban %s", t2);
            return 1;
        }
    }
    if (net_chat_is_text_cmd(msg) == 1) {
        const char *btxt = net_chat_text_cmd_text(msg);
        const char *bcol = net_chat_text_cmd_color(msg);
        if (str_len(btxt) > 0) {
                        /* Every banner carries its author's nick: "Dimasi4ek229: text". */
            net_banner_send(ds_concat(ds_concat(by, ": "), btxt), bcol);
            ds_log("admin text: %s (%s)", ds_concat(ds_concat(by, ": "), btxt), bcol);
            return 1;
        }
    }
    return 0;
}

/* Hide soft keyboard when closing the panel or leaving a tab; the typed
 * text goes back to the field it belongs to so nothing is lost. */
static void admin_keyboard_drop(void) {
    if (keyboard_visible() == 1) {
        if (admin_tab == 2) {
            keyboard_get_text_to(admin_cmd_text, sizeof(admin_cmd_text));
        } else {
            keyboard_get_text_to(admin_gift_target, sizeof(admin_gift_target));
        }
        keyboard_hide();
    }
}

static void admin_panel_close(void) {
    admin_keyboard_drop();
    admin_panel_open = 0;
}

static void admin_panel_press(double x, double y) {
    double px = admin_panel_x();
    double py = admin_panel_y();
    double pw = admin_panel_w();
    double hh = admin_header_h();
    double cxx = px + pw - hh * 0.55;
    double dx = x - cxx;
    double dy = y - (py + hh / 2);
    if (sqrt(dx * dx + dy * dy) <= hh * 0.38 + 8) {
        admin_panel_close();
        play_button_click();
        return;
    }
    if (hit_rect(x, y, px, py, pw, admin_panel_h()) == 0) {
        admin_panel_close();
        return;
    }
    if ((admin_tab == 1 && admin_gifts_allowed() == 0) ||
        (admin_tab == 2 && admin_commands_allowed() == 0)) {
        admin_keyboard_drop();
        admin_tab = 0;
    }
    double tab_y = admin_content_top();
    if (y >= tab_y && y <= tab_y + admin_row_h()) {
        double n = admin_tab_slots();
        if (n > 1) {
            double full = pw - 2 * admin_pad();
            double tw = (full - (n - 1) * admin_gap()) / n;
            double slot = floor((x - px - admin_pad()) / (tw + admin_gap()));
            slot = clamp(slot, 0, n - 1);
            double target = admin_tab_at_slot(slot);
            if (target != admin_tab) {
                admin_keyboard_drop();
            }
            admin_tab = target;
            play_button_click();
        }
        return;
    }
    if (admin_tab == 2 && admin_commands_allowed() == 1) {
        admin_cmd_press(x, y);
        return;
    }
    if (admin_tab == 1 && admin_gifts_allowed() == 1) {
        double ty = admin_content_top();
        double rhg = admin_row_h();
        double row_index = floor((y - ty) / (rhg + admin_gap()));
        if (row_index >= 1 && row_index <= NET_GIFT_TYPE_COUNT) {
            admin_gift_choice = row_index - 1;
            return;
        }
        if (row_index == 7) {
            keyboard_clear();
            if (admin_gift_target[0]) keyboard_type(admin_gift_target);
            keyboard_show();
            return;
        }
        if (row_index == 8) {
            keyboard_hide();
            if (admin_gift_target[0] && admin_gift_busy != 1) {
                admin_gift_busy = net_gift_action(admin_gift_choice, admin_gift_target, 1) == 0 ? 3 : 1;
            }
            return;
        }
        return;
    }
    double bw = (pw - 2 * admin_pad() - admin_gap()) / 2;
    double rh = admin_cheat_row_h();
    double row = floor((y - admin_grid_top()) / (rh + admin_gap()));
    if (row < 0 || row > floor((ADMIN_CHEATS - 1) / 2) || x < px + admin_pad() || x > px + pw - admin_pad()) {
        return;
    }
    double col = 0;
    if (x >= px + admin_pad() + bw + admin_gap()) {
        col = 1;
    }
    double idx = row * 2 + col;
    if (idx >= ADMIN_CHEATS) {
        /* The last row holds a single button: the empty half is not a button. */
        return;
    }
    admin_press(idx);
}

static double admin_ball_hit(double x, double y) {
    double dx = x - admin_ball_x;
    double dy = y - admin_ball_y;
    return sqrt(dx * dx + dy * dy) <= admin_ball_r() + 6;
}

static double admin_touch(double x, double y, double action, double id) {
    if (admin_modifiers_allowed() == 0) {
        return 0;
    }
    /* Consume all touches while admin panel is open. The commands tab owns
     * drag gestures over its event strip; everything else is a plain tap. */
    if (admin_panel_open == 1) {
        if (admin_cmd_touch(x, y, action, id) == 1) {
            return 1;
        }
        if (action == 0) {
            admin_panel_press(x, y);
        }
        return 1;
    }
    /* Ignore secondary pointers while dragging admin orb. */
    if (admin_ball_grab == 1 && id != admin_ball_id) {
        return 1;
    }
    if (admin_ball_grab == 1) {
        if (action == 2 || action == 1) {
            double br = admin_ball_r();
            admin_ball_x = clamp(x - admin_ball_grab_dx, br + 4, screen_w - br - 4);
            admin_ball_y = clamp(y - admin_ball_grab_dy, br + 6, screen_h - br - 6);
        }
        if (action == 1 || action == 3) {
            admin_ball_grab = 0;
            admin_ball_id = -1;
            double dx = x - admin_ball_px;
            double dy = y - admin_ball_py;
            if (action == 1 && sqrt(dx * dx + dy * dy) <= 13) {
                admin_panel_open = 1;
                play_button_click();
            }
        }
        return 1;
    }
    if (action == 0 && admin_ball_hit(x, y) == 1) {
        admin_ball_grab = 1;
        admin_ball_id = id;
        admin_ball_px = x;
        admin_ball_py = y;
        admin_ball_grab_dx = x - admin_ball_x;
        admin_ball_grab_dy = y - admin_ball_y;
        return 1;
    }
    return 0;
}

/* Draw opaque monochrome admin orb. */
static void admin_draw_ball(void) {
    const char *lab = admin_orb_label();
    double r = admin_ball_r();
    double cx = admin_ball_x;
    double cy = admin_ball_y;
    double iw = text_ink_width(lab);
    double ih = text_ink_height(lab);
    double sc = 0.5;
    /* The white outlined face stays visible: do not paint a second black disc
     * over it after the outline. */
    circle(cx, cy, r, admin_off());
    circle(cx, cy, r * 0.80, admin_frame());
    if (iw > 0 && ih > 0) {
        /* The label is drawn on the white face. */
        double sw = (r * 1.15) / iw;
        double sh = (r * 0.70) / ih;
        sc = sw < sh ? sw : sh;
    }
    double tw = text_ink_width(lab) * sc;
    double th = text_ink_height(lab) * sc;
    double tt = text_ink_top(lab) * sc;
    text_scaled(lab, cx - tw / 2, cy - th / 2 - tt, admin_body(), sc);
}

static void admin_draw_cheats(void) {
    double px = admin_panel_x() + admin_pad();
    double pw = admin_panel_w() - 2 * admin_pad();
    double bw = (pw - admin_gap()) / 2;
    double rh = admin_cheat_row_h();
    double idx = 0;
    while (idx < ADMIN_CHEATS) {
        double row = floor(idx / 2);
        double col = idx - row * 2;
        double bx = px + col * (bw + admin_gap());
        double by = admin_grid_top() + row * (rh + admin_gap());
        const char *label = admin_cheat_label(idx);
        admin_btn(label, bx, by, bw, rh, admin_cheat_value(idx), fit_text_scale(label, bw - 12, rh / 58.0));
        idx = idx + 1;
    }
}

static void admin_draw_gifts(void) {
    double px = admin_panel_x() + admin_pad();
    double y = admin_content_top() + admin_row_h() + admin_gap();
    double w = admin_panel_w() - 2 * admin_pad();
    double rh = admin_row_h();
    for (int i = 0; i < NET_GIFT_TYPE_COUNT; ++i) {
        const char *name = admin_gift_names[i];
        if (language == 1) name = i == 0 ? "Какашка" : i == 1 ? "Торт" : i == 2 ? "Мишка" :
                                  i == 3 ? "Ракета" : i == 4 ? "Изумруд" : "Тыква-куб";
        const char *label = ds_concat(admin_gift_choice == i ? "> " : "", name);
        admin_btn(label, px, y + i * (rh + admin_gap()), w, rh, admin_gift_choice == i, 0.55);
    }
    double target_y = y + NET_GIFT_TYPE_COUNT * (rh + admin_gap());
    roundrect(px, target_y, w, rh, 12, 0x1A1A1A);
    roundrect_outline(px, target_y, w, rh, 12, 2, admin_frame());
    const char *target_label = admin_gift_target[0] ? admin_gift_target : (language == 1 ? "Ник получателя" : "Recipient nickname");
    text_in_box(target_label, px + 8, target_y, w - 16, rh, admin_frame(), 0.55);
    const char *grant = admin_gift_busy == 1 ? (language == 1 ? "Выдаём..." : "Granting...") : admin_gift_busy == 2 ? (language == 1 ? "Подарок выдан!" : "Gift granted!") : admin_gift_busy == 3 ? (language == 1 ? "Ошибка, нажмите для повтора" : "Failed - tap to retry") : (language == 1 ? "Выдать подарок" : "Grant gift");
    admin_btn(grant, px, target_y + rh + admin_gap(), w, rh, 0, 0.55);
}
static void admin_draw_panel(void) {
    double px = admin_panel_x();
    double py = admin_panel_y();
    double pw = admin_panel_w();
    double ph = admin_panel_h();
    double hh = admin_header_h();
    const char *title = tr_admin_title();
    double title_w = pw - hh * 1.9 - 16;
    /* Draw translucent backdrop scrim; panel controls remain opaque. */
    rect(0, 0, screen_w, screen_h, admin_a(0x000000, 0.55));
    roundrect(px, py, pw, ph, 14, admin_body());
    roundrect(px, py, pw, hh, 14, admin_header());
    rect(px, py + hh / 2, pw, hh / 2, admin_header());
    /* The white frame runs around the whole panel and under the header, so the
     * window reads as one outlined box on the dark lobby. */
    roundrect_outline(px, py, pw, ph, 14, 2, admin_frame());
    line(px + 12, py + hh - 1, px + pw - 12, py + hh - 1, 2, admin_frame());
    text_in_field(title, px + 16, py, hh, admin_frame(), fit_text_scale(title, title_w, hh / 58.0));
    double cxx = px + pw - hh * 0.55;
    double cr = hh * 0.3;
    circle(cxx, py + hh / 2, cr, admin_off());
    ring(cxx, py + hh / 2, cr, clamp(cr * 0.14, 2, 3), admin_frame());
    text_in_box("X", cxx - cr, py + hh / 2 - cr, cr * 2, cr * 2, admin_frame(), cr / 30.0);
    admin_draw_tabs();
    if (admin_tab == 2 && admin_commands_allowed() == 1) {
        admin_cmd_draw();
    } else if (admin_tab == 1 && admin_gifts_allowed() == 1) {
        admin_draw_gifts();
    } else {
        admin_draw_cheats();
    }
}

static void admin_draw(void) {
    if (admin_modifiers_allowed() == 0) {
        return;
    }
    if ((game_state == ST_SOLO || game_state == ST_ONLINE) && hack_cinematic_lock() == 1) {
        return;
    }
    if (admin_ball_x == 0 && admin_ball_y == 0) {
        admin_ball_x = screen_w - (admin_ball_r() + 14);
        admin_ball_y = back_y + btn_h * 0.5;
    }
    if (admin_panel_open == 1) {
        admin_draw_panel();
    }
    admin_draw_ball();
}

/* Tick active continuous cheats, admin gift actions and the commands tab. */
static void admin_tick(void) {
    admin_ball_t = admin_ball_t + dt;
    admin_cmd_tick();
    if (admin_gifts_allowed() == 1 && admin_panel_open == 1 && admin_tab == 1) {
        if (keyboard_visible() == 1) keyboard_get_text_to(admin_gift_target, sizeof(admin_gift_target));
        if (admin_gift_busy == 1) {
            double gs = net_gift_action_status();
            if (gs >= 2) admin_gift_busy = gs == 2 ? 2 : 3;
        }
    }
    if (admin_modifiers_allowed() == 0 || admin_in_battle() == 0 || finished != 0) {
        return;
    }
    if (cheat_nocd == 1) {
        punch_cd = 0;
        super_cd = 0;
        dash_cd = 0;
        astra_cd = 0;
    }
    if (cheat_cc == 1) {
        player_stun = 0;
        player_freeze = 0;
        player_poison = 0;
    }
    if (cheat_estun == 1 && game_state == ST_SOLO) {
        if (enemy->stun < 0.25) {
            enemy->stun = 0.25;
        }
        cheat_estun_mark = 1;
    }
}
