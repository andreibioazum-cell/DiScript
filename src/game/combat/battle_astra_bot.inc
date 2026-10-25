/* Bot Astra purple parry shield and stun counter-strike. */

static double enemy_grab_active(void) {
    if (egrab_state != 0) {
        return 1;
    }
    return 0;
}

static void enemy_grab_swing(void) { egrab_punch_t = ASTRA_STRIKE_TIME; }

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
    ds_log("bot astra purple shield cast");
}

static void enemy_on_astra_parry_block(void) {
    egrab_ready_t = ASTRA_STRIKE_WINDOW;
    ds_log("bot astra parry unlocked strike");
}

static void enemy_grab_stop(double landed) {
    enemy_grab_clear();
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
    egrab_x = enemy->x;
    egrab_y = enemy->y;
    egrab_dx = cos(enemy->angle);
    egrab_dy = sin(enemy->angle);
    enemy_grab_swing();
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
    if (egrab_t < astra_ability_lunge_time && enemy->stun <= 0) {
        double step = dt;
        if (egrab_t + step > astra_ability_lunge_time) {
            step = astra_ability_lunge_time - egrab_t;
        }
        double lspd = astra_ability_lunge_dist / astra_ability_lunge_time;
        enemy->x = clamp(enemy->x + egrab_dx * lspd * step, enemy->size, screen_w - enemy->size);
        enemy->y = clamp(enemy->y + egrab_dy * lspd * step, enemy->size, screen_h - enemy->size);
        tick_enemy_dust(egrab_dx * lspd, egrab_dy * lspd);
    }
    egrab_x = enemy->x;
    egrab_y = enemy->y;
    if (egrab_hits == 0) {
        double target = punch_resolve_target(egrab_x, egrab_y, egrab_dx, egrab_dy, astra_zone_reach(enemy_level),
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
    egrab_t = egrab_t + dt;
    if (egrab_t >= ASTRA_STRIKE_TIME) {
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
