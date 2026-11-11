static double turret_alive(double i) {
    if (arr_get(turret_hp, i) > 0) {
        return 1;
    }
    return 0;
}

static double turret_count_alive(void) {
    double n = 0;
    double i = 0;
    while (i < turret_max) {
        if (turret_alive(i) == 1) {
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

static void push_station_boom(double x, double y) {
    arr_push(station_boom_xs, x);
    arr_push(station_boom_ys, y);
    arr_push(station_boom_ts, station_boom_time);
    if (arr_len(station_boom_ts) > station_booms_max) {
        double keep = station_booms_max;
        DSArray *nx = arr_new();
        DSArray *ny = arr_new();
        DSArray *nt = arr_new();
        double i = arr_len(station_boom_ts) - keep;
        while (i < arr_len(station_boom_ts)) {
            arr_push(nx, arr_get(station_boom_xs, i));
            arr_push(ny, arr_get(station_boom_ys, i));
            arr_push(nt, arr_get(station_boom_ts, i));
            i = i + 1;
        }
        station_boom_xs = nx;
        station_boom_ys = ny;
        station_boom_ts = nt;
    }
}

static void destroy_turret(double i) {
    if (i < 0 || i >= turret_max) {
        return;
    }
    if (turret_alive(i) == 0) {
        return;
    }
    push_station_boom(arr_get(turret_x, i), arr_get(turret_y, i));
    arr_set(turret_hp, i, 0);
    arr_set(turret_maxhp, i, 0);
    arr_set(turret_spawn_t, i, 0);
}

static void damage_turret(double i, double dmg) {
    if (i < 0 || i >= turret_max) {
        return;
    }
    if (turret_alive(i) == 0) {
        return;
    }
    double h = arr_get(turret_hp, i) - dmg;
    if (h <= 0) {
        push_station_boom(arr_get(turret_x, i), arr_get(turret_y, i));
        arr_set(turret_hp, i, 0);
        arr_set(turret_maxhp, i, 0);
        arr_set(turret_spawn_t, i, 0);
        return;
    }
    arr_set(turret_hp, i, h);
}

static void place_turret(void) {
    double slot = -1;
    double i = 0;
    while (i < turret_max) {
        if (turret_alive(i) == 0) {
            slot = i;
            i = turret_max;
        }
        i = i + 1;
    }
    if (slot < 0) {
        i = 0;
        while (i < turret_max) {
            if (turret_alive(i) == 1) {
                slot = i;
                i = turret_max;
            }
            i = i + 1;
        }
        if (slot < 0) {
            return;
        }
        destroy_turret(slot);
    }
    arr_set(turret_x, slot, player->x);
    arr_set(turret_y, slot, player->y);
    double mh = ebuc_station_hp_for(player_level);
    arr_set(turret_maxhp, slot, mh);
    arr_set(turret_hp, slot, mh);
    arr_set(turret_spawn_t, slot, station_spawn_time);
    last_turret_slot = slot;
    if (player_level >= 2) {
        player->hp = clamp(player->hp + ebuc_place_heal, 0, player->max_hp);
    }
}

static void tick_station_boom(void) {
    double i = 0;
    while (i < arr_len(station_boom_ts)) {
        arr_set(station_boom_ts, i, tick_down(arr_get(station_boom_ts, i)));
        i = i + 1;
    }
}

static void tick_turrets(void) {
    tick_station_boom();
    double i = 0;
    while (i < turret_max) {
        arr_set(turret_spawn_t, i, tick_down(arr_get(turret_spawn_t, i)));
        i = i + 1;
    }
    if (player_class == CLASS_EBUC && player_level >= 3 && turret_count_alive() > 0 && finished == 0 &&
        player->hp > 0) {
        turret_heal_t = turret_heal_t + dt;
        if (turret_heal_t >= 1) {
            turret_heal_t = turret_heal_t - 1;
            player->hp = clamp(player->hp + ebuc_beam_heal, 0, player->max_hp);
        }
    } else {
        turret_heal_t = 0;
    }
}
