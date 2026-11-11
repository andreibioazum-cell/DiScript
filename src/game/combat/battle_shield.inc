static double pierce_has(double mask, double bit) {
    double shifted = floor(mask / bit);
    return shifted - floor(shifted * 0.5) * 2;
}

static double turret_pierce_bit(double i) {
    if (i == 1) {
        return 2;
    }
    if (i == 2) {
        return 4;
    }
    return 1;
}

static double nearest_turret(DSArray *xs, DSArray *ys, DSArray *hps, double x, double y) {
    double best = -1;
    double bd = 0;
    double i = 0;
    while (i < turret_max) {
        if (arr_get(hps, i) > 0) {
            double d = dist(arr_get(xs, i), arr_get(ys, i), x, y);
            if (best == -1 || d < bd) {
                best = i;
                bd = d;
            }
        }
        i = i + 1;
    }
    return best;
}

static double shield_absorb_turrets(DSArray *xs, DSArray *ys, DSArray *hps, double x, double y, double mine,
                                    double dmg) {
    if (dmg <= 0) {
        return 0;
    }
    double left = dmg;
    while (left > 0) {
        double best = nearest_turret(xs, ys, hps, x, y);
        if (best < 0) {
            return left;
        }
        double take = arr_get(hps, best);
        if (left < take) {
            take = left;
        }
        if (mine == 1) {
            damage_turret(best, take);
        } else {
            enemy_damage_turret(best, take);
        }
        left = left - take;
    }
    return 0;
}

static double player_shield_absorb(double dmg) {
    if (player_class != CLASS_EBUC) {
        return dmg;
    }
    return shield_absorb_turrets(turret_x, turret_y, turret_hp, player->x, player->y, 1, dmg);
}

static double enemy_shield_absorb(double dmg) {
    if (enemy_class != CLASS_EBUC) {
        return dmg;
    }
    return shield_absorb_turrets(enemy_turret_x, enemy_turret_y, enemy_turret_hp, enemy->x, enemy->y, 0, dmg);
}

static void snow_chip_turrets(DSArray *xs, DSArray *ys, DSArray *hps, double mine, double x, double y) {
    double i = 0;
    double sr = snow_radius_for(mine == 1 ? gift->shot : enemy_gift->shot);
    while (i < turret_max) {
        if (arr_get(hps, i) > 0) {
            double bit = turret_pierce_bit(i);
            double pierce = enemy_gift->pierce;
            if (mine == 1) {
                pierce = gift->pierce;
            }
            if (pierce_has(pierce, bit) == 0) {
                if (circle_hits_turret(x, y, sr, arr_get(xs, i), arr_get(ys, i)) == 1) {
                    if (mine == 1) {
                        gift->pierce = gift->pierce + bit;
                        enemy_damage_turret(i, santa_super_damage);
                        ds_log("snow pierced enemy turret %g", i);
                    } else {
                        enemy_gift->pierce = enemy_gift->pierce + bit;
                        damage_turret(i, santa_super_damage * enemy_damage_mult_for(enemy_class));
                        ds_log("enemy snow pierced turret %g", i);
                    }
                }
            }
        }
        i = i + 1;
    }
}

static void snow_chip_enemy_turrets(void) {
    if (gift->active == 0) {
        return;
    }
    snow_chip_turrets(enemy_turret_x, enemy_turret_y, enemy_turret_hp, 1, gift->x, gift->y);
}

static void snow_chip_player_turrets(void) {
    if (enemy_gift->active == 0) {
        return;
    }
    snow_chip_turrets(turret_x, turret_y, turret_hp, 0, enemy_gift->x, enemy_gift->y);
}
