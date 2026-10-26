/* Online room browser with search, vertical scrolling, creation, and password join. */
#define ROOMS_MODE_LIST 0
#define ROOMS_MODE_CREATE 1
#define ROOMS_MODE_PASS 2

static double rooms_mode;
static double rooms_field; /* 1 = name, 2 = password, 3 = search */
static double rooms_waiting;
static char rooms_name_buf[25];
static const char *rooms_name;
static char rooms_pass_buf[64];
static const char *rooms_pass;
static char rooms_search_buf[25];
static const char *rooms_search;
static char rooms_selected[25];
static char rooms_current[25];
static char rooms_msg_buf[128];
static const char *rooms_msg;
static double rooms_scroll;
static double rooms_vel;
static double rooms_drag_id = -1;
static double rooms_drag_start_y;
static double rooms_drag_y;
static double rooms_drag_moved;
static double rooms_drag_row = -1;
static ScrollTrack rooms_track;

static double rooms_search_y(void) { return content_top() + 4; }
static double rooms_search_h(void) { return 44; }
static double rooms_list_y(void) { return content_top() + 58; }
static double rooms_actions_y(void) { return screen_h - 150; }
static double rooms_list_bottom(void) { return rooms_actions_y() - 16; }
static double rooms_row_h(void) { return 52; }
static double rooms_view_h(void) { return fmax(rooms_row_h(), rooms_list_bottom() - rooms_list_y()); }
static double rooms_row_y(double i) { return rooms_list_y() + i * (rooms_row_h() + 10) - rooms_scroll; }
static double rooms_w(void) { return fit_w(560); }
static double rooms_x(void) { return center_x(rooms_w()); }
static double rooms_form_y(double row) { return content_top() + 96 + row * 96; }
static double rooms_btn_y(void) { return rooms_form_y(2) - 24; }

static void rooms_set_msg(const char *text) {
    double i = 0;
    if (!text) text = "";
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
static const char *tr_rooms_no_match(void) { return language == 1 ? "Комнаты не найдены" : "No matching rooms"; }
static const char *tr_rooms_search_ph(void) { return language == 1 ? "Поиск комнаты..." : "Search room..."; }
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

/* Case-insensitive substring match against active search query. */
static double rooms_match_search(const char *name) {
    if (!rooms_search || rooms_search[0] == 0) return 1;
    if (!name || name[0] == 0) return 0;
    int i = 0;
    while (name[i] != 0) {
        int j = 0;
        while (rooms_search[j] != 0 && name[i + j] != 0) {
            char a = name[i + j];
            char b = rooms_search[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + ('a' - 'A'));
            if (b >= 'A' && b <= 'Z') b = (char)(b + ('a' - 'A'));
            if (a != b) break;
            j = j + 1;
        }
        if (rooms_search[j] == 0) return 1;
        i = i + 1;
    }
    return 0;
}

static double rooms_filtered_count(void) {
    double raw = net_roomlist_count();
    double n = 0;
    double i = 0;
    while (i < raw) {
        if (rooms_match_search(net_roomlist_name(i)) == 1) n = n + 1;
        i = i + 1;
    }
    return n;
}

static const char *rooms_filtered_name(double idx) {
    double raw = net_roomlist_count();
    double n = 0;
    double i = 0;
    while (i < raw) {
        const char *nm = net_roomlist_name(i);
        if (rooms_match_search(nm) == 1) {
            if (n == idx) return nm;
            n = n + 1;
        }
        i = i + 1;
    }
    return "";
}

static double rooms_content_h(void) {
    double n = rooms_filtered_count();
    return n <= 0 ? 0 : n * rooms_row_h() + (n - 1) * 10;
}

static double rooms_scroll_max(void) {
    double ex = rooms_content_h() - rooms_view_h();
    return ex > 0 ? ex : 0;
}

static double rooms_scroll_needed(void) { return rooms_scroll_max() > 0 ? 1 : 0; }

static void rooms_scroll_reset(void) {
    rooms_drag_id = -1;
    rooms_drag_moved = 0;
    rooms_drag_row = -1;
    rooms_vel = 0;
    scroll_track_reset(&rooms_track);
}

static void rooms_scroll_by(double dy) {
    if (rooms_scroll_needed() == 0) {
        rooms_scroll = 0;
        rooms_vel = 0;
        return;
    }
    rooms_scroll = clamp(rooms_scroll + dy, 0, rooms_scroll_max());
}

static void rooms_scroll_tick(void) {
    if (rooms_mode != ROOMS_MODE_LIST || rooms_scroll_needed() == 0) {
        rooms_scroll = 0;
        rooms_scroll_reset();
        return;
    }
    double mx = rooms_scroll_max();
    rooms_scroll = clamp(rooms_scroll, 0, mx);
    if (rooms_drag_id >= 0) {
        rooms_vel = 0;
        scroll_track_tick(&rooms_track);
        return;
    }
    if (fabs(rooms_vel) < 0.6) {
        rooms_vel = 0;
        return;
    }
    rooms_scroll = rooms_scroll + rooms_vel * dt;
    rooms_vel = rooms_vel * exp(-4.8 * dt);
    if (rooms_scroll <= 0 || rooms_scroll >= mx) {
        rooms_scroll = clamp(rooms_scroll, 0, mx);
        rooms_vel = 0;
    }
}

static void rooms_open(void) {
    rooms_mode = ROOMS_MODE_LIST;
    rooms_field = 0;
    rooms_waiting = 0;
    rooms_name_buf[0] = 0;
    rooms_pass_buf[0] = 0;
    rooms_search_buf[0] = 0;
    rooms_name = "";
    rooms_pass = "";
    rooms_search = "";
    rooms_selected[0] = 0;
    rooms_scroll = 0;
    rooms_scroll_reset();
    rooms_set_msg("");
    net_room_prep_reset();
    net_roomlist_fetch(FIREBASE);
}

static void rooms_field_draw(const char *label, const char *value, const char *ph, double y, double focused) {
    text(label, rooms_x() + login_lab_ox, y - login_lab, text_color);
    roundrect(rooms_x(), y, rooms_w(), login_fh, field_radius, card_bg);
    double caret = (focused == 1 && keyboard_visible() == 1) ? 1 : 0;
    const char *shown = value;
    if (str_len(value) == 0 && caret == 0) shown = ph;
    if (caret == 1) shown = ds_concat(value, "|");
    text_in_field(shown, rooms_x() + field_text_pad, y, login_fh, text_color, 0.6);
}

static void draw_rooms_search_bar(void) {
    double sx = rooms_x(), sy = rooms_search_y(), sw = rooms_w(), sh = rooms_search_h();
    double has_q = str_len(rooms_search) > 0 ? 1 : 0;
    roundrect(sx, sy, sw, sh, field_radius, card_bg);
    double caret = (rooms_field == 3 && keyboard_visible() == 1) ? 1 : 0;
    const char *shown = rooms_search;
    if (has_q == 0 && caret == 0) shown = tr_rooms_search_ph();
    if (caret == 1) shown = ds_concat(rooms_search, "|");
    text_in_field(shown, sx + field_text_pad, sy, sh, text_color, 0.55);
    if (has_q == 1) button_scaled("X", sx + sw - sh, sy, sh, sh, 0.5);
}

static void draw_rooms(void) {
    screen_fill();
    ctext(tr_rooms_title(), title_y(), text_color);
    if (rooms_mode == ROOMS_MODE_LIST) {
        draw_rooms_search_bar();
        double st = net_roomlist_status();
        if (st == 1) {
            ctext(tr_rooms_loading(), rooms_list_y() + 10, text_color);
        } else if (st == 4) {
            ctext(tr_no_connection(), rooms_list_y() + 10, text_color);
        } else {
            double raw = net_roomlist_count();
            double count = rooms_filtered_count();
            if (raw == 0) {
                ctext_scaled(tr_rooms_empty(), rooms_list_y() + 10, text_color, 0.55);
            } else if (count == 0) {
                ctext_scaled(tr_rooms_no_match(), rooms_list_y() + 10, text_color, 0.55);
            } else {
                rooms_scroll = clamp(rooms_scroll, 0, rooms_scroll_max());
                ds_gfx_scissor(rooms_x() - 4, rooms_list_y() - 2, rooms_w() + 8, rooms_view_h() + 4);
                double i = 0;
                while (i < count) {
                    double ry = rooms_row_y(i);
                    if (ry + rooms_row_h() >= rooms_list_y() - 2 && ry <= rooms_list_bottom() + 2) {
                        button_scaled(rooms_filtered_name(i), rooms_x(), ry, rooms_w(), rooms_row_h(), 0.6);
                    }
                    i = i + 1;
                }
                ds_gfx_scissor(0, 0, 0, 0);
                if (rooms_scroll_needed() == 1) {
                    double tx = rooms_x() + rooms_w() + 6, ty = rooms_list_y(), th = rooms_view_h();
                    roundrect(tx, ty, 5, th, 2, card_bg);
                    double bar_h = fmax(28, th * th / rooms_content_h());
                    if (bar_h > th) bar_h = th;
                    roundrect(tx, ty + (th - bar_h) * rooms_scroll / rooms_scroll_max(), 5, bar_h, 2, text_color);
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
    if (str_len(rooms_msg) > 0) ctext_scaled(rooms_msg, screen_h - 96, 0xFFFF6060, 0.55);
    draw_back();
}

#include "rooms_touch.inc"
