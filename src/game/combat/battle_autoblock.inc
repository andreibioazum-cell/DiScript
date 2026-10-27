/* Admin "Auto block" cheat: guard at the exact moment the opponent winds up,
 * then throw a punch as soon as it is safe. It plays the fight by itself, so
 * the admin can leave the game running and go to sleep. */

static double autoblock_hold;
static double autoblock_was_on;

/* 1 while the solo bot or a nearby online foe is in its windup/punch. */
static double autoblock_threat(void) {
    if (player == NULL || player->hp <= 0 || finished != 0) {
        return 0;
    }
    if (game_state == ST_SOLO) {
        if (enemy == NULL || enemy->hp <= 0) {
            return 0;
        }
        if (enemy->state == 1 || enemy->state == 2) {
            return 1;
        }
        if (enemy_super_windup > 0 || enemy_dash_active > 0) {
            return 1;
        }
        return 0;
    }
    if (game_state == ST_ONLINE) {
        double s = 0;
        double near2 = (punch_reach * 3) * (punch_reach * 3);
        while (s < max_players) {
            double rb = s * punch_fields;
            double b = s * remote_fields;
            if (s != net_slot() && arr_get(remotes, b + 5) == 1 && arr_get(remote_punches, rb) > 0) {
                double t = arr_get(remote_punches, rb + 5);
                double dx = arr_get(remotes, b + 1) - player->x;
                double dy = arr_get(remotes, b + 2) - player->y;
                if (t <= PUNCH_WINDUP + punch_time && dx * dx + dy * dy < near2) {
                    return 1;
                }
            }
            s = s + 1;
        }
    }
    return 0;
}

/* Face the closest living foe; return 1 when a punch can reach it. */
static double autoblock_face_foe(void) {
    double bx = 0, by = 0, best = 1e18, d;
    if (game_state == ST_SOLO) {
        if (enemy == NULL || enemy->hp <= 0) {
            return 0;
        }
        bx = enemy->x;
        by = enemy->y;
    } else if (game_state == ST_ONLINE) {
        double s = 0;
        while (s < max_players) {
            double b = s * remote_fields;
            if (s != net_slot() && arr_get(remotes, b + 5) == 1 && arr_get(remotes, b + 4) > 0) {
                double dx = arr_get(remotes, b + 1) - player->x;
                double dy = arr_get(remotes, b + 2) - player->y;
                double q = dx * dx + dy * dy;
                if (q < best) {
                    best = q;
                    bx = arr_get(remotes, b + 1);
                    by = arr_get(remotes, b + 2);
                }
            }
            s = s + 1;
        }
        if (best >= 1e17) {
            return 0;
        }
    } else {
        return 0;
    }
    d = sqrt((bx - player->x) * (bx - player->x) + (by - player->y) * (by - player->y));
    if (d < 0.001) {
        return 0;
    }
    player->angle = atan2(by - player->y, bx - player->x);
    return d <= punch_reach + 34 ? 1 : 0;
}

static void autoblock_tick(void) {
    if (admin_cheat_autoblock() != 1) {
        if (autoblock_was_on == 1) {
            player_block = 0;
            autoblock_hold = 0;
            autoblock_was_on = 0;
        }
        return;
    }
    autoblock_was_on = 1;
    if (game_state != ST_SOLO && game_state != ST_ONLINE) {
        player_block = 0;
        return;
    }
    if (finished != 0 || player == NULL || player->hp <= 0) {
        player_block = 0;
        return;
    }
    if (autoblock_threat() == 1) {
        player_block = 1;
        autoblock_hold = 0.12;
        return;
    }
    autoblock_hold = tick_down(autoblock_hold);
    if (autoblock_hold > 0) {
        player_block = 1;
        return;
    }
    player_block = 0;
    /* Punch back the moment the punch is off cooldown and a foe is in reach. */
    if (punch_cd > 0 || punch_left > 0 || player_stun > 0 || player_freeze > 0 ||
        player_grabbed() == 1 || dash_active > 0 || super_windup > 0) {
        return;
    }
    if (autoblock_face_foe() == 1) {
        fire_punch();
    }
}
