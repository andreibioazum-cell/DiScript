/* Input, form submission, and scroll gesture handling for the room browser. */
static void rooms_sync_field(void) {
    if (keyboard_visible() == 0) return;
    if (rooms_field == 1) {
        keyboard_get_text_to(rooms_name_buf, sizeof(rooms_name_buf));
        rooms_name = rooms_name_buf;
    } else if (rooms_field == 2) {
        keyboard_get_text_to(rooms_pass_buf, sizeof(rooms_pass_buf));
        rooms_pass = rooms_pass_buf;
    } else if (rooms_field == 3) {
        keyboard_get_text_to(rooms_search_buf, sizeof(rooms_search_buf));
        rooms_search = rooms_search_buf;
        rooms_scroll = clamp(rooms_scroll, 0, rooms_scroll_max());
    }
}

static void rooms_focus(double field) {
    rooms_sync_field();
    rooms_field = field;
    keyboard_clear();
    const char *value = field == 1 ? rooms_name : (field == 2 ? rooms_pass : rooms_search);
    if (str_len(value) > 0) keyboard_type(value);
    keyboard_show();
}

static void rooms_begin_join(const char *name) {
    double k = 0;
    while (name[(int)k] != 0 && k < (double)(sizeof(rooms_selected) - 1)) {
        rooms_selected[(int)k] = name[(int)k];
        k = k + 1;
    }
    rooms_selected[(int)k] = 0;
    rooms_pass = "";
    keyboard_hide();
    net_room_prepare(FIREBASE, rooms_selected, "", 0);
    rooms_waiting = 1;
}

static void rooms_submit_create(void) {
    rooms_sync_field();
    keyboard_hide();
    if (str_len(rooms_name) == 0) {
        rooms_set_msg(tr_rooms_bad_name());
        return;
    }
    net_room_prepare(FIREBASE, rooms_name, rooms_pass, 1);
    rooms_waiting = 2;
}

static void rooms_submit_join(void) {
    rooms_sync_field();
    keyboard_hide();
    net_room_prepare(FIREBASE, rooms_selected, rooms_pass, 0);
    rooms_waiting = 1;
}

/* A failed list refreshes itself a few times: room data lives behind the
 * published rules, so a dropped VPN link used to leave the page empty until the
 * player re-entered the screen. */
static double rooms_retry_t;
static double rooms_retry_left = 3;

static void rooms_retry_tick(void) {
    if (rooms_mode != ROOMS_MODE_LIST || net_roomlist_status() != 4) {
        rooms_retry_t = 0;
        rooms_retry_left = 3;
        return;
    }
    rooms_retry_t = rooms_retry_t + dt;
    if (rooms_retry_t < 2.5 || rooms_retry_left <= 0) return;
    rooms_retry_t = 0;
    rooms_retry_left = rooms_retry_left - 1;
    net_roomlist_fetch(FIREBASE);
}

static void update_rooms(void) {
    if (keyboard_visible() == 1) rooms_sync_field();
    rooms_scroll_tick();
    rooms_retry_tick();
    if (keyboard_enter_pressed() == 1 && rooms_waiting == 0) {
        if (rooms_mode == ROOMS_MODE_LIST && rooms_field == 3) {
            rooms_sync_field();
            keyboard_hide();
            rooms_field = 0;
        } else if (rooms_mode == ROOMS_MODE_CREATE) {
            if (rooms_field == 1) rooms_focus(2);
            else rooms_submit_create();
        } else if (rooms_mode == ROOMS_MODE_PASS) {
            rooms_submit_join();
        }
        return;
    }
    if (rooms_waiting == 0) return;
    double st = net_room_prep_status();
    if (st == 1 || st == 0) return;
    double was_create = rooms_waiting == 2 ? 1 : 0;
    rooms_waiting = 0;
    if (st == 2) {
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
    if (st == 4) { rooms_set_msg(tr_rooms_exists()); return; }
    if (st == 6) { rooms_set_msg(tr_rooms_bad_name()); return; }
    rooms_set_msg(tr_no_connection());
}

static double rooms_hit_row_at(double x, double y) {
    if (y < rooms_list_y() || y > rooms_list_bottom()) return -1;
    double count = rooms_filtered_count();
    double i = 0;
    while (i < count) {
        if (hit_button(x, y, rooms_x(), rooms_row_y(i), rooms_w(), rooms_row_h()) == 1) return i;
        i = i + 1;
    }
    return -1;
}

static double rooms_any_hit(double x, double y) {
    if (back_hit(x, y) == 1) return 1;
    if (rooms_mode == ROOMS_MODE_LIST) {
        double bw = (rooms_w() - 16) / 2;
        if (hit_button(x, y, rooms_x(), rooms_actions_y(), bw, btn_h) == 1) return 1;
        if (hit_button(x, y, rooms_x() + bw + 16, rooms_actions_y(), bw, btn_h) == 1) return 1;
        if (str_len(rooms_search) > 0 &&
            hit_button(x, y, rooms_x() + rooms_w() - rooms_search_h(), rooms_search_y(), rooms_search_h(), rooms_search_h()) == 1) {
            return 1;
        }
        return rooms_hit_row_at(x, y) >= 0 ? 1 : 0;
    }
    return hit_button(x, y, rooms_x(), rooms_mode == ROOMS_MODE_PASS ? rooms_btn_y() - 96 : rooms_btn_y(), rooms_w(), btn_h);
}

static void touch_rooms(double x, double y, double action, double id) {
    if (rooms_waiting != 0) return;
    if (action == 3) {
        if (rooms_drag_id < 0 || id == rooms_drag_id) rooms_scroll_reset();
        return;
    }
    if (action == 2) {
        if (rooms_mode != ROOMS_MODE_LIST || rooms_drag_id < 0 || id != rooms_drag_id) return;
        double d = rooms_drag_y - y;
        rooms_drag_y = y;
        if (fabs(d) < 1e-6) return;
        if (rooms_scroll_needed() == 1) {
            rooms_scroll_by(d);
            scroll_track_move(&rooms_track, d);
            if (fabs(y - rooms_drag_start_y) > 10) {
                rooms_drag_moved = 1;
                press_points_drop(id);
            }
        }
        return;
    }
    if (action == 1) {
        if (id != rooms_drag_id) return;
        double row = rooms_drag_row;
        double moved = rooms_drag_moved;
        rooms_drag_id = -1;
        rooms_drag_row = -1;
        rooms_drag_moved = 0;
        rooms_vel = rooms_scroll_needed() == 1 ? scroll_track_release(&rooms_track, 2200.0) : 0;
        if (moved == 0 && row >= 0 && rooms_hit_row_at(x, y) == row) {
            rooms_vel = 0;
            play_button_click();
            rooms_begin_join(rooms_filtered_name(row));
        }
        return;
    }
    if (action != 0) return;
    if (rooms_drag_id >= 0 && id != rooms_drag_id) {
        press_points_drop(id);
        return;
    }
    if (back_hit(x, y) == 1) {
        rooms_scroll_reset();
        play_button_click();
        keyboard_hide();
        keyboard_clear();
        if (rooms_mode == ROOMS_MODE_LIST) start_transition(ST_MODES);
        else { rooms_mode = ROOMS_MODE_LIST; rooms_field = 0; rooms_set_msg(""); }
        return;
    }
    if (rooms_mode == ROOMS_MODE_LIST) {
        /* The list is behind the published rules: without a token and without a
         * saved password the page asks for a login and this tap follows it. */
        if (net_roomlist_status() == 4 && net_cloud_ready() == 0 &&
            hit_rect(x, y, rooms_x(), rooms_list_y(), rooms_w(), rooms_view_h()) == 1) {
            play_button_click();
            login_after = ST_ROOMS;
            start_transition(ST_LOGIN);
            return;
        }
        double sh = rooms_search_h();
        if (str_len(rooms_search) > 0 &&
            hit_button(x, y, rooms_x() + rooms_w() - sh, rooms_search_y(), sh, sh) == 1) {
            play_button_click();
            rooms_search_buf[0] = 0;
            rooms_search = "";
            rooms_scroll = 0;
            if (rooms_field == 3) keyboard_clear();
            return;
        }
        if (hit_rect(x, y, rooms_x(), rooms_search_y(), rooms_w(), sh) == 1) {
            rooms_focus(3);
            return;
        }
        rooms_sync_field();
        if (rooms_field == 3) { keyboard_hide(); rooms_field = 0; }
        double bw = (rooms_w() - 16) / 2;
        if (hit_button(x, y, rooms_x(), rooms_actions_y(), bw, btn_h) == 1) {
            play_button_click();
            rooms_mode = ROOMS_MODE_CREATE;
            rooms_name = "";
            rooms_pass = "";
            rooms_set_msg("");
            rooms_focus(1);
            return;
        }
        if (hit_button(x, y, rooms_x() + bw + 16, rooms_actions_y(), bw, btn_h) == 1) {
            play_button_click();
            rooms_scroll = 0;
            rooms_scroll_reset();
            rooms_set_msg("");
            net_room_prep_reset();
            net_roomlist_fetch(FIREBASE);
            return;
        }
        double row = rooms_hit_row_at(x, y);
        if (rooms_scroll_needed() == 0) {
            if (row >= 0) {
                play_button_click();
                rooms_begin_join(rooms_filtered_name(row));
            }
            return;
        }
        if (y >= rooms_list_y() && y <= rooms_list_bottom() && x >= rooms_x() && x <= rooms_x() + rooms_w()) {
            rooms_vel = 0;
            scroll_track_reset(&rooms_track);
            rooms_drag_id = id;
            rooms_drag_start_y = y;
            rooms_drag_y = y;
            rooms_drag_moved = 0;
            rooms_drag_row = row;
        }
        return;
    }
    if (rooms_mode == ROOMS_MODE_CREATE) {
        if (hit_button(x, y, rooms_x(), rooms_form_y(0), rooms_w(), login_fh) == 1) { rooms_focus(1); return; }
        if (hit_button(x, y, rooms_x(), rooms_form_y(1), rooms_w(), login_fh) == 1) { rooms_focus(2); return; }
        if (hit_button(x, y, rooms_x(), rooms_btn_y(), rooms_w(), btn_h) == 1) { play_button_click(); rooms_submit_create(); return; }
    } else {
        if (hit_button(x, y, rooms_x(), rooms_form_y(0), rooms_w(), login_fh) == 1) { rooms_focus(2); return; }
        if (hit_button(x, y, rooms_x(), rooms_btn_y() - 96, rooms_w(), btn_h) == 1) { play_button_click(); rooms_submit_join(); return; }
    }
    rooms_sync_field();
    keyboard_hide();
}
