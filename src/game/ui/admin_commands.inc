/* CCP "Admin commands" tab: a banner text command on top (question, input
 * field, send button) and a horizontally scrollable strip of event buttons
 * (event 0..7) below, with a scrollbar and soft inertia. Root admins only -
 * the same accounts that can type these commands in chat. */

#define ADMIN_CMD_EVENTS 8
#define ADMIN_CMD_TEXT_MAX 128

static char admin_cmd_text[ADMIN_CMD_TEXT_MAX];
static double admin_cmd_status;   /* 1 banner sent, 2 command done, 3 event off */
static double admin_cmd_status_t;
static double admin_cmd_scroll;
static double admin_cmd_vel;
static double admin_cmd_drag_on;
static double admin_cmd_drag_id;
static double admin_cmd_drag_x;
static double admin_cmd_drag_move;
static ScrollTrack admin_cmd_track;

/* Chat-command powers belong to the two root admins (gift rights aside). */
static double admin_commands_allowed(void) {
    return is_admin();
}

static double admin_cmd_x(void) { return admin_panel_x() + admin_pad(); }
static double admin_cmd_w(void) { return admin_panel_w() - 2 * admin_pad(); }
static double admin_cmd_top(void) { return admin_grid_top(); }
static double admin_cmd_caption_h(void) { return admin_row_h() * 0.62; }
static double admin_cmd_input_y(void) {
    return admin_cmd_top() + admin_cmd_caption_h() + admin_gap();
}
static double admin_cmd_send_y(void) { return admin_cmd_input_y() + admin_row_h() + admin_gap(); }
static double admin_cmd_send_w(void) { return admin_cmd_w() * 0.45; }
static double admin_cmd_bar_h(void) { return clamp(admin_row_h() * 0.30, 8, 22); }
static double admin_cmd_bar_y(void) { return admin_cmd_send_y() + admin_row_h() + admin_gap(); }
static double admin_cmd_strip_y(void) { return admin_cmd_bar_y() + admin_cmd_bar_h() + admin_gap(); }
static double admin_cmd_strip_h(void) { return admin_row_h() * 1.5; }
static double admin_cmd_btn_w(void) { return admin_cmd_strip_h(); }
static double admin_cmd_step(void) { return admin_cmd_btn_w() + admin_gap(); }

/* Height of the whole tab body below the tab row. */
static double admin_cmd_content_h(void) {
    return admin_cmd_caption_h() + 2 * admin_row_h() + admin_cmd_bar_h() + admin_cmd_strip_h() +
           4 * admin_gap();
}

static double admin_cmd_content_w(void) {
    return ADMIN_CMD_EVENTS * admin_cmd_btn_w() + (ADMIN_CMD_EVENTS - 1) * admin_gap();
}

static double admin_cmd_scroll_max(void) {
    double m = admin_cmd_content_w() - admin_cmd_w();
    return m > 0 ? m : 0;
}

static const char *admin_cmd_caption(void) {
    if (admin_cmd_status_t > 0) {
        if (admin_cmd_status == 1) {
            return language == 1 ? "Баннер отправлен!" : "Banner sent!";
        }
        if (admin_cmd_status == 3) {
            return language == 1 ? "Событие выключено" : "Event switched off";
        }
        return language == 1 ? "Команда выполнена!" : "Command done!";
    }
    return language == 1 ? "Что вы хотите написать?" : "What do you want to write?";
}

/* Send the field as the chat `text "..."` banner command. A full command
 * typed into the field (text/event/ban/unban/passwd) runs as-is instead. */
static void admin_cmd_send(void) {
    if (admin_commands_allowed() == 0) {
        return;
    }
    char raw[ADMIN_CMD_TEXT_MAX];
    snprintf(raw, sizeof(raw), "%s", admin_cmd_text);
    size_t n = strlen(raw);
    while (n > 0 && (raw[n - 1] == ' ' || raw[n - 1] == '\t')) {
        raw[--n] = 0;
    }
    const char *t = raw;
    while (*t == ' ' || *t == '\t') {
        t = t + 1;
    }
    if (*t == 0) {
        return;
    }
    if (admin_handle(t) == 1) {
        admin_cmd_status = net_chat_is_text_cmd(t) == 1 ? 1 : 2;
    } else {
        /* Plain words become a white banner; quotes would break the command. */
        char safe[ADMIN_CMD_TEXT_MAX];
        size_t i = 0;
        while (t[i] != 0 && i < sizeof(safe) - 1) {
            safe[i] = t[i] == '"' ? '\'' : t[i];
            i = i + 1;
        }
        safe[i] = 0;
        admin_handle(ds_concat(ds_concat("text \"", safe), "\""));
        admin_cmd_status = 1;
    }
    admin_cmd_status_t = 2.5;
    admin_cmd_text[0] = 0;
    keyboard_clear();
    play_button_click();
}

/* Event buttons run the same commands as chat: 1..7 launch, 0 switches off. */
static void admin_cmd_event_press(double idx) {
    if (admin_commands_allowed() == 0) {
        return;
    }
    play_button_click();
    if (idx == 0) {
        if (net_event() != 0) {
            admin_handle("event 0");
            admin_cmd_status = 3;
            admin_cmd_status_t = 2.5;
        }
        return;
    }
    admin_handle(ds_concat("event ", ds_num_to_string(idx)));
    admin_cmd_status = 2;
    admin_cmd_status_t = 2.5;
}

/* Taps on the input field and the send button (tab row and close are handled
 * by the shared panel code; the event strip owns its gestures separately). */
static void admin_cmd_press(double x, double y) {
    if (hit_rect(x, y, admin_cmd_x(), admin_cmd_input_y(), admin_cmd_w(), admin_row_h()) == 1) {
        keyboard_clear();
        if (admin_cmd_text[0]) {
            keyboard_type(admin_cmd_text);
        }
        keyboard_show();
        return;
    }
    if (hit_rect(x, y, admin_cmd_x(), admin_cmd_send_y(), admin_cmd_send_w(), admin_row_h()) == 1) {
        admin_cmd_send();
    }
}

/* Horizontal drag/scroll over the scrollbar + button strip. Returns 1 while
 * the gesture belongs to the strip so the panel does not also react. */
static double admin_cmd_touch(double x, double y, double action, double id) {
    if (admin_tab != 2 || admin_commands_allowed() == 0) {
        return 0;
    }
    double sx = admin_cmd_x();
    double sw = admin_cmd_w();
    double sy = admin_cmd_bar_y();
    double sh = admin_cmd_strip_y() + admin_cmd_strip_h() - sy;
    if (admin_cmd_drag_on == 1) {
        if (id != admin_cmd_drag_id) {
            return 1;
        }
        if (action == 2) {
            double d = admin_cmd_drag_x - x;
            admin_cmd_drag_x = x;
            admin_cmd_drag_move = admin_cmd_drag_move + fabs(d);
            admin_cmd_scroll = clamp(admin_cmd_scroll + d, 0, admin_cmd_scroll_max());
            scroll_track_move(&admin_cmd_track, d);
            return 1;
        }
        if (action == 1 || action == 3) {
            admin_cmd_drag_on = 0;
            if (action == 1 && admin_cmd_drag_move < 12 &&
                y >= admin_cmd_strip_y() && y <= admin_cmd_strip_y() + admin_cmd_strip_h()) {
                double rel = x - sx + admin_cmd_scroll;
                double idx = floor(rel / admin_cmd_step());
                double off = rel - idx * admin_cmd_step();
                if (idx >= 0 && idx < ADMIN_CMD_EVENTS && off <= admin_cmd_btn_w()) {
                    admin_cmd_event_press(idx);
                }
            } else if (action == 1) {
                admin_cmd_vel = scroll_track_release(&admin_cmd_track, 2200.0);
            }
            if (action == 3) {
                scroll_track_reset(&admin_cmd_track);
            }
            return 1;
        }
        return 1;
    }
    if (action == 0 && hit_rect(x, y, sx, sy, sw, sh) == 1) {
        admin_cmd_drag_on = 1;
        admin_cmd_drag_id = id;
        admin_cmd_drag_x = x;
        admin_cmd_drag_move = 0;
        admin_cmd_vel = 0;
        scroll_track_reset(&admin_cmd_track);
        return 1;
    }
    return 0;
}

static void admin_cmd_tick(void) {
    if (admin_cmd_status_t > 0) {
        admin_cmd_status_t = tick_down(admin_cmd_status_t);
    }
    if (admin_panel_open != 1 || admin_tab != 2 || admin_commands_allowed() == 0) {
        admin_cmd_drag_on = 0;
        admin_cmd_vel = 0;
        return;
    }
    if (keyboard_visible() == 1) {
        keyboard_get_text_to(admin_cmd_text, sizeof(admin_cmd_text));
        if (keyboard_enter_pressed() == 1) {
            admin_cmd_send();
        }
    }
    double mx = admin_cmd_scroll_max();
    if (mx <= 0) {
        admin_cmd_scroll = 0;
        admin_cmd_vel = 0;
        return;
    }
    admin_cmd_scroll = clamp(admin_cmd_scroll, 0, mx);
    if (admin_cmd_drag_on == 1) {
        admin_cmd_vel = 0;
        scroll_track_tick(&admin_cmd_track);
        return;
    }
    if (fabs(admin_cmd_vel) < 0.6) {
        admin_cmd_vel = 0;
        return;
    }
    admin_cmd_scroll = admin_cmd_scroll + admin_cmd_vel * dt;
    admin_cmd_vel = admin_cmd_vel * exp(-4.8 * dt);
    if (admin_cmd_scroll <= 0) {
        admin_cmd_scroll = 0;
        admin_cmd_vel = 0;
    } else if (admin_cmd_scroll >= mx) {
        admin_cmd_scroll = mx;
        admin_cmd_vel = 0;
    }
}

static void admin_cmd_draw_scrollbar(void) {
    double x = admin_cmd_x();
    double y = admin_cmd_bar_y();
    double w = admin_cmd_w();
    double h = admin_cmd_bar_h();
    double content = admin_cmd_content_w();
    double mx = admin_cmd_scroll_max();
    roundrect(x, y, w, h, h / 2, admin_on());
    double tw = mx > 0 ? w * w / content : w;
    if (tw < h * 1.6) {
        tw = h * 1.6;
    }
    if (tw > w) {
        tw = w;
    }
    double tx = x + (mx > 0 ? (w - tw) * admin_cmd_scroll / mx : 0);
    roundrect(tx, y, tw, h, h / 2, admin_off());
    roundrect_outline(tx, y, tw, h, h / 2, 2, admin_frame());
}

static void admin_cmd_draw(void) {
    double x = admin_cmd_x();
    double w = admin_cmd_w();
    double rh = admin_row_h();
    /* Question (or short success feedback) above the input field. */
    const char *cap = admin_cmd_caption();
    text_in_field(cap, x + 4, admin_cmd_top(), admin_cmd_caption_h(), admin_frame(),
                  fit_text_scale(cap, w - 8, 0.6));
    /* Banner text input. */
    double iy = admin_cmd_input_y();
    roundrect(x, iy, w, rh, 8, admin_off());
    roundrect_outline(x, iy, w, rh, 8, 2, admin_frame());
    if (admin_cmd_text[0]) {
        const char *shown = keyboard_visible() == 1 ? ds_concat(admin_cmd_text, "|") : admin_cmd_text;
        text_in_field(shown, x + 10, iy, rh, admin_frame(), fit_text_scale(shown, w - 20, 0.55));
    } else {
        const char *ph = language == 1 ? "введите текст..." : "enter text...";
        text_in_field(ph, x + 10, iy, rh, 0x9A9A9A, 0.55);
    }
    /* Send button on the left, "run event." label right-aligned. */
    double sy = admin_cmd_send_y();
    admin_btn(language == 1 ? "Отправить" : "Send", x, sy, admin_cmd_send_w(), rh, 0, 0.55);
    const char *run = language == 1 ? "запустить event." : "run event.";
    double rsc = fit_text_scale(run, w - admin_cmd_send_w() - 16, 0.62);
    double rw = text_ink_width(run) * rsc;
    text_in_field(run, x + w - rw, sy, rh, admin_frame(), rsc);
    /* Scrollbar and the horizontally scrollable event button strip. */
    admin_cmd_draw_scrollbar();
    double by = admin_cmd_strip_y();
    double bh = admin_cmd_strip_h();
    double bw = admin_cmd_btn_w();
    double cur = net_event();
    ds_gfx_scissor(x, by - 2, w, bh + 4);
    int i = 0;
    while (i < ADMIN_CMD_EVENTS) {
        double bx = x + i * admin_cmd_step() - admin_cmd_scroll;
        if (bx + bw >= x && bx <= x + w) {
            const char *num = ds_num_to_string((double)i);
            double sc = fit_text_scale(num, bw - 14, fmin(bh / 64.0, 1.1));
            admin_btn(num, bx, by, bw, bh, cur == (double)i ? 1 : 0, sc);
        }
        i = i + 1;
    }
    ds_gfx_scissor(0, 0, 0, 0);
}
