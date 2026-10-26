static double disco_color(double phase) {
    double r = 127 + 127 * sin(phase);
    double g = 127 + 127 * sin(phase + 2.094);
    double b = 127 + 127 * sin(phase + 4.188);
    double a = 55 + 90 * (0.5 + 0.5 * sin(disco_t * 5));
    return ((((unsigned)(int)a) << 24) | (((unsigned)(int)r) << 16) | (((unsigned)(int)g) << 8) | ((unsigned)(int)b));
}

static void draw_disco(void) {
    if (event_mode != 1) {
        return;
    }
    double col = disco_color(disco_t * 6);
    rect(0, 0, screen_w, screen_h, col);
    double ct = disco_t * 4;
    double r = 110 + 40 * sin(ct);
    ring(screen_w / 2, screen_h / 2, r, 7, 0xC0FFFFFF);
}

static void update_event(void) {
    /* Event numbering: 0 off, 1 disco, 2 plates, 3 Ded Moroz, 4 cube. */
    event_mode = net_event();
    if (plates_round_on() == 1 && candies_on() == 0) {
        event_mode = 0;
    }
    event_t = event_t + dt;
    if (event_t > 100000) {
        event_t = 0;
    }
    if (event_mode == 1) {
        disco_t = disco_t + dt;
    } else {
        disco_t = 0;
    }
    tick_plates_event();
    update_dice_event();
    /* Unlock ebuC's gift achievement when playing Santa online during event 3. */
    if (game_state == ST_ONLINE && event_mode == 3 && player_class == CLASS_SANTA && online_ready == 1) {
        if (has_achievement(ACH_BUK_GIFT) == 0) {
            grant_achievement(ACH_BUK_GIFT);
        }
    }
}

static double banner_named_color(const char *name) {
    if (str_eq(name, "red") == 1) {
        return 0x00FF5252;
    }
    if (str_eq(name, "green") == 1) {
        return 0x0000E676;
    }
    if (str_eq(name, "blue") == 1) {
        return 0x004FC3F7;
    }
    if (str_eq(name, "yellow") == 1) {
        return 0x00FFD54A;
    }
    if (str_eq(name, "orange") == 1) {
        return 0x00FF9800;
    }
    if (str_eq(name, "purple") == 1) {
        return 0x00AB47BC;
    }
    if (str_eq(name, "pink") == 1) {
        return 0x00FF80AB;
    }
    if (str_eq(name, "cyan") == 1) {
        return 0x0000E5FF;
    }
    if (str_eq(name, "black") == 1) {
        return 0x00222222;
    }
    return 0x00FFFFFF;
}

static double banner_color_now(const char *name) {
    return banner_named_color(name);
}

static void update_banner(void) {
    if (game_state != ST_ONLINE) {
        return;
    }
    double ts = net_banner_ts();
    if (ts <= 0) {
        /* Hide banner immediately when cleared on server. */
        banner_t = 0;
        return;
    }
    if (ts != banner_seen_ts) {
        banner_seen_ts = ts;
        banner_t = banner_show_time;
    }
    if (banner_t > 0) {
        double before = banner_t;
        banner_t = tick_down(banner_t);
        if (before > 0 && banner_t <= 0) {
            /* Clear server banner node once fade-out completes. */
            net_banner_clear();
        }
    }
}

static void draw_banner(void) {
    if (banner_t <= 0) {
        return;
    }
    const char *txt = net_banner_text();
    if (str_len(txt) == 0) {
        return;
    }
    double elapsed = banner_show_time - banner_t;
    double a = elapsed;
    if (a > banner_t) {
        a = banner_t;
    }
    double alpha = floor(clamp(a / banner_fade, 0, 1) * 230);
    if (alpha <= 0) {
        return;
    }
    double sc = 1.05;
    double w = text_ink_width(txt) * sc;
    if (w > screen_w - 40) {
        sc = (screen_w - 40) / w;
        w = text_ink_width(txt) * sc;
    }
    double x = (screen_w - w) / 2;
    double y = 118;
    uint32_t col = banner_color_now(net_banner_color());
    text_scaled(txt, x, y, ((((unsigned)(int)alpha) << 24) | col), sc);
}

static void draw_overhead_nick(const char *nick, double cx, double top_y, uint32_t col) {
    double w = text_ink_width(nick) * 0.5;
    double th = text_ink_height(nick) * 0.5;
    double tt = text_ink_top(nick) * 0.5;
    if (nick_shimmers(nick) == 1) {
        text_shimmer(nick, cx - w / 2, top_y - 12 - th - tt, col, 0.5, gift_shimmer_time());
        return;
    }
    text_scaled(nick, cx - w / 2, top_y - 12 - th - tt, col, 0.5);
}

static void draw_online(void) {
    if (chat_open == 1) {
        draw_chat();
        return;
    }
    resolve_fighter_looks();
    motion_blur_track();
    draw_arena_background();
    draw_disco();
    draw_dice_event();
    draw_field();
    draw_event_plates();
    draw_event_candies();
    draw_dust();
    draw_player_shadow_layer();
    double i = 0;
    double b = 0;
    double rb = 0;
    double rx = 0;
    double ry = 0;
    double ra = 0;
    double punching = 0;
    while (i < max_players) {
        b = i * remote_fields;
        rb = i * punch_fields;
        if (arr_get(remotes, b + 5) == 1) {
            rx = arr_get(remotes, b + 1);
            ry = arr_get(remotes, b + 2);
            ra = arr_get(remotes, b + 3);
            punching = 0;
            if (arr_get(remote_punches, rb) > 0) {
                punching = 1;
                ra = atan2(arr_get(remote_punches, rb + 4), arr_get(remote_punches, rb + 3));
            } else if (remote_grab_punching(i) == 1 || remote_dash_get(i, DASH_ACTIVE) == 1) {
                punching = 1;
            }
            double rsh = astra_strike_shift(remote_grab_get(i, GRAB_ACTIVE) == 1, remote_grab_get(i, GRAB_T));
            draw_fighter_shadow(rx + cos(ra) * rsh, ry + sin(ra) * rsh, ra, fighter_look_of(FLOOK_REMOTE + i),
                                fighter_pose_of(FLOOK_REMOTE + i, punching));
        }
        i = i + 1;
    }
    draw_turret_shadows();
    draw_player_hitbox_layer();
    if (show_hitboxes == 1) {
        i = 0;
        while (i < max_players) {
            rb = i * punch_fields;
            if (arr_get(rpunch_a, i) > 0) {
                draw_punch_box(arr_get(remote_punches, rb + 1), arr_get(remote_punches, rb + 2),
                               arr_get(remote_punches, rb + 3), arr_get(remote_punches, rb + 4), punch_reach,
                               punch_width, arr_get(rpunch_a, i));
            }
            i = i + 1;
        }
    }
    draw_ability_hitboxes();
    draw_ball();
    draw_remote_bricks();
    draw_gift();
    draw_boom();
    draw_remote_snow();
    draw_universe();
    draw_remote_universe();
    draw_station();
    draw_remote_station();
    draw_codecracker_fx_local();
    draw_player_body_layer();
    double hw = 25 * cube_scale * player_scale;
    const char *myn = net_player_nick(net_slot());
    if (str_len(myn) > 0) {
        uint32_t ncol = admin_head_color_of(myn, 0xFFFFFF);
        draw_overhead_nick(myn, player->x, player->y - hw, ncol);
    }
    i = 0;
    while (i < max_players) {
        b = i * remote_fields;
        rb = i * punch_fields;
        if (arr_get(remotes, b + 5) == 1) {
            rx = arr_get(remotes, b + 1);
            ry = arr_get(remotes, b + 2);
            ra = arr_get(remotes, b + 3);
            punching = 0;
            if (arr_get(remote_punches, rb) > 0) {
                punching = 1;
                ra = atan2(arr_get(remote_punches, rb + 4), arr_get(remote_punches, rb + 3));
            } else if (remote_grab_punching(i) == 1 || remote_dash_get(i, DASH_ACTIVE) == 1) {
                punching = 1;
            }
            draw_codecracker_fx_slot(i, rx, ry, ra);
            double rsh = astra_strike_shift(remote_grab_get(i, GRAB_ACTIVE) == 1, remote_grab_get(i, GRAB_T));
            draw_fighter_body(rx + cos(ra) * rsh, ry + sin(ra) * rsh, ra, fighter_look_of(FLOOK_REMOTE + i),
                              fighter_pose_of(FLOOK_REMOTE + i, punching));
            double remote_tint = codecracker_tint_slot(i);
            draw_fighter_tint(rx + cos(ra) * rsh, ry + sin(ra) * rsh, ra, fighter_look_of(FLOOK_REMOTE + i),
                              fighter_pose_of(FLOOK_REMOTE + i, punching), player_scale, remote_tint);
            draw_guard(rx, ry, ra, net_player_block(i), player_scale);
            draw_purple_guard(rx, ry,
                              class_is_astra(arr_get(remotes, b + 10)) == 1 && net_player_codecracker_super(i) == 1,
                              player_scale);
            const char *rn = net_player_nick(i);
            if (str_len(rn) > 0) {
                uint32_t rncol = admin_head_color_of(rn, 0xFFFFFF);
                draw_overhead_nick(rn, rx, ry - hw, rncol);
            }
        }
        i = i + 1;
    }
    draw_event_santa();
    draw_chat_bubbles();
    draw_status_aura(player->x, player->y, player->size, freeze_rgb, freeze_rim, pfreeze_a);
    draw_status_aura(player->x, player->y, player->size, poison_rgb, poison_rim, ppoison_a);
    draw_status_aura(player->x, player->y, player->size, stun_rgb, stun_rim, pstun_a);
    draw_snow();
    draw_screen_fx();
    double w = screen_w / 3;
    hud_bar(20, 20, w, 28, player->hp, player->max_hp, 0x7ED321, 0);
    draw_controls();
    draw_hurt_flash();
    draw_fps_meter();
    draw_chat();
    draw_plates_hud();
    draw_alone_wait();
    if (finished == 1) {
        draw_result(tr_victory());
    } else if (finished == 2) {
        draw_result(tr_defeat());
    }
    draw_banner();
}

static void drop_joy(void) {
    joy_id = -1;
    joy.dx = 0;
    joy.dy = 0;
    joy.ox = 0;
    joy.oy = 0;
}

static void joy_touch(double x, double y, double action, double id) {
    if (action == 1) {
        if (id == joy_id) {
            drop_joy();
        }
        return;
    }
    if (action == 0) {
        if (joy_id != -1) {
            return;
        }
        if (sqrt((x - joy.x) * (x - joy.x) + (y - joy.y) * (y - joy.y)) > 150) {
            return;
        }
        joy_id = id;
    }
    if (id != joy_id) {
        return;
    }
    double dx = x - joy.x;
    double dy = y - joy.y;
    double d = sqrt(dx * dx + dy * dy);
    double knob_d = d;
    if (knob_d > joy.r) {
        knob_d = joy.r;
    }
    if (d > 0) {
        joy.ox = dx * knob_d / d;
        joy.oy = dy * knob_d / d;
    } else {
        joy.ox = 0;
        joy.oy = 0;
    }
    if (d > joy.r * 0.15) {
        /* Normalize stick direction outside deadzone. */
        joy.dx = dx / d;
        joy.dy = dy / d;
    } else {
        joy.dx = 0;
        joy.dy = 0;
    }
}

static void touch_game(double x, double y, double action, double id) {
    if (finished != 0) {
        if (action == 0 && finish_t >= alert_hold) {
            start_transition(0);
        }
        return;
    }
    if (action == 3) {
        blk_id = -1;
        player_block = 0;
        drop_joy();
        return;
    }
    if (game_state == ST_ONLINE) {
        if (touch_chat(x, y, action) == 1) {
            if (chat_open == 1) {
                drop_joy();
            }
            return;
        }
        if (chat_open == 1) {
            return;
        }
    }
    /* Exit battle on back button press. */
    if (action == 0 && battle_back_hit(x, y) == 1) {
        start_transition(0);
        return;
    }
    /* Toggle ebuC's ultimate mode on mode-button press. */
    if (player_class == CLASS_EBUC && (game_state == ST_SOLO || game_state == ST_ONLINE) && action == 0 &&
        ult_mode_hit(x, y) == 1) {
        ult_mode_toggle();
        return;
    }
    /* Dispatch combat pad touches: punch/super on press, block while held. */
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        double pr = battle_pad_r();
        double dp = dist(x, y, atk_x, atk_y);
        double dsu = class_has_super(player_class) == 1 ? dist(x, y, atk2_x, atk2_y) : 1e9;
        double db = dist(x, y, blk_x, blk_y);
        if (action == 0 && db <= pr && db <= dp && db <= dsu) {
            blk_id = id;
            player_block = 1;
            return;
        }
        if (id == blk_id) {
            if (action == 1 || action == 3) {
                blk_id = -1;
                player_block = 0;
            }
            return;
        }
        if (class_has_super(player_class) == 1) {
            /* The ult pad belongs to its own finger and fires immediately on press. */
            if (action == 0 && dsu <= pr && dsu <= dp && (player_block == 0 || class_is_astra(player_class) == 1)) {
                if (player_class == CLASS_EBUC && ball_mode == 1) {
                    if (game_state == ST_SOLO || online_ready == 1) {
                        ball_fire();
                    }
                    return;
                }
                if (game_state == ST_ONLINE) {
                    if (online_ready == 1) {
                        fire_super();
                    }
                } else if (game_state == ST_SOLO) {
                    fire_super();
                }
                return;
            }
        }
        if (action == 0 && dp <= pr) {
            if (game_state == ST_SOLO || online_ready == 1) {
                fire_punch();
            }
            return;
        }
    }
    joy_touch(x, y, action, id);
}
