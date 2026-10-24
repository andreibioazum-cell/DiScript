#define ACCT_MENU 1
#define ACCT_PASS 2
#define ACCT_DELETE 3
#define ACCT_GIFTS 4
#define ACCT_BUF 64
static double acct_gift_choice = -1; /* выбранный магазинный стикер, -1 - никакой */
static double acct_gift_is_owned = 0; /* 1 - выбран стикер из коллекции */
static double acct_gift_own_row = -1; /* какая строка коллекции выбрана */
static double acct_gift_scroll = 0;
static double acct_gift_vel = 0;
static double acct_gift_drag = -1;
static double acct_gift_drag_y = 0;
static double acct_gift_drag_move = 0;
static double acct_gift_drag_row = -1; /* as which card the drag started */
static double acct_gift_busy = 0;
static double acct_gift_busy_mode = 0;
static double acct_gift_acted = -1; /* стикер, о котором идёт запрос */
static double acct_gift_view_open = 0; /* 1 - осмотр стикера в полное разрешение */
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
static void acct_gift_scroll_tick(void);
static double acct_entry; /* режим, с которого открыли: «назад» возвращает туда */
static double acct_field; /* поле ввода в фокусе */
static char acct_buf[3][ACCT_BUF];
static char acct_mask_buf[3][ACCT_BUF + 4];
static double acct_busy; /* 1 - идёт удаление, 2 - идёт смена пароля */
static double acct_done; /* 1 - аккаунт удалён */
static double acct_knob;     /* положение ползунка удаления, 0..1 */
static double acct_drag = -1; /* палец, который тащит ползунок */
static double acct_pending;  /* 1 - открыть аккаунт в конце перехода */
static double acct_grab;     /* где палец взялся за ползунок */
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
    acct_clear_fields();
    ui_nav_gen = ui_nav_gen + 1;
    /* The panel takes the finger; the game below must not keep it. */
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
    ui_nav_gen = ui_nav_gen + 1;
    press_pulses_clear();
}
static void acct_back(void) {
        /* The viewer closes first: back does not leave the page while the view
         * window is open. */
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
static double acct_fields(void) { return acct_open == ACCT_PASS ? 3 : (acct_open == ACCT_DELETE || acct_open == ACCT_GIFTS ? 1 : 0); }
static double acct_fields_top(void) { return acct_open == ACCT_DELETE ? acct_top() + 116 : (acct_open == ACCT_GIFTS ? acct_gift_footer_top() + login_lab + 2 : acct_top() + 44); }
static double acct_field_y(double i) { return acct_fields_top() + login_lab + i * (login_fh + login_lab + 12); }
static double acct_below_fields(void) { return acct_field_y(acct_fields() - 1) + login_fh + 22; }
static double acct_slider_y(void) { return acct_below_fields(); }
static double acct_slider_h(void) { return btn_h + 8; }
static double acct_knob_w(void) { return acct_slider_h() * 1.5; }
static double acct_knob_x(void) { return acct_x() + 4 + acct_knob * (acct_w() - acct_knob_w() - 8); }
static double acct_msg_y(void) {
    if (acct_open == ACCT_MENU) {
        return acct_menu_y(4) + 4;
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
static double acct_button_hit(double x, double y) {
    return back_hit(x, y) == 1 || hit_acct_menu(x, y, 0) == 1 || hit_acct_menu(x, y, 1) == 1 ||
           hit_acct_menu(x, y, 2) == 1 || hit_acct_menu(x, y, 3) == 1 || hit_acct_menu(x, y, 4) == 1 ||
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
    }
}
static void touch_account(double x, double y, double action, double id) {
        /* An open viewer takes everything: any new press closes it, and the finger
         * lift that opened it does nothing. */
    if (acct_gift_view_open == 1) {
        if (action == 0) {
            acct_gift_view_open = 0;
            press_points_clear();
        }
        return;
    }
    /* The collection scrolls with its own down/move/up machine, so it gets
     * the move and up events before the usual press handling below. */
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
    if (acct_busy != 0 || acct_done == 1 || acct_gift_busy != 0) {
        return;
    }
    if (touch_acct_gift_list(x, y, action, id) == 1) return;
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
        progress_from_storage(); /* забыть стёртый прогресс, чтобы он не вернулся */
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
    roundrect(x, y, kx - x + kw + 4, h, h / 2, 0xFF8C1C2C);
    double fade = 1 - clamp(acct_knob * 1.6, 0, 1);
    if (fade > 0 && acct_busy == 0 && acct_done == 0) {
        double tx = kx + kw + 12;
        double tw = x + w - tx - 16;
        const char *label = tr_acct_swipe();
        double sc = fit_text_scale(label, tw, 0.55);
        text_in_box(label, tx, y, tw, h, (1 + floor(fade * 254)) * 0x1000000 + text_color, sc);
    }
    roundrect(kx, y + 4, kw, h - 8, (h - 8) / 2, 0xFFD23A4A);
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
                        : acct_open == ACCT_DELETE ? tr_delete_account()
                        : acct_open == ACCT_GIFTS ? (language == 1 ? "Подарки" : "Gifts")
                                                  : ds_concat(tr_acct_title(), net_login_nick());
    ctext_scaled(title, acct_top(), text_color, fit_text_scale(title, fit_w(screen_w), 0.8));
    if (acct_open == ACCT_MENU) {
        const char *rows[5] = {tr_acct_change(), tr_acct_switch(), tr_acct_logout(), tr_delete_account(), language == 1 ? "Подарки" : "Gifts"};
        double i = 0;
        while (i < 5) {
            const char *r = rows[(int)i];
            button_scaled(r, acct_x(), acct_menu_y(i), acct_w(), btn_h, fit_text_scale(r, acct_w() - 24, 0.65));
            i = i + 1;
        }
    } else if (acct_open == ACCT_GIFTS) {
        draw_acct_gifts();
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
