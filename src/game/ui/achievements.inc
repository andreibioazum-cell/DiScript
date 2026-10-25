/* Achievements list screen and vertical scroll handling. */

static double ach_scroll;
static double ach_vel;
static double ach_drag_id = -1;
static double ach_drag_y;
static double ach_drag_frame_d;
static double ach_drag_moved;

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
    double extra = ach_content_h() - ach_view_h();
    return extra > 0 ? extra : 0;
}

static double ach_scroll_needed(void) {
    return ach_scroll_max() > 0 ? 1 : 0;
}

static void ach_scroll_reset(void) {
    ach_drag_id = -1;
    ach_vel = 0;
    ach_drag_frame_d = 0;
    ach_drag_moved = 0;
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
        ach_scroll_reset();
        return;
    }
    double mx = ach_scroll_max();
    ach_scroll = clamp(ach_scroll, 0, mx);
    if (ach_drag_id >= 0) {
        if (ach_drag_moved == 0) {
            ach_vel = ach_vel * exp(-22.0 * dt);
            if (fabs(ach_vel) < 8.0) {
                ach_vel = 0;
            }
        }
        ach_drag_frame_d = 0;
        ach_drag_moved = 0;
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
        if (ach_drag_id < 0 || id == ach_drag_id) {
            ach_scroll_reset();
        }
        return;
    }
    if (action == 2) {
        if (ach_drag_id < 0 || id != ach_drag_id) {
            return;
        }
        press_points_drop(id);
        double d = ach_drag_y - y;
        ach_drag_y = y;
        if (fabs(d) < 1e-6) {
            return;
        }
        if (ach_scroll_needed() == 1) {
            ach_scroll_by(d);
            ach_drag_frame_d = ach_drag_frame_d + d;
            ach_drag_moved = 1;
            if (fabs(d) <= 4.5 && fabs(ach_vel) > 120.0 && d * ach_vel < 0) {
                ach_vel = ach_vel * 0.85;
            } else {
                double v = clamp(ach_drag_frame_d / fmax(dt, 0.008) * 0.6, -900.0, 900.0);
                if (v * ach_vel <= 0 || fabs(v) >= fabs(ach_vel)) {
                    ach_vel = v;
                } else {
                    ach_vel = ach_vel * 0.4 + v * 0.6;
                }
            }
        }
        return;
    }
    if (action == 1) {
        if (id == ach_drag_id) {
            ach_drag_id = -1;
            ach_drag_frame_d = 0;
            ach_drag_moved = 0;
        }
        return;
    }
    if (ach_drag_id >= 0 && id != ach_drag_id) {
        press_points_drop(id);
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
        ach_drag_frame_d = 0;
        ach_drag_moved = 0;
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
    ds_gfx_scissor(ach_x() - 2, vt, ach_w() + 4, vb - vt);
    double vi = 0;
    while (vi < count) {
        double cy = vt + vi * ach_step() - ach_scroll;
        if (cy + ach_card_h > vt && cy < vb) {
            draw_achievement_card_at(ach_visible_item(vi), cy);
        }
        vi = vi + 1;
    }
    ds_gfx_scissor(0, 0, 0, 0);
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
