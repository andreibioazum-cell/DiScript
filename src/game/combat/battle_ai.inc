static double start_enemy_attack(void) {
    if (enemy_block_left > 0) {
        return 0;
    }
    double ta = atan2(player->y - enemy->y, player->x - enemy->x);
    double da = ta - enemy->angle;
    if (da > pi) {
        da = da - 2 * pi;
    }
    if (da < 0 - pi) {
        da = da + 2 * pi;
    }
    double err = (rand() % 100 - 50) / 100.0 * enemy_human_wobble * pi / 180;
    if (rand() % 100 < enemy_human_miss * 100) {
        err = err + (rand() % 100 - 50) / 100.0 * 0.35;
    }
    enemy->aim_err = err;
    double turn_budget = enemy_attack_turn_speed * (enemy_windup - enemy_lock_time);
    if (fabs(da) > turn_budget) {
        return 0;
    }
    enemy->state = 1;
    enemy->state_time = enemy_windup;
    enemy->hit = 0;
    enemy_punch_x = enemy->x;
    enemy_punch_y = enemy->y;
    enemy_punch_dx = cos(enemy->angle);
    enemy_punch_dy = sin(enemy->angle);
    enemy->react = enemy_human_react_min + (rand() % 100) / 100.0 * (enemy_human_react_max - enemy_human_react_min);
    return 1;
}

static void enemy_face_player(void) {
    enemy_predict(enemy_lead);
    double ta = atan2(aim_ty - enemy->y, aim_tx - enemy->x);
    double da = ta - enemy->angle;
    if (da > pi) {
        da = da - 2 * pi;
    }
    if (da < 0 - pi) {
        da = da + 2 * pi;
    }
    double step = enemy_attack_turn_speed * dt;
    enemy->angle = enemy->angle + clamp(da, 0 - step, step);
}

static void enemy_engage(double desired) {
    double dx = aim_tx - enemy->x;
    double dy = aim_ty - enemy->y;
    double d = sqrt(dx * dx + dy * dy);
    if (d < 1) {
        d = 1;
    }
    desired = desired + (rand() % 100 - 50) / 100.0 * 18;
    double radial = clamp((d - desired) * enemy_close_gain, -bot_speed, bot_speed);
    double orbit = bot_strafe_speed * enemy->strafe * 0.5;
    if (d > desired + 110) {
        orbit = orbit * 0.4;
    }
    double vx = dx / d * radial - dy / d * orbit;
    double vy = dy / d * radial + dx / d * orbit;
    double cap = bot_speed * enemy->speed_var;
    double sp = sqrt(vx * vx + vy * vy);
    if (sp > cap && sp > 0.001) {
        vx = vx / sp * cap;
        vy = vy / sp * cap;
    }
    if (rand() % 1000 < 8) {
        vx = vx * 0.4;
        vy = vy * 0.4;
        enemy->hesit = 0.08 + (rand() % 100) / 1000.0;
    }
    if (enemy->hesit > 0) {
        enemy->hesit = enemy->hesit - dt;
        vx = vx * 0.55;
        vy = vy * 0.55;
        if (enemy->hesit < 0) {
            enemy->hesit = 0;
        }
    }
    double ms = clamp(dt * enemy_move_smooth, 0, 1);
    enemy->svx = lerp(enemy->svx, vx, ms);
    enemy->svy = lerp(enemy->svy, vy, ms);
    enemy_move(enemy->svx, enemy->svy);
}

static void enemy_tick_block(void) {
    if (enemy_block_left > 0) {
        enemy_block_left = enemy_block_left - dt;
        if (enemy_block_left <= 0) {
            enemy_block_left = 0;
            /* A guard is not held forever: drop it and rest a beat. */
            enemy_block_cd = 1.1 + (rand() % 100) / 100.0 * 1.4;
        }
    } else if (enemy_block_cd > 0) {
        enemy_block_cd = tick_down(enemy_block_cd);
    }
}

static void update_enemy(void) {
    enemy_track_player();
    enemy->mvx = 0;
    enemy->mvy = 0;
    if (enemy->freeze > 0) {
        enemy->freeze = enemy->freeze - dt;
        if (enemy->freeze < 0) {
            enemy->freeze = 0;
        }
    }
    if (enemy->cooldown > 0) {
        enemy->cooldown = enemy->cooldown - dt * enemy_freeze_factor();
        if (enemy->cooldown < 0) {
            enemy->cooldown = 0;
        }
    }
    enemy->dodge_cd = tick_down(enemy->dodge_cd);
    enemy_tick_block();
    if (enemy->flank > 0) {
        enemy->flank = enemy->flank - dt;
    }
    enemy->react = tick_down(enemy->react);
    if (enemy->poison > 0) {
        if (enemy->poison_tick <= 0) {
            enemy->poison_tick = santa_poison_tick_interval;
        }
        enemy->poison = tick_down(enemy->poison);
        enemy->poison_tick = enemy->poison_tick - dt;
        if (enemy->poison_tick <= 0) {
            double tick_dmg = santa_poison_damage_for(player_level);
            if (player_level >= 3 && enemy->poison_damage > 0) {
                tick_dmg = enemy->poison_damage;
            }
            enemy->poison = 0;
            enemy->poison_tick = 0;
            enemy->poison_damage = 0;
            if (enemy_apply_damage(tick_dmg) == 1) {
                return;
            }
        } else if (enemy->poison <= 0) {
            enemy->poison = 0;
            enemy->poison_tick = 0;
            enemy->poison_damage = 0;
        }
    }
    if (enemy->stun > 0) {
        enemy->stun = tick_down(enemy->stun);
        return;
    }
    if (enemy_tick_class_abilities() == 1) {
        return;
    }
    double dx = player->x - enemy->x;
    double dy = player->y - enemy->y;
    double d = sqrt(dx * dx + dy * dy);
    if (d < 1) {
        d = 1;
    }
    double pr = player_half();
    double reach = punch_forward_offset + enemy_punch_reach + pr;
    if (enemy_block_left > 0) {
        /* Guarding: planted feet, shield up, eyes on the player. */
        enemy_face_player();
        return;
    }
    if (enemy->state == 1) {
        enemy_punch_x = enemy->x;
        enemy_punch_y = enemy->y;
        enemy_punch_dx = cos(enemy->angle);
        enemy_punch_dy = sin(enemy->angle);
        enemy->state_time = enemy->state_time - dt;
        if (enemy->state_time > enemy_lock_time) {
            enemy_turn_prediction();
        }
        enemy_predict(enemy_lead);
        double adx = aim_tx - enemy->x;
        double ady = aim_ty - enemy->y;
        double ad = sqrt(adx * adx + ady * ady);
        if (ad < 1) {
            ad = 1;
        }
        if (ad > reach - enemy_hit_margin) {
            enemy_move(adx / ad * bot_speed * 0.85, ady / ad * bot_speed * 0.85);
        }
        if (enemy->state_time <= 0) {
            enemy->state = 2;
            enemy->state_time = enemy_punch_time;
            enemy->hit = 0;
        }
        return;
    }
    if (enemy->state == 2) {
        enemy_punch_x = enemy->x;
        enemy_punch_y = enemy->y;
        enemy_punch_dx = cos(enemy->angle);
        enemy_punch_dy = sin(enemy->angle);
        enemy->state_time = enemy->state_time - dt;
        if (enemy->hit == 0) {
            double target = punch_resolve_target(enemy_punch_x, enemy_punch_y, enemy_punch_dx, enemy_punch_dy,
                                                 enemy_punch_reach, enemy_punch_width);
            if (target == 0) {
                enemy->hit = 1;
                if (revive_prot <= 0) {
                    take_damage_at(class_punch_damage_for(enemy_class, enemy_level) * enemy_damage_mult_for(enemy_class),
                                   enemy->x, enemy->y);
                    if (enemy_class == CLASS_SANTA) {
                        if (player_poison <= 0) {
                            player_poison_tick = santa_poison_tick_interval;
                        }
                        player_poison = santa_poison_time_for(enemy_level);
                        player_poison_damage = santa_poison_damage_for(enemy_level) * enemy_damage_mult_for(enemy_class);
                    }
                    ds_log("enemy punch! hp %g", player->hp);
                    if (finished != 0) {
                        return;
                    }
                }
            } else if (target >= 1 && target <= turret_max) {
                enemy->hit = 1;
                damage_turret(target - 1,
                              class_punch_damage_for(enemy_class, enemy_level) * enemy_damage_mult_for(enemy_class));
                if (enemy_apply_damage(turret_counter_damage) == 1) {
                    return;
                }
                ds_log("enemy punch! turret hp %g", arr_get(turret_hp, target - 1));
            }
        }
        if (enemy->state_time <= 0) {
            enemy->state = 3;
            enemy->state_time = enemy_recovery;
            enemy->cooldown = enemy_roll_cooldown();
            if (enemy->hit == 1 && enemy_free_window() == 1) {
                enemy->cooldown = enemy_cooldown_min * 0.7;
                enemy->think = 0;
            } else {
                enemy->think = enemy_think_min + (rand() % 100) / 100.0 * (enemy_think_max - enemy_think_min);
            }
        }
        return;
    }
    if (enemy->state == 3) {
        enemy->state_time = enemy->state_time - dt;
        if (enemy_free_window() == 0 && d < reach * 0.85) {
            enemy_predict(0);
            enemy_engage(reach * 0.95);
        }
        enemy_face_player();
        if (enemy->state_time <= 0) {
            enemy->state = 0;
            if (enemy->flank <= 0) {
                enemy_pick_flank();
                enemy->flank = enemy_flank_time + (rand() % 100) / 100.0 * 0.4;
            }
        }
        return;
    }
    if (enemy->state == 4) {
        enemy->state_time = enemy->state_time - dt;
        double dpx = cos(player->angle);
        double dpy = sin(player->angle);
        double dodge_err = (rand() % 100 - 50) / 100.0 * 0.6;
        double dvx = -dpy * enemy->strafe * bot_dodge_speed * (0.7 + dodge_err) - dpx * bot_speed * 0.25;
        double dvy = dpx * enemy->strafe * bot_dodge_speed * (0.7 + dodge_err) - dpy * bot_speed * 0.25;
        enemy_move(dvx, dvy);
        enemy_face_motion();
        if (enemy->state_time <= 0) {
            enemy->state = 0;
            enemy->dodge_cd = enemy_dodge_cooldown + (rand() % 100) / 100.0 * 0.3;
            enemy->cooldown = 0;
            enemy->think = 0.08;
        }
        return;
    }
    if (punch_left > 0 && punch_left <= punch_time - enemy->react && enemy->dodge_cd <= 0 && enemy_in_danger() == 1) {
        if (enemy->react <= 0) {
            if (rand() % 100 < enemy_human_dodge_fail * 100) {
                ds_log("enemy dodge fail (human)");
            } else if (enemy_block_left <= 0 && enemy_block_cd <= 0 && rand() % 100 < 45) {
                /* Humans do not always side-step: sometimes they plant their
                 * feet and take the swing on a raised guard instead. */
                enemy_block_left = 0.55 + (rand() % 100) / 100.0 * 0.5;
                enemy->react =
                    enemy_human_react_min + (rand() % 100) / 100.0 * (enemy_human_react_max - enemy_human_react_min);
                ds_log("enemy blocks (human)");
                return;
            } else {
                double side = (enemy->y - player->y) * cos(player->angle) - (enemy->x - player->x) * sin(player->angle);
                if (side >= 0) {
                    enemy->strafe = 1;
                } else {
                    enemy->strafe = -1;
                }
                if (rand() % 100 < 18) {
                    enemy->strafe = 0 - enemy->strafe;
                }
                enemy->state = 4;
                enemy->state_time = enemy_dodge_time + (rand() % 100 - 50) / 100.0 * 0.08;
                enemy->react =
                    enemy_human_react_min + (rand() % 100) / 100.0 * (enemy_human_react_max - enemy_human_react_min);
                return;
            }
        }
    }
    if (enemy_free_window() == 1) {
        enemy->think = 0;
        if (enemy->cooldown <= 0 && (d <= reach || enemy_would_hit() == 1)) {
            if (rand() % 100 < 12) {
                enemy_pick_flank();
                enemy->flank = enemy_flank_time;
                enemy->think = 0.12;
                enemy_engage(reach * 1.2);
                enemy_face_motion();
                return;
            }
            if (start_enemy_attack() == 1) {
                return;
            }
            enemy_predict(enemy_lead);
            enemy_engage(reach * 0.55);
            enemy_face_player();
            return;
        }
        enemy_predict(enemy_lead);
        enemy_engage(reach * 0.55);
        if (d <= reach) {
            enemy_face_player();
        } else {
            enemy_face_motion();
        }
        return;
    }
    if (enemy->cooldown <= 0 && enemy->think <= 0 && (d <= reach * 0.95 || enemy_would_hit() == 1)) {
        if (enemy->react <= 0) {
            if (start_enemy_attack() == 1) {
                return;
            }
        }
    }
    if (enemy->think > 0) {
        enemy->think = enemy->think - dt;
    }
    if (enemy->flank <= 0) {
        enemy_pick_flank();
        enemy->flank = enemy_flank_time;
    }
    if (enemy_block_left <= 0 && enemy_block_cd <= 0 && rand() % 1000 < 5) {
        /* Occasionally turtle up out of nowhere - reads like a cautious human
         * catching their breath between exchanges. */
        enemy_block_left = 0.5 + (rand() % 100) / 100.0 * 0.7;
        return;
    }
    double desired = reach * 0.62;
    if (d < enemy_min_distance * 0.75) {
        desired = enemy_min_distance * 0.75;
    }
    enemy_predict(enemy_lead);
    enemy_engage(desired);
    if (d <= reach) {
        enemy_face_player();
    } else {
        enemy_face_motion();
    }
}
