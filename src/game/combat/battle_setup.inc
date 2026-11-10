static void reset_battle(void) {
    /* Refresh the developer permission once when a battle starts. */
    astra_ability_refresh();
    codecracker_refresh();
    codecracker_look_reset();
    player->x = screen_w / 2;
    player->y = screen_h / 2;
    player->angle = 0;
        /* Reset stored velocity at battle start. */
        player_vel_reset();
    reset_remote_smooth();
    ball_reset();
    christmas_reset();
    ice_reset();
    hack_reset();
    player->max_hp = astra_ability_max_hp_for(player_class, astra_ability_local());
    player->hp = player->max_hp;
    player->size = 25 * player_scale;
    enemy->size = 25 * enemy_scale;
    enemy->max_hp = enemy_base_hp * round_hp_mult();
    enemy->hp = enemy->max_hp;
    enemy->state = 0;
    enemy->state_time = 0;
    enemy->cooldown = enemy_first_delay;
    enemy->hit = 0;
    enemy->dodge_cd = 0;
    enemy->think = 0.2;
    enemy->mvx = 0;
    enemy->mvy = 0;
    enemy->svx = 0;
    enemy->svy = 0;
    enemy->freeze = 0;
    enemy->freeze_slow = 1;
    enemy->poison = 0;
    enemy->poison_tick = 0;
    enemy->poison_damage = 0;
    enemy->stun = 0;
    enemy->react = 0;
    enemy->speed_var = 1;
    enemy->aim_err = 0;
    enemy->hesit = 0;
    enemy_reset_class_state();
    finished = 0;
    finish_t = 0;
    punch->active = 0;
    punch->hit = 0;
    punch_left = 0;
    punch_cd = 0;
    aim_a = 0;
    santa_follow = 0;
    azum_revived = 0;
    revive_prot = 0;
    cups_awarded = 0;
    /* Mark battle stained if any admin cheat is active at start. */
    admin_battle_stain_reset();
    match_seen_foe = 0;
    foes_seen_t = 0;
    foes_gone_t = 0;
    alone_t = 0;
    gift->active = 0;
    gift->t = 0;
    gift->shot = 0;
    super_cd = 0;
    queued_punch = 0;
    queued_super = 0;
    boom_t = 0;
    boom_x = 0;
    boom_y = 0;
    freeze_a = 0;
    player_freeze = 0;
    player_freeze_slow = 1;
    pfreeze_a = 0;
    player_stun = 0;
    pstun_a = 0;
    player_was_hit = 0;
    mb_ok = 0;
    player_poison = 0;
    player_poison_tick = 0;
    player_poison_damage = 0;
    ppoison_a = 0;
    poison_a = 0;
    stun_a = 0;
    dash_active = 0;
    dash_t = 0;
    dash_dx = 0;
    dash_dy = 0;
    dash_hit = 0;
    dash_pub = 0;
    dash_x0 = 0;
    dash_y0 = 0;
    dash_cd = 0;
    dash_travel = 0;
    enemy_dash_travel = 0;
    enemy_punch_x = 0;
    enemy_punch_y = 0;
    enemy_punch_dx = 1;
    enemy_punch_dy = 0;
    dash_box_a = 0;
    astra_state = 0;
    astra_t = 0;
    astra_hits_done = 0;
    astra_caught = 0;
    astra_x = 0;
    astra_y = 0;
    astra_dx = 1;
    astra_dy = 0;
    astra_cd = 0;
    astra_seq = 0;
    astra_pub = 0;
    astra_box_a = 0;
    astra_parry_t = 0;
    astra_parry_blocked = 0;
    astra_strike_unlock_t = 0;
    astra_grab_ready_t = 0;
    pgrab_active = 0;
    pgrab_slot = -1;
    pthrow_t = 0;
    pthrow_dx = 0;
    pthrow_dy = 0;
    pthrow_speed = 0;
    enemy_throw_t = 0;
    enemy_throw_dx = 0;
    enemy_throw_dy = 0;
    enemy_throw_speed = 0;
    astra_punch_t = 0;
    player_grab_quiet = 0;
    enemy_grab_quiet = 0;
    snow_ball_a = 0;
    snow_boom_a = 0;
    universe_box_a = 0;
    universe_fx_a = 0;
    edash_box_a = 0;
    esnow_ball_a = 0;
    esnow_boom_a = 0;
    enemy_punch_a = 0;
    arr_clear(turret_x);
    arr_clear(turret_y);
    arr_clear(turret_hp);
    arr_clear(turret_maxhp);
    arr_clear(turret_spawn_t);
    arr_clear(turret_box_a);
    double i = 0;
    while (i < turret_max) {
        arr_push(turret_x, 0);
        arr_push(turret_y, 0);
        arr_push(turret_hp, 0);
        arr_push(turret_maxhp, 0);
        arr_push(turret_spawn_t, 0);
        arr_push(turret_box_a, 0);
        i = i + 1;
    }
    turret_heal_t = 0;
    last_turret_slot = 0;
    arr_clear(station_boom_xs);
    arr_clear(station_boom_ys);
    arr_clear(station_boom_ts);
    universe_active = 0;
    universe_t = 0;
    universe_x = 0;
    universe_y = 0;
    universe_id = 0;
    event_mode = 0;
    event_t = 0;
    disco_t = 0;
    snow_t = 0;
    plates_prev_mode = 0;
    plates_event_reset();
    dice_event_reset();
    columns_phase = 0;
    columns_t = 0;
    columns_shot_active = 0;
    columns_shot_x = 0;
    columns_shot_y = screen_h / 2;
    columns_shot_cd = 0;
    columns_thrown = 0;
    columns_dy = 0;
    ds_gfx_gray(0);
    ds_gfx_shake(0, 0);
    player_block = 0;
    block_success_t = 0;
    blk_id = -1;
    codecracker_touch_cd_left = 0;
    codecracker_super_t = 0;
    super_cd = 0;
    arr_clear(codecracker_touch_cds);
    double cci = 0;
    while (cci < max_players * 2) {
        arr_push(codecracker_touch_cds, 0);
        cci = cci + 1;
    }
    enemy_block_left = 0;
    enemy_block_cd = 0;
    joy.x = 130;
    joy.y = screen_h - 150;
    joy.r = 70;
        /* Space combat pad centers by more than two radii to prevent overlap. */
        atk_x = screen_w - 132;
    atk_y = screen_h - 138;
    atk2_x = screen_w - 132;
    atk2_y = screen_h - 330;
    blk_x = screen_w - 258;
    blk_y = screen_h - 234;
    drop_joy();
    arr_clear(remotes);
    arr_clear(remote_punches);
    arr_clear(candy_x);
    arr_clear(candy_y);
    arr_clear(candy_t);
    arr_clear(candy_pick);
    arr_clear(candy_ang);
    arr_clear(remote_snow);
    arr_clear(remote_station);
    arr_clear(remote_universe);
    arr_clear(remote_dash);
    arr_clear(remote_grab);
    arr_clear(rgrab_box_a);
    arr_clear(rgrab_punch_t);
    arr_clear(fighter_looks);
    arr_clear(fighter_swings);
    arr_clear(rpunch_a);
    arr_clear(rdash_box_a);
    arr_clear(rsnow_ball_a);
    arr_clear(rsnow_boom_a);
    arr_clear(runiverse_box_a);
    arr_clear(runiverse_fx_a);
    arr_clear(rstation_box_a);
    i = 0;
    while (i < max_players * remote_fields) {
        arr_push(remotes, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players * punch_fields) {
        arr_push(remote_punches, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players * snow_fields) {
        arr_push(remote_snow, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players * station_fields) {
        arr_push(remote_station, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players * universe_fields) {
        arr_push(remote_universe, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players * dash_fields) {
        arr_push(remote_dash, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players * grab_fields) {
        arr_push(remote_grab, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players) {
        arr_push(rpunch_a, 0);
        arr_push(rdash_box_a, 0);
        arr_push(rsnow_ball_a, 0);
        arr_push(rsnow_boom_a, 0);
        arr_push(runiverse_box_a, 0);
        arr_push(runiverse_fx_a, 0);
        arr_push(rgrab_box_a, 0);
        arr_push(rgrab_punch_t, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players + FLOOK_REMOTE) {
        arr_push(fighter_looks, LOOK_NONE);
        arr_push(fighter_swings, 0);
        i = i + 1;
    }
    i = 0;
    while (i < max_players * turret_max) {
        arr_push(rstation_box_a, 0);
        i = i + 1;
    }
    arr_clear(dust);
    dust_cd = 0;
    enemy_dust_cd = 0;
    i = 0;
    while (i < dust_max * dust_fields) {
        arr_push(dust, 0);
        i = i + 1;
    }
    arr_clear(remote_dust_cd);
    i = 0;
    while (i < max_players) {
        arr_push(remote_dust_cd, 0);
        i = i + 1;
    }
    thud_seq = 0;
    arr_clear(remote_thuds);
    i = 0;
    while (i < max_players) {
        arr_push(remote_thuds, 0);
        i = i + 1;
    }
}

static void init_game(void) {
    reset_battle();
    enemy->hp = enemy->max_hp;
    spawn_enemy();
    spawn_all_candies();
}

static void init_online(void) {
    reset_battle();
    chat_set_open(0);
    chat_bubbles_reset();
    online_ready = 0;
    /* Reset seen-banner timestamp so active server banner displays on join. */
    banner_seen_ts = 0;
    banner_t = 0;
    progress_push_to_net();
    net_connect(FIREBASE, ROOM);
}

static void place_online(void) {
    double s = net_slot();
    double row = floor(s / 2);
    double col = s - row * 2;
    player->x = screen_w * (0.3 + 0.4 * col);
    player->y = screen_h * (0.35 + 0.3 * row);
    player_vel_reset();
    online_ready = 1;
}

static void spawn_enemy(void) {
    double margin = 70;
    double i = 0;
    while (i < 50) {
        enemy->x = margin + rand() % (screen_w - 140);
        enemy->y = margin + rand() % (screen_h - 140);
        if (dist(enemy->x, enemy->y, player->x, player->y) > 300) {
            i = 50;
        } else {
            i = i + 1;
        }
    }
    enemy->state = 0;
    enemy->state_time = 0;
    enemy->cooldown = enemy_first_delay;
    enemy->hit = 0;
    enemy->dodge_cd = 0;
    enemy->think = 0.3;
    enemy->mvx = 0;
    enemy->mvy = 0;
    enemy->svx = 0;
    enemy->svy = 0;
    enemy->react = enemy_human_react_min + (rand() % 100) / 100.0 * (enemy_human_react_max - enemy_human_react_min);
    enemy->speed_var = 0.85 + (rand() % 20) / 100.0;
    enemy->aim_err = 0;
    if (rand() % 2 == 0) {
        enemy->strafe = -1;
    } else {
        enemy->strafe = 1;
    }
    enemy_pick_random_class();
    ds_log("enemy spawned instantly");
}

static void candy_rand_pos(double i) {
    double m = candy_margin;
    double k = 0;
    double cx = 0;
    double cy = 0;
    double ang = 0;
    double rad = 0;
    double ang_deg = 0;
    double max_r = screen_w;
    if (screen_h < max_r) {
        max_r = screen_h;
    }
    max_r = max_r / 2 - m - 10;
    if (max_r < 100) {
        max_r = 100;
    }
    while (k < 40) {
        ang_deg = (rand() % 360 + rand() % 180 + (int)(i * 137) + (int)(k * 53)) % 360;
        ang = ang_deg * pi / 180;
        rad = 60 + rand() % (int)(max_r - 60 + 1);
        cx = screen_w / 2 + cos(ang) * rad;
        cy = screen_h / 2 + sin(ang) * rad;
        if (cx < m) {
            cx = m;
        }
        if (cx > screen_w - m) {
            cx = screen_w - m;
        }
        if (cy < m) {
            cy = m;
        }
        if (cy > screen_h - m) {
            cy = screen_h - m;
        }
        if (dist(cx, cy, player->x, player->y) < 80 || dist(cx, cy, enemy->x, enemy->y) < 80) {
            k = k + 1;
        } else {
            k = 40;
        }
    }
    arr_set(candy_x, i, cx);
    arr_set(candy_y, i, cy);
    arr_set(candy_ang, i, ang);
    arr_set(candy_t, i, 0);
}

static void spawn_all_candies(void) {
    if (candies_on() == 0) {
        return;
    }
    double i = 0;
    while (i < candy_count) {
        candy_rand_pos(i);
        i = i + 1;
    }
}

static double sprite_a(double a) {
    return a + pi / 2;
}
