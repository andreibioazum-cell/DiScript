static double enemy_turret_alive(double i) {
    if (i < 0 || i >= turret_max) {
        return 0;
    }
    if (arr_get(enemy_turret_hp, i) > 0) {
        return 1;
    }
    return 0;
}

static double enemy_turret_count_alive(void) {
    double n = 0;
    double i = 0;
    while (i < turret_max) {
        if (enemy_turret_alive(i) == 1) {
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

static void enemy_destroy_turret(double i) {
    if (enemy_turret_alive(i) == 0) {
        return;
    }
    push_station_boom(arr_get(enemy_turret_x, i), arr_get(enemy_turret_y, i));
    arr_set(enemy_turret_hp, i, 0);
    arr_set(enemy_turret_maxhp, i, 0);
    arr_set(enemy_turret_spawn_t, i, 0);
}

static void enemy_damage_turret(double i, double dmg) {
    if (enemy_turret_alive(i) == 0) {
        return;
    }
    double h = arr_get(enemy_turret_hp, i) - dmg;
    if (h <= 0) {
        push_station_boom(arr_get(enemy_turret_x, i), arr_get(enemy_turret_y, i));
        arr_set(enemy_turret_hp, i, 0);
        arr_set(enemy_turret_maxhp, i, 0);
        arr_set(enemy_turret_spawn_t, i, 0);
        return;
    }
    arr_set(enemy_turret_hp, i, h);
}

static void enemy_place_turret(void) {
    double slot = -1;
    double i = 0;
    while (i < turret_max) {
        if (enemy_turret_alive(i) == 0) {
            slot = i;
            i = turret_max;
        }
        i = i + 1;
    }
    if (slot < 0) {
        i = 0;
        while (i < turret_max) {
            if (enemy_turret_alive(i) == 1) {
                slot = i;
                i = turret_max;
            }
            i = i + 1;
        }
        if (slot < 0) {
            return;
        }
        enemy_destroy_turret(slot);
    }
    arr_set(enemy_turret_x, slot, enemy->x);
    arr_set(enemy_turret_y, slot, enemy->y);
    double mh = ebuc_station_hp;
    arr_set(enemy_turret_maxhp, slot, mh);
    arr_set(enemy_turret_hp, slot, mh);
    arr_set(enemy_turret_spawn_t, slot, station_spawn_time);
    enemy_last_turret_slot = slot;
    ds_log("enemy turret placed, alive %g/%g", enemy_turret_count_alive(), turret_max);
}

static double enemy_punch_turret_target(double px, double py, double pdx, double pdy) {
    double best = -1;
    double bfw = 0;
    double i = 0;
    double bw = along(px, py, pdx, pdy, enemy->x, enemy->y);
    while (i < turret_max) {
        if (enemy_turret_alive(i) == 1) {
            double hx = arr_get(enemy_turret_x, i);
            double hy = arr_get(enemy_turret_y, i);
            if (punch_hits_turret(px, py, pdx, pdy, punch_reach, punch_width, hx, hy) == 1) {
                double tf = along(px, py, pdx, pdy, hx, hy);
                if (tf <= bw + 30) {
                    if (best == -1 || tf < bfw) {
                        best = i + 1;
                        bfw = tf;
                    }
                }
            }
        }
        i = i + 1;
    }
    return best;
}

static void enemy_tick_turrets(void) {
    double i = 0;
    while (i < turret_max) {
        arr_set(enemy_turret_spawn_t, i, tick_down(arr_get(enemy_turret_spawn_t, i)));
        i = i + 1;
    }
}

static void draw_enemy_turrets(void) {
    double beam = -1;
    if (station_beam_visible(enemy->hp) == 1) {
        if (enemy_turret_alive(enemy_last_turret_slot) == 1) {
            beam = enemy_last_turret_slot;
        } else {
            double j = 0;
            while (j < turret_max) {
                if (enemy_turret_alive(j) == 1) {
                    beam = j;
                    j = turret_max;
                }
                j = j + 1;
            }
        }
    }
    if (beam >= 0) {
        draw_station_beam(arr_get(enemy_turret_x, beam), arr_get(enemy_turret_y, beam), enemy->x, enemy->y);
    }
    double i = 0;
    while (i < turret_max) {
        if (enemy_turret_alive(i) == 1) {
            draw_turret_at(arr_get(enemy_turret_x, i), arr_get(enemy_turret_y, i), arr_get(enemy_turret_hp, i),
                           arr_get(enemy_turret_maxhp, i), arr_get(enemy_turret_spawn_t, i));
        }
        i = i + 1;
    }
}
