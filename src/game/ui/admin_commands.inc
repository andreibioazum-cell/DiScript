/* CCP "Admin commands": flat monochrome controls matching the supplied mockup.
 * The event strip is a real clipped horizontal viewport. It opens at its right
 * edge (part of 4, then 5, 6, 7), and supports finger drag, inertia, a draggable
 * thumb, track paging and the PC mouse wheel. Root admins only. */

#define ADMIN_CMD_EVENTS 8
#define ADMIN_CMD_TEXT_MAX 128

static char admin_cmd_text[ADMIN_CMD_TEXT_MAX];
static double admin_cmd_status;   /* 1 banner sent, 2 command done, 3 event off */
static double admin_cmd_status_t;
static double admin_cmd_scroll;
static double admin_cmd_vel;
static double admin_cmd_view_ready;
static double admin_cmd_drag_on;
static double admin_cmd_drag_bar;
static double admin_cmd_drag_id;
static double admin_cmd_drag_x;
static double admin_cmd_drag_y;
static double admin_cmd_drag_offset;
static double admin_cmd_drag_move;
static ScrollTrack admin_cmd_track;

static double admin_commands_allowed(void) { return is_admin(); }
static double admin_cmd_x(void) { return admin_panel_x() + admin_pad(); }
static double admin_cmd_w(void) { return admin_panel_w() - 2 * admin_pad(); }
static double admin_cmd_top(void) { return admin_grid_top(); }
static double admin_cmd_caption_h(void) { return admin_row_h() * 0.62; }
static double admin_cmd_input_y(void) {
    return admin_cmd_top() + admin_cmd_caption_h() + admin_gap();
}
static double admin_cmd_send_y(void) { return admin_cmd_input_y() + admin_row_h() + admin_gap(); }
static double admin_cmd_send_w(void) { return admin_cmd_w() * 0.45; }
static double admin_cmd_strip_h(void) { return clamp(admin_cmd_w() * 0.205, 42, 152); }
static double admin_cmd_btn_w(void) { return clamp(admin_cmd_w() * 0.22, 48, 164); }
static double admin_cmd_gap(void) { return clamp(admin_cmd_w() * 0.075, 10, 64); }
static double admin_cmd_step(void) { return admin_cmd_btn_w() + admin_cmd_gap(); }
static double admin_cmd_bar_h(void) { return clamp(admin_cmd_strip_h() * 0.34, 18, 44); }
static double admin_cmd_bar_y(void) { return admin_cmd_send_y() + admin_row_h() + admin_gap() * 2; }
static double admin_cmd_strip_y(void) { return admin_cmd_bar_y() + admin_cmd_bar_h() + admin_gap(); }
static double admin_cmd_content_h(void) {
    return admin_cmd_caption_h() + 2 * admin_row_h() + admin_cmd_bar_h() + admin_cmd_strip_h() +
           5 * admin_gap();
}
static double admin_cmd_content_w(void) {
    return ADMIN_CMD_EVENTS * admin_cmd_btn_w() + (ADMIN_CMD_EVENTS - 1) * admin_cmd_gap();
}
static double admin_cmd_scroll_max(void) {
    double m = admin_cmd_content_w() - admin_cmd_w();
    return m > 0 ? m : 0;
}
static double admin_cmd_thumb_w(void) {
    double w = admin_cmd_w(), content = admin_cmd_content_w();
    return content > w ? clamp(w * w / content, admin_cmd_bar_h() * 1.6, w) : w;
}
static double admin_cmd_thumb_x(void) {
    double mx = admin_cmd_scroll_max();
    double travel = admin_cmd_w() - admin_cmd_thumb_w();
    return admin_cmd_x() + (mx > 0 ? travel * admin_cmd_scroll / mx : 0);
}
static double admin_cmd_view_active(void) {
    return admin_panel_open == 1 && admin_tab == 2 && admin_commands_allowed() == 1;
}
static void admin_cmd_prepare_view(void) {
    if (admin_cmd_view_active() == 0 || admin_cmd_view_ready == 1) return;
    admin_cmd_scroll = admin_cmd_scroll_max();
    admin_cmd_vel = 0;
    admin_cmd_view_ready = 1;
}
static void admin_cmd_reset(void) {
    admin_cmd_view_ready = 0;
    admin_cmd_drag_on = 0;
    admin_cmd_vel = 0;
    admin_cmd_scroll = 0;
    scroll_track_reset(&admin_cmd_track);
}
static void admin_cmd_scroll_to(double value) {
    admin_cmd_scroll = clamp(value, 0, admin_cmd_scroll_max());
    if (admin_cmd_scroll <= 0 || admin_cmd_scroll >= admin_cmd_scroll_max()) admin_cmd_vel = 0;
}
static double admin_cmd_wheel(double steps) {
    if (admin_cmd_view_active() == 0 || admin_cmd_scroll_max() <= 0) return 0;
    admin_cmd_prepare_view();
    admin_cmd_vel = 0;
    admin_cmd_scroll_to(admin_cmd_scroll - steps * admin_cmd_step());
    return 1;
}
static const char *admin_cmd_caption(void) {
    if (admin_cmd_status_t > 0) {
        if (admin_cmd_status == 1) return language == 1 ? "Баннер отправлен!" : "Banner sent!";
        if (admin_cmd_status == 3) return language == 1 ? "Событие выключено" : "Event switched off";
        return language == 1 ? "Команда выполнена!" : "Command done!";
    }
    return language == 1 ? "Что вы хотите написать?" : "What do you want to write?";
}

/* Send plain words as a signed white banner; full chat commands run as-is. */
static void admin_cmd_send(void) {
    if (admin_commands_allowed() == 0) return;
    char raw[ADMIN_CMD_TEXT_MAX];
    snprintf(raw, sizeof(raw), "%s", admin_cmd_text);
    size_t n = strlen(raw);
    while (n > 0 && (raw[n - 1] == ' ' || raw[n - 1] == '\t')) raw[--n] = 0;
    const char *t = raw;
    while (*t == ' ' || *t == '\t') t = t + 1;
    if (*t == 0) return;
    if (admin_handle(t) == 1) {
        admin_cmd_status = net_chat_is_text_cmd(t) == 1 ? 1 : 2;
    } else {
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
static void admin_cmd_event_press(double idx) {
    if (admin_commands_allowed() == 0) return;
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
static void admin_cmd_press(double x, double y) {
    if (hit_rect(x, y, admin_cmd_x(), admin_cmd_input_y(), admin_cmd_w(), admin_row_h()) == 1) {
        keyboard_clear();
        if (admin_cmd_text[0]) keyboard_type(admin_cmd_text);
        keyboard_show();
        return;
    }
    if (hit_rect(x, y, admin_cmd_x(), admin_cmd_send_y(), admin_cmd_send_w(), admin_row_h()) == 1)
        admin_cmd_send();
}
static double admin_cmd_hit_button(double x, double y) {
    double sx = admin_cmd_x(), sy = admin_cmd_strip_y();
    if (hit_rect(x, y, sx, sy, admin_cmd_w(), admin_cmd_strip_h()) == 0) return -1;
    double rel = x - sx + admin_cmd_scroll;
    double idx = floor(rel / admin_cmd_step());
    double off = rel - idx * admin_cmd_step();
    if (idx < 0 || idx >= ADMIN_CMD_EVENTS || off > admin_cmd_btn_w()) return -1;
    return idx;
}
static void admin_cmd_bar_jump(double x) {
    double travel = admin_cmd_w() - admin_cmd_thumb_w();
    double mx = admin_cmd_scroll_max();
    if (travel <= 0 || mx <= 0) return;
    double rel = clamp(x - admin_cmd_x() - admin_cmd_drag_offset, 0, travel);
    admin_cmd_scroll_to(rel / travel * mx);
}

/* Returns 1 when this tab owns the gesture, including cancelled/up events. */
static double admin_cmd_touch(double x, double y, double action, double id) {
    if (admin_cmd_view_active() == 0) return 0;
    admin_cmd_prepare_view();
    double sx = admin_cmd_x(), sw = admin_cmd_w();
    double by = admin_cmd_bar_y(), bh = admin_cmd_bar_h();
    double sy = admin_cmd_strip_y(), sh = admin_cmd_strip_h();
    if (admin_cmd_drag_on == 1) {
        if (id != admin_cmd_drag_id) return 1;
        if (action == 2) {
            double d = admin_cmd_drag_x - x;
            admin_cmd_drag_x = x;
            admin_cmd_drag_move = fmax(admin_cmd_drag_move + fabs(d), fabs(y - admin_cmd_drag_y));
            if (admin_cmd_drag_bar == 1) {
                double old = admin_cmd_scroll;
                admin_cmd_bar_jump(x);
                scroll_track_move(&admin_cmd_track, admin_cmd_scroll - old);
            } else {
                admin_cmd_scroll_to(admin_cmd_scroll + d);
                scroll_track_move(&admin_cmd_track, d);
            }
            return 1;
        }
        if (action == 1 || action == 3) {
            admin_cmd_drag_on = 0;
            if (action == 1 && admin_cmd_drag_move < 12 && admin_cmd_drag_bar == 0) {
                double idx = admin_cmd_hit_button(x, y);
                if (idx >= 0 && fabs(y - admin_cmd_drag_y) < 12) admin_cmd_event_press(idx);
            } else if (action == 1 && admin_cmd_drag_move >= 12) {
                admin_cmd_vel = scroll_track_release(&admin_cmd_track, 2200.0);
            }
            if (action == 3) scroll_track_reset(&admin_cmd_track);
            return 1;
        }
        return 1;
    }
    if (action != 0) return 0;
    double pad = fmax(0, (28 - bh) / 2);
    double on_bar = hit_rect(x, y, sx, by - pad, sw, bh + 2 * pad);
    double on_strip = hit_rect(x, y, sx, sy, sw, sh);
    if (on_bar == 0 && on_strip == 0) return 0;
    /* The native keyboard covers the lower panel on phones. Keep the draft,
     * but dismiss it before manipulating the scrollbar or event buttons. */
    if (keyboard_visible() == 1) {
        keyboard_get_text_to(admin_cmd_text, sizeof(admin_cmd_text));
        keyboard_hide();
    }
    admin_cmd_drag_on = 1;
    admin_cmd_drag_bar = on_bar;
    admin_cmd_drag_id = id;
    admin_cmd_drag_x = x;
    admin_cmd_drag_y = y;
    admin_cmd_drag_move = 0;
    admin_cmd_vel = 0;
    scroll_track_reset(&admin_cmd_track);
    if (on_bar == 1) {
        double tx = admin_cmd_thumb_x(), tw = admin_cmd_thumb_w();
        admin_cmd_drag_offset = x >= tx && x <= tx + tw ? x - tx : tw / 2;
        admin_cmd_bar_jump(x);
    }
    return 1;
}
static void admin_cmd_tick(void) {
    if (admin_cmd_status_t > 0) admin_cmd_status_t = tick_down(admin_cmd_status_t);
    if (admin_cmd_view_active() == 0) {
        admin_cmd_drag_on = 0;
        admin_cmd_vel = 0;
        return;
    }
    admin_cmd_prepare_view();
    if (keyboard_visible() == 1) {
        keyboard_get_text_to(admin_cmd_text, sizeof(admin_cmd_text));
        if (keyboard_enter_pressed() == 1) admin_cmd_send();
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
    admin_cmd_scroll_to(admin_cmd_scroll + admin_cmd_vel * dt);
    admin_cmd_vel = admin_cmd_vel * exp(-4.8 * dt);
}
static void admin_cmd_flat_btn(const char *label, double x, double y, double w, double h,
                               double selected, double scale) {
    roundrect(x, y, w, h, fmin(w, h) * 0.24, selected == 1 ? 0x4A4A4A : 0x333333);
    roundrect_outline(x, y, w, h, fmin(w, h) * 0.24, 2, admin_frame());
    text_in_box(label, x, y, w, h, admin_frame(), scale);
}
static void admin_cmd_draw_scrollbar(void) {
    double x = admin_cmd_x(), y = admin_cmd_bar_y(), w = admin_cmd_w(), h = admin_cmd_bar_h();
    roundrect(x, y, w, h, h * 0.38, 0x333333);
    roundrect_outline(x, y, w, h, h * 0.38, 2, admin_frame());
    roundrect(admin_cmd_thumb_x(), y, admin_cmd_thumb_w(), h, h * 0.38, 0x050505);
    roundrect_outline(admin_cmd_thumb_x(), y, admin_cmd_thumb_w(), h, h * 0.38, 1.5, admin_frame());
}
static void admin_cmd_draw(void) {
    if (admin_cmd_view_active() == 0) return;
    admin_cmd_prepare_view();
    double x = admin_cmd_x(), w = admin_cmd_w(), rh = admin_row_h();
    const char *cap = admin_cmd_caption();
    double csc = fit_text_scale(cap, w - 8, 0.64);
    /* A second subpixel pass gives the caption the mockup's heavier weight. */
    text_in_field(cap, x + 4, admin_cmd_top(), admin_cmd_caption_h(), admin_frame(), csc);
    text_in_field(cap, x + 4.7, admin_cmd_top(), admin_cmd_caption_h(), admin_frame(), csc);
    double iy = admin_cmd_input_y();
    roundrect(x, iy, w, rh, rh * 0.28, 0x333333);
    roundrect_outline(x, iy, w, rh, rh * 0.28, 2, admin_frame());
    const char *value = admin_cmd_text;
    if (keyboard_visible() == 1) value = ds_concat(value, "|");
    if (!value[0]) value = language == 1 ? "введите текст..." : "enter text...";
    text_in_field(value, x + 14, iy, rh, admin_cmd_text[0] ? admin_frame() : 0x9A9A9A,
                  fit_text_scale(value, w - 28, 0.55));
    double sy = admin_cmd_send_y();
    admin_cmd_flat_btn(language == 1 ? "Отправить" : "Send", x, sy, admin_cmd_send_w(), rh, 0, 0.55);
    const char *run = language == 1 ? "запустить event." : "run event.";
    double rsc = fit_text_scale(run, w - admin_cmd_send_w() - 16, 0.62);
    text_in_field(run, x + w - text_ink_width(run) * rsc, sy, rh, admin_frame(), rsc);
    admin_cmd_draw_scrollbar();
    double by = admin_cmd_strip_y(), bh = admin_cmd_strip_h(), bw = admin_cmd_btn_w();
    double cur = net_event();
    ds_gfx_scissor(x, by, w, bh);
    for (int i = 0; i < ADMIN_CMD_EVENTS; i++) {
        double bx = x + i * admin_cmd_step() - admin_cmd_scroll;
        if (bx + bw < x || bx > x + w) continue;
        const char *num = ds_num_to_string((double)i);
        admin_cmd_flat_btn(num, bx, by, bw, bh, cur == (double)i,
                           fit_text_scale(num, bw - 14, fmin(bh / 64.0, 1.1)));
    }
    ds_gfx_scissor(0, 0, 0, 0);
}
