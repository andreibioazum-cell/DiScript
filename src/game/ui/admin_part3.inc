/* CCP touch routing, monochrome panel rendering, and continuous admin ticks. */

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
    if (admin_gifts_allowed() == 0) {
        if (admin_tab != 0) admin_keyboard_drop();
        admin_tab = 0;
    }
    double tab_y = admin_content_top();
    if (y >= tab_y && y <= tab_y + admin_row_h()) {
        if (admin_gifts_allowed() == 1) {
            double inner_x = px + admin_pad();
            double inner_w = pw - 2 * admin_pad();
            double tw = (inner_w - 3 * admin_gap()) / 4;
            int next_tab = (int)floor((x - inner_x) / (tw + admin_gap()));
            if (next_tab < 0) next_tab = 0;
            if (next_tab > 3) next_tab = 3;
            if ((int)admin_tab != next_tab) {
                admin_keyboard_drop();
                admin_tab = next_tab;
            }
            play_button_click();
        }
        return;
    }
    if (admin_gifts_allowed() == 1 && admin_tab == 1) {
        double ty = admin_content_top();
        double rhg = admin_row_h();
        double row_index = floor((y - ty) / (rhg + admin_gap()));
        if (row_index >= 1 && row_index <= 5) {
            admin_gift_choice = row_index - 1;
            return;
        }
        if (row_index == 6) {
            admin_kb_open(1, admin_gift_target);
            return;
        }
        if (row_index == 7) {
            admin_keyboard_drop();
            if (admin_gift_target[0] && admin_gift_busy != 1) {
                admin_gift_busy = net_gift_action(admin_gift_choice, admin_gift_target, 1, 0) == 0 ? 3 : 1;
            }
            return;
        }
        return;
    }
    if (admin_gifts_allowed() == 1 && admin_tab == 2) {
        admin_press_events_tab(x, y, px, pw);
        return;
    }
    if (admin_gifts_allowed() == 1 && admin_tab == 3) {
        admin_press_banner_tab(x, y, px, pw);
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
    /* Consume all touches while admin panel is open. */
    if (admin_panel_open == 1) {
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
    circle(cx, cy, r + 3, admin_off());
    circle(cx, cy, r, admin_body());
    ring(cx, cy, r, clamp(r * 0.11, 2, 3), admin_frame());
    if (iw > 0 && ih > 0) {
        double sw = (r * 1.5) / iw;
        double sh = (r * 0.95) / ih;
        sc = sw < sh ? sw : sh;
    }
    double tw = text_ink_width(lab) * sc;
    double th = text_ink_height(lab) * sc;
    double tt = text_ink_top(lab) * sc;
    text_scaled(lab, cx - tw / 2, cy - th / 2 - tt, admin_frame(), sc);
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

static void admin_draw_tabs(void) {
    double px = admin_panel_x() + admin_pad();
    double y = admin_content_top();
    double h = admin_row_h();
    if (admin_gifts_allowed() == 0) {
        admin_btn(language == 1 ? "Модификаторы" : "Modifiers", px, y, admin_panel_w() - 2 * admin_pad(), h, 1, 0.55);
        return;
    }
    double total_w = admin_panel_w() - 2 * admin_pad();
    double w = (total_w - 3 * admin_gap()) / 4;
    const char *t0 = language == 1 ? "Читы" : "Cheats";
    const char *t1 = language == 1 ? "Подарки" : "Gifts";
    const char *t2 = language == 1 ? "Ивенты" : "Events";
    const char *t3 = language == 1 ? "Текст" : "Text";
    admin_btn(t0, px, y, w, h, admin_tab == 0, fit_text_scale(t0, w - 10, 0.55));
    admin_btn(t1, px + (w + admin_gap()), y, w, h, admin_tab == 1, fit_text_scale(t1, w - 10, 0.55));
    admin_btn(t2, px + 2 * (w + admin_gap()), y, w, h, admin_tab == 2, fit_text_scale(t2, w - 10, 0.55));
    admin_btn(t3, px + 3 * (w + admin_gap()), y, w, h, admin_tab == 3, fit_text_scale(t3, w - 10, 0.55));
}

static void admin_draw_gifts(void) {
    double px = admin_panel_x() + admin_pad();
    double y = admin_content_top() + admin_row_h() + admin_gap();
    double w = admin_panel_w() - 2 * admin_pad();
    double rh = admin_row_h();
    for (int i = 0; i < 5; ++i) {
        const char *name = admin_gift_names[i];
        if (language == 1) name = i == 0 ? "Какашка" : i == 1 ? "Торт" : i == 2 ? "Мишка" : i == 3 ? "Ракета" : "Изумруд";
        const char *label = ds_concat(admin_gift_choice == i ? "> " : "", name);
        admin_btn(label, px, y + i * (rh + admin_gap()), w, rh, admin_gift_choice == i, 0.55);
    }
    double target_y = y + 5 * (rh + admin_gap());
    roundrect(px, target_y, w, rh, 8, admin_off());
    roundrect_outline(px, target_y, w, rh, 8, 2, admin_frame());
    const char *shown = admin_gift_target;
    if (admin_kb_field == 1 && keyboard_visible() == 1) {
        shown = ds_concat(admin_gift_target, "|");
    }
    const char *target_label = shown[0] ? shown : (language == 1 ? "Ник получателя" : "Recipient nickname");
    text_in_box(target_label, px + 8, target_y, w - 16, rh, admin_frame(), fit_text_scale(target_label, w - 20, 0.55));
    const char *grant = admin_gift_busy == 1 ? (language == 1 ? "Выдаём..." : "Granting...")
                        : admin_gift_busy == 2 ? (language == 1 ? "Подарок выдан!" : "Gift granted!")
                        : admin_gift_busy == 3 ? (language == 1 ? "Ошибка, нажмите для повтора" : "Failed - tap to retry")
                                               : (language == 1 ? "Выдать подарок" : "Grant gift");
    admin_btn(grant, px, target_y + rh + admin_gap(), w, rh, 0, fit_text_scale(grant, w - 20, 0.55));
}

/* Reference key of all events. It is drawn as plain text and cannot be
 * tapped: the event comes from the typed number in the field above. */
static const char *admin_event_key_line(int part) {
    if (language == 1) {
        return part == 0 ? "0 - выкл · 1 - Диско · 2 - Плиты"
                         : "3 - Дед Мороз · 4 - Кубик · 5 - Колонки";
    }
    return part == 0 ? "0 - off · 1 - Disco · 2 - Plates"
                     : "3 - Ded Moroz · 4 - Dice · 5 - Columns";
}

static void admin_draw_events(void) {
    double px = admin_panel_x() + admin_pad();
    double y = admin_content_top() + admin_row_h() + admin_gap();
    double w = admin_panel_w() - 2 * admin_pad();
    double rh = admin_row_h();
    double step = rh + admin_gap();
    int cur = (int)net_event();
    int typed = admin_event_from_input();
    /* Row 1: the input field. Nothing else in this tab picks an event. */
    roundrect(px, y, w, rh, 8, admin_off());
    roundrect_outline(px, y, w, rh, 8, 2, admin_frame());
    const char *ev_line;
    if (admin_kb_field == 2 && keyboard_visible() == 1) {
        const char *pre = language == 1 ? "Ивент № (0-5): " : "Event # (0-5): ";
        ev_line = ds_concat(ds_concat(pre, admin_event_input), "|");
    } else if (admin_event_input[0]) {
        const char *pre = language == 1 ? "Ивент №: " : "Event #: ";
        ev_line = ds_concat(pre, admin_event_input);
    } else {
        ev_line = language == 1 ? "Введите номер ивента: 0-5" : "Type the event number: 0-5";
    }
    text_in_box(ev_line, px + 8, y, w - 16, rh, admin_frame(), fit_text_scale(ev_line, w - 20, 0.55));

    /* Row 2: the whole key of the events, for reading only - no taps here. */
    double hint_y = y + step;
    const char *hint0 = admin_event_key_line(0);
    const char *hint1 = admin_event_key_line(1);
    centered_text(hint0, px, w, hint_y + rh * 0.18, admin_frame(), fit_text_scale(hint0, w - 8, 0.42));
    centered_text(hint1, px, w, hint_y + rh * 0.58, admin_frame(), fit_text_scale(hint1, w - 8, 0.42));

    /* Row 3: only two buttons - enable what was typed, or switch everything off. */
    double act_y = y + 2 * step;
    double hw = (w - admin_gap()) / 2;
    int on_now = (typed > 0 && cur == typed) ? 1 : 0;
    const char *act_lab = on_now == 1 ? (language == 1 ? "Выключить" : "Turn off")
                                      : (language == 1 ? "Включить ивент" : "Enable event");
    const char *off_lab = language == 1 ? "Выкл (0)" : "Off (0)";
    admin_btn(act_lab, px, act_y, hw, rh, on_now, fit_text_scale(act_lab, hw - 12, 0.55));
    admin_btn(off_lab, px + hw + admin_gap(), act_y, hw, rh, cur == 0 ? 1 : 0, fit_text_scale(off_lab, hw - 12, 0.55));

    /* Row 4: what the typed number will do once the button is tapped. */
    const char *status;
    if (typed < 0) {
        status = language == 1 ? "Введите номер ивента в поле выше" : "Type the event number above";
    } else if (typed == 0) {
        status = language == 1 ? "0. Выкл - событие выключится" : "0. Off - the event turns off";
    } else if (cur == typed) {
        status = ds_concat(language == 1 ? "Сейчас включён: " : "On now: ", admin_event_title(typed));
    } else {
        status = ds_concat(language == 1 ? "Включится: " : "Will turn on: ", admin_event_title(typed));
    }
    centered_text(status, px, w, act_y + step + rh * 0.3, admin_frame(), fit_text_scale(status, w - 8, 0.46));
}

static void admin_draw_banner_tab(void) {
    double px = admin_panel_x() + admin_pad();
    double y = admin_content_top() + admin_row_h() + admin_gap();
    double w = admin_panel_w() - 2 * admin_pad();
    double rh = admin_row_h();
    double step = rh + admin_gap();
    roundrect(px, y, w, rh, 8, admin_off());
    roundrect_outline(px, y, w, rh, 8, 2, admin_frame());
    const char *txt_line;
    if (admin_kb_field == 3 && keyboard_visible() == 1) {
        txt_line = admin_banner_input[0] ? ds_concat(admin_banner_input, "|") : "|";
    } else if (admin_banner_input[0]) {
        txt_line = admin_banner_input;
    } else {
        txt_line = language == 1 ? "Поле текста наверху..." : "Top banner text...";
    }
    text_in_box(txt_line, px + 8, y, w - 16, rh, admin_frame(), fit_text_scale(txt_line, w - 20, 0.55));

    const char *col_lab = admin_banner_color_label(admin_banner_color_idx);
    admin_btn(col_lab, px, y + step, w, rh, 0, fit_text_scale(col_lab, w - 20, 0.55));

    const char *send_lab = admin_banner_status_t > 0
                               ? (language == 1 ? "Текст отправлен!" : "Text sent!")
                               : (language == 1 ? "Написать наверху" : "Show text at top");
    admin_btn(send_lab, px, y + 2 * step, w, rh, admin_banner_status_t > 0 ? 1 : 0, fit_text_scale(send_lab, w - 20, 0.55));

    const char *clr_lab = language == 1 ? "Убрать текст" : "Clear text";
    admin_btn(clr_lab, px, y + 3 * step, w, rh, 0, fit_text_scale(clr_lab, w - 20, 0.55));
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
    roundrect_outline(px, py, pw, ph, 14, 3, admin_frame());
    roundrect(px, py, pw, hh, 14, admin_header());
    rect(px, py + hh / 2, pw, hh / 2, admin_header());
    line(px + 12, py + hh - 1, px + pw - 12, py + hh - 1, 2, admin_frame());
    text_in_field(title, px + 16, py, hh, admin_frame(), fit_text_scale(title, title_w, hh / 58.0));
    double cxx = px + pw - hh * 0.55;
    double cr = hh * 0.3;
    circle(cxx, py + hh / 2, cr, admin_off());
    ring(cxx, py + hh / 2, cr, clamp(cr * 0.14, 2, 3), admin_frame());
    text_in_box("X", cxx - cr, py + hh / 2 - cr, cr * 2, cr * 2, admin_frame(), cr / 30.0);
    admin_draw_tabs();
    if (admin_gifts_allowed() == 0 || admin_tab == 0) {
        admin_draw_cheats();
    } else if (admin_tab == 1) {
        admin_draw_gifts();
    } else if (admin_tab == 2) {
        admin_draw_events();
    } else {
        admin_draw_banner_tab();
    }
}

static void admin_draw(void) {
    if (admin_modifiers_allowed() == 0) {
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

/* Tick active continuous cheats, CCP inputs, and admin gift actions. */
static void admin_tick(void) {
    admin_ball_t = admin_ball_t + dt;
    if (admin_banner_status_t > 0) {
        admin_banner_status_t = tick_down(admin_banner_status_t);
    }
    if (admin_gifts_allowed() == 1 && admin_panel_open == 1) {
        if (keyboard_visible() == 1) {
            admin_kb_flush();
            if (keyboard_enter_pressed() == 1) {
                if (admin_tab == 2) {
                    admin_keyboard_drop();
                    admin_apply_typed_event();
                } else if (admin_tab == 3) {
                    admin_submit_banner();
                }
            }
        }
        if (admin_tab == 1 && admin_gift_busy == 1) {
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
