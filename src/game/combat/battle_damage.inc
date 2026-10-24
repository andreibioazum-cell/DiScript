/* The screen shakes on every damage event that lands, not on ability casts:
 * punches, supers, poison ticks - if health drops, the camera taps. */
static void damage_taken_shake(double dmg) {
    double amp = 3 + dmg * 1.2;
    if (amp > 7) {
        amp = 7;
    }
    ds_gfx_shake(amp, 0.12);
}

/* Does the raised shield cover this hit? Only strikes from the front do;
 * sources behind the fighter punch through the guard at full force. */
static double block_covers(void) {
    if (dmg_from_k == 0) {
        return 1;
    }
    double a = atan2(dmg_from_y - player->y, dmg_from_x - player->x);
    double da = a - player->angle;
    while (da > pi) {
        da = da - 2 * pi;
    }
    while (da < 0 - pi) {
        da = da + 2 * pi;
    }
    return fabs(da) <= block_front_angle;
}

static double take_damage(double dmg) {
    /* God mode from the admin panel: the hit never lands at all. */
    if (admin_cheat_god() == 1) {
        dmg_from_k = 0;
        return 0;
    }
        /* Track the hit before ebuC's turrets absorb it: shielded damage still
     * disqualifies the no-damage victory achievement. */
    if (dmg > 0) {
        player_was_hit = 1;
    }
    double real = dmg;
    /* Held guard cuts every ability that reaches it from the front. */
    if (player_block == 1 && block_covers() == 1) {
        real = real * block_guard;
    }
    dmg_from_k = 0;
    if (player_class == CLASS_CODECRACKER && codecracker_super_t > 0) {
        /* The running overclock absorbs half of everything that lands. */
        real = real * (1 - codecracker_absorb_for(player_level));
    }
    if (player_class == CLASS_EBUC) {
        real = player_shield_absorb(real);
    }
    if (player_class == CLASS_AZUM && azum_revived >= 1 && player_level >= 1) {
        real = real * (1 - clamp(azum_second_life_damage_reduction, 0, 0.9));
    }
    if (real > 0) {
        player_hurt_a = 1;
        damage_taken_shake(real);
    }
    player->hp = clamp(player->hp - real, 0, player->max_hp);
    if (player->hp <= 0) {
        player->hp = 0;
        if (try_revive() == 0) {
            finish_game(2);
            return 1;
        }
    }
    return 0;
}

/* Same as take_damage but remembers where the strike came from, so the shield
 * can tell a frontal hit from a backstab. */
static double take_damage_at(double dmg, double sx, double sy) {
    dmg_from_x = sx;
    dmg_from_y = sy;
    dmg_from_k = 1;
    return take_damage(dmg);
}

static void heal_after_attack(void) {
    double amount = 0;
    if (player_class == CLASS_AZUM && player_level >= 3) {
        amount = azum_attack_heal;
    } else if (player_class == CLASS_ORDINARY && player_level >= 3) {
        amount = ordinary_attack_heal;
    }
    if (amount > 0) {
        player->hp = clamp(player->hp + amount, 0, player->max_hp);
    }
}

static void stun_enemy(double t) {
    if (t > enemy->stun) {
        enemy->stun = t;
    }
        /* A real stun from the game: the admin stun mark is dropped and the ring
         * shows the battle stun again. */
    cheat_estun_mark = 0;
    if (enemy->state >= 1 && enemy->state <= 3) {
        enemy->state = 0;
        enemy->state_time = 0;
        enemy->cooldown = 0.25;
        enemy->think = 0.15;
    }
    enemy_super_windup = 0;
    enemy_super_kind = 0;
}

static void stun_player(double t) {
    if (t > player_stun) {
        player_stun = t;
    }
}

static double try_revive(void) {
    if (player->hp > 0) {
        return 0;
    }
    if (player_class != CLASS_AZUM) {
        return 0;
    }
    if (azum_revived >= (azum_buff_active == 1 ? 2 : 1)) {
        return 0;
    }
    azum_revived = azum_revived + 1;
    player->hp = player->max_hp;
    revive_prot = 1.2;
    double k = 0;
    while (k < 50) {
        double rx = 70 + rand() % (screen_w - 140);
        double ry = 70 + rand() % (screen_h - 140);
        if (dist(rx, ry, enemy->x, enemy->y) > 200) {
            k = 50;
            player->x = rx;
            player->y = ry;
                        /* Revival moves the cube: the old momentum must not carry it away
                         * from the new spot. */
            player_vel_reset();
        } else {
            k = k + 1;
        }
    }
    azum_revive_count = azum_revive_count + 1;
    net_save_azum_revives(azum_revive_count);
    ds_log("azum revived (%g/%g)", azum_revive_count, ACH_LEGENDS_NEED);
    if (azum_revive_count >= ACH_LEGENDS_NEED) {
        grant_achievement(ACH_LEGENDS);
    }
    return 1;
}

static void tick_revive(void) {
    if (revive_prot > 0) {
        revive_prot = revive_prot - dt;
        if (revive_prot < 0) {
            revive_prot = 0;
        }
    }
}

static void tick_hurt_fades(void) {
    player_hurt_a = fade_down_a(player_hurt_a, hurt_fade);
    enemy_hurt_a = fade_down_a(enemy_hurt_a, hurt_fade);
}

static void tick_alert_hold(void) {
    if (finished != 0 && finish_t < alert_hold) {
        finish_t = finish_t + dt;
        if (finish_t > alert_hold) {
            finish_t = alert_hold;
        }
    }
}
