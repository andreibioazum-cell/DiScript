/* Legal documents overlay, personal data consent flow, and startup warning. */

static double docs_page;
/* 1 when requesting personal data consent from an already signed-in player. */
static double docs_ask;
static double docs_asked;
static double login_consent;

static double consent_ok(void) { return settings_pd_consent_version() >= LEGAL_DOCS_VERSION ? 1 : 0; }

static void consent_give(void) {
    if (consent_ok() == 0) {
        settings_set_pd_consent(LEGAL_DOCS_VERSION);
    }
    net_pd_consent_upload();
}

static void docs_show(double page, double ask) {
    docs_open = 1;
    docs_page = clamp(page, 0, LEGAL_PAGES - 1);
    docs_ask = ask;
    keyboard_hide();
    ui_nav_gen = ui_nav_gen + 1;
}

static void docs_close(void) {
    docs_open = 0;
    docs_ask = 0;
    ui_nav_gen = ui_nav_gen + 1;
}

static double docs_pending_page, docs_pending_ask;
static void docs_open_pending(void) { docs_show(docs_pending_page, docs_pending_ask); }
static void docs_transition(double page, double ask) {
    if (t_dir != 0) {
        return;
    }
    docs_pending_page = page;
    docs_pending_ask = ask;
    start_action_transition(docs_open_pending);
}
static void docs_prev_page(void) { docs_page = docs_page - 1; }
static void docs_next_page(void) { docs_page = docs_page + 1; }
static void docs_agree(void) {
    consent_give();
    docs_close();
}
static void docs_refuse(void) {
    net_logout();
    progress_reset_on_signout();
    docs_close();
}

/* --- layout ------------------------------------------------------------ */

static double docs_top(void) { return clamp(screen_h / 2 - 236, back_y + btn_h + 20, screen_h); }
static double docs_line_y(double i) { return docs_top() + 50 + i * 26; }
static double docs_row_y(void) { return docs_line_y(LEGAL_LINES) + 18; }
static double docs_row_w(void) { return fit_w(1000); }
static double docs_nav_w(void) { return clamp(docs_row_w() * 0.24, 120, 240); }
static double docs_prev_x(void) { return center_x(docs_row_w()); }
static double docs_next_x(void) { return center_x(docs_row_w()) + docs_row_w() - docs_nav_w(); }
static double docs_mid_w(void) { return docs_row_w() - 2 * docs_nav_w() - 32; }
static double docs_mid_x(void) { return center_x(docs_mid_w()); }

static double docs_has_mid(void) {
    if (docs_ask == 1) {
        return docs_page == LEGAL_PAGE_CONSENT;
    }
    return docs_page == LEGAL_PAGE_RIGHTS && net_login_status() == 2;
}

/* In question mode the second button, Decline, sits one row lower. */
static double docs_refuse_y(void) { return docs_row_y() + btn_h + 14; }

static double hit_docs_close(double x, double y) { return back_hit(x, y); }
static double hit_docs_prev(double x, double y) {
    return docs_page > 0 && hit_button(x, y, docs_prev_x(), docs_row_y(), docs_nav_w(), btn_h) == 1;
}
static double hit_docs_next(double x, double y) {
    return docs_page < LEGAL_PAGES - 1 && hit_button(x, y, docs_next_x(), docs_row_y(), docs_nav_w(), btn_h) == 1;
}
static double hit_docs_mid(double x, double y) {
    return docs_has_mid() == 1 && hit_button(x, y, docs_mid_x(), docs_row_y(), docs_mid_w(), btn_h) == 1;
}
static double hit_docs_refuse(double x, double y) {
    return docs_ask == 1 && docs_page == LEGAL_PAGE_CONSENT &&
           hit_button(x, y, docs_mid_x(), docs_refuse_y(), docs_mid_w(), btn_h) == 1;
}

/* --- drawing ----------------------------------------------------------- */

static void docs_text(const char *s, double y, double max_scale) {
    ctext_scaled(s, y, text_color, fit_text_scale(s, fit_w(screen_w), max_scale));
}

static const char *docs_mid_label(void) {
    if (docs_ask == 1) {
        return tr_consent_agree();
    }
    return tr_delete_account();
}

static void draw_docs(void) {
    screen_fill();
    press_overlay = 2;
    button(docs_ask == 1 ? tr_legal_close() : tr_back(), (screen_w - btn_w) / 2, back_y, btn_w, btn_h);
    const char *title = ds_concat(ds_concat(ds_concat(legal_line(docs_page, 0), "  ("),
                                            ds_num_to_string(docs_page + 1)),
                                  ds_concat("/", ds_concat(ds_num_to_string(LEGAL_PAGES), ")")));
    docs_text(title, docs_top(), 0.8);
    double i = 1;
    while (i <= LEGAL_LINES) {
        docs_text(legal_line(docs_page, i), docs_line_y(i - 1), 0.5);
        i = i + 1;
    }
    if (docs_page == LEGAL_PAGE_CONSENT) {
        docs_text(consent_ok() == 1 ? tr_consent_given() : tr_consent_missing(), docs_line_y(LEGAL_LINES - 1), 0.5);
    }
    if (docs_page == LEGAL_PAGE_RIGHTS) {
        const char *note = net_login_status() == 2 ? "" : tr_delete_need_login();
        docs_text(note, docs_line_y(LEGAL_LINES - 1), 0.5);
    }
    if (docs_page > 0) {
        button_scaled(tr_legal_prev(), docs_prev_x(), docs_row_y(), docs_nav_w(), btn_h, 0.6);
    }
    if (docs_page < LEGAL_PAGES - 1) {
        button_scaled(tr_legal_next(), docs_next_x(), docs_row_y(), docs_nav_w(), btn_h, 0.6);
    }
    if (docs_has_mid() == 1) {
        const char *mid = docs_mid_label();
        button_scaled(mid, docs_mid_x(), docs_row_y(), docs_mid_w(), btn_h, fit_text_scale(mid, docs_mid_w() - 24, 0.6));
    }
    if (docs_ask == 1 && docs_page == LEGAL_PAGE_CONSENT) {
        button_scaled(tr_consent_refuse(), docs_mid_x(), docs_refuse_y(), docs_mid_w(), btn_h,
                      fit_text_scale(tr_consent_refuse(), docs_mid_w() - 24, 0.6));
    }
    press_overlay = 0;
}

/* --- touches ----------------------------------------------------------- */

static double docs_button_hit(double x, double y) {
    return hit_docs_close(x, y) == 1 || hit_docs_prev(x, y) == 1 || hit_docs_next(x, y) == 1 ||
           hit_docs_mid(x, y) == 1 || hit_docs_refuse(x, y) == 1;
}

static void touch_docs(double x, double y, double action) {
    if (action != 0) {
        return;
    }
    if (docs_button_hit(x, y) == 1) {
        play_button_click();
    }
    if (hit_docs_close(x, y) == 1) {
        start_action_transition(docs_close);
        return;
    }
    if (hit_docs_prev(x, y) == 1) {
        start_action_transition(docs_prev_page);
        return;
    }
    if (hit_docs_next(x, y) == 1) {
        start_action_transition(docs_next_page);
        return;
    }
    if (hit_docs_mid(x, y) == 1) {
        if (docs_ask == 1) {
            start_action_transition(docs_agree);
            return;
        }
                /* Deletion lives on the account screen: password and swipe right. */
        start_action_transition(acct_delete_window);
        return;
    }
    if (hit_docs_refuse(x, y) == 1) {
        /* Sign out to offline mode when personal data consent is declined. */
        start_action_transition(docs_refuse);
    }
}

static void docs_tick(void) {
    /* Prompt signed-in accounts once per launch when consent version is outdated. */
    if (docs_open == 0 && acct_open == 0 && docs_asked == 0 && warn_open == 0 && studio_open == 0 &&
        t_dir == 0 &&
        game_state == ST_LOBBY && net_login_status() == 2 && consent_ok() == 0) {
        docs_asked = 1;
        docs_show(LEGAL_PAGE_CONSENT, 1);
    }
}

/* --- 12+ badge --------------------------------------------------------- */

static void draw_age_mark(double x, double y, double size, uint32_t c) {
    roundrect_outline(x, y, size, size * 0.62, size * 0.14, 2, c);
    text_in_box(tr_age_mark(), x, y, size, size * 0.62, c, fit_text_scale(tr_age_mark(), size * 0.78, 0.7));
}

/* --- consent checkbox on the login screen ------------------------------- */

static double login_consent_y(void) { return login_msg_y() + 40; }
static double login_box_sz(void) { return 40; }
static double login_consent_w(void) { return fit_w(login_box_sz() + 16 + text_ink_width(tr_consent_box()) * 0.5); }
static double login_consent_x(void) { return center_x(login_consent_w()); }
static double login_more_w(void) { return fit_w(420); }
static double login_more_y(void) { return login_consent_y() + login_box_sz() + 14; }

static double hit_login_consent(double x, double y) {
    return hit_rect(x, y, login_consent_x() - 8, login_consent_y() - 8, login_consent_w() + 16, login_box_sz() + 16);
}

static double hit_login_more(double x, double y) {
    return hit_button(x, y, center_x(login_more_w()), login_more_y(), login_more_w(), btn_h * 0.8);
}

static void draw_login_consent(void) {
    double bx = login_consent_x();
    double by = login_consent_y();
    double sz = login_box_sz();
    roundrect(bx, by, sz, sz, 8, card_bg);
    roundrect_outline(bx, by, sz, sz, 8, 2, text_color);
    if (login_consent == 1) {
        line(bx + sz * 0.22, by + sz * 0.52, bx + sz * 0.42, by + sz * 0.74, 4, text_color);
        line(bx + sz * 0.42, by + sz * 0.74, bx + sz * 0.80, by + sz * 0.26, 4, text_color);
    }
    double tw = login_consent_w() - sz - 16;
    text_in_field(tr_consent_box(), bx + sz + 16, by, sz, text_color, fit_text_scale(tr_consent_box(), tw, 0.5));
    button_scaled(tr_consent_more(), center_x(login_more_w()), login_more_y(), login_more_w(), btn_h * 0.8,
                  fit_text_scale(tr_consent_more(), login_more_w() - 24, 0.5));
}

/* Bilingual startup epilepsy and terms warning text. */
static const char *const warn_ru[] = {"ВНИМАНИЕ", "В игре есть яркие вспышки и быстрая",
                                      "смена цветов (диско-режим в онлайне).", "При эпилепсии или чувствительности",
                                      "к свету играйте осторожно.", "Нажимая кнопку согласия, вы принимаете",
                                      "Пользовательское соглашение (см. «Документы»)."};
static const char *const warn_en[] = {"WARNING", "This game contains bright flashes and",
                                      "fast color changes (disco mode online).", "If you have epilepsy or are sensitive",
                                      "to light, please play with caution.", "By tapping the consent button you accept",
                                      "the Terms of Use (see \"Documents\")."};

static double warn_rgb(double rgb) {
    return (1 + floor(warn_a * 254)) * 0x1000000 + rgb;
}

static const char *tr_legal_accept(void) {
    if (language == 1) {
        return "Понимаю и принимаю";
    }
    return "I understand and accept";
}

static void warn_block(const char *const *lines, double y, uint32_t body) {
    double i = 1;
    ctext_scaled(lines[0], y, body, 1.0);
    while (i < 7) {
        const char *s = lines[(int)i];
        ctext_scaled(s, y + 18 + i * 25, body, fit_text_scale(s, fit_w(screen_w), 0.5));
        i = i + 1;
    }
}

/* Draw startup warning button using overlay press channel. */
static void warn_button(const char *label, double bx, double bw) {
    double by = warn_btn_y();
    double scale = fit_text_scale(label, bw - 24, 0.8);
    press_overlay = 1;
    double p = button_press_k(bx, by, bw, btn_h);
    press_overlay = 0;
    double s = 1 - PRESS_SQUEEZE * p;
    double k = 1 - PRESS_SHADE * p;
    double sx = squeeze_x(bx, bw, s);
    double sy = squeeze_y(by, by, btn_h, s);
    roundrect(sx, sy, bw * s, btn_h * s, btn_radius, warn_rgb(0));
    roundrect_outline(sx, sy, bw * s, btn_h * s, btn_radius, warn_frame_th, warn_rgb(shade(0xFFFFFF, k)));
    text_in_box(label, sx, sy, bw * s, btn_h * s, warn_rgb(shade(0xFFFFFF, k)), scale * s);
}

static void draw_warning(void) {
    rect(0, 0, screen_w, screen_h, warn_rgb(0));
    uint32_t body = warn_rgb(text_color);
    double y = clamp(screen_h / 2 - 244, 8, screen_h);
        /* Game language first (picked from the phone language), the copy below. */
    warn_block(language == 1 ? warn_ru : warn_en, y, body);
    warn_block(language == 1 ? warn_en : warn_ru, y + 214, body);
    draw_age_mark(screen_w - screen_margin - 96, back_y, 96, body);
    const char *label = tr_legal_accept();
    if (warn_ready < 1) {
        label =
            ds_concat(ds_concat(ds_concat(label, " ("), ds_num_to_string((double)((warn_wait - floor(warn_t))))), ")");
    }
    warn_button(tr_legal_docs(), warn_docs_x(), warn_docs_w());
    warn_button(label, warn_btn_x(), warn_btn_w());
}

/* Open startup epilepsy warning on launch. */
static void start_gate_init(void) {
    warn_open = 1;
    warn_a = 1;
    warn_ready = 0;
    warn_t = 0;
    warn_closing = 0;
    warn_close_t = 0;
}

static void legal_accept(void) {
    if (warn_ready < 1 || warn_closing == 1) {
        return;
    }
    warn_close_t = 0;
    warn_closing = 1;
    settings_mark_legal();
}

static void touch_warn(double x, double y, double action) {
        /* Warning buttons fire on press, like every other button. */
    if (action != 0) {
        return;
    }
    if (hit_warn_docs(x, y) == 1) {
        play_button_click();
        docs_transition(0, 0);
        return;
    }
    if (hit_warn_btn(x, y) == 1) {
        play_button_click();
        legal_accept();
    }
}
