/* Owner-only screen with player complaints: ban the author or clear the note.
 * The list lives in the database, so it can be checked from any device. */

static double reports_open_flag;

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
static double reports_row_h(void) { return 92; }
static double reports_rows_max(void) {
    double room = reports_card_y() + reports_card_h() - 90 - reports_rows_top();
    double n = floor(room / reports_row_h());
    return n < 1 ? 1 : (n > 7 ? 7 : n);
}
static double reports_btn_w(void) { return 74; }
static double reports_ban_x(void) { return reports_card_x() + reports_card_w() - 2 * reports_btn_w() - 22; }
static double reports_done_x(void) { return reports_ban_x() + reports_btn_w() + 6; }
static double reports_row_y(double i) { return reports_rows_top() + i * reports_row_h(); }
static double reports_refresh_x(void) { return reports_card_x() + 20; }
static double reports_close_x(void) { return reports_card_x() + 40 + (reports_card_w() - 60) / 2; }
static double reports_bottom_y(void) { return reports_card_y() + reports_card_h() - 66; }
static double reports_bottom_w(void) { return (reports_card_w() - 60) / 2; }

static void reports_open(void) {
    reports_open_flag = 1;
    net_reports_fetch();
    play_button_click();
    ui_nav_gen = ui_nav_gen + 1;
}

static void reports_close(void) { reports_open_flag = 0; }

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
    rect(0, 0, screen_w, screen_h, 0xE6000000);
    roundrect(cx, cy, cw, ch, 16, card_bg);
    ctext_scaled(tr_reports(), cy + 22, text_color, 0.7);
    count = net_reports_count();
    maxn = reports_rows_max();
    if (net_reports_status() == 1) {
        ctext_scaled(tr_reports_loading(), cy + 130, text_color, 0.55);
    } else if (net_reports_status() == 3) {
        ctext_scaled(tr_reports_failed(), cy + 130, RGB(255,154,154), 0.55);
    } else if (count <= 0) {
        ctext_scaled(tr_reports_empty(), cy + 130, text_color, 0.55);
    } else {
        i = 0;
        while (i < count && i < maxn) {
            double y = reports_row_y(i);
            roundrect(cx + 12, y, cw - 24, reports_row_h() - 8, 10, RGB(46,42,70));
            text_scaled(reports_line(i), cx + 22, y + 8, text_color,
                        fit_text_scale(reports_line(i), cw - 200, 0.45));
            text_scaled(net_report_text(i), cx + 22, y + 38, RGB(184,184,200),
                        fit_text_scale(net_report_text(i), cw - 60, 0.42));
            button_scaled(tr_reports_ban(), reports_ban_x(), y + 12, reports_btn_w(), 34, 0.5);
            button_scaled(tr_reports_done(), reports_done_x(), y + 12, reports_btn_w(), 34, 0.45);
            i = i + 1;
        }
    }
    button_scaled(tr_reports_refresh(), reports_refresh_x(), reports_bottom_y(), reports_bottom_w(), 50, 0.5);
    button_scaled(tr_reports_close(), reports_close_x(), reports_bottom_y(), reports_bottom_w(), 50, 0.5);
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
        return;
    }
    if (hit_button(x, y, reports_close_x(), reports_bottom_y(), reports_bottom_w(), 50) == 1) {
        play_button_click();
        reports_close();
        return;
    }
    count = net_reports_count();
    maxn = reports_rows_max();
    i = 0;
    while (i < count && i < maxn) {
        double ry = reports_row_y(i);
        if (hit_button(x, y, reports_ban_x(), ry + 12, reports_btn_w(), 34) == 1) {
            char nick[24];
            snprintf(nick, sizeof(nick), "%s", net_report_target(i));
            play_button_click();
            if (nick[0])
                net_ban_set(nick, 1);
            net_report_delete(i);
            return;
        }
        if (hit_button(x, y, reports_done_x(), ry + 12, reports_btn_w(), 34) == 1) {
            play_button_click();
            net_report_delete(i);
            return;
        }
        i = i + 1;
    }
}
