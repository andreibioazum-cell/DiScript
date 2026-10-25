/* Astra purple parry shield and stun counter-strike. */

#define ASTRA_PARRY_TIME 0.4
#define ASTRA_STRIKE_WINDOW 0.9
#define ASTRA_GRAB_WINDOW ASTRA_STRIKE_WINDOW

static double astra_zone_reach(double lv) {
    if (lv >= 2) {
        return astra_grab_reach * (1 + astra_level_zone_bonus);
    }
    return astra_grab_reach;
}

static double astra_zone_width(double lv) {
    if (lv >= 2) {
        return astra_grab_width * (1 + astra_level_zone_bonus);
    }
    return astra_grab_width;
}

static double astra_cd_for(double lv) {
    double c = astra_grab_cd;
    if (lv >= 1) {
        c = c - astra_level_cd_bonus;
    }
    if (c < 1) {
        c = 1;
    }
    return c;
}

static void astra_swing_now(void) { astra_punch_t = ASTRA_STRIKE_TIME; }

static void tick_astra_poses(void) {
    astra_punch_t = tick_down(astra_punch_t);
    egrab_punch_t = tick_down(egrab_punch_t);
    double s = 0;
    while (s < max_players) {
        arr_set(rgrab_punch_t, s, tick_down(arr_get(rgrab_punch_t, s)));
        s = s + 1;
    }
}

static void tick_grab_quiet(void) {
    player_grab_quiet = tick_down(player_grab_quiet);
    enemy_grab_quiet = tick_down(enemy_grab_quiet);
}

static double astra_zone_hits_enemy(double gx, double gy, double gdx, double gdy, double lv) {
    if (target_is_forward(gx, gy, gdx, gdy, enemy->x, enemy->y) == 0) {
        return 0;
    }
    return strip_hits_enemy(gx, gy, gdx, gdy, astra_grab_forward_offset,
                            astra_grab_forward_offset + astra_zone_reach(lv), astra_zone_width(lv) / 2);
}

static double astra_locked(void) {
    if (class_is_astra(player_class) == 0) {
        return 0;
    }
    if (astra_state == 1 && astra_ability_lunge_active() == 1) {
        return 1;
    }
    return 0;
}

static double astra_parry_active(void) {
    if (class_is_astra(player_class) == 1 && astra_parry_t > 0) {
        return 1;
    }
    return 0;
}

static void start_astra_parry_now(void) {
    astra_parry_t = ASTRA_PARRY_TIME;
    astra_parry_blocked = 0;
    astra_grab_ready_t = 0;
    player_freeze = 0;
    queued_super = 0;
    ds_log("astra purple shield cast");
}

static void astra_on_parry_block(void) {
    astra_parry_blocked = 1;
    astra_grab_ready_t = ASTRA_STRIKE_WINDOW;
    ds_log("astra parry unlocked strike");
}

static double player_grabbed(void) { return pgrab_active; }

static void start_grab_now(void) {
    /* Start short forward lunge and counter-strike swing. */
    astra_parry_t = 0;
    astra_parry_blocked = 0;
    astra_grab_ready_t = 0;
    astra_ability_lunge_start();
    punch_snd();
    astra_state = 1;
    astra_t = 0;
    astra_hits_done = 0;
    astra_caught = 0;
    astra_x = player->x;
    astra_y = player->y;
    astra_dx = cos(player->angle);
    astra_dy = sin(player->angle);
    astra_seq = astra_seq + 1;
    astra_pub = 0;
    queued_super = 0;
    astra_swing_now();
    punch->active = 0;
    punch->hit = 0;
    punch_left = 0;
    santa_follow = 0;
    ds_log("astra strike cast");
}

static void astra_release(double landed) {
    astra_state = 0;
    astra_t = 0;
    astra_caught = 0;
    astra_hits_done = 0;
    if (admin_cheat_nocd() == 1) {
        astra_cd = 0;
    } else if (landed == 1) {
        astra_cd = astra_cd_for(player_level);
    } else {
        astra_cd = astra_grab_miss_cd;
    }
}

static void tick_player_throw(void) {
    if (pthrow_t <= 0) {
        return;
    }
    double step = dt;
    if (step > pthrow_t) {
        step = pthrow_t;
    }
    player->x = clamp(player->x + pthrow_dx * pthrow_speed * step, player->size, screen_w - player->size);
    player->y = clamp(player->y + pthrow_dy * pthrow_speed * step, player->size, screen_h - player->size);
    pthrow_t = tick_down(pthrow_t);
}

static void tick_enemy_throw(void) {
    if (enemy_throw_t <= 0) {
        return;
    }
    double m = enemy->size;
    double step = dt;
    if (step > enemy_throw_t) {
        step = enemy_throw_t;
    }
    enemy->x = clamp(enemy->x + enemy_throw_dx * enemy_throw_speed * step, m, screen_w - m);
    enemy->y = clamp(enemy->y + enemy_throw_dy * enemy_throw_speed * step, m, screen_h - m);
    enemy_throw_t = tick_down(enemy_throw_t);
}

static double astra_strike_hit_check(void) {
    if (game_state == ST_ONLINE) {
        double s = 0;
        while (s < max_players) {
            double b = s * remote_fields;
            if (s != net_slot() && arr_get(remotes, b + 5) == 1) {
                double tx = arr_get(remotes, b + 1);
                double ty = arr_get(remotes, b + 2);
                if (target_is_forward(astra_x, astra_y, astra_dx, astra_dy, tx, ty) == 1 &&
                    strip_hits_box(astra_x, astra_y, astra_dx, astra_dy, astra_grab_forward_offset,
                                   astra_grab_forward_offset + astra_zone_reach(player_level),
                                   astra_zone_width(player_level) / 2, tx, ty, arr_get(remotes, b + 3),
                                   player_half()) == 1) {
                    astra_caught = 1;
                    astra_hits_done = 1;
                    astra_box_a = 1;
                    ds_log("astra strike hit slot %g", s);
                    return 0;
                }
            }
            s = s + 1;
        }
        return 0;
    }
    double turret_target = enemy_punch_turret_target(astra_x, astra_y, astra_dx, astra_dy);
    if (turret_target >= 1) {
        astra_caught = 1;
        astra_hits_done = 1;
        astra_box_a = 1;
        enemy_damage_turret(turret_target - 1, astra_strike_damage_for(player_level));
        if (revive_prot <= 0) {
            if (take_damage(turret_counter_damage * enemy_damage_mult_for(enemy_class)) == 1) {
                return 1;
            }
        }
        return 0;
    }
    if (enemy->hp <= 0 || enemy_revive_prot > 0) {
        return 0;
    }
    if (astra_zone_hits_enemy(astra_x, astra_y, astra_dx, astra_dy, player_level) == 1) {
        astra_caught = 1;
        astra_hits_done = 1;
        astra_box_a = 1;
        stun_enemy(astra_strike_stun_for(player_level));
        ds_log("astra strike stunned bot");
        return enemy_apply_damage_at(astra_strike_damage_for(player_level), player->x, player->y);
    }
    return 0;
}

static void tick_astra(void) {
    astra_cd = tick_down(astra_cd);
    if (admin_cheat_nocd() == 1) {
        astra_cd = 0;
    }
    tick_astra_poses();
    astra_ability_lunge_tick();
    tick_grab_quiet();
    if (game_state != ST_ONLINE) {
        tick_enemy_throw();
    }
    if (finished != 0 || player->hp <= 0) {
        astra_parry_t = 0;
        astra_parry_blocked = 0;
        astra_grab_ready_t = 0;
    } else {
        double had_parry = astra_parry_t > 0;
        astra_parry_t = tick_down(astra_parry_t);
        if (had_parry == 1 && astra_parry_t <= 0) {
            if (astra_parry_blocked == 0 && astra_grab_ready_t <= 0 && astra_state == 0) {
                astra_cd = admin_cheat_nocd() == 1 ? 0 : astra_grab_miss_cd;
            }
            astra_parry_blocked = 0;
        }
        double had_ready = astra_grab_ready_t > 0;
        astra_grab_ready_t = tick_down(astra_grab_ready_t);
        if (had_ready == 1 && astra_grab_ready_t <= 0 && astra_state == 0) {
            astra_parry_t = 0;
            astra_parry_blocked = 0;
            astra_cd = admin_cheat_nocd() == 1 ? 0 : astra_grab_miss_cd;
        }
    }
    if (astra_state == 0) {
        return;
    }
    if (finished != 0 || player->hp <= 0) {
        astra_release(1);
        return;
    }
    astra_x = player->x;
    astra_y = player->y;
    astra_dx = cos(player->angle);
    astra_dy = sin(player->angle);
    if (astra_caught == 0) {
        if (astra_strike_hit_check() == 1) {
            astra_release(1);
            return;
        }
    }
    astra_t = astra_t + dt;
    if (astra_t >= ASTRA_STRIKE_TIME) {
        astra_release(astra_caught);
    }
}
