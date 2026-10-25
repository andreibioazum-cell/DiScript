/* Bot Astra purple parry shield and stun counter-strike. */

static double enemy_grab_active(void) {
    if (egrab_state != 0) {
        return 1;
    }
    return 0;
}

static void enemy_grab_swing(void) { egrab_punch_t = fmax(punch_time, ASTRA_STRIKE_TIME); }

static double enemy_grab_punching(void) {
    if (egrab_punch_t > 0) {
        return 1;
    }
    return 0;
}

static void enemy_grab_clear(void) {
    egrab_state = 0;
    egrab_t = 0;
    egrab_hits = 0;
    egrab_punch_t = 0;
    egrab_parry_t = 0;
    egrab_ready_t = 0;
    if (pgrab_active == 1 && pgrab_slot == grab_slot_bot) {
        pgrab_active = 0;
        pgrab_slot = -1;
    }
}

static void enemy_start_astra_parry(void) {
    egrab_parry_t = ASTRA_PARRY_TIME;
    egrab_ready_t = 0;
    enemy->freeze = 0;
    enemy_block_left = 0;
    ds_log("bot astra purple shield cast");
}

static void enemy_on_astra_parry_block(void) {
    egrab_ready_t = ASTRA_STRIKE_WINDOW;
    ds_log("bot astra parry unlocked strike");
}

static void enemy_grab_stop(double landed) {
    egrab_state = 0;
    egrab_t = 0;
    egrab_hits = 0;
    egrab_parry_t = 0;
    egrab_ready_t = 0;
    if (finished != 0 || enemy->hp <= 0) {
        egrab_punch_t = 0;
    }
    if (pgrab_active == 1 && pgrab_slot == grab_slot_bot) {
        pgrab_active = 0;
        pgrab_slot = -1;
    }
    if (landed == 1) {
        enemy_super_cd = enemy_astra_cd_time;
    } else {
        enemy_super_cd = astra_grab_miss_cd;
    }
}

static void enemy_start_grab(void) {
    egrab_parry_t = 0;
    egrab_ready_t = 0;
    egrab_state = 1;
    egrab_t = 0;
    egrab_hits = 0;
    egrab_box_a = 0;
    enemy_grab_swing();
    punch_snd();
    egrab_x = enemy->x;
    egrab_y = enemy->y;
    egrab_dx = cos(enemy->angle);
    egrab_dy = sin(enemy->angle);
    ds_log("bot astra strike cast");
}

static void tick_enemy_grab(void) {
    if (finished != 0 || enemy->hp <= 0 || enemy_revive_prot > 0) {
        egrab_parry_t = 0;
        egrab_ready_t = 0;
        if (egrab_state != 0) {
            enemy_grab_stop(0);
        }
        return;
    }
    double had_parry = egrab_parry_t > 0;
    egrab_parry_t = tick_down(egrab_parry_t);
    if (had_parry == 1 && egrab_parry_t <= 0 && egrab_ready_t <= 0 && egrab_state == 0) {
        enemy_super_cd = astra_grab_miss_cd;
    }
    double had_ready = egrab_ready_t > 0;
    egrab_ready_t = tick_down(egrab_ready_t);
    if (egrab_ready_t > 0 && egrab_state == 0 && enemy->stun <= 0 &&
        player->hp > 0 && revive_prot <= 0 &&
        dist(enemy->x, enemy->y, player->x, player->y) <= enemy_astra_range) {
        enemy_face_player();
        enemy_start_grab();
    } else if (had_ready == 1 && egrab_ready_t <= 0 && egrab_state == 0) {
        egrab_parry_t = 0;
        enemy_super_cd = astra_grab_miss_cd;
    }
    if (egrab_state == 0) {
        return;
    }
    double prev_ex = enemy->x;
    double prev_ey = enemy->y;
    if (egrab_t < astra_ability_lunge_time && enemy->stun <= 0) {
        double rem = dt;
        double cur = egrab_t;
        if (cur < ASTRA_PULLBACK_TIME) {
            double s1 = rem;
            if (cur + s1 > ASTRA_PULLBACK_TIME) {
                s1 = ASTRA_PULLBACK_TIME - cur;
            }
            double bspd = ASTRA_PULLBACK_DIST / ASTRA_PULLBACK_TIME;
            enemy->x = clamp(enemy->x - egrab_dx * bspd * s1, enemy->size, screen_w - enemy->size);
            enemy->y = clamp(enemy->y - egrab_dy * bspd * s1, enemy->size, screen_h - enemy->size);
            cur = cur + s1;
            rem = rem - s1;
        }
        if (rem > 0 && cur < astra_ability_lunge_time) {
            double s2 = rem;
            if (cur + s2 > astra_ability_lunge_time) {
                s2 = astra_ability_lunge_time - cur;
            }
            double lspd = (astra_ability_lunge_dist + ASTRA_PULLBACK_DIST) / ASTRA_DASH_TIME;
            enemy->x = clamp(enemy->x + egrab_dx * lspd * s2, enemy->size, screen_w - enemy->size);
            enemy->y = clamp(enemy->y + egrab_dy * lspd * s2, enemy->size, screen_h - enemy->size);
            tick_enemy_dust(egrab_dx * lspd, egrab_dy * lspd);
        }
    }
    egrab_x = enemy->x;
    egrab_y = enemy->y;
    egrab_t = egrab_t + dt;
    if (egrab_t >= ASTRA_STRIKE_DELAY - 1e-9 && egrab_hits == 0) {
        double step_back = fmax(0.0, (egrab_x - prev_ex) * egrab_dx + (egrab_y - prev_ey) * egrab_dy);
        double f0 = astra_grab_forward_offset - step_back;
        double f1 = astra_grab_forward_offset + astra_zone_reach(enemy_level);
        double hw = astra_zone_width(enemy_level) / 2;
        double hit_p = strip_hits_player(egrab_x, egrab_y, egrab_dx, egrab_dy, f0, f1, hw);
        double target = hit_p == 1 ? 0 : punch_resolve_target(egrab_x, egrab_y, egrab_dx, egrab_dy,
                                                              astra_zone_reach(enemy_level),
                                                              astra_zone_width(enemy_level));
        if (target == 0) {
            egrab_hits = 1;
            egrab_box_a = 1;
            if (revive_prot <= 0) {
                stun_player(astra_strike_stun_for(enemy_level));
                if (take_damage_at(astra_strike_damage_for(enemy_level) * enemy_damage_mult_for(enemy_class),
                                   enemy->x, enemy->y) == 1) {
                    enemy_grab_stop(1);
                    return;
                }
            }
        } else if (target >= 1 && target <= turret_max) {
            egrab_hits = 1;
            egrab_box_a = 1;
            damage_turret(target - 1, astra_strike_damage_for(enemy_level) * enemy_damage_mult_for(enemy_class));
            if (enemy_apply_damage(turret_counter_damage) == 1) {
                enemy_grab_stop(1);
                return;
            }
        }
    }
    if (egrab_t >= ASTRA_STRIKE_TIME - 1e-9) {
        enemy_grab_stop(egrab_hits > 0 ? 1 : 0);
    }
}

static double enemy_wants_grab(void) {
    if (class_is_astra(enemy_class) == 0) {
        return 0;
    }
    if (enemy_super_cd > 0 || egrab_state != 0 || egrab_parry_t > 0 || egrab_ready_t > 0) {
        return 0;
    }
    if (player->hp <= 0 || revive_prot > 0) {
        return 0;
    }
    if (dist(enemy->x, enemy->y, player->x, player->y) > enemy_astra_range) {
        return 0;
    }
    return 1;
}
