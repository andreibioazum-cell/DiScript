/* Achievements screen and vertical scroll.
 *
 * Every player except Dimasi4ek229 himself has "mov eax, 0x229" in their
 * achievements list (locked "???" until earned, open afterwards): whenever
 * that achievement is in the list (6 cards total), vertical scrolling and the
 * scrollbar are active regardless of whether it has been unlocked yet.
 * For Dimasi4ek229 himself, the "meet Dimasi4ek229" achievement is absent from
 * the list (5 cards total), so the list fits on one static screen with no scroll. */

static double ach_scroll;
static double ach_vel;
static double ach_drag_id = -1;
static double ach_drag_y;

static double achievement_count(void) {
    return 6;
}

static double achievement_bit(double i) {
    if (i == 0) {
        return ACH_LEGENDS;
    }
    if (i == 1) {
        return ACH_FIRST_WIN;
    }
    if (i == 2) {
        return ACH_ALL_CHARACTERS;
    }
    if (i == 3) {
        return ACH_LAST_HP;
    }
    if (i == 4) {
        return ACH_EAX229;
    }
    if (i == 5) {
        return ACH_BUK_GIFT;
    }
    return ACH_LEGENDS;
}

static const char *achievement_title(double i) {
    if (i == 0) {
        return tr_achievement_legends();
    }
    if (i == 1) {
        return tr_achievement_first_win();
    }
    if (i == 2) {
        return tr_achievement_grass();
    }
    if (i == 3) {
        return tr_achievement_last_hp();
    }
    if (i == 4) {
        return tr_achievement_eax229();
    }
    if (i == 5) {
        return tr_achievement_buk();
    }
    return tr_achievement_legends();
}

static const char *achievement_desc(double i) {
    if (i == 0) {
        return tr_achievement_legends_desc();
    }
    if (i == 1) {
        return tr_achievement_first_win_desc();
    }
    if (i == 2) {
        return tr_achievement_grass_desc();
    }
    if (i == 3) {
        return tr_achievement_last_hp_desc();
    }
    if (i == 4) {
        return tr_achievement_eax229_desc();
    }
    if (i == 5) {
        return tr_achievement_buk_desc();
    }
    return tr_achievement_legends_desc();
}

static double ach_scroll_allowed(void) {
    return nick_is_dimasi229(net_login_nick()) == 1 ? 0 : 1;
}

static double ach_visible_count(void) {
    return ach_scroll_allowed() == 1 ? achievement_count() : achievement_count() - 1;
}

static double ach_visible_item(double vi) {
    if (ach_scroll_allowed() == 1) {
        return vi;
    }
    return vi < 4 ? vi : vi + 1;
}

static double ach_step(void) {
    return ach_card_h + ach_card_gap;
}

static double ach_five_cards_h(void) {
    return 5 * ach_card_h + 4 * ach_card_gap;
}

static double ach_view_top(void) {
    return ach_top();
}

static double ach_view_h(void) {
    double room = screen_h - 24 - ach_view_top();
    if (room < ach_card_h) {
        room = ach_card_h;
    }
    return room < ach_five_cards_h() ? room : ach_five_cards_h();
}

static double ach_view_bottom(void) {
    return ach_view_top() + ach_view_h();
}

static double ach_content_h(void) {
    double n = ach_visible_count();
    return n * ach_card_h + (n - 1) * ach_card_gap;
}

static double ach_scroll_max(void) {
    if (ach_scroll_allowed() == 0) {
        return 0;
    }
    double extra = ach_content_h() - ach_view_h();
    return extra > 0 ? extra : 0;
}

static double ach_scroll_needed(void) {
    return ach_scroll_max() > 0 ? 1 : 0;
}

static void ach_scroll_reset(void) {
    ach_drag_id = -1;
    ach_vel = 0;
}

static void ach_open(void) {
    ach_scroll = 0;
    ach_scroll_reset();
}

static void ach_scroll_by(double dy) {
    if (ach_scroll_needed() == 0) {
        ach_scroll = 0;
        ach_vel = 0;
        return;
    }
    ach_scroll = clamp(ach_scroll + dy, 0, ach_scroll_max());
}

static void ach_scroll_tick(void) {
    if (ach_scroll_needed() == 0) {
        ach_scroll = 0;
        ach_vel = 0;
        ach_drag_id = -1;
        return;
    }
    double mx = ach_scroll_max();
    ach_scroll = clamp(ach_scroll, 0, mx);
    if (ach_drag_id >= 0) {
        return;
    }
    if (fabs(ach_vel) < 0.6) {
        ach_vel = 0;
        return;
    }
    ach_scroll = ach_scroll + ach_vel * dt;
    ach_vel = ach_vel * exp(-6.0 * dt);
    if (ach_scroll <= 0) {
        ach_scroll = 0;
        ach_vel = 0;
    } else if (ach_scroll >= mx) {
        ach_scroll = mx;
        ach_vel = 0;
    }
}

static void touch_achievements(double x, double y, double action, double id) {
    if (action == 3) {
        ach_scroll_reset();
        return;
    }
    if (action == 2) {
        if (ach_drag_id < 0 || id != ach_drag_id) {
            return;
        }
        if (ach_scroll_needed() == 1) {
            double dy = y - ach_drag_y;
            ach_scroll_by(-dy);
            ach_vel = clamp(-dy / fmax(dt, 0.008) * 0.6, -900, 900);
        }
        ach_drag_y = y;
        return;
    }
    if (action == 1) {
        if (id == ach_drag_id) {
            ach_drag_id = -1;
        }
        return;
    }
    if (back_hit(x, y) == 1) {
        ach_scroll_reset();
        play_button_click();
        start_transition(ST_LOBBY);
        return;
    }
    if (ach_scroll_needed() == 1 && x >= screen_margin && x <= screen_w - screen_margin &&
        y >= ach_view_top() && y <= ach_view_bottom()) {
        press_points_drop(id);
        ach_drag_id = id;
        ach_drag_y = y;
        ach_vel = 0;
    }
}

static void draw_achievement_card_at(double i, double y) {
    double locked = 1 - has_achievement(achievement_bit(i));
    double x = ach_x();
    panel(x, y, ach_w(), ach_card_h, locked);
    uint32_t c = panel_text(locked);
    const char *status = locked == 1 ? tr_achievement_locked() : tr_achievement_open();
    text_scaled(achievement_title(i), x + card_text_pad, y + 14, c, 0.62);
    text_scaled(achievement_desc(i), x + card_text_pad, y + 46, c, 0.46);
    text_scaled(status, x + card_text_pad, y + 70, c, 0.42);
}

static void draw_achievement_card_clipped(double i, double card_y, double clip_top, double clip_bottom) {
    double locked = 1 - has_achievement(achievement_bit(i));
    double x = ach_x();
    double w = ach_w();
    double y = card_y > clip_top ? card_y : clip_top;
    double bottom = card_y + ach_card_h < clip_bottom ? card_y + ach_card_h : clip_bottom;
    double h = bottom - y;
    if (h < 2) {
        return;
    }
    uint32_t face = (uint32_t)(locked == 1 ? locked_bg() : ui_purple);
    uint32_t c = panel_text(locked);
    const char *status = locked == 1 ? tr_achievement_locked() : tr_achievement_open();
    roundrect(x, y, w, h, btn_radius, face);
    if (card_y < clip_top) {
        rect(x, y, w, fmin(btn_radius, h), face);
    }
    if (card_y + ach_card_h > clip_bottom) {
        rect(x, y + h - fmin(btn_radius, h), w, fmin(btn_radius, h), face);
    }
    if (card_y + 8 >= clip_top && card_y + 38 <= clip_bottom) {
        text_scaled(achievement_title(i), x + card_text_pad, card_y + 14, c, 0.62);
    }
    if (card_y + 40 >= clip_top && card_y + 66 <= clip_bottom) {
        text_scaled(achievement_desc(i), x + card_text_pad, card_y + 46, c, 0.46);
    }
    if (card_y + 64 >= clip_top && card_y + 88 <= clip_bottom) {
        text_scaled(status, x + card_text_pad, card_y + 70, c, 0.42);
    }
}

static void draw_achievements(void) {
    check_achievements();
    screen_fill();
    double count = ach_visible_count();
    if (ach_scroll_needed() == 0) {
        ach_scroll = 0;
        double vi = 0;
        while (vi < count) {
            draw_achievement_card_at(ach_visible_item(vi), ach_card_y(vi));
            vi = vi + 1;
        }
        draw_back();
        return;
    }
    ach_scroll = clamp(ach_scroll, 0, ach_scroll_max());
    double vt = ach_view_top();
    double vb = ach_view_bottom();
    double vi = 0;
    while (vi < count) {
        double cy = vt + vi * ach_step() - ach_scroll;
        draw_achievement_card_clipped(ach_visible_item(vi), cy, vt, vb);
        vi = vi + 1;
    }
    double track_w = 5;
    double track_x = ach_x() + ach_w() + 10;
    if (track_x + track_w > screen_w - 6) {
        track_x = screen_w - 6 - track_w;
    }
    double track_h = vb - vt;
    roundrect(track_x, vt, track_w, track_h, 2, card_bg);
    double thumb_h = fmax(28, track_h * ach_view_h() / ach_content_h());
    if (thumb_h > track_h) {
        thumb_h = track_h;
    }
    double thumb_y = vt + (track_h - thumb_h) * ach_scroll / ach_scroll_max();
    roundrect(track_x, thumb_y, track_w, thumb_h, 2, text_color);
    draw_back();
}
