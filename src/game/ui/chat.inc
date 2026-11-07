static double chat_close_x(void) {
    return (screen_w - btn_w) / 2;
}

static double chat_top_y(void) {
    return back_y;
}

static double chat_msg_top(void) {
    return chat_top_y() + btn_h + 4;
}

static double chat_input_y(void) {
    return screen_h - 72;
}

static double chat_visible_limit(void) {
    double ly = chat_msg_top();
    double iy = chat_input_y();
    double lh = iy - ly - 12;
    double line_h = 28;
    double maxl = floor(lh / line_h);
    if (maxl < 1) {
        maxl = 1;
    }
    return maxl;
}

static void chat_sync_input(void) {
    keyboard_get_text_to(chat_input_buf, sizeof(chat_input_buf));
    chat_input = chat_input_buf;
}

/* Return 1 when signed-in account nick is an admin. */
static double is_admin(void) {
    if (net_login_status() != 2) {
        return 0;
    }
    return admin_slot(net_login_nick()) != 0 ? 1 : 0;
}

static char admin_passwd_target_buf[24];

static const char *admin_actor_nick(void) {
    return admin_nick_of(admin_slot(net_login_nick()));
}

static void chat_send(void) {
    if (parent_chat_off() == 1) {
        return;
    }
    chat_sync_input();
    if (str_len(chat_input) == 0) {
        return;
    }
    if (is_admin() == 1) {
        if (str_eq(chat_input, "event 0") == 1 || str_eq(chat_input, "event 1") == 1 ||
            str_eq(chat_input, "event 2") == 1 || str_eq(chat_input, "event 3") == 1 ||
            str_eq(chat_input, "event 4") == 1 || str_eq(chat_input, "event 5") == 1 ||
            str_eq(chat_input, "event 6") == 1 || str_eq(chat_input, "event 7") == 1) {
            admin_handle(chat_input);
            chat_input = "";
            keyboard_clear();
            return;
        }
        if (net_chat_is_ban(chat_input) == 1 || net_chat_is_unban(chat_input) == 1 ||
            net_chat_is_passwd(chat_input) == 1) {
            admin_handle(chat_input);
            chat_input = "";
            keyboard_clear();
            return;
        }
        if (net_chat_is_text_cmd(chat_input) == 1) {
            admin_handle(chat_input);
            chat_input = "";
            keyboard_clear();
            return;
        }
    }
        /* Swearing and links go to the chat as asterisks (the game is 12+). */
    net_chat_send(chat_clean(chat_input));
    ds_log("chat: %s", chat_input);
    chat_input = "";
    keyboard_clear();
    net_chat_trim(chat_visible_limit());
}

static void chat_set_open(double v) {
    /* Parental control with 1-10 years keeps the chat closed. */
    chat_open = parent_chat_off() == 1 ? 0 : v;
    keyboard_hide();
}

/* Return 1 for legacy "/sticker ..." service messages hidden from chat. */
static double chat_is_sticker_msg(const char *msg) {
    return str_starts_with(msg, "/sticker ") == 1;
}

static void chat_update(void) {
    if (game_state != 5 || chat_open == 0) {
        return;
    }
    { double rs = net_admin_reset_status();
      if (rs == 2) {
          net_chat_send(ds_concat(language == 1 ? "Пароль изменён: " : "Password changed: ", admin_passwd_target_buf));
      } else if (rs >= 3) {
          net_chat_send(language == 1 ? "Не удалось изменить пароль (функция не развёрнута?)" : "Password change failed (function not deployed?)");
      } }
    if (keyboard_visible() == 1) {
        chat_sync_input();
    }
    if (keyboard_enter_pressed() == 1) {
        chat_send();
    }
    if (net_chat_count() > chat_visible_limit()) {
        net_chat_trim(chat_visible_limit());
    }
}

static double chat_btn_x(void) {
    return 16;
}

static double chat_btn_y(void) {
    return 108;
}

static double chat_btn_w(void) {
    return btn_w;
}

static double chat_btn_h(void) {
    return btn_h;
}

/* Selected message for a complaint: the snapshot keeps it valid even when
 * new lines push the buffer around. */
static double chat_sel;
static char chat_sel_nick[24];
static char chat_sel_text[128];
static double chat_row_y[16];
static double chat_row_idx[16];
static double chat_row_n;

static double chat_line_h(void) { return 28; }

static void chat_select_clear(void) {
    chat_sel = 0;
    chat_sel_nick[0] = 0;
    chat_sel_text[0] = 0;
}

static void chat_select(double idx) {
    const char *nick = net_chat_uid(idx);
    const char *text = net_chat_text(idx);
    if (!nick || !*nick || !text || !*text || chat_is_sticker_msg(text) == 1)
        return;
    if (str_eq(nick, net_login_nick()) == 1) {
        return; /* you cannot report yourself */
    }
    snprintf(chat_sel_nick, sizeof(chat_sel_nick), "%s", nick);
    snprintf(chat_sel_text, sizeof(chat_sel_text), "%s", chat_clean(text));
    chat_sel = 1;
}

static void chat_report(void) {
    if (chat_sel != 1 || !chat_sel_nick[0] || !chat_sel_text[0]) {
        chat_select_clear();
        return;
    }
    net_report_send(chat_sel_nick, chat_sel_text, net_current_room());
    chat_select_clear();
}

static double chat_report_x(void) { return screen_w - 46; }
static double chat_row_h(void) { return 22; }

static double chat_sel_bar_y(void) { return chat_input_y() - 46; }
static double chat_sel_btn_w(void) { return 86; }
static double chat_sel_send_x(void) { return 16 + (screen_w - 32) - 2 * chat_sel_btn_w() - 6; }
static double chat_sel_cancel_x(void) { return chat_sel_send_x() + chat_sel_btn_w() + 6; }

/* Shorten a message so "nick: text" stays left of the "!" report button. */
static const char *chat_clip_msg(const char *nick, const char *rest, double w, double scale) {
    static char out[192];
    size_t len = strlen(rest);
    double avail = w - text_ink_width(nick) * scale;
    char probe[196];
    if (len >= sizeof(out)) {
        len = sizeof(out) - 1;
    }
    memcpy(out, rest, len);
    out[len] = 0;
    if (avail <= 0) {
        out[0] = 0;
        return out;
    }
    if (text_ink_width(out) * scale <= avail) {
        return out;
    }
    while (len > 0) {
        len--;
        while (len > 0 && ((unsigned char)out[len] & 0xC0) == 0x80) {
            len--;
        }
        out[len] = 0;
        snprintf(probe, sizeof(probe), "%s...", out);
        if (text_ink_width(probe) * scale <= avail) {
            snprintf(out, sizeof(out), "%s", probe);
            return out;
        }
    }
    snprintf(out, sizeof(out), "%s", "...");
    return out;
}

static void draw_chat(void) {
    if (game_state != 5) {
        return;
    }
    if (chat_open == 0) {
        /* Parental control for 1-10 years: the chat entry point is gone. */
        if (parent_chat_off() == 0)
            button(tr_chat(), chat_btn_x(), chat_btn_y(), chat_btn_w(), chat_btn_h());
        return;
    }
    screen_fill();
    double cx = chat_close_x();
    /* The open chat covers the fight, so its own buttons claim the press. */
    press_overlay = 1;
    button(tr_close(), cx, chat_top_y(), btn_w, btn_h);
    /* Header: the room this chat belongs to (from the connection itself). */
    const char *room_title = net_current_room();
    if (str_len(room_title) == 0) {
        room_title = ROOM;
    }
    const char *room_header = ds_concat(language == 1 ? "Комната: " : "Room: ", room_title);
    text_scaled(room_header, 16, chat_top_y() + 16, text_color,
                fit_text_scale(room_header, (screen_w - btn_w) / 2 - 24, 0.6));
    double lx = 16;
    double lw = screen_w - 32;
    double iy = chat_input_y();
    double ih = 52;
    double ly = chat_msg_top();
    double line_h = 28;
    double maxl = chat_visible_limit();
    if (net_chat_count() > maxl) {
        net_chat_trim(maxl);
    }
    double total = net_chat_count();
    double start = total - maxl;
    if (start < 0) {
        start = 0;
    }
    if (total == 0) {
        text_scaled(tr_no_messages(), lx, ly, text_color, 0.5);
    } else {
        double idx = start;
        double yy = ly;
        double bottom = iy - (chat_sel == 1 ? 52 : 8);
        chat_row_n = 0;
        while (idx < total) {
            if (yy + line_h > bottom) {
                idx = total;
            } else {
                if (chat_is_sticker_msg(net_chat_text(idx)) == 0) {
                    if (chat_row_n < 16) {
                        chat_row_y[(int)chat_row_n] = yy;
                        chat_row_idx[(int)chat_row_n] = idx;
                        chat_row_n = chat_row_n + 1;
                    }
                    if (chat_sel == 1 && str_eq(chat_sel_nick, net_chat_uid(idx)) == 1)
                        roundrect(lx - 6, yy - 4, lw + 12, line_h, 8, RGB(58,53,96));
                    const char *who = parent_nick(net_chat_uid(idx));
                    const char *rest = ds_concat(": ", chat_clean(net_chat_text(idx)));
                    /* Keep the line clear of the "!" button on the right edge. */
                    const char *shown_msg = chat_clip_msg(who, rest, chat_report_x() - lx - 10, 0.5);
                    draw_nick_msg(who, shown_msg, lx, yy, text_color, 0.5);
                    if (str_eq(net_chat_uid(idx), net_login_nick()) == 0)
                        button_scaled_face("!", chat_report_x(), yy - 3, 28, 26, 0.5, RGB(192,48,74));
                    yy = yy + line_h;
                }
                idx = idx + 1;
            }
        }
    }
    if (chat_sel == 1) {
        const char *label = ds_concat(language == 1 ? "Жалоба на " : "Report ", chat_sel_nick);
        roundrect(lx, chat_sel_bar_y(), lw, 38, 10, RGB(46,42,70));
        text_scaled(label, lx + 12, chat_sel_bar_y() + 10, text_color,
                    fit_text_scale(label, lw - 2 * chat_sel_btn_w() - 40, 0.5));
        button_scaled(language == 1 ? "Жалоба" : "Report", chat_sel_send_x(), chat_sel_bar_y() + 2,
                      chat_sel_btn_w(), 34, 0.45);
        button_scaled(language == 1 ? "Отмена" : "Cancel", chat_sel_cancel_x(), chat_sel_bar_y() + 2,
                      chat_sel_btn_w(), 34, 0.45);
    }
    roundrect(lx, iy, lw - 122, ih, 10, card_bg);
    chat_sync_input();
    const char *shown = chat_input;
    if (keyboard_visible() == 1) {
        shown = ds_concat(chat_input, "|");
    }
    if (str_len(chat_input) == 0 && keyboard_visible() == 0) {
        text_in_field(tr_type_message(), lx + 10, iy, ih, text_color, 0.55);
    } else if (str_len(shown) == 0) {
        text_in_field("|", lx + 10, iy, ih, text_color, 0.55);
    } else {
        text_in_field(shown, lx + 10, iy, ih, text_color, 0.55);
    }
    button_scaled(tr_send(), lx + lw - 112, iy, 112, ih, 0.5);
    press_overlay = 0;
}

static double touch_chat(double x, double y, double action) {
    if (action != 0 || parent_chat_off() == 1) {
        return 0;
    }
    if (chat_open == 0) {
        if (hit_button(x, y, chat_btn_x(), chat_btn_y(), chat_btn_w(), chat_btn_h()) == 1) {
            chat_set_open(1);
            return 1;
        }
        return 0;
    }
    if (hit_button(x, y, chat_close_x(), chat_top_y(), btn_w, btn_h) == 1) {
        chat_set_open(0);
        return 1;
    }
    double lx = 16;
    double lw = screen_w - 32;
    double iy = chat_input_y();
    double ih = 52;
    if (hit_rect(x, y, lx, iy, lw - 122, ih) == 1) {
        if (keyboard_visible() == 0) {
            keyboard_show();
        }
        return 1;
    }
    if (hit_button(x, y, lx + lw - 112, iy, 112, ih) == 1) {
        chat_send();
        keyboard_hide();
        return 1;
    }
    if (chat_sel == 1) {
        if (hit_button(x, y, chat_sel_send_x(), chat_sel_bar_y() + 2, chat_sel_btn_w(), 34) == 1) {
            chat_report();
            return 1;
        }
        if (hit_button(x, y, chat_sel_cancel_x(), chat_sel_bar_y() + 2, chat_sel_btn_w(), 34) == 1) {
            chat_select_clear();
            return 1;
        }
    }
    /* The "!" button next to a message files the complaint in one tap. */
    {
        double i = 0;
        while (i < chat_row_n) {
            double ry = chat_row_y[(int)i];
            if (x >= chat_report_x() && hit_rect(x, y, chat_report_x(), ry - 3, 28, 26) == 1) {
                chat_select(chat_row_idx[(int)i]);
                chat_report();
                return 1;
            }
            i = i + 1;
        }
    }
    /* Tapping a message picks it for a complaint. */
    {
        double i = 0;
        while (i < chat_row_n) {
            double ry = chat_row_y[(int)i];
            if (y >= ry - 4 && y < ry + chat_line_h() && x > 8 && x < chat_report_x() - 4) {
                chat_select(chat_row_idx[(int)i]);
                return 1;
            }
            i = i + 1;
        }
    }
    keyboard_hide();
    return 1;
}

/* Mirror of touch_chat() for the click sound: only real controls count. */
static double chat_button_hit(double x, double y) {
    double lx = 16;
    double lw = screen_w - 32;
    double iy = chat_input_y();
    double ih = 52;
    if (chat_open == 0) {
        return parent_chat_off() == 0 && hit_button(x, y, chat_btn_x(), chat_btn_y(), chat_btn_w(), chat_btn_h());
    }
    if (hit_button(x, y, chat_close_x(), chat_top_y(), btn_w, btn_h) == 1 ||
        hit_button(x, y, lx + lw - 112, iy, 112, ih) == 1) {
        return 1;
    }
    return 0;
}
