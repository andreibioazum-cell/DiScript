static double classes_vel;
static double classes_swipe_travel;
static ScrollTrack classes_track;

static void classes_swipe_reset(void) {
    classes_swipe_id = -1;
    classes_swipe_card = -1;
    classes_swipe_moved = 0;
    classes_vel = 0;
    classes_swipe_travel = 0;
    scroll_track_reset(&classes_track);
}

static void classes_scroll_tick(void) {
    if (classes_scroll_needed() == 0) {
        classes_scroll = 0;
        classes_swipe_reset();
        return;
    }
    classes_scroll_clamp();
    if (classes_swipe_id >= 0) {
        classes_vel = 0;
        scroll_track_tick(&classes_track);
        return;
    }
    if (fabs(classes_vel) < 0.6) {
        classes_vel = 0;
        return;
    }
    double mx = classes_scroll_max();
    classes_scroll = classes_scroll + classes_vel * dt;
    classes_vel = classes_vel * exp(-4.8 * dt);
    if (classes_scroll <= 0) {
        classes_scroll = 0;
        classes_vel = 0;
    } else if (classes_scroll >= mx) {
        classes_scroll = mx;
        classes_vel = 0;
    }
}

static double classes_swipe_threshold(void) { return clamp(screen_w * 0.08, 48, 88); }

static double classes_button_hit(double x, double y) {
    return back_hit(x, y) == 1 || hit_levels_btn(x, y) == 1 || hit_skins_btn(x, y) == 1;
}

static void touch_classes(double x, double y, double action, double id) {
    if (action == 3) {
        if (classes_swipe_id < 0 || id == classes_swipe_id) classes_swipe_reset();
        return;
    }
    if (action == 2) {
        if (classes_swipe_id != id || classes_swipe_id == -1) {
            return;
        }
        double d = classes_swipe_last_x - x;
        classes_swipe_last_x = x;
        if (fabs(d) < 1e-6) return;
        classes_swipe_travel = classes_swipe_travel + fabs(d);
        if (classes_scroll_needed() == 1) {
            classes_scroll_by(d);
            scroll_track_move(&classes_track, d);
            if (classes_swipe_travel > 12 || fabs(x - classes_swipe_start_x) > classes_swipe_threshold() * 0.35) {
                classes_swipe_moved = 1;
                press_points_drop(id);
            }
        }
        return;
    }
    if (action == 1) {
        if (classes_swipe_id != id) return;
        double vi = classes_swipe_card;
        double moved = classes_swipe_moved;
        classes_swipe_id = -1;
        classes_swipe_card = -1;
        classes_swipe_moved = 0;
        classes_swipe_travel = 0;
        classes_vel = classes_scroll_needed() == 1 ? scroll_track_release(&classes_track, 2400.0) : 0;
        if (moved == 0 && vi >= 0 && hit_class_card(x, y, vi) == 1) {
            classes_vel = 0;
            pick_class(visible_class_at(vi));
        }
        return;
    }
    if (action != 0) return;
    if (classes_swipe_id >= 0 && id != classes_swipe_id) {
        press_points_drop(id);
        return;
    }
    /* action == 0: every button acts on the touch itself. */
    if (classes_button_hit(x, y) == 1) {
        classes_swipe_reset();
        play_button_click();
        if (back_hit(x, y) == 1) {
            start_transition(ST_LOBBY);
        } else if (hit_levels_btn(x, y) == 1) {
            start_transition(ST_LEVELS);
        } else {
            start_transition(ST_SKINS);
        }
        return;
    }
    double vi = 0;
    double last = classes_visible_count();
    while (vi < last) {
        if (hit_class_card(x, y, vi) == 1) {
            classes_vel = 0;
            scroll_track_reset(&classes_track);
            if (hit_class_pill(x, y, vi) == 1) {
                play_button_click();
                pick_class(visible_class_at(vi));
            } else {
                play_button_click();
                classes_swipe_id = id;
                classes_swipe_start_x = x;
                classes_swipe_start_y = y;
                classes_swipe_last_x = x;
                classes_swipe_moved = 0;
                classes_swipe_travel = 0;
                classes_swipe_card = vi;
            }
            return;
        }
        vi = vi + 1;
    }
    if (classes_swipe_zone(x, y) == 1) {
        classes_vel = 0;
        scroll_track_reset(&classes_track);
        classes_swipe_id = id;
        classes_swipe_start_x = x;
        classes_swipe_start_y = y;
        classes_swipe_last_x = x;
        classes_swipe_moved = 0;
        classes_swipe_travel = 0;
        classes_swipe_card = -1;
    }
}


static double menu_button_hit(double x, double y) {
    double i = 0;
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        /* Play UI click sound only on battle UI buttons, not joysticks or combat pads. */
        if (finished != 0) {
            return finish_t >= alert_hold;
        }
        if (game_state == ST_ONLINE && chat_open == 1) {
            return chat_button_hit(x, y);
        }
        return battle_back_hit(x, y);
    }
    if (game_state == ST_LOBBY) {
        while (i <= 4) {
            if (hit_menu_row(x, y, i) == 1) return 1;
            i = i + 1;
        }
        return bpass_allowed() == 1 ? hit_bpass_corner(x, y) : 0;
    }
    if (game_state == ST_MODES) {
        while (i <= 3) {
            if (hit_modes_row(x, y, i) == 1) return 1;
            i = i + 1;
        }
        if (hit_modes_row(x, y, 4) == 1) return 1;
        return hit_promo_corner(x, y);
    }
    if (game_state == ST_SETTINGS) {
        while (i <= 4) {
            if (hit_settings_row(x, y, i) == 1) return 1;
            i = i + 1;
        }
        return 0;
    }
    if (game_state == ST_CLASSES) {
        if (back_hit(x, y) == 1 || hit_levels_btn(x, y) == 1 || hit_skins_btn(x, y) == 1) return 1;
        double last = classes_visible_count();
        while (i < last) {
            if (hit_class_card(x, y, i) == 1) return 1;
            i = i + 1;
        }
        return 0;
    }
    if (game_state == ST_LEVELS) {
        if (back_hit(x, y) == 1) return 1;
        i = 1;
        while (i <= max_level) {
            if (hit_level_row(x, y, i) == 1) return 1;
            i = i + 1;
        }
        return 0;
    }
    if (game_state == ST_BPASS) {
        return bpass_any_hit(x, y);
    }
    if (game_state == ST_SKINS) {
        if (back_hit(x, y) == 1) return 1;
        while (i < SKIN_COUNT) {
            if (hit_skin_card(x, y, i) == 1) return 1;
            i = i + 1;
        }
        return 0;
    }
    if (game_state == ST_LOGIN) {
        return hit_login_btn(x, y) == 1 || back_hit(x, y) == 1 || hit_login_more(x, y) == 1 ||
               hit_login_consent(x, y) == 1;
    }
    if (game_state == ST_PROMO) {
        double w = fit_w(560);
        double fx = center_x(w);
        return back_hit(x, y) == 1 || hit_button(x, y, fx, promo_btn_y(), w, btn_h) == 1;
    }
    if (game_state == ST_ROOMS) {
        return rooms_any_hit(x, y);
    }
    if (game_state == ST_ACHIEVEMENTS || game_state == ST_LEADERBOARD) {
        return back_hit(x, y);
    }
    return 0;
}

static void touch_menu(double x, double y, double action, double id) {
    if (game_state == ST_CLASSES) {
        touch_classes(x, y, action, id);
        return;
    }
    if (game_state == ST_ACHIEVEMENTS) {
        touch_achievements(x, y, action, id);
        return;
    }
    if (game_state == ST_ROOMS) {
        touch_rooms(x, y, action, id);
        return;
    }
    if (action != 0) {
        return;
    }
    if (game_state == ST_MODES) {
        touch_modes(x, y);
        return;
    }
    if (game_state == ST_SETTINGS) {
        touch_settings(x, y);
        return;
    }
    if (game_state == ST_LOGIN) {
        touch_login(x, y, action);
        return;
    }
    if (game_state == ST_LEVELS) {
        touch_levels(x, y, action);
        return;
    }
    if (game_state == ST_BPASS) {
        touch_bpass(x, y, action);
        return;
    }
    if (game_state == ST_SKINS) {
        touch_skins(x, y, action);
        return;
    }
    if (game_state == ST_LEADERBOARD) {
        touch_leaderboard(x, y, action);
        return;
    }
    if (game_state == ST_PROMO) {
        touch_promo(x, y, action);
    }
}
