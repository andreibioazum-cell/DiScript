/* Owner-only screen with player complaints: ban the author or clear the note.
 * The list lives in the database, so it can be checked from any device.
 *
 * One complaint is picked at a time: a tap on the row selects it, and only the
 * selected row shows its action buttons. The overlay follows the active theme,
 * so in the New Year look it is ice blue rather than the purple of the menu. */

static double reports_open_flag;

/* Index of the single selected complaint, -1 when nothing is selected. */
static double reports_sel = -1;

/* Ice palette used when the New Year theme is on. The overlay would otherwise
 * keep the purple card colours and look foreign on the frosty screens. */
static uint32_t reports_blue_panel(void) {
    return winter_theme_on() == 1 ? RGB(26,56,82) : RGB(46,42,70);
}
static uint32_t reports_blue_row(void) {
    return winter_theme_on() == 1 ? RGB(38,78,110) : RGB(46,42,70);
}
static uint32_t reports_blue_row_sel(void) {
    return winter_theme_on() == 1 ? RGB(58,110,150) : RGB(58,53,96);
}
static uint32_t reports_blue_ring(void) {
    return winter_theme_on() == 1 ? RGB(143,200,232) : card_ring;
}
static uint32_t reports_blue_text_dim(void) {
    return winter_theme_on() == 1 ? RGB(186,216,236) : RGB(184,184,200);
}
static uint32_t reports_blue_face(void) {
    return winter_theme_on() == 1 ? RGB(62,127,181) : ui_purple;
}
static uint32_t reports_blue_face_soft(void) {
    return winter_theme_on() == 1 ? RGB(44,90,120) : ui_purple;
}

static const char *tr_reports(void) { return language == 1 ? "Жалобы" : "Reports"; }
static const char *tr_reports_empty(void) { return language == 1 ? "Жалоб нет" : "No reports yet"; }
static const char *tr_reports_loading(void) { return language == 1 ? "Загрузка..." : "Loading..."; }
static const char *tr_reports_failed(void) {
    return language == 1 ? "Не удалось загрузить" : "Could not load";
}
static const char *tr_reports_ban(void) { return language == 1 ? "Бан" : "Ban"; }
static const char *tr_reports_done(void) { return language == 1 ? "Готово" : "Done"; }
static const char *tr_reports_refresh(void) { return language == 1 ? "Обновить" : "Refresh"; }
static const char *tr_reports_close(void) { return language == 1 ? "Закрыть" : "Close"; }

static double reports_card_x(void) { return 16; }
static double reports_card_w(void) { return screen_w - 32; }
static double reports_card_y(void) { return 16; }
static double reports_card_h(void) { return screen_h - 32; }
static double reports_rows_top(void) { return reports_card_y() + 70; }
/* One row: the complaint on top, the action buttons centred underneath it. */
static double reports_row_h(void) { return 104; }
static double reports_rows_max(void) {
    double room = reports_card_y() + reports_card_h() - 90 - reports_rows_top();
    double n = floor(room / reports_row_h());
    return n < 1 ? 1 : (n > 6 ? 6 : n);
}
static double reports_btn_w(void) { return 96; }
static double reports_btn_h(void) { return 32; }
static double reports_btn_gap(void) { return 10; }
static double reports_btn_y(double row_y) { return row_y + reports_row_h() - 8 - reports_btn_h() - 12; }
/* The pair of buttons sits exactly in the middle of the complaint card. */
static double reports_ban_x(void) {
    double total = 2 * reports_btn_w() + reports_btn_gap();
    return (screen_w - total) / 2;
}
static double reports_done_x(void) { return reports_ban_x() + reports_btn_w() + reports_btn_gap(); }
static double reports_row_y(double i) { return reports_rows_top() + i * reports_row_h(); }
static double reports_refresh_x(void) { return reports_card_x() + 20; }
static double reports_close_x(void) { return reports_card_x() + 40 + (reports_card_w() - 60) / 2; }
static double reports_bottom_y(void) { return reports_card_y() + reports_card_h() - 66; }
static double reports_bottom_w(void) { return (reports_card_w() - 60) / 2; }

static void reports_open(void) {
    reports_open_flag = 1;
    reports_sel = -1;
    net_reports_fetch();
    play_button_click();
    ui_nav_gen = ui_nav_gen + 1;
}

static void reports_close(void) {
    reports_open_flag = 0;
    reports_sel = -1;
}

/* Keep the selection inside the list after a delete or a refresh. */
static void reports_clamp_sel(void) {
    double count = net_reports_count();
    if (reports_sel >= count) {
        reports_sel = count - 1;
    }
    if (reports_sel < -1) {
        reports_sel = -1;
    }
}

/* "A reports B · room" header for one complaint. */
static const char *reports_line(double i) {
    const char *from = net_report_from(i);
    const char *target = net_report_target(i);
    const char *room = net_report_room(i);
    return ds_concat(from, ds_concat(language == 1 ? " жалуется на " : " reports ",
                                     ds_concat(target, ds_concat(" · ", room))));
}

static void draw_reports(void) {
    double cx = reports_card_x(), cy = reports_card_y(), cw = reports_card_w(), ch = reports_card_h();
    double i, count, maxn;
    double row_w = cw - 24;
    reports_clamp_sel();
    rect(0, 0, screen_w, screen_h, 0xE6000000);
    roundrect(cx, cy, cw, ch, 16, reports_blue_panel());
    ctext_scaled(tr_reports(), cy + 22, text_color, 0.7);
    count = net_reports_count();
    maxn = reports_rows_max();
    if (net_reports_status() == 1) {
        ctext_scaled(tr_reports_loading(), cy + 130, reports_blue_text_dim(), 0.55);
    } else if (net_reports_status() == 3) {
        ctext_scaled(tr_reports_failed(), cy + 130, RGB(255,154,154), 0.55);
    } else if (count <= 0) {
        ctext_scaled(tr_reports_empty(), cy + 130, reports_blue_text_dim(), 0.55);
    } else {
        i = 0;
        while (i < count && i < maxn) {
            double y = reports_row_y(i);
            double picked = reports_sel == i ? 1 : 0;
            if (picked == 1) {
                roundrect(cx + 9, y - 3, row_w + 6, reports_row_h() - 2, 14, reports_blue_ring());
            }
            roundrect(cx + 12, y, row_w, reports_row_h() - 8, 10,
                      picked == 1 ? reports_blue_row_sel() : reports_blue_row());
            text_scaled(reports_line(i), cx + 22, y + 8, text_color,
                        fit_text_scale(reports_line(i), row_w - 20, 0.45));
            text_scaled(net_report_text(i), cx + 22, y + 36, reports_blue_text_dim(),
                        fit_text_scale(net_report_text(i), row_w - 20, 0.42));
            /* Only the picked complaint offers its actions: one complaint at a
             * time, and the buttons sit in the middle of its card. */
            if (picked == 1) {
                double by = reports_btn_y(y);
                button_scaled_face(tr_reports_ban(), reports_ban_x(), by, reports_btn_w(), reports_btn_h(),
                                   0.48, reports_blue_face());
                button_scaled_face(tr_reports_done(), reports_done_x(), by, reports_btn_w(), reports_btn_h(),
                                   0.46, reports_blue_face_soft());
            }
            i = i + 1;
        }
    }
    button_scaled_face(tr_reports_refresh(), reports_refresh_x(), reports_bottom_y(), reports_bottom_w(), 50,
                       0.5, reports_blue_face());
    button_scaled_face(tr_reports_close(), reports_close_x(), reports_bottom_y(), reports_bottom_w(), 50,
                       0.5, reports_blue_face_soft());
}

/* Row hit box of one complaint: the whole card, buttons included. */
static double reports_row_hit(double x, double y, double i) {
    return hit_rect(x, y, reports_card_x() + 12, reports_row_y(i), reports_card_w() - 24, reports_row_h() - 8);
}

static void touch_reports(double x, double y, double action, double id) {
    double i, count, maxn;
    if (action == 3) {
        press_points_drop(id);
        return;
    }
    if (action != 0)
        return;
    if (hit_button(x, y, reports_refresh_x(), reports_bottom_y(), reports_bottom_w(), 50) == 1) {
        play_button_click();
        net_reports_fetch();
        reports_clamp_sel();
        return;
    }
    if (hit_button(x, y, reports_close_x(), reports_bottom_y(), reports_bottom_w(), 50) == 1) {
        play_button_click();
        reports_close();
        return;
    }
    count = net_reports_count();
    maxn = reports_rows_max();
    /* The buttons belong to the selected complaint only. */
    if (reports_sel >= 0 && reports_sel < count && reports_sel < maxn) {
        double ry = reports_row_y(reports_sel);
        double by = reports_btn_y(ry);
        if (hit_button(x, y, reports_ban_x(), by, reports_btn_w(), reports_btn_h()) == 1) {
            char nick[24];
            snprintf(nick, sizeof(nick), "%s", net_report_target(reports_sel));
            play_button_click();
            if (nick[0])
                net_ban_set(nick, 1);
            net_report_delete(reports_sel);
            reports_sel = -1;
            return;
        }
        if (hit_button(x, y, reports_done_x(), by, reports_btn_w(), reports_btn_h()) == 1) {
            play_button_click();
            net_report_delete(reports_sel);
            reports_sel = -1;
            return;
        }
    }
    i = 0;
    while (i < count && i < maxn) {
        if (reports_row_hit(x, y, i) == 1) {
            /* A tap on a complaint picks it, a tap on the picked one drops it. */
            play_button_click();
            reports_sel = reports_sel == i ? -1 : i;
            return;
        }
        i = i + 1;
    }
}

/* ---------------------------------------------------------------------------
 * Online complaint: a "Жалоба" button during a fight opens a short panel with
 * the players of the current room. One player is picked at a time; only then
 * the reason row and the send button appear, so a complaint always needs a
 * deliberate tap and never fires by accident in the middle of a fight.
 * ------------------------------------------------------------------------- */

static double report_pick_open;
static double report_pick_slot_sel;   /* NET slot picked in the panel, -1 = none */
static double report_pick_reason;     /* 0..3, index into the reason list */
static double report_pick_sent_t;     /* > 0 while the "sent" note is visible */

static const char *tr_report_pick(void) { return language == 1 ? "Жалоба" : "Report"; }
static const char *tr_report_pick_title(void) {
    return language == 1 ? "Жалоба на игрока" : "Report a player";
}
static const char *tr_report_pick_empty(void) {
    return language == 1 ? "В комнате нет других игроков" : "Nobody else in this room";
}
static const char *tr_report_pick_why(void) {
    return language == 1 ? "За что жалоба?" : "What did they do?";
}
static const char *tr_report_pick_cancel(void) { return language == 1 ? "Отмена" : "Cancel"; }
static const char *tr_report_pick_send(void) { return language == 1 ? "Пожаловаться" : "Send report"; }
static const char *tr_report_pick_sent(void) { return language == 1 ? "Жалоба отправлена" : "Report sent"; }

/* Four preset reasons keep the report short and readable for the moderator. */
static double report_pick_reason_count(void) { return 4; }
static const char *report_pick_reason_text(double i) {
    if (language == 1) {
        if (i == 0) return "Оскорбления в чате";
        if (i == 1) return "Читы или баги";
        if (i == 2) return "Мешает играть";
        return "Другое";
    }
    if (i == 0) return "Chat abuse";
    if (i == 1) return "Cheats or exploits";
    if (i == 2) return "Ruining the match";
    return "Other";
}

/* Ice look in the New Year theme, the usual purple card otherwise. */
static uint32_t report_pick_panel(void) { return winter_theme_on() == 1 ? RGB(22,50,74) : card_bg; }
static uint32_t report_pick_row(void) { return winter_theme_on() == 1 ? RGB(38,78,110) : RGB(46,42,70); }
static uint32_t report_pick_row_sel(void) { return winter_theme_on() == 1 ? RGB(58,110,150) : RGB(58,53,96); }
static uint32_t report_pick_ring(void) { return winter_theme_on() == 1 ? RGB(143,200,232) : card_ring; }
static uint32_t report_pick_dim(void) { return winter_theme_on() == 1 ? RGB(186,216,236) : RGB(184,184,200); }
static uint32_t report_pick_face(void) { return winter_theme_on() == 1 ? RGB(62,127,181) : ui_purple; }
static uint32_t report_pick_face_soft(void) { return winter_theme_on() == 1 ? RGB(44,90,120) : ui_purple; }

static double report_pick_w(void) { return screen_w - 48; }
static double report_pick_x(void) { return 24; }
static double report_pick_h(void) { return screen_h - 200; }
static double report_pick_y(void) { return 100; }
static double report_pick_close_x(void) { return report_pick_x() + report_pick_w() - btn_w - 8; }
static double report_pick_close_y(void) { return report_pick_y() + 8; }
static double report_pick_row_h(void) { return 46; }
static double report_pick_rows_top(void) { return report_pick_y() + 78; }
static double report_pick_rows_max(void) {
    double room = report_pick_y() + report_pick_h() - 180 - report_pick_rows_top();
    double n = floor(room / report_pick_row_h());
    return n < 1 ? 1 : (n > 4 ? 4 : n);
}
/* Reason chips and the action buttons sit under the player list. */
static double report_pick_why_y(void) { return report_pick_y() + report_pick_h() - 168; }
static double report_pick_chip_w(void) { return (report_pick_w() - 24 - 3 * 8) / 4; }
static double report_pick_chip_x(double i) { return report_pick_x() + 12 + i * (report_pick_chip_w() + 8); }
static double report_pick_chips_y(void) { return report_pick_why_y() + 26; }
static double report_pick_actions_y(void) { return report_pick_y() + report_pick_h() - 56; }
static double report_pick_action_w(void) { return (report_pick_w() - 36) / 2; }
static double report_pick_send_x(void) { return report_pick_x() + 12; }
static double report_pick_cancel_x(void) { return report_pick_send_x() + report_pick_action_w() + 12; }

/* Number of players listed in the panel (you are not listed). */
static double report_pick_count(void) {
    double n = 0;
    double s = 0;
    while (s < NET_SLOTS) {
        if (net_player_online(s) == 1 && s != net_slot())
            n = n + 1;
        s = s + 1;
    }
    return n;
}

/* Slot behind the n-th row of the panel, -1 when out of range. */
static double report_pick_slot(double row) {
    double n = 0;
    double s = 0;
    while (s < NET_SLOTS) {
        if (net_player_online(s) == 1 && s != net_slot()) {
            if (n == row)
                return s;
            n = n + 1;
        }
        s = s + 1;
    }
    return -1;
}

static void report_pick_reset(void) {
    report_pick_slot_sel = -1;
    report_pick_reason = 0;
}

static void report_pick_open_panel(void) {
    report_pick_open = 1;
    report_pick_sent_t = 0;
    report_pick_reset();
    play_button_click();
    ui_nav_gen = ui_nav_gen + 1;
}

static void report_pick_close_panel(void) {
    report_pick_open = 0;
    report_pick_reset();
}

/* Small "Жалоба" button on the battle screen: quiet, out of the way, and only
 * in online mode. */
static double report_pick_btn_w(void) { return 128; }
static double report_pick_btn_h(void) { return 42; }
static double report_pick_btn_x(void) { return 16; }
static double report_pick_btn_y(void) { return screen_h - 62; }

static double report_pick_btn_hit(double x, double y) {
    if (game_state != ST_ONLINE)
        return 0;
    return hit_button(x, y, report_pick_btn_x(), report_pick_btn_y(), report_pick_btn_w(), report_pick_btn_h());
}

static void draw_report_pick_button(void) {
    if (game_state != ST_ONLINE || chat_open == 1 || report_pick_open == 1)
        return;
    button_scaled_face(language == 1 ? "Жалоба" : "Report", report_pick_btn_x(), report_pick_btn_y(),
                       report_pick_btn_w(), report_pick_btn_h(), 0.42, report_pick_face_soft());
}

static void report_pick_send(void) {
    double slot = report_pick_slot_sel;
    const char *nick;
    const char *reason;
    char text[160];
    if (slot < 0)
        return;
    nick = net_player_nick(slot);
    if (!nick || !*nick)
        return;
    reason = report_pick_reason_text(report_pick_reason);
    snprintf(text, sizeof(text), "%s", reason);
    net_report_send(nick, text, net_current_room());
    report_pick_sent_t = 2.2;
    report_pick_reset();
    play_button_click();
}

static void tick_report_pick(void) {
    if (report_pick_sent_t > 0) {
        report_pick_sent_t = tick_down(report_pick_sent_t);
        if (report_pick_sent_t < 0)
            report_pick_sent_t = 0;
    }
}

static void draw_report_pick(void) {
    double px = report_pick_x(), py = report_pick_y(), pw = report_pick_w(), ph = report_pick_h();
    double i = 0, count = report_pick_count(), maxn = report_pick_rows_max();
    /* Keep the selection valid: a player may leave the room mid-report. */
    if (report_pick_slot_sel >= 0 &&
        (net_player_online(report_pick_slot_sel) != 1 || report_pick_slot_sel == net_slot()))
        report_pick_reset();
    rect(0, 0, screen_w, screen_h, 0xE6000000);
    roundrect(px, py, pw, ph, 16, report_pick_panel());
    ctext_scaled(tr_report_pick_title(), py + 20, text_color, 0.62);
    button_scaled_face(tr_reports_close(), report_pick_close_x(), report_pick_close_y(), btn_w, 44, 0.45,
                       report_pick_face_soft());
    if (count <= 0) {
        ctext_scaled(tr_report_pick_empty(), py + 130, report_pick_dim(), 0.5);
    } else {
        while (i < count && i < maxn) {
            double ry = report_pick_rows_top() + i * report_pick_row_h();
            double slot = report_pick_slot(i);
            double picked = report_pick_slot_sel == slot ? 1 : 0;
            if (picked == 1)
                roundrect(px + 9, ry - 3, pw - 18, report_pick_row_h() - 2, 12, report_pick_ring());
            roundrect(px + 12, ry, pw - 24, report_pick_row_h() - 8, 10,
                      picked == 1 ? report_pick_row_sel() : report_pick_row());
            const char *nick = net_player_nick(slot);
            text_scaled(nick, px + 22, ry + 9, text_color,
                        fit_text_scale(nick, pw - 44, 0.5));
            i = i + 1;
        }
    }
    if (report_pick_slot_sel >= 0) {
        text_scaled(tr_report_pick_why(), px + 12, report_pick_why_y(), report_pick_dim(), 0.45);
        i = 0;
        while (i < report_pick_reason_count()) {
            double cx = report_pick_chip_x(i);
            double cy = report_pick_chips_y();
            double picked = report_pick_reason == i ? 1 : 0;
            if (picked == 1)
                roundrect(cx - 2, cy - 2, report_pick_chip_w() + 4, 36, 10, report_pick_ring());
            button_scaled_face(report_pick_reason_text(i), cx, cy, report_pick_chip_w(), 32, 0.34,
                               picked == 1 ? report_pick_face_soft() : report_pick_row());
            i = i + 1;
        }
        button_scaled_face(tr_report_pick_send(), report_pick_send_x(), report_pick_actions_y(),
                           report_pick_action_w(), 46, 0.42, report_pick_face());
        button_scaled_face(tr_report_pick_cancel(), report_pick_cancel_x(), report_pick_actions_y(),
                           report_pick_action_w(), 46, 0.42, report_pick_face_soft());
    }
    if (report_pick_sent_t > 0) {
        ctext_scaled(tr_report_pick_sent(), report_pick_actions_y() + 8, RGB(190,240,200), 0.5);
    }
}

static void touch_report_pick(double x, double y, double action, double id) {
    double i = 0, count, maxn;
    if (action == 3) {
        press_points_drop(id);
        return;
    }
    if (action != 0)
        return;
    if (hit_button(x, y, report_pick_close_x(), report_pick_close_y(), btn_w, 44) == 1) {
        play_button_click();
        report_pick_close_panel();
        return;
    }
    if (report_pick_slot_sel >= 0) {
        i = 0;
        while (i < report_pick_reason_count()) {
            if (hit_button(x, y, report_pick_chip_x(i), report_pick_chips_y(), report_pick_chip_w(), 32) == 1) {
                play_button_click();
                report_pick_reason = i;
                return;
            }
            i = i + 1;
        }
        if (hit_button(x, y, report_pick_send_x(), report_pick_actions_y(), report_pick_action_w(), 46) == 1) {
            report_pick_send();
            return;
        }
        if (hit_button(x, y, report_pick_cancel_x(), report_pick_actions_y(), report_pick_action_w(), 46) == 1) {
            play_button_click();
            report_pick_reset();
            return;
        }
    }
    count = report_pick_count();
    maxn = report_pick_rows_max();
    while (i < count && i < maxn) {
        double ry = report_pick_rows_top() + i * report_pick_row_h();
        if (hit_rect(x, y, report_pick_x() + 12, ry, report_pick_w() - 24, report_pick_row_h() - 8) == 1) {
            double slot = report_pick_slot(i);
            /* One tap picks one player, a second tap on the same row drops it. */
            play_button_click();
            if (report_pick_slot_sel == slot) {
                report_pick_reset();
            } else {
                report_pick_slot_sel = slot;
                report_pick_reason = 0;
            }
            return;
        }
        i = i + 1;
    }
}
