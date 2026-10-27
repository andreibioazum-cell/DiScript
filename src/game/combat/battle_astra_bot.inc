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
    egrab_unlock_t = 0;
    egrab_ready_t = 0;
    if (pgrab_active == 1 && pgrab_slot == grab_slot_bot) {
        pgrab_active = 0;
        pgrab_slot = -1;
    }
}

/* A reactive parry answers a punch that is already in the air: a short shield,
 * a real cooldown and a coin flip, so the bot reads the attack but stays
 * beatable - the strike it unlocks is the same one its super earns. */
#define ASTRA_REACT_PARRY_TIME 0.45
#define ASTRA_REACT_PARRY_CD 2.4
#define ASTRA_REACT_PARRY_CHANCE 45
#define ASTRA_REACT_PARRY_ANGLE 1.2

static void enemy_start_astra_parry(void) {
    egrab_parry_t = ASTRA_PARRY_TIME;
    egrab_unlock_t = 0;
    egrab_ready_t = 0;
    enemy->freeze = 0;
    enemy_block_left = 0;
    egrab_parry_super = 1;
    ds_log("bot astra purple shield cast");
}

/* The bot has an answer ready: shield down, no strike running, no cooldown. */
static double enemy_parry_ready(void) {
    if (class_is_astra(enemy_class) == 0) {
        return 0;
    }
    if (egrab_parry_t > 0 || egrab_state != 0 || egrab_ready_t > 0 || egrab_unlock_t > 0) {
        return 0;
    }
    if (egrab_parry_cd > 0 || enemy->stun > 0 || enemy->freeze > 0) {
        return 0;
    }
    if (enemy->state == 1 || enemy->state == 2) {
        /* Mid-punch: the shield cannot come up while the fist is already out. */
        return 0;
    }
    if (finished != 0 || player->hp <= 0 || revive_prot > 0) {
        return 0;
    }
    return 1;
}

/* The shield only answers what the bot can see: the player has to be in front. */
static double enemy_sees_player(void) {
    double a = atan2(player->y - enemy->y, player->x - enemy->x);
    double da = a - enemy->angle;
    while (da > pi) {
        da = da - 2 * pi;
    }
    while (da < 0 - pi) {
        da = da + 2 * pi;
    }
    return fabs(da) <= ASTRA_REACT_PARRY_ANGLE ? 1 : 0;
}

/* A player punch that is winding up and would reach the bot: this is what a
 * parry is for. The shield comes up before the hit lands and eats it. */
static double enemy_punch_incoming(void) {
    if (punch->active == 0 || punch->hit == 1 || punch_left <= 0) {
        return 0;
    }
    return punch_hits_enemy(punch->x, punch->y, punch->dx, punch->dy, punch_reach, punch_width);
}

static void enemy_try_parry_reaction(void) {
    if (enemy_parry_ready() == 0 || enemy_punch_incoming() == 0 || enemy_sees_player() == 0) {
        return;
    }
    if (rand() % 100 >= ASTRA_REACT_PARRY_CHANCE) {
        egrab_parry_cd = ASTRA_REACT_PARRY_CD;
        return;
    }
    egrab_parry_t = ASTRA_REACT_PARRY_TIME;
    egrab_unlock_t = 0;
    egrab_ready_t = 0;
    enemy->freeze = 0;
    enemy_block_left = 0;
    egrab_parry_super = 0;
    egrab_parry_cd = ASTRA_REACT_PARRY_CD;
    ds_log("bot astra parry reaction");
}

static void enemy_on_astra_parry_block(void) {
    if (egrab_ready_t > 0) {
        egrab_ready_t = ASTRA_STRIKE_WINDOW;
    } else if (egrab_unlock_t <= 0) {
        egrab_unlock_t = ASTRA_STRIKE_UNLOCK_DELAY;
    }
    /* The shield answered the hit - even a reactive one now earns its counter. */
    egrab_parry_super = 0;
    ds_log("bot astra parry unlocked strike");
}

static void enemy_grab_stop(double landed) {
    egrab_state = 0;
    egrab_t = 0;
    egrab_hits = 0;
    egrab_parry_t = 0;
    egrab_unlock_t = 0;
    egrab_ready_t = 0;
    egrab_parry_super = 0;
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
    egrab_unlock_t = 0;
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
        egrab_unlock_t = 0;
        egrab_ready_t = 0;
        if (egrab_state != 0) {
            enemy_grab_stop(0);
        }
        return;
    }
    double had_parry = egrab_parry_t > 0;
    double was_super = egrab_parry_super;
    egrab_parry_t = tick_down(egrab_parry_t);
    if (had_parry == 1 && egrab_parry_t <= 0 && egrab_unlock_t <= 0 && egrab_ready_t <= 0 && egrab_state == 0) {
        /* A shield that caught nothing only costs the super when it was the
         * super: the quick reaction parry lives on its own cooldown. */
        if (was_super == 1) {
            enemy_super_cd = astra_grab_miss_cd;
        }
        egrab_parry_super = 0;
    }
    double had_ready = egrab_ready_t > 0;
    egrab_ready_t = tick_down(egrab_ready_t);
    double had_unlock = egrab_unlock_t > 0;
    egrab_unlock_t = tick_down(egrab_unlock_t);
    if (had_unlock == 1 && egrab_unlock_t <= 0 && egrab_state == 0) {
        egrab_ready_t = ASTRA_STRIKE_WINDOW;
    }
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
            enemy_face_player();
            egrab_dx = cos(enemy->angle);
            egrab_dy = sin(enemy->angle);
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
