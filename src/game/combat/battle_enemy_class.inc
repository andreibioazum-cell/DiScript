static void enemy_reset_class_state(void) {
    enemy_class = CLASS_ORDINARY;
    enemy_level = 0;
    enemy_super_cd = 0;
    enemy_super_kind = 0;
    enemy_revived = 0;
    enemy_revive_prot = 0;
    enemy_gift->active = 0;
    enemy_gift->t = 0;
    enemy_gift->shot = 0;
    enemy_boom_t = 0;
    enemy_boom_x = 0;
    enemy_boom_y = 0;
    enemy_dash_active = 0;
    enemy_dash_t = 0;
    enemy_dash_dx = 0;
    enemy_dash_dy = 0;
    enemy_dash_hit = 0;
    enemy_dash_cd = 0;
    enemy_dash_x0 = 0;
    enemy_dash_y0 = 0;
    enemy_grab_clear();
    egrab_x = 0;
    egrab_y = 0;
    egrab_dx = 1;
    egrab_dy = 0;
    egrab_punch_t = 0;
    egrab_box_a = 0;
    enemy_last_turret_slot = 0;
    enemy_ball_reset();
    arr_clear(enemy_turret_x);
    arr_clear(enemy_turret_y);
    arr_clear(enemy_turret_hp);
    arr_clear(enemy_turret_maxhp);
    arr_clear(enemy_turret_spawn_t);
    arr_clear(enemy_turret_box_a);
    double i = 0;
    while (i < turret_max) {
        arr_push(enemy_turret_x, 0);
        arr_push(enemy_turret_y, 0);
        arr_push(enemy_turret_hp, 0);
        arr_push(enemy_turret_maxhp, 0);
        arr_push(enemy_turret_spawn_t, 0);
        arr_push(enemy_turret_box_a, 0);
        i = i + 1;
    }
}

static void enemy_pick_random_class(void) {
    double roll = rand() % 100;
    double azum_to = enemy_class_azum_chance;
    double ebuc_to = azum_to + enemy_class_ebuc_chance;
    double santa_to = ebuc_to + enemy_class_santa_chance;
    double astra_to = santa_to + enemy_class_astra_chance;
    if (roll < azum_to) {
        enemy_class = CLASS_AZUM;
    } else if (roll < ebuc_to) {
        enemy_class = CLASS_EBUC;
    } else if (roll < santa_to) {
        /* Restrict bot Santa spawns to developer accounts. */
        enemy_class = dev_access() == 1 ? CLASS_SANTA : CLASS_ORDINARY;
    } else if (roll < astra_to) {
        enemy_class = CLASS_ASTRA;
    } else {
        enemy_class = CLASS_ORDINARY;
    }
    enemy_level = 0;
    enemy_revived = 0;
    enemy_revive_prot = 0;
    enemy_super_cd = enemy_first_super_min + (rand() % 100) / 100.0 * (enemy_first_super_max - enemy_first_super_min);
    enemy_super_kind = 0;
    enemy_dash_cd = enemy_first_super_min + (rand() % 100) / 100.0 * 2.0;
    enemy_dash_active = 0;
    enemy_grab_clear();
    enemy->max_hp = astra_ability_max_hp_for(enemy_class, astra_ability_of(enemy_class, SKIN_NORMAL));
    enemy->hp = enemy->max_hp;
    ds_log("enemy class %g hp %g", enemy_class, enemy->max_hp);
}

/* Return 1 when enemy guard faces the incoming damage source. */
static double enemy_block_covers(void) {
    if (dmg_from_k == 0) {
        return 1;
    }
    double a = atan2(dmg_from_y - enemy->y, dmg_from_x - enemy->x);
    double da = a - enemy->angle;
    while (da > pi) {
        da = da - 2 * pi;
    }
    while (da < 0 - pi) {
        da = da + 2 * pi;
    }
    return fabs(da) <= block_front_angle;
}

static double enemy_apply_damage(double dmg) {
    if (finished != 0) {
        dmg_from_k = 0;
        return 1;
    }
    /* Apply event and admin damage multipliers. */
    dmg = dmg * weak_hit_damage_mult();
    dmg = dmg * admin_cheat_damage_mult();
    if (enemy_revive_prot > 0) {
        dmg_from_k = 0;
        return 0;
    }
    if (class_is_astra(enemy_class) == 1 && egrab_parry_t > 0) {
        dmg_from_k = 0;
        if (dmg > 0) {
            enemy_on_astra_parry_block();
        }
        return 0;
    }
    /* Reduce frontal damage while enemy is blocking. */
    if (enemy_block_left > 0 && enemy_block_covers() == 1) {
        dmg = dmg * block_guard;
    }
    dmg_from_k = 0;
    if (enemy_class == CLASS_EBUC) {
        dmg = enemy_shield_absorb(dmg);
    }
    /* Match the player-side safeguard: any full life needs more than two
     * unblocked damaging hits, even against a high-damage ability. */
    if (dmg > max_hit_damage) {
        dmg = max_hit_damage;
    }
    if (dmg > 0) {
        enemy_hurt_a = 1;
        damage_taken_shake(dmg);
    /* Chance to raise guard after taking a hit. */
    if (enemy_block_left <= 0 && enemy_block_cd <= 0 && enemy->stun <= 0 && enemy->state == 0 &&
            rand() % 100 < 10) {
            enemy_block_left = 0.4 + (rand() % 100) / 100.0 * 0.3;
        }
    }
    enemy->hp = enemy->hp - dmg;
    if (enemy->hp < 0) {
        enemy->hp = 0;
    }
    if (enemy->hp <= 0) {
        if (enemy_try_revive() == 1) {
            return 0;
        }
        finish_game(1);
        return 1;
    }
    return 0;
}

static double enemy_apply_damage_at(double dmg, double sx, double sy) {
    dmg_from_x = sx;
    dmg_from_y = sy;
    dmg_from_k = 1;
    return enemy_apply_damage(dmg);
}

static double enemy_try_revive(void) {
    if (enemy->hp > 0) {
        return 0;
    }
    if (enemy_class != CLASS_AZUM) {
        return 0;
    }
    if (enemy_revived >= (azum_buff_active == 1 ? 2 : 1)) {
        return 0;
    }
    enemy_revived = enemy_revived + 1;
    enemy->hp = enemy->max_hp;
    enemy_revive_prot = enemy_revive_prot_time;
    double k = 0;
    while (k < 50) {
        double rx = 70 + rand() % (screen_w - 140);
        double ry = 70 + rand() % (screen_h - 140);
        if (dist(rx, ry, player->x, player->y) > 200) {
            k = 50;
            enemy->x = rx;
            enemy->y = ry;
        } else {
            k = k + 1;
        }
    }
    enemy->state = 0;
    enemy->state_time = 0;
    enemy->cooldown = 0.5;
    enemy_dash_active = 0;
    enemy_dash_t = 0;
    enemy_dash_hit = 0;
    enemy_super_kind = 0;
    ds_log("enemy azum revived");
    return 1;
}

static void enemy_start_snow(void) {
    enemy_gift->x = enemy->x;
    enemy_gift->y = enemy->y;
    enemy_gift->x0 = enemy->x;
    enemy_gift->y0 = enemy->y;
    double a = atan2(player->y - enemy->y, player->x - enemy->x);
    enemy_gift->dx = cos(a);
    enemy_gift->dy = sin(a);
    enemy_gift->active = 1;
    enemy_gift->t = 0;
    enemy_gift->shot = enemy_gift->shot + 1;
    enemy_gift->pierce = 0;
    ds_log("enemy snow thrown");
}

static void enemy_snow_explode(void) {
    enemy_gift->active = 0;
    enemy_boom_x = enemy_gift->x;
    enemy_boom_y = enemy_gift->y;
    enemy_boom_t = boom_time;
    if (circle_hits_player(enemy_boom_x, enemy_boom_y, super_radius) == 1) {
        if (revive_prot <= 0) {
            if (astra_parry_active() == 0 && player_freeze < santa_freeze_time_for(enemy_level)) {
                player_freeze = santa_freeze_time_for(enemy_level);
                player_freeze_slow = super_freeze_slow;
            }
            take_damage_at(santa_super_damage * enemy_damage_mult_for(enemy_class), enemy_boom_x, enemy_boom_y);
        }
    }
}

static void tick_enemy_gift(void) {
    if (enemy_boom_t > 0) {
        enemy_boom_t = tick_down(enemy_boom_t);
    }
    if (enemy_gift->active == 0) {
        return;
    }
    if (finished != 0) {
        enemy_gift->active = 0;
        return;
    }
    if (enemy_gift->t == 0) {
        enemy_gift->x0 = enemy_gift->x;
        enemy_gift->y0 = enemy_gift->y;
    }
    enemy_gift->t = enemy_gift->t + dt;
    snow_step_pos(enemy_gift->x0, enemy_gift->y0, enemy_gift->dx, enemy_gift->dy, enemy_gift->t, enemy_gift->shot,
                  &enemy_gift->x, &enemy_gift->y);
    snow_chip_player_turrets();
    double touched = 0;
    if (circle_hits_player(enemy_gift->x, enemy_gift->y, snow_radius_for(enemy_gift->shot)) == 1) {
        touched = 1;
    }
    if (enemy_gift->t > super_range / super_speed) {
        touched = 1;
    }
    if (touched == 1) {
        enemy_snow_explode();
    }
}

static void draw_enemy_gift(void) {
    if (enemy_gift->active == 0) {
        return;
    }
    double sc = snow_scale_for(enemy_gift->shot);
    double hw = 16 * sc;
    double rot = enemy_gift->t * 3;
    tex(enemy_gift->x - hw, enemy_gift->y - hw, SNOWFLAKE_TEX, rot, 0.5 * sc);
}

static void draw_enemy_boom(void) {
    draw_boom_at(enemy_boom_x, enemy_boom_y, enemy_boom_t);
}
