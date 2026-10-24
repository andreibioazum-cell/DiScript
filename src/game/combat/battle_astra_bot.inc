static double enemy_grab_active(void) {
    if (egrab_state != 0) {
        return 1;
    }
    return 0;
}

static void enemy_grab_swing(void) { egrab_punch_t = astra_hit_show; }

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
    if (egrab_ready_t <= 0) {
        egrab_ready_t = ASTRA_GRAB_WINDOW;
        ds_log("bot astra parry unlocked grab");
    }
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
    ds_log("bot astra grab cast");
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
    egrab_t = egrab_t + dt;
    double kit = astra_ability_of(enemy_class, SKIN_NORMAL);
    if (egrab_state == 1) {
        if (egrab_t <= astra_ability_catch_time_for(kit)) {
            if (pgrab_active == 0 && revive_prot <= 0 && player->hp > 0) {
                if (astra_zone_hits_player(egrab_x, egrab_y, egrab_dx, egrab_dy, enemy_level) == 1) {
                    egrab_state = 2;
                    pgrab_active = 1;
                    pgrab_slot = grab_slot_bot;
                    grab_cancel_actions();
                    ds_log("caught by the bot astra");
                }
            }
        } else {
            enemy_grab_stop(0);
            return;
        }
    }
    if (egrab_state != 2) {
        return;
    }
    grab_quiet_player(astra_ability_hit_interval_for(kit));
    double due = astra_ability_beats_done_for(egrab_t, kit);
    while (egrab_hits < due) {
        egrab_hits = egrab_hits + 1;
        enemy_grab_swing();
        if (take_damage_at(astra_beat_damage(player->max_hp) * enemy_damage_mult_for(enemy_class), enemy->x,
                           enemy->y) == 1) {
            enemy_grab_stop(1);
            return;
        }
        if (player->hp <= 0 || finished != 0 || revive_prot > 0) {
            enemy_grab_stop(1);
            return;
        }
    }
    if (egrab_t >= astra_ability_final_time_for(kit)) {
        enemy_grab_swing();
        /* enemy_grab_stop() clears the grab immediately, so explicitly pulse
         * the final damage zone and let the normal fade-out render it. */
        egrab_box_a = 1;
        double dead =
            take_damage_at(astra_final_damage(player->max_hp) * enemy_damage_mult_for(enemy_class), enemy->x, enemy->y);
        enemy_grab_stop(1);
        if (dead == 1) {
            return;
        }
        if (player->hp > 0 && finished == 0 && revive_prot <= 0) {
            throw_player(egrab_dx, egrab_dy, astra_ability_throw_dist_for(enemy_level, kit),
                         astra_ability_throw_stun_for(enemy_level, kit));
            ds_log("thrown by the bot astra");
        }
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
