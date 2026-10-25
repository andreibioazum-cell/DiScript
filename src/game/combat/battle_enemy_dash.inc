static void enemy_start_dash(void) {
    double a = atan2(player->y - enemy->y, player->x - enemy->x);
    enemy_dash_dx = cos(a);
    enemy_dash_dy = sin(a);
    enemy_dash_x0 = enemy->x;
    enemy_dash_y0 = enemy->y;
    enemy_dash_travel = 0;
    enemy_dash_active = 1;
    enemy_dash_t = azum_dash_time;
    enemy_dash_hit = 0;
    enemy_dash_cd = enemy_dash_cd_time;
    enemy->angle = a;
    ds_log("enemy dash started");
}

static void tick_enemy_dash(void) {
    enemy_dash_cd = tick_down(enemy_dash_cd);
    if (enemy_dash_active == 0) {
        return;
    }
    if (finished != 0) {
        enemy_dash_active = 0;
        return;
    }
    if (enemy->stun > 0) {
        enemy_dash_t = enemy_dash_t - dt;
        if (enemy_dash_t <= 0) {
            enemy_dash_t = 0;
            enemy_dash_active = 0;
            enemy_dash_hit = 0;
        }
        return;
    }
    enemy->x = enemy->x + enemy_dash_dx * azum_dash_speed * dt;
    enemy->y = enemy->y + enemy_dash_dy * azum_dash_speed * dt;
    if (enemy->x < enemy->size) {
        enemy->x = enemy->size;
        enemy_dash_t = 0;
    }
    if (enemy->x > screen_w - enemy->size) {
        enemy->x = screen_w - enemy->size;
        enemy_dash_t = 0;
    }
    if (enemy->y < enemy->size) {
        enemy->y = enemy->size;
        enemy_dash_t = 0;
    }
    if (enemy->y > screen_h - enemy->size) {
        enemy->y = screen_h - enemy->size;
        enemy_dash_t = 0;
    }
    double tvx = enemy->x - enemy_dash_x0;
    double tvy = enemy->y - enemy_dash_y0;
    enemy_dash_travel = tvx * enemy_dash_dx + tvy * enemy_dash_dy;
    if (enemy_dash_travel < 0) {
        enemy_dash_travel = 0;
    }
    if (enemy_dash_travel > azum_dash_speed * azum_dash_time) {
        enemy_dash_travel = azum_dash_speed * azum_dash_time;
    }
    tick_enemy_dust(enemy_dash_dx * azum_dash_speed, enemy_dash_dy * azum_dash_speed);
    if (enemy_dash_hit == 0) {
        double dti = 0;
        while (dti < turret_max) {
            if (enemy_dash_hit == 0 && turret_alive(dti) == 1) {
                if (strip_hits_turret(enemy_dash_x0, enemy_dash_y0, enemy_dash_dx, enemy_dash_dy, 0, enemy_dash_travel,
                                      dash_hit_radius_solo(), arr_get(turret_x, dti), arr_get(turret_y, dti)) == 1) {
                    enemy_dash_hit = 1;
                    damage_turret(dti, enemy_azum_dash_damage_for());
                    enemy_apply_damage(turret_counter_damage);
                }
            }
            dti = dti + 1;
        }
    }
    if (enemy_dash_hit == 0 && finished == 0 && revive_prot <= 0) {
        if (strip_hits_player(enemy_dash_x0, enemy_dash_y0, enemy_dash_dx, enemy_dash_dy, 0, enemy_dash_travel,
                              dash_hit_radius_solo()) == 1) {
            enemy_dash_hit = 1;
            take_damage_at(enemy_azum_dash_damage_for(), enemy->x, enemy->y);
            stun_player(azum_dash_stun);
            ds_log("enemy dash hit player");
        }
    }
    enemy_dash_t = enemy_dash_t - dt;
    if (enemy_dash_t <= 0) {
        enemy_dash_t = 0;
        enemy_dash_active = 0;
        enemy_dash_hit = 0;
    }
}

static void tick_dash_solo_hit(void) {
    if (dash_active == 0 || dash_hit == 1 || finished != 0) {
        return;
    }
    double ti = 0;
    while (ti < turret_max) {
        if (dash_hit == 0 && enemy_turret_alive(ti) == 1) {
            if (strip_hits_turret(dash_x0, dash_y0, dash_dx, dash_dy, 0, dash_travel, dash_hit_radius_solo(),
                                  arr_get(enemy_turret_x, ti), arr_get(enemy_turret_y, ti)) == 1) {
                dash_hit = 1;
                enemy_damage_turret(ti, azum_dash_damage_for());
                if (revive_prot <= 0) {
                    take_damage(turret_counter_damage * enemy_damage_mult_for(enemy_class));
                    ds_log("dash hit enemy turret");
                }
            }
        }
        ti = ti + 1;
    }
    if (dash_hit == 1 || finished != 0) {
        return;
    }
    if (enemy_revive_prot > 0) {
        return;
    }
    if (strip_hits_enemy(dash_x0, dash_y0, dash_dx, dash_dy, 0, dash_travel, dash_hit_radius_solo()) == 1) {
        dash_hit = 1;
        stun_enemy(azum_dash_stun);
        ds_log("dash hit enemy, hp %g", enemy->hp);
        enemy_apply_damage_at(azum_dash_damage_for(), player->x, player->y);
    }
}

static void tick_enemy_class(void) {
    if (game_state != ST_SOLO) {
        return;
    }
    enemy_super_cd = tick_down(enemy_super_cd);
    if (enemy_revive_prot > 0) {
        enemy_revive_prot = tick_down(enemy_revive_prot);
    }
    enemy_tick_turrets();
    tick_enemy_gift();
    tick_enemy_dash();
    tick_enemy_grab();
    enemy_ball_tick();
    enemy_ball_fade_tick();
}

static double enemy_tick_class_abilities(void) {
    if (game_state != ST_SOLO) {
        return 0;
    }
    if (finished != 0) {
        return 0;
    }
    if (enemy_dash_active == 1) {
        return 1;
    }
    if (enemy_grab_active() == 1) {
        return 1;
    }
    if (enemy_super_windup > 0) {
        enemy_face_player();
        enemy_super_windup = enemy_super_windup - dt;
        if (enemy_super_windup <= 0) {
            enemy_super_windup = 0;
            if (enemy_super_kind == 1) {
                enemy_start_snow();
            } else if (enemy_super_kind == 2) {
                enemy_place_turret();
            } else if (enemy_super_kind == 3) {
                enemy_start_grab();
            } else if (enemy_super_kind == 4) {
                enemy_ball_fire();
            }
            enemy_super_kind = 0;
        }
        return 1;
    }
    if (enemy->state == 1 || enemy->state == 2 || enemy->state == 4) {
        return 0;
    }
    double d = dist(enemy->x, enemy->y, player->x, player->y);
    if (enemy_class == CLASS_SANTA) {
        if (enemy_super_cd <= 0 && d <= enemy_santa_range) {
            enemy_super_kind = 1;
            enemy_super_windup = enemy_super_windup_time;
            enemy_super_cd = enemy_santa_cd_time;
            enemy->state = 0;
            enemy->state_time = 0;
            return 1;
        }
    } else if (enemy_class == CLASS_EBUC) {
        if (enemy_super_cd <= 0) {
            if (enemy_turret_count_alive() >= turret_max) {
                enemy_ball_mode = 1;
            } else if (enemy_ball_active() == 1) {
                enemy_ball_mode = 0;
            }
            if (enemy_ball_mode == 1 && enemy_ball_active() == 0) {
                enemy_super_kind = 4;
                enemy_super_windup = enemy_super_windup_time;
                enemy_super_cd = enemy_station_cd_time;
                enemy_ball_mode = 0;
                enemy->state = 0;
                enemy->state_time = 0;
                return 1;
            }
            if (enemy_turret_count_alive() < turret_max) {
                enemy_super_kind = 2;
                enemy_super_windup = enemy_super_windup_time;
                enemy_super_cd = enemy_station_cd_time;
                enemy_ball_mode = 1;
                enemy->state = 0;
                enemy->state_time = 0;
                return 1;
            }
        }
    } else if (enemy_class == CLASS_AZUM) {
        if (enemy_dash_cd <= 0 && d >= enemy_dash_min_range && d <= enemy_dash_max_range) {
            enemy->state = 0;
            enemy->state_time = 0;
            enemy_start_dash();
            return 1;
        }
    } else if (class_is_astra(enemy_class) == 1) {
        if (enemy_wants_grab() == 1) {
            enemy->state = 0;
            enemy->state_time = 0;
            enemy_start_astra_parry();
        }
    }
    return 0;
}
