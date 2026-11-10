/* Texture packs screen: pick which pack replaces the built-in assets. */

static double packs_open;
static double packs_scroll;
static double packs_sel = -1;
static double packs_fs_open;
static void fs_open_picker(void);

static const char *tr_packs_btn(void) { return packs_tr("Паки", "PACKS"); }
static const char *tr_packs_title(void) { return packs_tr("Текстурпаки", "Texture packs"); }
static const char *tr_packs_none(void) {
    return packs_tr("Паков нет. Импорт или папка packs.", "No packs. Import one or use the packs folder.");
}
static const char *tr_packs_hint(void) {
    return packs_tr("Свой пак: Экспорт -> .zip, правь PNG, Импорт",
                    "Make a pack: Export -> .zip, edit the PNGs, Import");
}
static const char *tr_packs_off(void) { return packs_tr("Обычные", "Built-in"); }
static const char *tr_packs_import(void) { return packs_tr("Импорт", "Import"); }
static const char *tr_packs_export(void) { return packs_tr("Экспорт", "Export"); }
static const char *tr_packs_refresh(void) { return packs_tr("Обновить", "Refresh"); }
static const char *tr_packs_close(void) { return packs_tr("Закрыть", "Close"); }
static const char *tr_packs_nopicker(void) {
    return packs_tr("Системный выбор файла недоступен", "No system file picker here");
}

/* The hint under the title is a timed message: it must disappear on its own. */
static void packs_tick_msg(void) {
    pack_export_tick();
    packs_msg_t = tick_down(packs_msg_t);
    if (packs_msg_t <= 0)
        packs_msg[0] = 0;
}

static double packs_card_w(void) { return fit_w(620); }
static double packs_card_x(void) { return center_x(packs_card_w()); }
static double packs_card_y(void) { return 26; }
static double packs_card_h(void) { return screen_h - 52; }
static double packs_rows_top(void) { return packs_card_y() + 96; }
static double packs_row_h(void) { return 52; }
static double packs_actions_y(double row) { return packs_card_y() + packs_card_h() - 24 - (2 - row) * (btn_h + 10); }
static double packs_rows_visible(void) {
    double space = packs_actions_y(0) - 40 - packs_rows_top();
    double rows = floor(space / packs_row_h());
    if (rows < 1)
        rows = 1;
    return rows;
}
static double packs_row_y(double i) { return packs_rows_top() + (i - packs_scroll) * packs_row_h(); }
static double packs_action_w(void) { return (packs_card_w() - 40 - 2 * 10) / 3; }
static double packs_action_x(double col) { return packs_card_x() + 20 + col * (packs_action_w() + 10); }

static void draw_packs_lobby_button(void) {
    button_scaled(tr_packs_btn(), 16, 16, 92, 34, 0.45);
}

static int hit_packs_lobby_button(double x, double y) {
    return hit_rect(x, y, 16, 16, 92, 34);
}

static void packs_open_screen(void) {
    packs_scan();
    packs_scroll = 0;
    packs_sel = -1;
    packs_open = 1;
    packs_msg_t = 0;
    ui_nav_gen = ui_nav_gen + 1;
    press_pulses_clear();
}

static void packs_close_screen(void) {
    packs_open = 0;
    packs_fs_open = 0;
    packs_msg_t = 0;
    ui_nav_gen = ui_nav_gen + 1;
    press_pulses_clear();
}

static void packs_touch_row(double row) {
    int index = (int)(row + packs_scroll);
    if (index <= 0) {
        packs_sel = -1;
        packs_set_active("");
        return;
    }
    index = index - 1;
    if (index < 0 || index >= packs_count)
        return;
    packs_sel = (double)index;
    packs_set_active(packs_names[index]);
}

static void packs_import_press(void) {
#if defined(__ANDROID__)
    if (net_fs_pick_import() == 1)
        packs_msg_set(tr_packs_hint());
    else
        packs_msg_set(tr_packs_nopicker());
#else
    fs_open_picker();
#endif
}

static void draw_packs_row(double row, const char *label, int active) {
    double x = packs_card_x() + 20, y = packs_row_y(row), w = packs_card_w() - 40;
    roundrect(x, y, w, packs_row_h() - 8, 12, active ? RGB(123,47,216) : card_bg);
    text_scaled(label, x + 16, y + 12, text_color, 0.5);
    if (active == 1)
        text_scaled(">", x + w - 30, y + 12, text_color, 0.5);
}

static void draw_packs(void) {
    double cx = packs_card_x(), cy = packs_card_y(), cw = packs_card_w(), ch = packs_card_h();
    double rows = packs_rows_visible(), i;
    double total = (double)packs_count + 1;
    packs_tick_msg();
    rect(0, 0, screen_w, screen_h, 0xCC000000);
    roundrect(cx, cy, cw, ch, 18, card_bg);
    ctext_scaled(tr_packs_title(), cy + 18, text_color, 0.7);
    ctext_scaled(tr_packs_hint(), cy + 56, RGB(152,152,176), 0.4);
    ctext_scaled(packs_msg, cy + 74, RGB(184,184,200), 0.4);
    draw_packs_row(0, tr_packs_off(), packs_active[0] ? 0 : 1);
    i = 0;
    while (i < (double)packs_count && i < rows - 1) {
        draw_packs_row(i + 1, packs_label_of((int)(i + packs_scroll)),
                       strcmp(packs_names[(int)(i + packs_scroll)], packs_active) == 0);
        i = i + 1;
    }
    if (packs_count == 0 && rows > 2)
        ctext_scaled(tr_packs_none(), packs_rows_top() + 2 * packs_row_h() + 8, RGB(184,184,200), 0.45);
    if (total > rows) {
        char page[24];
        snprintf(page, sizeof(page), "%d/%d", (int)packs_scroll + 1, (int)(total - rows) + 1);
        text_scaled(page, cx + cw - 70, cy + 74, RGB(152,152,176), 0.38);
    }
    button_scaled("<", packs_action_x(0), packs_actions_y(0), packs_action_w(), btn_h, 0.5);
    button_scaled(tr_packs_refresh(), packs_action_x(1), packs_actions_y(0), packs_action_w(), btn_h, 0.44);
    button_scaled(">", packs_action_x(2), packs_actions_y(0), packs_action_w(), btn_h, 0.5);
    button_scaled(tr_packs_import(), packs_action_x(0), packs_actions_y(1), packs_action_w(), btn_h, 0.44);
    button_scaled(tr_packs_export(), packs_action_x(1), packs_actions_y(1), packs_action_w(), btn_h, 0.44);
    button_scaled(tr_packs_close(), packs_action_x(2), packs_actions_y(1), packs_action_w(), btn_h, 0.44);
}

static void touch_packs(double x, double y, double action, double id) {
    double rows = packs_rows_visible(), i;
    if (action == 3) {
        press_points_drop(id);
        return;
    }
    if (action != 0)
        return;
    i = 0;
    while (i < rows) {
        if (hit_rect(x, y, packs_card_x() + 20, packs_row_y(i), packs_card_w() - 40, packs_row_h() - 8) == 1) {
            play_button_click();
            packs_touch_row(i);
            return;
        }
        i = i + 1;
    }
    if (hit_button(x, y, packs_action_x(0), packs_actions_y(0), packs_action_w(), btn_h) == 1) {
        play_button_click();
        if (packs_scroll > 0)
            packs_scroll = packs_scroll - 1;
        return;
    }
    if (hit_button(x, y, packs_action_x(2), packs_actions_y(0), packs_action_w(), btn_h) == 1) {
        play_button_click();
        if (packs_scroll + packs_rows_visible() < (double)packs_count + 1)
            packs_scroll = packs_scroll + 1;
        return;
    }
    if (hit_button(x, y, packs_action_x(1), packs_actions_y(0), packs_action_w(), btn_h) == 1) {
        play_button_click();
        packs_scan();
        packs_msg_set(tr_packs_refresh());
        return;
    }
    if (hit_button(x, y, packs_action_x(0), packs_actions_y(1), packs_action_w(), btn_h) == 1) {
        play_button_click();
        packs_import_press();
        return;
    }
    if (hit_button(x, y, packs_action_x(1), packs_actions_y(1), packs_action_w(), btn_h) == 1) {
        play_button_click();
        pack_export_save();
        return;
    }
    if (hit_button(x, y, packs_action_x(2), packs_actions_y(1), packs_action_w(), btn_h) == 1) {
        play_button_click();
        packs_close_screen();
    }
}
