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
    chat_sync_input();
    if (str_len(chat_input) == 0) {
        return;
    }
    if (is_admin() == 1) {
        if (str_eq(chat_input, "event 0") == 1 || str_eq(chat_input, "event 1") == 1 ||
            str_eq(chat_input, "event 2") == 1 || str_eq(chat_input, "event 3") == 1 ||
            str_eq(chat_input, "event 4") == 1) {
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
    chat_open = v;
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

static void draw_chat(void) {
    if (game_state != 5) {
        return;
    }
    if (chat_open == 0) {
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
        while (idx < total) {
            if (yy + line_h > iy - 8) {
                idx = total;
            } else {
                if (chat_is_sticker_msg(net_chat_text(idx)) == 0) {
                    draw_nick_msg(net_chat_uid(idx), ds_concat(": ", chat_clean(net_chat_text(idx))), lx, yy, text_color, 0.5);
                    yy = yy + line_h;
                }
                idx = idx + 1;
            }
        }
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
    if (action != 0) {
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
        return hit_button(x, y, chat_btn_x(), chat_btn_y(), chat_btn_w(), chat_btn_h());
    }
    if (hit_button(x, y, chat_close_x(), chat_top_y(), btn_w, btn_h) == 1 ||
        hit_button(x, y, lx + lw - 112, iy, 112, ih) == 1) {
        return 1;
    }
    return 0;
}
