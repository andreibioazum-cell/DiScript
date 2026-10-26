static void start_transition(double target) {
    if (t_dir != 0) {
        return;
    }
    classes_swipe_reset();
    ach_scroll_reset();
    rooms_scroll_reset();
    acct_gift_scroll_reset();
    t_target = target;
        /* Hold briefly before fade-out so button squeeze remains visible. */
        t_hold = transition_hold;
    t_dir = 1;
}

static void start_action_transition(void (*action)(void)) {
    if (t_dir != 0) {
        return;
    }
    t_action = action;
    start_transition(game_state);
}

static void leave_screen(double state) {
    if (acct_open != 0) {
        acct_close();
    }
    if (state == ST_LOGIN || state == ST_PROMO || state == ST_ROOMS) {
        keyboard_hide();
    }
    if (state == ST_ONLINE) {
        chat_set_open(0);
        net_disconnect();
        event_mode = 0;
        event_t = 0;
    }
    if (state == ST_SOLO || state == ST_ONLINE) {
        ds_gfx_gray(0);
        ds_gfx_shake(0, 0);
        save_progress();
    }
}

static double enter_screen(double state) {
    if (state == ST_LOBBY && acct_pending == 1) {
        acct_pending = 0;
        acct_show(ACCT_MENU);
        return 1;
    }
    if (state == ST_SOLO) {
        init_game();
        return 1;
    }
    if (state == ST_LOGIN) {
        login_open();
        return 1;
    }
    if (state == ST_CLASSES) {
        /* Refresh live server flags on shop entry. */
        net_live_flags_refresh();
        return 1;
    }
    if (state == ST_ROOMS) {
        rooms_open();
        return 1;
    }
    if (state == ST_LEADERBOARD) {
        open_leaderboard();
        return 1;
    }
    if (state == ST_ACHIEVEMENTS) {
        ach_open();
        return 1;
    }
    if (state == ST_ONLINE) {
        if (net_is_banned(net_login_nick()) == 1) {
            banned_msg = tr_banned();
            game_state = ST_LOBBY;
            ui_nav_gen = ui_nav_gen + 1;
            press_pulses_clear();
            t_dir = -1;
            t_fade = 1;
            return 0;
        }
        init_online();
    }
    return 1;
}

static void update_transition(void) {
    if (t_dir == 1) {
        if (t_hold > 0) {
            t_hold = t_hold - dt;
            if (t_hold < 0) {
                t_hold = 0;
            }
            return;
        }
        t_fade = t_fade + dt / transition_duration;
        if (t_fade >= 1) {
            t_fade = 1;
            t_dir = 2;
        }
        return;
    }
    if (t_dir == 2) {
        if (t_action) {
            void (*action)(void) = t_action;
            t_action = NULL;
            action();
            ui_nav_gen = ui_nav_gen + 1;
            /* A new screen must not inherit a squeeze from the old one. */
            press_pulses_clear();
            t_dir = -1;
            return;
        }
        leave_screen(game_state);
        game_state = t_target;
        ui_nav_gen = ui_nav_gen + 1;
        press_pulses_clear();
        if (enter_screen(t_target) == 1) {
            t_dir = -1;
        }
        return;
    }
    if (t_dir == -1) {
        t_fade = t_fade - dt / transition_duration;
        if (t_fade <= 0) {
            t_fade = 0;
            t_dir = 0;
        }
    }
}

