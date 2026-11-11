#define ACCT_MENU 1
#define ACCT_PASS 2
#define ACCT_DELETE 3
#define ACCT_GIFTS 4
#define ACCT_PIN 5
#define ACCT_BUF 64
static double acct_gift_choice = -1;
static double acct_gift_is_owned = 0;
static double acct_gift_own_row = -1;
static double acct_gift_scroll = 0, acct_gift_vel = 0;
static double acct_gift_shop_scroll = 0, acct_gift_shop_vel = 0;
static double acct_gift_drag = -1, acct_gift_drag_y = 0, acct_gift_drag_move = 0;
static double acct_gift_drag_row = -1, acct_gift_drag_owned = 1;
static ScrollTrack acct_gift_track;
static double acct_gift_busy = 0;
static double acct_gift_busy_mode = 0;
static double acct_gift_acted = -1;
static double acct_gift_view_open = 0;
static double acct_gift_footer_top(void);
static double hit_acct_gift_any(double x, double y);
static double hit_acct_gift_buy(double x, double y);
static double hit_acct_gift_send(double x, double y);
static double hit_acct_gift_sell(double x, double y);
static double hit_acct_gift_inspect(double x, double y);
static double touch_acct_gift_list(double x, double y, double action, double id);
static void acct_gifts_page(void);
static void acct_gift_buy(void);
static void acct_gift_send(void);
static void acct_gift_sell(void);
static void draw_acct_gifts(void);
static void draw_acct_gift_viewer(void);
static void acct_poll_gifts(void);
static void acct_gift_scroll_reset(void);
static void acct_gift_scroll_by(double dy);
static void acct_gift_scroll_tick(void);
static double acct_entry;
static double acct_field;
static char acct_buf[3][ACCT_BUF];
static char acct_mask_buf[3][ACCT_BUF + 4];
static double acct_busy;
static double acct_done;
static double acct_knob;
static double acct_drag = -1;
static double acct_pending;
static double acct_grab;
static const char *acct_tr(const char *ru, const char *en) { return language == 1 ? ru : en; }
static const char *tr_acct_title(void) { return acct_tr("Аккаунт: ", "Account: "); }
static const char *tr_acct_change(void) { return acct_tr("Сменить пароль", "Change password"); }
static const char *tr_acct_switch(void) { return acct_tr("Войти в другой аккаунт", "Sign in to another account"); }
static const char *tr_acct_logout(void) { return acct_tr("Выйти из аккаунта", "Sign out"); }
static const char *tr_acct_old(void) { return acct_tr("Текущий пароль", "Current password"); }
static const char *tr_acct_new(void) { return acct_tr("Новый пароль", "New password"); }
static const char *tr_acct_repeat(void) { return acct_tr("Повторите новый пароль", "Repeat the new password"); }
static const char *tr_acct_pass(void) { return acct_tr("Пароль аккаунта", "Account password"); }
static const char *tr_acct_need_pass(void) { return acct_tr("Введите пароль", "Enter the password"); }
static const char *tr_acct_need_new(void) { return acct_tr("Введите новый пароль", "Enter a new password"); }
static const char *tr_acct_mismatch(void) { return acct_tr("Новые пароли не совпадают", "The new passwords do not match"); }
static const char *tr_acct_same(void) { return acct_tr("Новый пароль совпадает со старым", "The new password is the same"); }
static const char *tr_acct_too_many(void) {
    return acct_tr("Слишком много попыток, попробуйте позже", "Too many attempts, try again later");
}
static const char *tr_acct_admin(void) {
    return acct_tr("Пароль админа задан в коде игры", "The admin password is set in the game code");
}
static const char *tr_acct_wrong(void) { return acct_tr("Неверный пароль", "Wrong password"); }
static const char *tr_acct_changing(void) { return acct_tr("Меняем пароль...", "Changing the password..."); }
static const char *tr_acct_changed(void) { return acct_tr("Пароль изменён", "Password changed"); }
static const char *tr_acct_failed(void) { return acct_tr("Не удалось: нет связи, повторите позже", "Failed: no connection, try again later"); }
static const char *tr_acct_signed_out(void) { return acct_tr("Сначала войдите в аккаунт", "Sign in first"); }
static const char *tr_acct_warn1(void) {
    return acct_tr("Аккаунт, прогресс и все данные будут удалены", "The account, progress and all data will be");
}
static const char *tr_acct_warn2(void) {
    return acct_tr("навсегда, восстановить их будет нельзя.", "deleted forever and cannot be restored.");
}
static const char *tr_acct_swipe(void) { return acct_tr("Проведите вправо, чтобы удалить", "Swipe right to delete"); }
static const char *tr_pin2_title(void) { return acct_tr("Доп. пароль", "Extra password"); }
static const char *tr_pin2_new(void) { return acct_tr("Новый доп. пароль", "New extra password"); }
static const char *tr_pin2_repeat(void) { return acct_tr("Повторите пароль", "Repeat the password"); }
static const char *tr_pin2_save(void) { return acct_tr("Сохранить", "Save"); }
static const char *tr_pin2_del(void) { return acct_tr("Удалить", "Delete"); }
static const char *tr_pin2_saved(void) { return acct_tr("Доп. пароль сохранён", "Extra password saved"); }
static const char *tr_pin2_removed(void) { return acct_tr("Доп. пароль удалён", "Extra password deleted"); }
static const char *tr_pin2_short(void) { return acct_tr("Пароль от 4 символов", "At least 4 characters"); }
static const char *tr_pin2_mismatch(void) { return acct_tr("Пароли не совпадают", "Passwords do not match"); }
static const char *tr_pin2_hint(void) {
    return acct_tr("Хранится только на этом устройстве", "Stored on this device only");
}
static const char *tr_pin2_menu(void) {
    return net_load_pin2() > 0 ? acct_tr("Доп. пароль: вкл", "Extra password: on")
                               : acct_tr("Доп. пароль: выкл", "Extra password: off");
}
static void acct_clear_fields(void) {
    memset(acct_buf, 0, sizeof(acct_buf));
    acct_field = 0;
    keyboard_clear();
    keyboard_hide();
}
static void acct_go(double mode) {
    acct_open = mode;
    acct_msg_set("");
    acct_knob = 0;
    acct_drag = -1;
    acct_gift_view_open = 0;
    acct_gift_scroll_reset();
    acct_clear_fields();
    ui_nav_gen = ui_nav_gen + 1;
    press_pulses_clear();
}
static void acct_show(double mode) {
    acct_entry = mode;
    acct_done = 0;
    acct_go(mode);
}
static void acct_close(void) {
    acct_pending = 0;
    acct_clear_fields();
    acct_open = 0;
    acct_drag = -1;
    acct_gift_view_open = 0;
    acct_gift_scroll_reset();
    ui_nav_gen = ui_nav_gen + 1;
    press_pulses_clear();
}
static void acct_back(void) {
    /* Close sticker preview modal before leaving gifts page. */
    if (acct_gift_view_open == 1) {
        acct_gift_view_open = 0;
        return;
    }
    if (acct_open != ACCT_MENU && acct_entry == ACCT_MENU && acct_done == 0 && net_login_status() == 2) {
        acct_go(ACCT_MENU);
        return;
    }
    acct_close();
}
static double acct_top(void) { return back_y + btn_h + 24; }
static double acct_w(void) { return fit_w(620); }
static double acct_x(void) { return center_x(acct_w()); }
static double acct_menu_y(double i) { return acct_top() + 56 + i * (btn_h + 16); }
static double acct_fields(void) {
    if (acct_open == ACCT_PASS)
        return 3;
    if (acct_open == ACCT_PIN)
        return 2;
    return acct_open == ACCT_DELETE || acct_open == ACCT_GIFTS ? 1 : 0;
}
static double acct_fields_top(void) {
    if (acct_open == ACCT_DELETE)
        return acct_top() + 116;
    if (acct_open == ACCT_GIFTS)
        return acct_gift_footer_top() + login_lab + 2;
    if (acct_open == ACCT_PIN)
        return acct_top() + 60;
    return acct_top() + 44;
}
static double acct_field_y(double i) { return acct_fields_top() + login_lab + i * (login_fh + login_lab + 12); }
static double acct_below_fields(void) { return acct_field_y(acct_fields() - 1) + login_fh + 22; }
static double acct_slider_y(void) { return acct_below_fields(); }
static double acct_slider_h(void) { return btn_h + 8; }
static double acct_knob_w(void) { return acct_slider_h() * 1.5; }
static double acct_knob_x(void) { return acct_x() + 4 + acct_knob * (acct_w() - acct_knob_w() - 8); }
static double acct_msg_y(void) {
    if (acct_open == ACCT_MENU) {
        return acct_menu_y(5) + 4;
    }
    return acct_below_fields() + (acct_open == ACCT_DELETE ? acct_slider_h() : btn_h) + 18;
}
static double hit_acct_menu(double x, double y, double i) {
    return acct_open == ACCT_MENU && hit_button(x, y, acct_x(), acct_menu_y(i), acct_w(), btn_h) == 1;
}
static double hit_acct_field(double x, double y, double i) {
    return i < acct_fields() && hit_rect(x, y, acct_x(), acct_field_y(i), acct_w(), login_fh) == 1;
}
static double hit_acct_submit(double x, double y) {
    return acct_open == ACCT_PASS && hit_button(x, y, acct_x(), acct_below_fields(), acct_w(), btn_h) == 1;
}
static double hit_acct_knob(double x, double y) {
    return acct_open == ACCT_DELETE && acct_done == 0 && acct_busy == 0 &&
           hit_rect(x, y, acct_knob_x() - 12, acct_slider_y() - 12, acct_knob_w() + 24, acct_slider_h() + 24) == 1;
}
static double hit_acct_pin_save(double x, double y) {
    if (acct_open != ACCT_PIN)
        return 0;
    if (net_load_pin2() > 0)
        return hit_button(x, y, acct_x(), acct_below_fields(), acct_w() * 0.48, btn_h) == 1;
    return hit_button(x, y, acct_x(), acct_below_fields(), acct_w(), btn_h) == 1;
}
static double hit_acct_pin_del(double x, double y) {
    return acct_open == ACCT_PIN && net_load_pin2() > 0 &&
           hit_button(x, y, acct_x() + acct_w() * 0.52, acct_below_fields(), acct_w() * 0.48, btn_h) == 1;
}
static double acct_button_hit(double x, double y) {
    return back_hit(x, y) == 1 || hit_acct_menu(x, y, 0) == 1 || hit_acct_menu(x, y, 1) == 1 ||
           hit_acct_menu(x, y, 2) == 1 || hit_acct_menu(x, y, 3) == 1 || hit_acct_menu(x, y, 4) == 1 ||
           hit_acct_menu(x, y, 5) == 1 || hit_acct_pin_del(x, y) == 1 || hit_acct_pin_save(x, y) == 1 ||
           hit_acct_submit(x, y) == 1 || hit_acct_gift_buy(x, y) == 1 || hit_acct_gift_send(x, y) == 1 ||
           hit_acct_gift_sell(x, y) == 1 || hit_acct_gift_inspect(x, y) == 1 || hit_acct_gift_any(x, y) == 1;
}
static void acct_sync(void) {
    if (keyboard_visible() == 1 && acct_field < acct_fields()) {
        keyboard_get_text_to(acct_buf[(int)acct_field], ACCT_BUF);
    }
}
static void acct_focus(double i) {
    acct_sync();
    acct_field = i;
    keyboard_clear();
    if (acct_buf[(int)i][0]) {
        keyboard_type(acct_buf[(int)i]);
    }
    keyboard_show();
}
static const char *acct_mask(double i) {
    const unsigned char *s = (const unsigned char *)acct_buf[(int)i];
    char *out = acct_mask_buf[(int)i];
    int n = 0;
    while (*s && n < ACCT_BUF) {
        if ((*s & 0xC0) != 0x80) {
            out[n++] = '*';
        }
        s++;
    }
    out[n] = 0;
    return out;
}
static void draw_acct_field(const char *label, double i) {
    double y = acct_field_y(i);
    double caret = acct_field == i && keyboard_visible() == 1;
    const char *shown = caret == 1 ? ds_concat(acct_mask(i), "|") : acct_mask(i);
    text(label, acct_x() + login_lab_ox, y - login_lab, text_color);
    roundrect(acct_x(), y, acct_w(), login_fh, field_radius, card_bg);
    text_in_field(shown, acct_x() + field_text_pad, y, login_fh, text_color, 0.6);
}
static double acct_is_admin_nick(void) {
    return admin_slot(net_login_nick()) != 0 ? 1 : 0;
}
static void acct_pass_try(void) {
    acct_sync();
    if (!acct_buf[0][0]) {
        acct_msg_set(tr_acct_need_pass());
    } else if (!acct_buf[1][0]) {
        acct_msg_set(tr_acct_need_new());
    } else if (strcmp(acct_buf[1], acct_buf[2]) != 0) {
        acct_msg_set(tr_acct_mismatch());
    } else if (strcmp(acct_buf[0], acct_buf[1]) == 0) {
        acct_msg_set(tr_acct_same());
    } else if (acct_is_admin_nick() == 1) {
        acct_msg_set(tr_acct_admin());
    } else {
        acct_busy = 2;
        acct_msg_set(tr_acct_changing());
        net_change_password(acct_buf[0], acct_buf[1]);
        acct_clear_fields();
        return;
    }
    keyboard_hide();
}
static void acct_delete_try(void) {
    acct_sync();
    keyboard_hide();
    if (!acct_buf[0][0]) {
        acct_msg_set(tr_acct_need_pass());
        return;
    }
    acct_busy = 1;
    acct_msg_set(tr_delete_busy());
    acct_knob = 1;
    net_delete_account(acct_buf[0]);
    acct_clear_fields();
}
/* Extra password: hash only, device-only, like the parental pin. */
static void acct_pin_save(void) {
    acct_sync();
    keyboard_hide();
    if (str_len(acct_buf[0]) < 4) {
        acct_msg_set(tr_pin2_short());
        return;
    }
    if (strcmp(acct_buf[0], acct_buf[1]) != 0) {
        acct_msg_set(tr_pin2_mismatch());
        return;
    }
    net_save_pin2((int)parent_hash(acct_buf[0]));
    acct_clear_fields();
    acct_msg_set(tr_pin2_saved());
}
static void acct_pin_remove(void) {
    net_save_pin2(0);
    acct_clear_fields();
    keyboard_hide();
    acct_msg_set(tr_pin2_removed());
}
static void acct_pin_page(void) { acct_go(ACCT_PIN); }
static void acct_password_page(void) { acct_go(ACCT_PASS); }
static void acct_delete_page(void) { acct_go(ACCT_DELETE); }
static void acct_delete_window(void) { acct_show(ACCT_DELETE); }
static void acct_sign_out(void) {
    net_logout();
    progress_reset_on_signout();
    acct_close();
}
static void touch_acct_menu(double x, double y) {
    if (hit_acct_menu(x, y, 0) == 1) {
        start_action_transition(acct_password_page);
    } else if (hit_acct_menu(x, y, 1) == 1) {
        login_after = ST_LOBBY;
        start_transition(ST_LOGIN);
    } else if (hit_acct_menu(x, y, 2) == 1) {
        start_action_transition(acct_sign_out);
    } else if (hit_acct_menu(x, y, 3) == 1) {
        start_action_transition(acct_delete_page);
    } else if (hit_acct_menu(x, y, 4) == 1) {
        start_action_transition(acct_gifts_page);
    } else if (hit_acct_menu(x, y, 5) == 1) {
        start_action_transition(acct_pin_page);
    }
}
static void touch_account(double x, double y, double action, double id) {
        /* Dismiss sticker preview modal on any press. */
        if (acct_gift_view_open == 1) {
        if (action == 0) {
            acct_gift_view_open = 0;
            press_points_clear();
        }
        return;
    }
    /* Route move/release events to collection scroll handler first. */
    if (action != 0 && touch_acct_gift_list(x, y, action, id) == 1) return;
    if (action == 2) {
        if (id == acct_drag) {
            acct_knob = clamp((x - acct_grab - acct_x() - 4) / (acct_w() - acct_knob_w() - 8), 0, 1);
        }
        return;
    }
    if (action == 1 || action == 3) {
        if (id == acct_drag) {
            acct_drag = -1;
            if (action == 1 && acct_knob >= 0.95) {
                acct_delete_try();
            }
        }
        return;
    }
    if (acct_gift_drag >= 0 && id != acct_gift_drag) { press_points_drop(id); return; }
    if (acct_button_hit(x, y) == 1) {
        play_button_click();
    }
    if (back_hit(x, y) == 1) {
        start_action_transition(acct_back);
        return;
    }
    if (acct_open == ACCT_MENU) {
        touch_acct_menu(x, y);
        return;
    }
    if (acct_busy == 0 && acct_done == 0 && touch_acct_gift_list(x, y, action, id) == 1) return;
    if (acct_busy != 0 || acct_done == 1 || acct_gift_busy != 0) {
        return;
    }
    double i = 0;
    while (i < acct_fields()) {
        if (hit_acct_field(x, y, i) == 1) {
            acct_focus(i);
            return;
        }
        i = i + 1;
    }
    if (hit_acct_gift_buy(x, y) == 1) { acct_gift_buy(); return; }
    if (hit_acct_gift_send(x, y) == 1) { acct_gift_send(); return; }
    if (hit_acct_gift_sell(x, y) == 1) { acct_gift_sell(); return; }
    if (hit_acct_gift_inspect(x, y) == 1) { acct_gift_view_open = 1; return; }
    if (hit_acct_submit(x, y) == 1) { acct_pass_try(); return; }
    if (hit_acct_pin_save(x, y) == 1) { acct_pin_save(); return; }
    if (hit_acct_pin_del(x, y) == 1) { acct_pin_remove(); return; }
    if (hit_acct_knob(x, y) == 1) {
        acct_sync();
        keyboard_hide();
        acct_drag = id;
        acct_grab = x - acct_knob_x();
        return;
    }
    acct_sync();
    keyboard_hide();
}
static void acct_poll_delete(void) {
    double st = net_delete_account_status();
    if (st == 2) {
        acct_busy = 0;
        acct_done = 1;
        acct_msg_set(tr_delete_done());
        progress_from_storage(); /* Reload wiped local progress. */
    } else if (st >= 3) {
        acct_busy = 0;
        acct_msg_set(st == 4 ? tr_acct_wrong() : tr_acct_failed());
    }
}
static void acct_poll_password(void) {
    double st = net_change_password_status();
    if (st < 2) {
        return;
    }
    acct_busy = 0;
    if (st == 2) {
        acct_msg_set(tr_acct_changed());
    } else if (st == 4) {
        acct_msg_set(tr_acct_wrong());
    } else if (st == 5) {
        acct_msg_set(tr_bad_pass());
    } else if (st == 6) {
        acct_msg_set(tr_acct_signed_out());
    } else if (st == 7) {
        acct_msg_set(tr_no_connection());
    } else if (st == 8) {
        acct_msg_set(tr_acct_too_many());
    } else {
        acct_msg_set(tr_acct_failed());
    }
}
static void acct_tick(void) {
    acct_gift_scroll_tick();
    acct_poll_gifts();
    if (acct_busy == 1) {
        acct_poll_delete();
    } else if (acct_busy == 2) {
        acct_poll_password();
    }
    if (acct_open == 0 || t_dir != 0) {
        return;
    }
    if (keyboard_visible() == 1) {
        acct_sync();
    }
    if (keyboard_enter_pressed() == 1) {
        if (acct_field + 1 < acct_fields()) {
            acct_focus(acct_field + 1);
        } else if (acct_open == ACCT_PASS && acct_busy == 0) {
            acct_pass_try();
        } else if (acct_open == ACCT_PIN) {
            acct_pin_save();
        } else {
            keyboard_hide();
        }
    }
    if (acct_drag == -1 && acct_busy == 0 && acct_done == 0 && acct_knob > 0) {
        acct_knob = clamp(acct_knob - dt * 3, 0, 1);
    }
}
static void draw_acct_slider(void) {
    double x = acct_x();
    double y = acct_slider_y();
    double w = acct_w();
    double h = acct_slider_h();
    double kx = acct_knob_x();
    double kw = acct_knob_w();
    roundrect(x, y, w, h, h / 2, card_bg);
    roundrect(x, y, kx - x + kw + 4, h, h / 2, RGB(140,28,44));
    double fade = 1 - clamp(acct_knob * 1.6, 0, 1);
    if (fade > 0 && acct_busy == 0 && acct_done == 0) {
        double tx = kx + kw + 12;
        double tw = x + w - tx - 16;
        const char *label = tr_acct_swipe();
        double sc = fit_text_scale(label, tw, 0.55);
        text_in_box(label, tx, y, tw, h, (1 + floor(fade * 254)) * 0x1000000 + text_color, sc);
    }
    roundrect(kx, y + 4, kw, h - 8, (h - 8) / 2, RGB(210,58,74));
    text_in_box(">>", kx, y + 4, kw, h - 8, text_color, 0.7);
}
static void draw_acct_msg(void) {
    if (str_len(acct_msg) > 0) {
        ctext_scaled(acct_msg, acct_msg_y(), text_color, fit_text_scale(acct_msg, fit_w(screen_w), 0.55));
    }
}
static void draw_account(void) {
    screen_fill();
    press_overlay = 3;
    draw_back();
    const char *title = acct_open == ACCT_PASS ? tr_acct_change()
                        : acct_open == ACCT_PIN ? tr_pin2_title()
                        : acct_open == ACCT_DELETE ? tr_delete_account()
                        : acct_open == ACCT_GIFTS ? (language == 1 ? "Подарки" : "Gifts")
                                                  : ds_concat(tr_acct_title(), net_login_nick());
    double tscale = fit_text_scale(title, fit_w(screen_w), 0.8);
    if (acct_open == ACCT_MENU) {
        const char *pre = tr_acct_title();
        const char *anick = net_login_nick();
        double pw = text_ink_width(pre) * tscale;
        double nw = text_ink_width(anick) * tscale;
        double tx = (screen_w - (pw + nw)) / 2;
        text_scaled(pre, tx, acct_top(), text_color, tscale);
        draw_styled_nick(anick, tx + pw, acct_top(), text_color, tscale);
    } else {
        ctext_scaled(title, acct_top(), text_color, tscale);
    }
    if (acct_open == ACCT_MENU) {
        const char *rows[6] = {tr_acct_change(), tr_acct_switch(), tr_acct_logout(), tr_delete_account(),
                               language == 1 ? "Подарки" : "Gifts", tr_pin2_menu()};
        double i = 0;
        while (i < 6) {
            const char *r = rows[(int)i];
            button_scaled(r, acct_x(), acct_menu_y(i), acct_w(), btn_h, fit_text_scale(r, acct_w() - 24, 0.65));
            i = i + 1;
        }
    } else if (acct_open == ACCT_GIFTS) {
        draw_acct_gifts();
    } else if (acct_open == ACCT_PIN) {
        draw_acct_field(tr_pin2_new(), 0);
        draw_acct_field(tr_pin2_repeat(), 1);
        ctext_scaled(tr_pin2_hint(), acct_field_y(0) - login_lab - 26, text_color,
                     fit_text_scale(tr_pin2_hint(), acct_w(), 0.45));
        if (net_load_pin2() > 0) {
            const char *save = tr_pin2_save();
            const char *del = tr_pin2_del();
            button_scaled(save, acct_x(), acct_below_fields(), acct_w() * 0.48, btn_h, 0.6);
            button_scaled(del, acct_x() + acct_w() * 0.52, acct_below_fields(), acct_w() * 0.48, btn_h, 0.6);
        } else {
            button_scaled(tr_pin2_save(), acct_x(), acct_below_fields(), acct_w(), btn_h, 0.65);
        }
    } else if (acct_open == ACCT_PASS) {
        draw_acct_field(tr_acct_old(), 0);
        draw_acct_field(tr_acct_new(), 1);
        draw_acct_field(tr_acct_repeat(), 2);
        const char *b = acct_busy == 2 ? tr_acct_changing() : tr_acct_change();
        button_scaled(b, acct_x(), acct_below_fields(), acct_w(), btn_h, fit_text_scale(b, acct_w() - 24, 0.65));
    } else {
        ctext_scaled(tr_acct_warn1(), acct_top() + 44, text_color, fit_text_scale(tr_acct_warn1(), fit_w(screen_w), 0.5));
        ctext_scaled(tr_acct_warn2(), acct_top() + 72, text_color, fit_text_scale(tr_acct_warn2(), fit_w(screen_w), 0.5));
        if (acct_done == 0) {
            draw_acct_field(tr_acct_pass(), 0);
            draw_acct_slider();
        }
    }
    draw_acct_msg();
    if (acct_open == ACCT_GIFTS && acct_gift_view_open == 1) {
        draw_acct_gift_viewer();
    }
    press_overlay = 0;
}

#include "account_gifts.inc"
#include "account_gifts_actions.inc"
