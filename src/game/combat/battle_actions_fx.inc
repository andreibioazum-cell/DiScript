static void start_punch_now(void) {
    punch->x = player->x;
    punch->y = player->y;
    punch->dx = cos(player->angle);
    punch->dy = sin(player->angle);
    punch->active = 1;
    punch->shot = punch->shot + 1;
    punch->hit = 0;
    punch_left = punch_time;
    punch_cd = punch_cooldown;
    queued_punch = 0;
    chaos_punch_escape();
    punch_snd();
}

static double station_boom_visual_busy(void) {
    double i = 0;
    while (i < arr_len(station_boom_ts)) {
        if (arr_get(station_boom_ts, i) > 0) {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

static double ultimate_visual_busy(void) {
    if (super_windup > 0 || gift->active == 1 || universe_active == 1 || boom_t > 0) {
        return 1;
    }
    if (astra_locked() == 1) {
        return 1;
    }
    if (station_boom_visual_busy() == 1) {
        return 1;
    }
    double i = 0;
    while (i < turret_max) {
        if (arr_get(turret_spawn_t, i) > 0) {
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

static void fire_punch(void) {
    if (dash_active > 0 || player_stun > 0 || player_block == 1) {
        return;
    }
    if (astra_locked() == 1 || player_grabbed() == 1) {
        queued_punch = 0;
        return;
    }
    if (punch_cd > 0 || ultimate_visual_busy() == 1) {
        queued_punch = 1;
        return;
    }
    start_punch_now();
    if (player_class == 2) {
        santa_follow = santa_follow_delay;
    }
}

static void tick_santa_follow(void) {
    if (santa_follow <= 0) {
        return;
    }
    santa_follow = tick_down(santa_follow);
    if (santa_follow > 0) {
        return;
    }
    if (finished != 0 || player_class != 2) {
        return;
    }
    if (super_windup > 0 || gift->active == 1 || universe_active == 1 || boom_t > 0) {
        return;
    }
    start_punch_now();
}

static void start_dash_now(void) {
    dash_active = 1;
    dash_t = azum_dash_time;
    dash_dx = cos(player->angle);
    dash_dy = sin(player->angle);
    dash_hit = 0;
    dash_x0 = player->x;
    dash_y0 = player->y;
    dash_travel = 0;
    dash_cd = azum_dash_cd;
    dash_seq = dash_seq + 1;
    dash_pub = 0;
    dash_snd();
    queued_super = 0;
    punch->active = 0;
    punch->hit = 0;
    punch_left = 0;
    santa_follow = 0;
}

static void fire_super(void) {
    if (player_grabbed() == 1 || player_block == 1) {
        queued_super = 0;
        return;
    }
    if (class_is_astra(player_class) == 1) {
        if (astra_cd > 0 || astra_state != 0) {
            queued_super = 0;
            return;
        }
        if (player_stun > 0 || player_freeze > 0 || player_grabbed() == 1) {
            queued_super = 0;
            return;
        }
        if (punch_left > 0 || punch_cd > 0 || ultimate_visual_busy() == 1) {
            queued_super = 1;
            return;
        }
        queued_super = 0;
        start_grab_now();
        return;
    }
    if (player_class == CLASS_AZUM) {
        if (dash_cd > 0) {
            queued_super = 0;
            return;
        }
        if (player_stun > 0 || player_freeze > 0) {
            queued_super = 0;
            return;
        }
        if (punch_left > 0 || punch_cd > 0 || ultimate_visual_busy() == 1) {
            queued_super = 1;
            return;
        }
        queued_super = 0;
        start_dash_now();
        return;
    }
    if (player_class == CLASS_CODECRACKER) {
        if (codecracker_on() == 0 || super_cd > 0 || codecracker_super_t > 0) {
            queued_super = 0;
            return;
        }
        if (player_stun > 0 || player_freeze > 0) {
            queued_super = 0;
            return;
        }
        if (punch_left > 0 || punch_cd > 0 || ultimate_visual_busy() == 1) {
            queued_super = 1;
            return;
        }
        queued_super = 0;
        codecracker_super_t = codecracker_super_time;
        ability_snd(OVERCLOCK_SOUND);
                /* The cooldown does NOT start here: it starts when the dash (overclock)
                 * ends - see codecracker_tick_super_timer(). */
        return;
    }
    if (player_class != 2 && player_class != 3) {
        return;
    }
    if (super_cd > 0) {
        return;
    }
    if (punch_left > 0 || punch_cd > 0 || gift->active == 1 || universe_active == 1 || boom_t > 0 ||
        station_boom_visual_busy() == 1) {
        queued_super = 1;
        return;
    }
    if (super_windup > 0) {
        queued_super = 1;
        return;
    }
    santa_follow = 0;
    queued_super = 0;
    super_windup = punch_time;
    punch_left = punch_time;
    punch->active = 0;
    punch->hit = 0;
    ability_snd(player_class == CLASS_SANTA ? GIFT_SOUND : STATION_SOUND);
    if (player_class == CLASS_EBUC) {
        super_cd = ebuc_station_cd;
    } else {
        super_cd = super_cd_time;
    }
}

static void tick_super_windup(void) {
    if (super_windup <= 0) {
        return;
    }
    super_windup = super_windup - dt;
    if (super_windup > 0) {
        return;
    }
    super_windup = 0;
    if (finished != 0) {
        return;
    }
    if (player_class == 2) {
        gift->x = player->x;
        gift->y = player->y;
        gift->dx = cos(player->angle);
        gift->dy = sin(player->angle);
        gift->active = 1;
        gift->t = 0;
        gift->shot = gift->shot + 1;
        gift->pierce = 0;
        if (game_state == ST_ONLINE) {
            net_publish_snow(gift->x / screen_w, gift->y / screen_h, gift->dx, gift->dy, gift->shot);
        }
        return;
    }
    if (player_class == CLASS_EBUC) {
        place_turret();
        ds_log("turret placed, alive %g/%g", turret_count_alive(), turret_max);
    }
}

static void update_gift(void) {
    if (gift->active == 0) {
        return;
    }
    gift->t = gift->t + dt;
    gift->x = gift->x + gift->dx * super_speed * dt;
    gift->y = gift->y + gift->dy * super_speed * dt;
    double touched = 0;
    if (game_state == ST_ONLINE) {
        if (gift->t > super_range / super_speed) {
            touched = 1;
        }
    } else {
        snow_chip_enemy_turrets();
        if (circle_hits_enemy(gift->x, gift->y, snow_ball_r) == 1) {
            touched = 1;
        }
        if (gift->t > super_range / super_speed) {
            touched = 1;
        }
    }
    if (touched == 0) {
        return;
    }
    gift->active = 0;
    boom_x = gift->x;
    boom_y = gift->y;
    boom_t = boom_time;
    if (game_state != ST_ONLINE) {
        if (circle_hits_enemy(boom_x, boom_y, super_radius) == 1 && enemy_revive_prot <= 0) {
            freeze_enemy(santa_freeze_time_for(player_level), super_freeze_slow);
            enemy_apply_damage_at(santa_super_damage, boom_x, boom_y);
        }
    }
}

static void draw_gift(void) {
    if (gift->active == 0) {
        return;
    }
    double hw = 16;
    double rot = gift->t * 3;
    tex(gift->x - hw, gift->y - hw, SNOWFLAKE_TEX, rot, 0.5);
}

static void draw_boom_at_r(double x, double y, double bt, double rad) {
    if (bt <= 0) {
        return;
    }
    double p = 1 - bt / boom_time;
    double r = rad * p;
    double fade_in = clamp(p / boom_fade_in, 0, 1);
    double fade_out = clamp(bt / (boom_time * boom_fade_out), 0, 1);
    double a = floor(fade_in * fade_out * 255);
    if (a <= 0) {
        return;
    }
    double c0 = (((unsigned)(int)a) << 24) | 0x00B3E5FC;
    double c1 = (((unsigned)(int)a) << 24) | 0x00E1F5FE;
    ring(x, y, r, 9, c0);
    ring(x, y, r * 0.6, 6, c1);
    circle(x, y, rad * 0.3, c0);
}

static void draw_boom_at(double x, double y, double bt) {
    draw_boom_at_r(x, y, bt, super_radius);
}

static void draw_boom(void) {
    draw_boom_at(boom_x, boom_y, boom_t);
}
