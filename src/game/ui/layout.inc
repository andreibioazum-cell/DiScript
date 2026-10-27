static double fit_w(double w) { return clamp(w, 0, screen_w - 2 * screen_margin); }

static double center_x(double w) { return (screen_w - w) / 2; }

static double title_y(void) { return back_y + btn_h + title_gap; }

static double content_top(void) { return title_y() + list_top_gap; }

static double menu_btn_w(void) { return btn_w; }

/* The new theme centres the menu; the old one keeps it at the left margin. */
static double menu_x(void) {
    if (new_theme == 1) {
        return center_x(btn_w);
    }
    return screen_margin;
}

static double menu_stride(void) { return btn_h + btn_gap; }

static double menu_top(void) { return screen_h / 2 - (lobby_rows * btn_h + (lobby_rows - 1) * btn_gap) / 2; }

static double menu_row(double i) { return menu_top() + i * menu_stride(); }

static double hit_menu_row(double px, double py, double i) {
    return hit_button(px, py, menu_x(), menu_row(i), menu_btn_w(), btn_h);
}

static double settings_stride(void) {
    double s = menu_stride();
    /* Fit all settings rows below menu_top() on short screens. */
    double fit = (screen_h - 16 - menu_top()) / SETTINGS_ROWS;
    if (fit < s) {
        s = fit;
    }
    if (s < 24) {
        s = 24;
    }
    return s;
}

static double settings_btn_h(void) {
    double h = settings_stride();
    if (h > btn_h) {
        h = btn_h;
    }
    return h;
}

static double settings_top(void) {
        /* The settings list starts at the same height as the Play button. */
    return menu_top();
}

static double settings_row_y(double i) { return settings_top() + i * settings_stride(); }

static double hit_settings_row(double px, double py, double i) {
    return hit_button(px, py, menu_x(), settings_row_y(i), menu_btn_w(), settings_btn_h());
}

static double lobby_title_y(void) { return menu_top() - 70; }

/* Center lobby caption horizontally over the button column. */
static double lobby_label_x(double w) {
    return menu_x() + (btn_w - w) / 2;
}

static double lobby_hello_y(void) { return 18; }

static double lobby_money_y(void) { return menu_row(lobby_rows - 1) + btn_h + 12; }

static double lobby_msg_y(void) { return lobby_money_y() + money_line_h + 6; }

static double classes_card_w(void) {
    double n = classes_page_size();
    double total_gap = class_gap * (n - 1);
    double w = (screen_w - 2 * screen_margin - total_gap) / n;
    return clamp(w, class_card_min_w, class_card_max_w);
}

static double classes_top(void) { return back_y + btn_h + title_gap; }

static double classes_card_x(double vi) {
    return classes_row_x() + vi * (classes_card_w() + class_gap);
}

/* Compute shop card portrait render size scaled to card width. */
static double class_render_px(void) {
    double w = classes_card_w();
    double t = (w - class_card_min_w) / (class_card_max_w - class_card_min_w);
    t = clamp(t, 0, 1);
    return (84 + t * (108 - 84)) * 0.88;
}

static double class_sprite_y(void) { return classes_top() + 14; }

static double class_title_scale(void) { return 0.85; }

static double class_title_y(void) { return classes_top() + 126; }

static double class_desc_y(void) { return classes_top() + 161; }

static double class_level_y(void) { return classes_top() + 185; }

static double class_pick_inset(void) { return 24; }

static double class_pick_w(void) { return classes_card_w() - 2 * class_pick_inset(); }

static double class_pick_y(void) { return classes_top() + class_card_h - class_pick_h - class_pick_pad; }

static double hit_class_card(double px, double py, double vi) {
    return hit_button(px, py, classes_card_x(vi), classes_top(), classes_card_w(), class_card_h);
}

/* Test whether point hits the buy/select pill inside shop card `vi`. */
static double hit_class_pill(double px, double py, double vi) {
    return hit_button(px, py, classes_card_x(vi) + class_pick_inset(), class_pick_y(), class_pick_w(), class_pick_h);
}

static double classes_swipe_zone(double px, double py) {
    /* Enable horizontal strip dragging only when cards overflow viewport. */
    return classes_scroll_needed() == 1 && px >= screen_margin && px <= screen_w - screen_margin &&
           py >= classes_top() && py <= classes_top() + class_card_h;
}

static double class_actions_y(void) { return classes_top() + class_card_h + class_gap; }

static double class_action_w(void) {
    double half = (screen_w - 2 * screen_margin - class_action_gap) / 2;
    return clamp(half, 0, btn_w);
}

static double levels_btn_x(void) {
    if (class_has_skins(player_class) == 0) {
        return screen_w / 2 - class_action_w() / 2;
    }
    return screen_w / 2 - class_action_gap / 2 - class_action_w();
}

static double skins_btn_x(void) { return screen_w / 2 + class_action_gap / 2; }

static double class_has_skins(double cls) { return cls == CLASS_AZUM || cls == CLASS_EBUC; }

static double skins_btn_visible(void) { return class_has_skins(player_class); }

static double hit_levels_btn(double px, double py) {
    return hit_button(px, py, levels_btn_x(), class_actions_y(), class_action_w(), btn_h);
}

static double hit_skins_btn(double px, double py) {
    if (skins_btn_visible() == 0) {
        return 0;
    }
    return hit_button(px, py, skins_btn_x(), class_actions_y(), class_action_w(), btn_h);
}

static double class_msg_y(void) { return class_actions_y() + btn_h + 14; }

static double stat_motto_y(void) { return screen_h - 142; }

static double levels_w(void) { return fit_w(list_w); }

static double levels_x(void) { return center_x(levels_w()); }

static double levels_top(void) { return content_top() + 30; }

static double levels_row_h_fit(void) {
    double avail = screen_h - 40 - levels_top();
    double fit = (avail - (max_level - 1) * level_row_gap) / max_level;
    if (fit < level_row_h) {
        return fit < 34 ? 34 : fit;
    }
    return level_row_h;
}

static double levels_row_y(double n) { return levels_top() + (n - 1) * (levels_row_h_fit() + level_row_gap); }

static double levels_msg_y(void) { return levels_row_y(max_level) + levels_row_h_fit() + 12; }

static double hit_level_row(double px, double py, double n) {
    return hit_button(px, py, levels_x(), levels_row_y(n), levels_w(), levels_row_h_fit());
}

static double skins_total_w(void) { return skin_card_w * SKIN_COUNT + skin_card_gap * (SKIN_COUNT - 1); }

static double skins_card_x(double i) { return center_x(skins_total_w()) + i * (skin_card_w + skin_card_gap); }

static double skins_top(void) { return content_top() + 30; }

static double skin_preview_scale(void) { return 1.8; }

static double skin_preview_y(void) { return skins_top() + 16; }

static double skin_name_y(void) { return skins_top() + 118; }

static double skin_status_y(void) { return skins_top() + 146; }

static double skins_msg_y(void) { return skins_top() + skin_card_h + 16; }

static double hit_skin_card(double px, double py, double i) {
    return hit_button(px, py, skins_card_x(i), skins_top(), skin_card_w, skin_card_h);
}

static double ach_w(void) { return fit_w(760); }

static double ach_x(void) { return center_x(ach_w()); }

static double ach_top(void) { return content_top(); }

static double ach_card_y(double i) { return ach_top() + i * (ach_card_h + ach_card_gap); }

static double lb_w(void) { return fit_w(lb_w_max); }

static double lb_x(void) { return center_x(lb_w()); }

static double lb_top(void) { return content_top(); }

static double lb_row_y(double i) { return lb_top() + i * (lb_row_h + lb_row_gap); }

static double login_x(void) { return center_x(login_w) + login_ox; }

static double login_nick_y(void) { return screen_h / 2 - 100 + login_oy; }

static double login_pass_y(void) { return login_nick_y() + login_fh + login_field_gap; }

static double login_btn_y(void) { return login_pass_y() + login_fh + login_btn_gap; }

static double login_btn_x(void) { return center_x(login_btn_w); }

static double login_msg_y(void) { return login_btn_y() + btn_h + 16; }

static double hit_login_field(double px, double py, double field_y) {
    return hit_rect(px, py, login_x(), field_y, login_w, login_fh);
}

static double hit_login_btn(double px, double py) {
    return hit_button(px, py, login_btn_x(), login_btn_y(), login_btn_w, btn_h);
}

static double modes_top(void) { return screen_h / 2 - (modes_rows * btn_h + (modes_rows - 1) * btn_gap) / 2; }

static double modes_row(double i) { return modes_top() + i * menu_stride(); }

static double hit_modes_row(double px, double py, double i) {
    return hit_button(px, py, menu_x(), modes_row(i), menu_btn_w(), btn_h);
}

/* Promo code: its own button in the bottom right corner of the modes screen. */
static double promo_corner_w(void) { return fit_w(220); }

static double promo_corner_x(void) { return screen_w - screen_margin - promo_corner_w(); }

static double promo_corner_y(void) { return screen_h - screen_margin - btn_h; }

static double hit_promo_corner(double px, double py) {
    return hit_button(px, py, promo_corner_x(), promo_corner_y(), promo_corner_w(), btn_h);
}

/* Lobby bottom-right battle pass button width. */
static double bpass_corner_w(void) { return fit_w(240); }

static double bpass_corner_x(void) { return screen_w - screen_margin - bpass_corner_w(); }

static double bpass_corner_y(void) { return screen_h - screen_margin - btn_h; }

static double hit_bpass_corner(double px, double py) {
    return hit_button(px, py, bpass_corner_x(), bpass_corner_y(), bpass_corner_w(), btn_h);
}

/* The warning has two actions: Documents on the left, consent on the right. */
static double warn_row_w(void) { return fit_w(920); }

static double warn_docs_w(void) { return clamp(warn_row_w() * 0.34, 150, 300); }

static double warn_docs_x(void) { return center_x(warn_row_w()); }

static double warn_btn_w(void) { return warn_row_w() - warn_docs_w() - 24; }

static double warn_btn_x(void) { return warn_docs_x() + warn_docs_w() + 24; }

static double warn_btn_y(void) { return screen_h / 2 + 176; }

static double hit_warn_btn(double px, double py) {
    return hit_button(px, py, warn_btn_x(), warn_btn_y(), warn_btn_w(), btn_h);
}

static double hit_warn_docs(double px, double py) {
    return hit_button(px, py, warn_docs_x(), warn_btn_y(), warn_docs_w(), btn_h);
}
