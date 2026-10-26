/* Compute effective player walking speed with active buffs and debuffs. */
static double player_walk_speed(void) {
    double spd = joy_speed;
    if (player_class == CLASS_AZUM && player_level >= 2) {
        spd = spd * (1 + azum_speed_bonus);
    } else if (player_class == CLASS_ORDINARY && player_level >= 2) {
        spd = spd * (1 + ordinary_speed_bonus);
    }
    if (player_class == CLASS_CODECRACKER && codecracker_super_t > 0) {
        /* Apply CodeCracker overclock speed multiplier. */
        spd = spd * codecracker_speed_mult;
    }
    spd = spd * admin_cheat_speed_mult();
    if (astra_parry_active() == 1) {
        /* Reduce movement speed by 10% while Astra's purple shield is active. */
        spd = spd * ASTRA_PARRY_SPEED_K;
    } else {
        if (player_freeze > 0) {
            spd = spd * player_freeze_slow;
        }
        if (player_block == 1) {
            /* Reduce movement speed while blocking. */
            spd = spd * block_speed_k;
        }
    }
    return spd;
}

/* Zero player velocity while preserving facing direction. */
static void player_vel_reset(void) {
    player_vx = 0;
    player_vy = 0;
    player_stop_from = 0;
    player_dir_x = 1;
    player_dir_y = 0;
}

/* Move player along joystick direction with acceleration and braking ramps. */
static void player_walk_move(double spd) {
    double jm = sqrt(joy.dx * joy.dx + joy.dy * joy.dy);
    double have = sqrt(player_vx * player_vx + player_vy * player_vy);
    double want = 0;
    double step, rate;
    if (jm > 0.15) {
        player_dir_x = joy.dx / jm;
        player_dir_y = joy.dy / jm;
        want = spd;
        player->angle = atan2(joy.dy, joy.dx);
    }
    if (want > have) {
        player_stop_from = 0;
        rate = (spd > 1 ? spd : 1) / (walk_accel_time > 0 ? walk_accel_time : 0.001);
    } else {
                /* Decelerate from speed at stick release so stop time is constant. */
                if (player_stop_from <= 0) {
            player_stop_from = have;
        }
        rate = player_stop_from / (walk_stop_time > 0 ? walk_stop_time : 0.001);
    }
    step = rate * dt;
    if (step < 0) {
        step = 0;
    }
    if (want >= have) {
        have = have + step;
        if (have > want) {
            have = want;
        }
    } else {
        have = have - step;
        if (have < want) {
            have = want;
        }
    }
    if (have <= 0.0001) {
        have = 0;
        player_stop_from = 0;
    }
    player_vx = player_dir_x * have;
    player_vy = player_dir_y * have;
    player->x = player->x + player_vx * dt;
    player->y = player->y + player_vy * dt;
        /* Zero normal velocity component on wall contact so fighter slides along edges. */
        if (player->x < player->size) {
        player->x = player->size;
        if (player_vx < 0) player_vx = 0;
    }
    if (player->x > screen_w - player->size) {
        player->x = screen_w - player->size;
        if (player_vx > 0) player_vx = 0;
    }
    if (player->y < player->size) {
        player->y = player->size;
        if (player_vy < 0) player_vy = 0;
    }
    if (player->y > screen_h - player->size) {
        player->y = screen_h - player->size;
        if (player_vy > 0) player_vy = 0;
    }
    if (have > 1) {
                /* Scale dust emission with current speed. */
                tick_dust(player_vx / have, player_vy / have, have / (spd > 1 ? spd : 1));
    }
}

static void move_player(void) {
    if (pthrow_t > 0) {
        player_vel_reset();
        tick_player_throw();
        return;
    }
    if (player_stun > 0 || plates_lock_me == 1) {
        player_vel_reset();
        return;
    }
    if (pgrab_active == 1) {
        player_vel_reset();
        return;
    }
    if (astra_locked() == 1) {
        player_vx = 0;
        player_vy = 0;
        player_stop_from = 0;
        double jml = sqrt(joy.dx * joy.dx + joy.dy * joy.dy);
        if (jml > 0.15) {
            player_dir_x = joy.dx / jml;
            player_dir_y = joy.dy / jml;
            player->angle = atan2(joy.dy, joy.dx);
        }
        return;
    }
    if (dash_active > 0) {
        double jm2 = sqrt(joy.dx * joy.dx + joy.dy * joy.dy);
        if (jm2 > 0.15) {
            player->angle = atan2(joy.dy, joy.dx);
        }
        player->x = player->x + dash_dx * azum_dash_speed * dt;
        player->y = player->y + dash_dy * azum_dash_speed * dt;
        if (player->x < player->size) {
            player->x = player->size;
            dash_t = 0;
        }
        if (player->x > screen_w - player->size) {
            player->x = screen_w - player->size;
            dash_t = 0;
        }
        if (player->y < player->size) {
            player->y = player->size;
            dash_t = 0;
        }
        if (player->y > screen_h - player->size) {
            player->y = screen_h - player->size;
            dash_t = 0;
        }
        double tvx = player->x - dash_x0;
        double tvy = player->y - dash_y0;
        dash_travel = tvx * dash_dx + tvy * dash_dy;
        if (dash_travel < 0) {
            dash_travel = 0;
        }
        if (dash_travel > azum_dash_speed * azum_dash_time) {
            dash_travel = azum_dash_speed * azum_dash_time;
        }
        tick_dust(dash_dx, dash_dy, 1);
        return;
    }
    player_walk_move(player_walk_speed());
}

static double enemy_in_punch(void) {
    return punch_hits_enemy(punch->x, punch->y, punch->dx, punch->dy, punch_reach, punch_width);
}

static void update_aim(void) {
    if (punch->active == 1 && punch_left > 0 && punch_left <= punch_time + 1e-9 && super_windup <= 0 &&
        finished == 0) {
        aim_a = fade_up_a(aim_a, aim_fade_in);
    } else {
        aim_a = fade_down_a(aim_a, aim_fade_out);
    }
    if (punch_cd > 0) {
        punch_cd = tick_down(punch_cd);
        if (admin_cheat_nocd() == 1) {
            punch_cd = 0;
        }
    }
}

static void follow_punch(void) {
    if (punch_left <= 0) {
        return;
    }
    punch->x = player->x;
    punch->y = player->y;
    punch->dx = cos(player->angle);
    punch->dy = sin(player->angle);
}

static double enemy_freeze_factor(void) {
    if (class_is_astra(enemy_class) == 1 && egrab_parry_t > 0) {
        return 1;
    }
    if (enemy->freeze > 0) {
        return enemy->freeze_slow;
    }
    return 1;
}

static void freeze_enemy(double t, double slow) {
    if (class_is_astra(enemy_class) == 1 && egrab_parry_t > 0) {
        return;
    }
    if (t > enemy->freeze) {
        enemy->freeze = t;
        enemy->freeze_slow = slow;
    }
}

static void enemy_move(double vx, double vy) {
    double f = enemy_freeze_factor() * enemy->speed_var;
    if (class_is_astra(enemy_class) == 1 && egrab_parry_t > 0) {
        f = f * ASTRA_PARRY_SPEED_K;
    } else if (enemy_block_left > 0) {
        /* Reduce enemy movement speed while blocking. */
        f = f * block_speed_k;
    }
    enemy->mvx = vx * f;
    enemy->mvy = vy * f;
    enemy->svx = vx;
    enemy->svy = vy;
    enemy->x = enemy->x + vx * f * dt;
    enemy->y = enemy->y + vy * f * dt;
    if (enemy->x < enemy->size) {
        enemy->x = enemy->size;
    }
    if (enemy->x > screen_w - enemy->size) {
        enemy->x = screen_w - enemy->size;
    }
    if (enemy->y < enemy->size) {
        enemy->y = enemy->size;
    }
    if (enemy->y > screen_h - enemy->size) {
        enemy->y = screen_h - enemy->size;
    }
    tick_enemy_dust(enemy->mvx, enemy->mvy);
}

static void enemy_face_motion(void) {
    double sp = sqrt(enemy->mvx * enemy->mvx + enemy->mvy * enemy->mvy);
    if (sp < 10) {
        return;
    }
    double ta = atan2(enemy->mvy, enemy->mvx);
    double da = ta - enemy->angle;
    while (da > pi) {
        da = da - 2 * pi;
    }
    while (da < 0 - pi) {
        da = da + 2 * pi;
    }
    double turn = da * clamp(dt * enemy_face_speed, 0, 1);
    double maxturn = enemy_face_max_speed * dt;
    enemy->angle = enemy->angle + clamp(turn, 0 - maxturn, maxturn);
    while (enemy->angle > pi) {
        enemy->angle = enemy->angle - 2 * pi;
    }
    while (enemy->angle < 0 - pi) {
        enemy->angle = enemy->angle + 2 * pi;
    }
}

static void enemy_track_player(void) {
    if (enemy->seen == 0) {
        enemy->px = player->x;
        enemy->py = player->y;
        enemy->pvx = 0;
        enemy->pvy = 0;
        enemy->seen = 1;
        return;
    }
    double vx = 0;
    double vy = 0;
    if (dt > 0) {
        vx = (player->x - enemy->px) / dt;
        vy = (player->y - enemy->py) / dt;
    }
    double sp = sqrt(vx * vx + vy * vy);
    if (sp > joy_speed) {
        vx = vx / sp * joy_speed;
        vy = vy / sp * joy_speed;
    }
    vx = vx + (rand() % 100 - 50) / 100.0 * 12;
    vy = vy + (rand() % 100 - 50) / 100.0 * 12;
    enemy->pvx = lerp(enemy->pvx, vx, 0.25);
    enemy->pvy = lerp(enemy->pvy, vy, 0.25);
    enemy->px = player->x;
    enemy->py = player->y;
}

static void enemy_predict(double lead) {
    double errx = (rand() % 100 - 50) / 100.0 * 18;
    double erry = (rand() % 100 - 50) / 100.0 * 18;
    aim_tx = clamp(player->x + enemy->pvx * lead + errx, player->size, screen_w - player->size);
    aim_ty = clamp(player->y + enemy->pvy * lead + erry, player->size, screen_h - player->size);
}

static void enemy_turn_prediction(void) {
    enemy_predict(enemy_lead);
    double ta = atan2(aim_ty - enemy->y, aim_tx - enemy->x) + enemy->aim_err;
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

static double enemy_would_hit(void) {
    enemy_predict(enemy_lead);
    double rx = aim_tx - enemy->x;
    double ry = aim_ty - enemy->y;
    double fd = sqrt(rx * rx + ry * ry);
    double pr = player_half();
    double margin = enemy_hit_margin;
    if (sqrt(enemy->pvx * enemy->pvx + enemy->pvy * enemy->pvy) < enemy_still_speed) {
        margin = 0;
    }
    if (fd <= punch_forward_offset + enemy_punch_reach + pr - margin) {
        return 1;
    }
    return 0;
}

static double enemy_in_danger(void) {
    return punch_hits_enemy(player->x, player->y, cos(player->angle), sin(player->angle), punch_reach, punch_width);
}

static double enemy_free_window(void) {
    if (punch_left <= 0 && punch_cd > enemy_punish_react) {
        return 1;
    }
    return 0;
}

static void enemy_pick_flank(void) {
    double side = (enemy->y - player->y) * cos(player->angle) - (enemy->x - player->x) * sin(player->angle);
    if (side >= 0) {
        enemy->strafe = 1;
    } else {
        enemy->strafe = -1;
    }
    if (rand() % 100 < 22) {
        enemy->strafe = 0 - enemy->strafe;
    }
}

static double enemy_roll_cooldown(void) {
    double r = enemy_cooldown_min + (rand() % 1000) / 1000.0 * (enemy_cooldown_max - enemy_cooldown_min);
    enemy->speed_var = 0.85 + (rand() % 100) / 100.0 * enemy_human_speed_var;
    if (r < enemy_cooldown_min) {
        r = enemy_cooldown_min;
    }
    return r;
}
