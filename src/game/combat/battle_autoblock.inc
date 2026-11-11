/* Admin "Auto block" cheat: guard at the exact moment the opponent winds up,
 * then answer with the class ability. On Astra the guard is her purple parry
 * shield (the one that unlocks the strike), not the ordinary blue block.
 * It plays the fight by itself, so the admin can leave the game running. */

static double autoblock_hold;
static double autoblock_was_on;
static double autoblock_skill_pending;
static double autoblock_skill_wait;

/* 1 while the solo bot or a nearby online foe has an active attack. */
static double autoblock_threat(void) {
    if (player == NULL || player->hp <= 0 || finished != 0) {
        return 0;
    }
    if (game_state == ST_SOLO) {
        if (enemy == NULL || enemy->hp <= 0) {
            return 0;
        }
        if (enemy->state == 2) {
            return 1;
        }
        if (enemy_dash_active > 0) {
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

/* Astra's super pad either raises the purple shield or, once the parry
 * connected, fires the unlocked strike. Both are the same fire_super() call. */
static double autoblock_astra_cast_ready(void) {
    if (class_is_astra(player_class) == 0 || astra_state != 0 || player_stun > 0 ||
        player_grabbed() == 1) {
        return 0;
    }
    if (game_state == ST_ONLINE && online_ready != 1) {
        return 0;
    }
    if (astra_grab_ready_t > 0) {
        return 1;                        /* strike unlocked - hit with it */
    }
    if (astra_parry_t > 0 || astra_strike_unlock_t > 0 || astra_cd > 0) {
        return 0;                        /* shield active / unlocking / cooling down */
    }
    return 1;                            /* idle - raise the purple shield */
}

/* 1 once the queued ability has actually started (so we stop waiting). */
static double autoblock_super_active(void) {
    if (class_is_astra(player_class) == 1) {
        return astra_state != 0 ? 1 : 0;
    }
    if (player_class == CLASS_AZUM) {
        return dash_active > 0 ? 1 : 0;
    }
    if (player_class == CLASS_CODECRACKER) {
        return codecracker_super_t > 0 ? 1 : 0;
    }
    return ball.active > 0 || gift->active > 0 ? 1 : 0;
}

static void autoblock_reset(void) {
    player_block = 0;
    autoblock_hold = 0;
    autoblock_skill_pending = 0;
    autoblock_skill_wait = 0;
    autoblock_was_on = 0;
}

static void autoblock_tick(void) {
    /* Runs every battle frame even with the cheat off, so the pulse decays. */
    block_success_t = tick_down(block_success_t);
    autoblock_skill_wait = tick_down(autoblock_skill_wait);

    if (admin_cheat_autoblock() != 1) {
        if (autoblock_was_on == 1) {
            if (autoblock_skill_pending == 1) {
                queued_super = 0;
            }
            autoblock_reset();
        }
        return;
    }
    autoblock_was_on = 1;
    if (game_state != ST_SOLO && game_state != ST_ONLINE) {
        autoblock_reset();
        return;
    }
    if (finished != 0 || player == NULL || player->hp <= 0) {
        autoblock_reset();
        return;
    }

    /* A real guarded hit (blue block or Astra's purple parry) is the signal
     * to drop the guard and answer with the class ability. */
    if (block_success_t > 0) {
        block_success_t = 0;
        player_block = 0;
        blk_id = -1;
        autoblock_hold = 0;
        autoblock_skill_pending = 1;
        autoblock_skill_wait = 2.0;
        autoblock_face_foe();
    }

    if (autoblock_skill_pending == 1) {
        if (autoblock_super_active() == 1 || autoblock_skill_wait <= 0) {
            /* Ability started (or the window expired) - back to guarding. */
            autoblock_skill_pending = 0;
            queued_super = 0;
        } else {
            /* flush_queued_action() fires fire_super() as soon as the class
             * ability is ready - for Astra that is when the parry unlocks the
             * strike. No guarding or punching while we wait for that. */
            autoblock_face_foe();
            queued_super = 1;
            return;
        }
    }

    if (autoblock_threat() == 1) {
        if (class_is_astra(player_class) == 1) {
            if (autoblock_astra_cast_ready() == 1) {
                fire_super();            /* purple shield (or the strike) */
            } else if (astra_parry_t <= 0 && astra_state == 0) {
                player_block = 1;        /* shield on cooldown: ordinary guard */
            }
        } else {
            player_block = 1;
        }
        autoblock_hold = 0.12;
        return;
    }
    autoblock_hold = tick_down(autoblock_hold);
    /* Astra keeps her purple shield; the blue block is only a fallback for
     * the other classes while the last threat still echoes. */
    if (autoblock_hold > 0 && class_is_astra(player_class) == 0) {
        player_block = 1;
        return;
    }
    player_block = 0;
    if (class_is_astra(player_class) == 1 &&
        (astra_parry_t > 0 || astra_strike_unlock_t > 0 || astra_grab_ready_t > 0 || astra_state != 0)) {
        return;                          /* let the parry / strike play out */
    }
    /* With no ability pending, keep the ordinary pressure with punches. */
    if (punch_cd > 0 || punch_left > 0 || player_stun > 0 || player_freeze > 0 ||
        player_grabbed() == 1 || dash_active > 0) {
        return;
    }
    if (autoblock_face_foe() == 1) {
        fire_punch();
    }
}
