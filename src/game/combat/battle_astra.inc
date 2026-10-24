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

static double astra_throw_dist_for(double lv) {
    if (lv >= 3) {
        return astra_throw_dist * (1 + astra_level_throw_bonus);
    }
    return astra_throw_dist;
}

static double astra_throw_stun_for(double lv) {
    if (lv >= 3) {
        return astra_throw_stun + astra_level_stun_bonus;
    }
    return astra_throw_stun;
}

static double astra_zone_live(double t, double caught) { return astra_ability_zone_live_for(t, caught, 0); }

static void astra_swing_now(void) { astra_punch_t = astra_hit_show; }

static void tick_astra_poses(void) {
    astra_punch_t = tick_down(astra_punch_t);
    egrab_punch_t = tick_down(egrab_punch_t);
    double s = 0;
    while (s < max_players) {
        arr_set(rgrab_punch_t, s, tick_down(arr_get(rgrab_punch_t, s)));
        s = s + 1;
    }
}

static void grab_quiet_player(double t) {
    if (t > player_grab_quiet) {
        player_grab_quiet = t;
    }
}

static void grab_quiet_enemy(double t) {
    if (t > enemy_grab_quiet) {
        enemy_grab_quiet = t;
    }
}

static void tick_grab_quiet(void) {
    player_grab_quiet = tick_down(player_grab_quiet);
    enemy_grab_quiet = tick_down(enemy_grab_quiet);
}

static double astra_beat_damage(double max_hp) { return max_hp * astra_hit_fraction; }

static double astra_final_damage(double max_hp) { return max_hp * astra_final_fraction; }

static double astra_zone_hits_player(double gx, double gy, double gdx, double gdy, double lv) {
    if (target_is_forward(gx, gy, gdx, gdy, player->x, player->y) == 0) {
        return 0;
    }
    return strip_hits_player(gx, gy, gdx, gdy, astra_grab_forward_offset,
                             astra_grab_forward_offset + astra_zone_reach(lv), astra_zone_width(lv) / 2);
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
    if (astra_state == 1 || astra_state == 2) {
        return 1;
    }
    return 0;
}

#define ASTRA_PARRY_TIME 0.4
#define ASTRA_GRAB_WINDOW 0.8

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
    queued_super = 0;
    queued_punch = 0;
    punch->active = 0;
    punch->hit = 0;
    punch_left = 0;
    santa_follow = 0;
    ds_log("astra purple shield cast");
}

static void astra_on_parry_block(void) {
    astra_parry_blocked = 1;
    if (astra_grab_ready_t <= 0) {
        astra_grab_ready_t = ASTRA_GRAB_WINDOW;
        ds_log("astra parry unlocked grab");
    }
}

static double player_grabbed(void) { return pgrab_active; }

static void grab_cancel_actions(void) {
    punch->active = 0;
    punch->hit = 0;
    punch_left = 0;
    queued_punch = 0;
    queued_super = 0;
    super_windup = 0;
    santa_follow = 0;
    dash_active = 0;
    dash_t = 0;
    dash_hit = 0;
    astra_parry_t = 0;
    astra_parry_blocked = 0;
    astra_grab_ready_t = 0;
}

static void start_grab_now(void) {
    /* Start forward lunge and lock grab origin at its endpoint. */
    astra_parry_t = 0;
    astra_parry_blocked = 0;
    astra_grab_ready_t = 0;
    astra_ability_lunge_start();
    ability_snd(GRAB_SOUND);
    astra_state = 1;
    astra_t = 0;
    astra_hits_done = 0;
    astra_caught = 0;
    astra_x = astra_ability_lunge_tx;
    astra_y = astra_ability_lunge_ty;
    astra_dx = cos(player->angle);
    astra_dy = sin(player->angle);
    astra_seq = astra_seq + 1;
    astra_pub = 0;
    queued_super = 0;
    /* Trigger punch pose immediately only when lunge duration is zero. */
    if (astra_ability_lunge_time_for(astra_ability_local()) == 0) {
        astra_swing_now();
    } else {
        astra_punch_t = 0;
    }
    punch->active = 0;
    punch->hit = 0;
    punch_left = 0;
    santa_follow = 0;
    ds_log("astra grab cast");
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

static void throw_player(double dx, double dy, double dist, double stun_time) {
    if (dist <= 0 || astra_throw_time <= 0) {
        return;
    }
    pthrow_dx = dx;
    pthrow_dy = dy;
    pthrow_speed = dist / astra_throw_time;
    pthrow_t = astra_throw_time;
    stun_player(astra_throw_time + stun_time);
    grab_quiet_player(astra_throw_time + stun_time);
}

static void throw_enemy(double dx, double dy, double dist, double stun_time) {
    if (dist <= 0 || astra_throw_time <= 0) {
        return;
    }
    enemy_throw_dx = dx;
    enemy_throw_dy = dy;
    enemy_throw_speed = dist / astra_throw_time;
    enemy_throw_t = astra_throw_time;
    stun_enemy(astra_throw_time + stun_time);
    grab_quiet_enemy(astra_throw_time + stun_time);
}

static void tick_player_throw(void) {
    if (pthrow_t <= 0) {
        return;
    }
    double step = dt;
    if (step > pthrow_t) {
        step = pthrow_t;
    }
    player->x = player->x + pthrow_dx * pthrow_speed * step;
    player->y = player->y + pthrow_dy * pthrow_speed * step;
    if (player->x < player->size) {
        player->x = player->size;
    }
    if (player->x > screen_w - player->size) {
        player->x = screen_w - player->size;
    }
    if (player->y < player->size) {
        player->y = player->size;
    }
    if (player->y > screen_h - player->size) {
        player->y = screen_h - player->size;
    }
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
    enemy->x = enemy->x + enemy_throw_dx * enemy_throw_speed * step;
    enemy->y = enemy->y + enemy_throw_dy * enemy_throw_speed * step;
    if (enemy->x < m) {
        enemy->x = m;
    }
    if (enemy->x > screen_w - m) {
        enemy->x = screen_w - m;
    }
    if (enemy->y < m) {
        enemy->y = m;
    }
    if (enemy->y > screen_h - m) {
        enemy->y = screen_h - m;
    }
    enemy_throw_t = tick_down(enemy_throw_t);
}

static double astra_try_catch(void) {
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
                    ds_log("astra grab caught slot %g", s);
                    return 1;
                }
            }
            s = s + 1;
        }
        return 0;
    }
    if (enemy->hp <= 0 || enemy_revive_prot > 0) {
        return 0;
    }
    if (astra_zone_hits_enemy(astra_x, astra_y, astra_dx, astra_dy, player_level) == 1) {
        astra_caught = 1;
        ds_log("astra grab caught the bot");
        return 1;
    }
    return 0;
}

static double astra_beat_solo_damage(double dmg) {
    if (game_state == ST_ONLINE) {
        return 0;
    }
    if (enemy_revive_prot > 0) {
        return 0;
    }
    return enemy_apply_damage(dmg);
}

static double astra_beat_solo(void) { return astra_beat_solo_damage(astra_beat_damage(enemy->max_hp)); }

static double astra_final_solo(void) {
    if (game_state == ST_ONLINE) {
        return 0;
    }
    if (astra_beat_solo_damage(astra_final_damage(enemy->max_hp)) == 1) {
        return 1;
    }
    if (enemy->hp <= 0) {
        return 1;
    }
    throw_enemy(astra_dx, astra_dy, astra_ability_throw_dist_for(player_level, astra_ability_local()),
                astra_ability_throw_stun_for(player_level, astra_ability_local()));
    ds_log("astra throw");
    return 0;
}

static void tick_astra(void) {
    double kit = astra_ability_local();
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
    astra_t = astra_t + dt;
    if (astra_caught == 0) {
        if (astra_t >= astra_ability_lunge_time_for(kit) && astra_try_catch() == 1) {
            astra_state = 2;
        } else if (astra_t > astra_ability_catch_time_for(kit)) {
            astra_release(0);
            return;
        }
    }
    if (astra_caught != 1) {
        return;
    }
    if (game_state != ST_ONLINE) {
        if (enemy->hp <= 0) {
            astra_release(1);
            return;
        }
        stun_enemy(astra_ability_hit_interval_for(kit));
        grab_quiet_enemy(astra_ability_hit_interval_for(kit));
    }
    double due = astra_ability_beats_done_for(astra_t, kit);
    while (astra_hits_done < due) {
        astra_hits_done = astra_hits_done + 1;
        astra_swing_now();
        if (astra_beat_solo() == 1) {
            astra_release(1);
            return;
        }
    }
    if (astra_t >= astra_ability_final_time_for(kit)) {
        astra_swing_now();
        /* Keep final hit zone visible for fade-out after release. */
        astra_box_a = 1;
        astra_final_solo();
        astra_release(1);
    }
}
