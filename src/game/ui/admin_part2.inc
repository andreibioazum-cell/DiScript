/* Admin chat commands, CCP input handling, event scroll, and banner text actions. */

static const char *admin_event_title(int ev) {
    if (language == 1) {
        if (ev == 1) return "1. Диско";
        if (ev == 2) return "2. Плиты";
        if (ev == 3) return "3. Дед Мороз";
        if (ev == 4) return "4. Кубик";
        if (ev == 5) return "5. Колонки";
        return "0. Выкл";
    }
    if (ev == 1) return "1. Disco";
    if (ev == 2) return "2. Plates";
    if (ev == 3) return "3. Ded Moroz";
    if (ev == 4) return "4. Dice";
    if (ev == 5) return "5. Columns";
    return "0. Off";
}

static void admin_set_event_mode(int ev) {
    const char *by = admin_actor_nick();
    if (ev <= 0) {
        net_event_set(0);
        net_chat_send(ds_concat("СОБЫТИЕ ВЫКЛЮЧИЛ ", by));
        ds_log("admin: event 0 -> OFF");
        return;
    }
    if (ev == 1) {
        net_event_set(1);
        net_chat_send(ds_concat(ds_concat("ДИСКО-СОБЫТИЕ от ", by), "!"));
        ds_log("admin: event 1 disco");
    } else if (ev == 2) {
        net_event_set(2);
        net_chat_send(ds_concat("СОБЫТИЕ «ПЛИТЫ» от ", by));
        ds_log("admin: event 2 plates");
    } else if (ev == 3) {
        net_event_set(3);
        net_chat_send(ds_concat("СОБЫТИЕ «ДЕД МОРОЗ» от ", by));
        ds_log("admin: event 3 santa");
    } else if (ev == 4) {
        net_event_set(4);
        net_chat_send(ds_concat(ds_concat("СОБЫТИЕ «КУБИК» от ", by), "!"));
        ds_log("admin: event 4 dice");
    } else if (ev == 5) {
        net_event_set(5);
        net_chat_send(ds_concat(ds_concat("СОБЫТИЕ «КОЛОНКИ» от ", by), "!"));
        ds_log("admin: event 5 columns");
    }
}

static void admin_sync_event_scroll(void) {
    int found = -1;
    int i = 0;
    while (admin_event_input[i] != 0) {
        if (admin_event_input[i] >= '0' && admin_event_input[i] <= '5') {
            found = admin_event_input[i] - '0';
        }
        i = i + 1;
    }
    if (found < 0) {
        return;
    }
    admin_event_choice = found;
    if (found == 0) {
        admin_event_scroll = 0;
    } else {
        int sc = found - 1;
        int max_sc = ADMIN_EVENT_COUNT - ADMIN_EVENT_VIEW;
        if (sc < 0) sc = 0;
        if (sc > max_sc) sc = max_sc;
        admin_event_scroll = sc;
    }
}

static const char *admin_banner_color_id(int idx) {
    static const char *const ids[ADMIN_BANNER_COLORS] = {
        "white", "red", "yellow", "blue", "green", "orange", "purple", "pink", "cyan"
    };
    if (idx < 0 || idx >= ADMIN_BANNER_COLORS) {
        return "white";
    }
    return ids[idx];
}

static const char *admin_banner_color_label(int idx) {
    if (language == 1) {
        if (idx == 1) return "Цвет: Красный";
        if (idx == 2) return "Цвет: Жёлтый";
        if (idx == 3) return "Цвет: Голубой";
        if (idx == 4) return "Цвет: Зелёный";
        if (idx == 5) return "Цвет: Оранжевый";
        if (idx == 6) return "Цвет: Фиолетовый";
        if (idx == 7) return "Цвет: Розовый";
        if (idx == 8) return "Цвет: Бирюзовый";
        return "Цвет: Белый";
    }
    if (idx == 1) return "Color: Red";
    if (idx == 2) return "Color: Yellow";
    if (idx == 3) return "Color: Blue";
    if (idx == 4) return "Color: Green";
    if (idx == 5) return "Color: Orange";
    if (idx == 6) return "Color: Purple";
    if (idx == 7) return "Color: Pink";
    if (idx == 8) return "Color: Cyan";
    return "Color: White";
}

static void admin_kb_flush(void) {
    if (keyboard_visible() != 1) {
        return;
    }
    int f = admin_kb_field;
    if (f == 0) {
        f = admin_tab == 1 ? 1 : (admin_tab == 2 ? 2 : (admin_tab == 3 ? 3 : 0));
    }
    if (f == 1) {
        keyboard_get_text_to(admin_gift_target, sizeof(admin_gift_target));
    } else if (f == 2) {
        keyboard_get_text_to(admin_event_input, sizeof(admin_event_input));
        admin_sync_event_scroll();
    } else if (f == 3) {
        keyboard_get_text_to(admin_banner_input, sizeof(admin_banner_input));
    }
}

/* Hide soft keyboard when closing admin panel or switching tabs. */
static void admin_keyboard_drop(void) {
    if (keyboard_visible() == 1) {
        admin_kb_flush();
        keyboard_hide();
    }
    admin_kb_field = 0;
}

static void admin_kb_open(int field, const char *initial) {
    admin_kb_flush();
    admin_kb_field = field;
    keyboard_clear();
    if (initial && initial[0]) {
        keyboard_type(initial);
    }
    keyboard_show();
}

static void admin_submit_banner(void) {
    admin_keyboard_drop();
    int start = 0;
    while (admin_banner_input[start] == ' ' || admin_banner_input[start] == '\t') {
        start = start + 1;
    }
    int end = (int)strlen(admin_banner_input);
    while (end > start && (admin_banner_input[end - 1] == ' ' || admin_banner_input[end - 1] == '\t')) {
        end = end - 1;
    }
    if (end <= start) {
        return;
    }
    char clean[72];
    int n = end - start;
    if (n >= (int)sizeof(clean)) {
        n = (int)sizeof(clean) - 1;
    }
    memcpy(clean, admin_banner_input + start, (size_t)n);
    clean[n] = 0;
    const char *by = admin_actor_nick();
    const char *col = admin_banner_color_id(admin_banner_color_idx);
    const char *full = ds_concat(ds_concat(by, ": "), clean);
    net_banner_send(full, col);
    banner_seen_ts = net_banner_ts();
    banner_t = banner_show_time;
    admin_banner_input[0] = 0;
    keyboard_clear();
    admin_banner_status_t = 1.6;
    play_button_click();
    ds_log("admin ccp text: %s (%s)", full, col);
}

static void admin_clear_banner(void) {
    admin_keyboard_drop();
    net_banner_clear();
    banner_seen_ts = 0;
    banner_t = 0;
    admin_banner_status_t = 0;
    play_button_click();
}

static double admin_handle(const char *msg) {
    if (is_admin() == 0) {
        return 0;
    }
    const char *by = admin_actor_nick();
    if (str_eq(msg, "event 0") == 1) {
        admin_set_event_mode(net_event() == 0 ? 1 : 0);
        return 1;
    }
    if (str_eq(msg, "event 1") == 1) {
        admin_set_event_mode(1);
        return 1;
    }
    if (str_eq(msg, "event 2") == 1) {
        admin_set_event_mode(2);
        return 1;
    }
    if (str_eq(msg, "event 3") == 1) {
        admin_set_event_mode(3);
        return 1;
    }
    if (str_eq(msg, "event 4") == 1) {
        admin_set_event_mode(4);
        return 1;
    }
    if (str_eq(msg, "event 5") == 1) {
        admin_set_event_mode(5);
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
            while (t1[(int)i] != 0 && i < (double)(sizeof(admin_passwd_target_buf) - 1)) {
                admin_passwd_target_buf[(int)i] = t1[(int)i];
                i = i + 1;
            }
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

static void admin_panel_close(void) {
    admin_keyboard_drop();
    admin_panel_open = 0;
}

static void admin_press_events_tab(double x, double y, double px, double pw) {
    double ty = admin_content_top();
    double rhg = admin_row_h();
    int row_index = (int)floor((y - ty) / (rhg + admin_gap()));
    if (x < px + admin_pad() || x > px + pw - admin_pad()) {
        return;
    }
    if (row_index == 1) {
        admin_event_input[0] = 0;
        admin_kb_open(2, "");
        play_button_click();
        return;
    }
    if (row_index >= 2 && row_index <= 1 + ADMIN_EVENT_VIEW) {
        int ev = admin_event_scroll + 1 + (row_index - 2);
        if (ev >= 1 && ev <= ADMIN_EVENT_COUNT) {
            admin_keyboard_drop();
            admin_event_choice = ev;
            admin_event_input[0] = (char)('0' + ev);
            admin_event_input[1] = 0;
            admin_set_event_mode((int)net_event() == ev ? 0 : ev);
            play_button_click();
        }
        return;
    }
    if (row_index == 2 + ADMIN_EVENT_VIEW) {
        admin_keyboard_drop();
        if (x < px + pw / 2) {
            int target = (admin_event_choice <= 0 || (int)net_event() == admin_event_choice) ? 0 : admin_event_choice;
            admin_set_event_mode(target);
        } else {
            admin_event_choice = 0;
            admin_event_input[0] = '0';
            admin_event_input[1] = 0;
            admin_set_event_mode(0);
        }
        play_button_click();
    }
}

static void admin_press_banner_tab(double x, double y, double px, double pw) {
    double ty = admin_content_top();
    double rhg = admin_row_h();
    int row_index = (int)floor((y - ty) / (rhg + admin_gap()));
    if (x < px + admin_pad() || x > px + pw - admin_pad()) {
        return;
    }
    if (row_index == 1) {
        admin_banner_status_t = 0;
        admin_kb_open(3, admin_banner_input);
        play_button_click();
        return;
    }
    if (row_index == 2) {
        admin_kb_flush();
        admin_banner_color_idx = (admin_banner_color_idx + 1) % ADMIN_BANNER_COLORS;
        play_button_click();
        return;
    }
    if (row_index == 3) {
        admin_submit_banner();
        return;
    }
    if (row_index == 4) {
        admin_clear_banner();
    }
}
