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

/* Reporting is deliberately a chat command, not a row, menu, or battle
 * button.  The short private notice confirms what happened without posting a
 * fake system message to everyone in the room. */
static char chat_notice[128];
static double chat_notice_t;
static double chat_notice_bad;

static void chat_set_notice(const char *text, double bad) {
    snprintf(chat_notice, sizeof(chat_notice), "%s", text ? text : "");
    chat_notice_t = 3.0;
    chat_notice_bad = bad;
}

static const char *chat_report_hint(void) {
    return language == 1 ? "Сообщение · /жалоба Ник причина" : "Message · /report Nick reason";
}

static const char *chat_skip_spaces(const char *text) {
    while (text && (*text == ' ' || *text == '\t'))
        text++;
    return text;
}

/* Return the text after a report command, or NULL for an ordinary chat line.
 * Both command spellings work in either game language so a room can switch
 * languages without changing the report syntax. */
static const char *chat_report_payload(const char *text) {
    const char *commands[] = {"/report", "/жалоба"};
    size_t i;
    if (!text)
        return NULL;
    for (i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        size_t n = strlen(commands[i]);
        if (strncmp(text, commands[i], n) == 0 &&
            (text[n] == 0 || text[n] == ' ' || text[n] == '\t'))
            return text + n;
    }
    return NULL;
}

/* Handle /report Nick reason and /жалоба Ник причина.  A handled command is
 * never sent to public chat. */
static double chat_report_command(const char *text) {
    const char *payload = chat_report_payload(text);
    const char *reason_start;
    char nick[24];
    char reason[128];
    size_t n = 0;
    if (!payload)
        return 0;
    payload = chat_skip_spaces(payload);
    while (payload[n] && payload[n] != ' ' && payload[n] != '\t') {
        if (n + 1 >= sizeof(nick)) {
            chat_set_notice(language == 1 ? "Слишком длинный ник" : "Nickname is too long", 1);
            return 1;
        }
        nick[n] = payload[n];
        n++;
    }
    nick[n] = 0;
    reason_start = chat_skip_spaces(payload + n);
    if (n == 0 || !reason_start || !*reason_start) {
        chat_set_notice(language == 1 ? "Формат: /жалоба Ник причина" : "Format: /report Nick reason", 1);
        return 1;
    }
    if (str_eq(nick, net_login_nick()) == 1) {
        chat_set_notice(language == 1 ? "Нельзя пожаловаться на себя" : "You cannot report yourself", 1);
        return 1;
    }
    snprintf(reason, sizeof(reason), "%s", chat_clean(reason_start));
    if (!reason[0]) {
        chat_set_notice(language == 1 ? "Укажите причину жалобы" : "Add a report reason", 1);
        return 1;
    }
    net_report_send(nick, reason, net_current_room());
    chat_set_notice(language == 1 ? "Жалоба отправлена" : "Report sent", 0);
    return 1;
}

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
    if (chat_report_command(chat_input) == 1) {
        chat_input = "";
        keyboard_clear();
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
    if (chat_notice_t > 0) {
        chat_notice_t = fmax(0, chat_notice_t - dt);
    }
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

/* Keep message text inside the chat column. */
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
    double notice_h = chat_notice_t > 0 ? 34 : 0;
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
        double bottom = iy - notice_h - 8;
        while (idx < total) {
            if (yy + line_h > bottom) {
                idx = total;
            } else {
                if (chat_is_sticker_msg(net_chat_text(idx)) == 0) {
                    const char *who = parent_nick(net_chat_uid(idx));
                    const char *rest = ds_concat(": ", chat_clean(net_chat_text(idx)));
                    const char *shown_msg = chat_clip_msg(who, rest, lw - 8, 0.5);
                    draw_nick_msg(who, shown_msg, lx, yy, text_color, 0.5);
                    yy = yy + line_h;
                }
                idx = idx + 1;
            }
        }
    }
    if (chat_notice_t > 0) {
        text_scaled(chat_notice, lx, iy - 24,
                    chat_notice_bad == 1 ? RGB(255,154,154) : RGB(190,240,200),
                    fit_text_scale(chat_notice, lw, 0.48));
    }
    roundrect(lx, iy, lw - 122, ih, 10, card_bg);
    chat_sync_input();
    const char *shown = chat_input;
    if (keyboard_visible() == 1) {
        shown = ds_concat(chat_input, "|");
    }
    if (str_len(chat_input) == 0 && keyboard_visible() == 0) {
        const char *hint = chat_report_hint();
        text_in_field(hint, lx + 10, iy, ih, text_color, fit_text_scale(hint, lw - 142, 0.55));
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
