/* Online room browser, creation/join modal, and password prompt. */
#define ROOMS_MODE_LIST 0
#define ROOMS_MODE_CREATE 1
#define ROOMS_MODE_PASS 2
#define ROOMS_MAX_ROWS 6

static double rooms_mode;
static double rooms_field; /* 1 = name, 2 = password */
static double rooms_waiting;
static char rooms_name_buf[25];
static const char *rooms_name;
static char rooms_pass_buf[64];
static const char *rooms_pass;
static char rooms_selected[25];
static char rooms_current[25];
static char rooms_msg_buf[128];
static const char *rooms_msg;

static double rooms_list_y(void) { return content_top() + 58; }
static double rooms_row_h(void) { return 52; }
static double rooms_row_y(double i) { return rooms_list_y() + i * (rooms_row_h() + 10); }
static double rooms_w(void) { return fit_w(560); }
static double rooms_x(void) { return center_x(rooms_w()); }
static double rooms_form_y(double row) { return content_top() + 96 + row * 96; }
static double rooms_btn_y(void) { return rooms_form_y(2) - 24; }
static double rooms_actions_y(void) { return screen_h - 150; }

static void rooms_set_msg(const char *text) {
    double i = 0;
    if (!text) {
        text = "";
    }
    while (text[(int)i] != 0 && i < (double)(sizeof(rooms_msg_buf) - 1)) {
        rooms_msg_buf[(int)i] = text[(int)i];
        i = i + 1;
    }
    rooms_msg_buf[(int)i] = 0;
    rooms_msg = rooms_msg_buf;
}

static const char *tr_rooms_title(void) { return language == 1 ? "Комнаты" : "Rooms"; }
static const char *tr_rooms_create(void) { return language == 1 ? "Создать комнату" : "Create room"; }
static const char *tr_rooms_refresh(void) { return language == 1 ? "Обновить" : "Refresh"; }
static const char *tr_rooms_loading(void) { return language == 1 ? "Загрузка..." : "Loading..."; }
static const char *tr_rooms_empty(void) { return language == 1 ? "Комнат пока нет - создай первую!" : "No rooms yet - create the first!"; }
static const char *tr_rooms_tap_hint(void) { return language == 1 ? "Нажми на комнату, чтобы войти" : "Tap a room to join"; }
static const char *tr_rooms_name(void) { return language == 1 ? "Название комнаты" : "Room name"; }
static const char *tr_rooms_name_ph(void) { return language == 1 ? "Например: dima-room" : "E.g.: dima-room"; }
static const char *tr_rooms_pass(void) { return language == 1 ? "Пароль (необязательно)" : "Password (optional)"; }
static const char *tr_rooms_pass_req(void) { return language == 1 ? "Пароль комнаты" : "Room password"; }
static const char *tr_rooms_join(void) { return language == 1 ? "Войти" : "Join"; }
static const char *tr_rooms_need_pass(void) { return language == 1 ? "У комнаты есть пароль" : "This room has a password"; }
static const char *tr_rooms_bad_pass(void) { return language == 1 ? "Неверный пароль" : "Wrong password"; }
static const char *tr_rooms_exists(void) { return language == 1 ? "Такая комната уже есть" : "That room already exists"; }
static const char *tr_rooms_bad_name(void) {
    return language == 1 ? "Название: 1-24 символа, латиница, цифры, _ и -" : "Name: 1-24 chars, letters, digits, _ and -";
}

static void rooms_open(void) {
    rooms_mode = ROOMS_MODE_LIST;
    rooms_field = 1;
    rooms_waiting = 0;
    rooms_name_buf[0] = 0;
    rooms_pass_buf[0] = 0;
    rooms_name = "";
    rooms_pass = "";
    rooms_selected[0] = 0;
    rooms_set_msg("");
    net_room_prep_reset();
    net_roomlist_fetch(FIREBASE);
}

static void rooms_sync_field(void) {
    if (keyboard_visible() == 0) {
        return;
    }
    if (rooms_field == 1) {
        keyboard_get_text_to(rooms_name_buf, sizeof(rooms_name_buf));
        rooms_name = rooms_name_buf;
    } else {
        keyboard_get_text_to(rooms_pass_buf, sizeof(rooms_pass_buf));
        rooms_pass = rooms_pass_buf;
    }
}

static void rooms_focus(double field) {
    rooms_sync_field();
    rooms_field = field;
    keyboard_clear();
    const char *value = field == 1 ? rooms_name : rooms_pass;
    if (str_len(value) > 0) {
        keyboard_type(value);
    }
    keyboard_show();
}

static void rooms_submit_create(void) {
    rooms_sync_field();
    keyboard_hide();
    if (str_len(rooms_name) == 0) {
        rooms_set_msg(tr_rooms_bad_name());
        return;
    }
    net_room_prepare(FIREBASE, rooms_name, rooms_pass, 1);
    rooms_waiting = 2; /* creating */
}

static void rooms_submit_join(void) {
    rooms_sync_field();
    keyboard_hide();
    net_room_prepare(FIREBASE, rooms_selected, rooms_pass, 0);
    rooms_waiting = 1; /* joining */
}

static void update_rooms(void) {
    if (keyboard_visible() == 1) {
        rooms_sync_field();
    }
    if (keyboard_enter_pressed() == 1 && rooms_waiting == 0) {
        if (rooms_mode == ROOMS_MODE_CREATE) {
            if (rooms_field == 1) {
                rooms_focus(2);
            } else {
                rooms_submit_create();
            }
        } else if (rooms_mode == ROOMS_MODE_PASS) {
            rooms_submit_join();
        }
        return;
    }
    if (rooms_waiting == 0) {
        return;
    }
    double st = net_room_prep_status();
    if (st == 1 || st == 0) {
        return;
    }
    double was_create = rooms_waiting == 2 ? 1 : 0;
    rooms_waiting = 0;
    if (st == 2) {
        /* Allowed in: hand the room over and enter the battle screen. */
        const char *name = was_create == 1 ? rooms_name : rooms_selected;
        double i = 0;
        while (name[(int)i] != 0 && i < (double)(sizeof(rooms_current) - 1)) {
            rooms_current[(int)i] = name[(int)i];
            i = i + 1;
        }
        rooms_current[(int)i] = 0;
        ROOM = rooms_current;
        keyboard_hide();
        start_transition(ST_ONLINE);
        return;
    }
    if (st == 3 && was_create == 0) {
        rooms_mode = ROOMS_MODE_PASS;
        rooms_field = 2;
        rooms_set_msg(str_len(rooms_pass) > 0 ? tr_rooms_bad_pass() : tr_rooms_need_pass());
        return;
    }
    if (st == 4) {
        rooms_set_msg(tr_rooms_exists());
        return;
    }
    if (st == 6) {
        rooms_set_msg(tr_rooms_bad_name());
        return;
    }
    rooms_set_msg(tr_no_connection());
}

static void rooms_field_draw(const char *label, const char *value, const char *ph, double y, double focused) {
    text(label, rooms_x() + login_lab_ox, y - login_lab, text_color);
    roundrect(rooms_x(), y, rooms_w(), login_fh, field_radius, card_bg);
    double caret = 0;
    if (focused == 1 && keyboard_visible() == 1) {
        caret = 1;
    }
    const char *shown = value;
    if (str_len(value) == 0 && caret == 0) {
        shown = ph;
    }
    if (caret == 1) {
        shown = ds_concat(value, "|");
    }
    text_in_field(shown, rooms_x() + field_text_pad, y, login_fh, text_color, 0.6);
}

static void draw_rooms(void) {
    screen_fill();
    ctext(tr_rooms_title(), title_y(), text_color);
    if (rooms_mode == ROOMS_MODE_LIST) {
        double st = net_roomlist_status();
        if (st == 1) {
            ctext(tr_rooms_loading(), rooms_list_y() + 10, text_color);
        } else if (st == 4) {
            ctext(tr_no_connection(), rooms_list_y() + 10, text_color);
        } else {
            double count = net_roomlist_count();
            if (count == 0) {
                ctext_scaled(tr_rooms_empty(), rooms_list_y() + 10, text_color, 0.55);
            } else {
                ctext_scaled(tr_rooms_tap_hint(), rooms_list_y() - 34, text_color, 0.5);
                double i = 0;
                while (i < count && i < ROOMS_MAX_ROWS) {
                    button_scaled(net_roomlist_name(i), rooms_x(), rooms_row_y(i), rooms_w(), rooms_row_h(), 0.6);
                    i = i + 1;
                }
            }
        }
        double bw = (rooms_w() - 16) / 2;
        button_scaled(tr_rooms_create(), rooms_x(), rooms_actions_y(), bw, btn_h, 0.6);
        button_scaled(tr_rooms_refresh(), rooms_x() + bw + 16, rooms_actions_y(), bw, btn_h, 0.6);
    } else if (rooms_mode == ROOMS_MODE_CREATE) {
        rooms_field_draw(tr_rooms_name(), rooms_name, tr_rooms_name_ph(), rooms_form_y(0), rooms_field == 1);
        rooms_field_draw(tr_rooms_pass(), rooms_pass, "", rooms_form_y(1), rooms_field == 2);
        button_scaled(tr_rooms_create(), rooms_x(), rooms_btn_y(), rooms_w(), btn_h, 0.65);
    } else {
        ctext_scaled(ds_concat(ds_concat(tr_rooms_title(), ": "), rooms_selected), rooms_form_y(0) - 40, text_color, 0.6);
        rooms_field_draw(tr_rooms_pass_req(), rooms_pass, "", rooms_form_y(0), rooms_field == 2);
        button_scaled(tr_rooms_join(), rooms_x(), rooms_btn_y() - 96, rooms_w(), btn_h, 0.65);
    }
    if (str_len(rooms_msg) > 0) {
        ctext_scaled(rooms_msg, screen_h - 96, 0xFFFF6060, 0.55);
    }
    draw_back();
}

static double rooms_any_hit(double x, double y) {
    if (back_hit(x, y) == 1) {
        return 1;
    }
    if (rooms_mode == ROOMS_MODE_LIST) {
        double bw = (rooms_w() - 16) / 2;
        if (hit_button(x, y, rooms_x(), rooms_actions_y(), bw, btn_h) == 1) {
            return 1;
        }
        if (hit_button(x, y, rooms_x() + bw + 16, rooms_actions_y(), bw, btn_h) == 1) {
            return 1;
        }
        double i = 0;
        double count = net_roomlist_count();
        while (i < count && i < ROOMS_MAX_ROWS) {
            if (hit_button(x, y, rooms_x(), rooms_row_y(i), rooms_w(), rooms_row_h()) == 1) {
                return 1;
            }
            i = i + 1;
        }
        return 0;
    }
    return hit_button(x, y, rooms_x(), rooms_mode == ROOMS_MODE_PASS ? rooms_btn_y() - 96 : rooms_btn_y(), rooms_w(), btn_h);
}

static void touch_rooms(double x, double y, double action) {
    if (action != 0 || rooms_waiting != 0) {
        return;
    }
    if (back_hit(x, y) == 1) {
        keyboard_hide();
        keyboard_clear();
        if (rooms_mode == ROOMS_MODE_LIST) {
            start_transition(ST_MODES);
        } else {
            rooms_mode = ROOMS_MODE_LIST;
            rooms_set_msg("");
        }
        return;
    }
    if (rooms_mode == ROOMS_MODE_LIST) {
        double bw = (rooms_w() - 16) / 2;
        if (hit_button(x, y, rooms_x(), rooms_actions_y(), bw, btn_h) == 1) {
            rooms_mode = ROOMS_MODE_CREATE;
            rooms_name = "";
            rooms_pass = "";
            rooms_set_msg("");
            rooms_focus(1);
            return;
        }
        if (hit_button(x, y, rooms_x() + bw + 16, rooms_actions_y(), bw, btn_h) == 1) {
            rooms_open();
            return;
        }
        double i = 0;
        double count = net_roomlist_count();
        while (i < count && i < ROOMS_MAX_ROWS) {
            if (hit_button(x, y, rooms_x(), rooms_row_y(i), rooms_w(), rooms_row_h()) == 1) {
                const char *name = net_roomlist_name(i);
                double k = 0;
                while (name[(int)k] != 0 && k < (double)(sizeof(rooms_selected) - 1)) {
                    rooms_selected[(int)k] = name[(int)k];
                    k = k + 1;
                }
                rooms_selected[(int)k] = 0;
                rooms_pass = "";
                net_room_prepare(FIREBASE, rooms_selected, "", 0);
                rooms_waiting = 1;
                return;
            }
            i = i + 1;
        }
        return;
    }
    if (rooms_mode == ROOMS_MODE_CREATE) {
        if (hit_button(x, y, rooms_x(), rooms_form_y(0), rooms_w(), login_fh) == 1) {
            rooms_focus(1);
            return;
        }
        if (hit_button(x, y, rooms_x(), rooms_form_y(1), rooms_w(), login_fh) == 1) {
            rooms_focus(2);
            return;
        }
        if (hit_button(x, y, rooms_x(), rooms_btn_y(), rooms_w(), btn_h) == 1) {
            rooms_submit_create();
            return;
        }
    } else {
        if (hit_button(x, y, rooms_x(), rooms_form_y(0), rooms_w(), login_fh) == 1) {
            rooms_focus(2);
            return;
        }
        if (hit_button(x, y, rooms_x(), rooms_btn_y() - 96, rooms_w(), btn_h) == 1) {
            rooms_submit_join();
            return;
        }
    }
    rooms_sync_field();
    keyboard_hide();
}
