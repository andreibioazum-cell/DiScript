static void dust_spawn(double px, double py, double jdx, double jdy) {
    double i = 0;
    double base = 0;
    double found = 0;
    while (i < dust_max) {
        base = i * dust_fields;
        if (arr_get(dust, base + 4) <= 0) {
            found = 1;
            i = dust_max;
        }
        i = i + 1;
    }
    if (found == 0) {
        return;
    }
    double angle = (rand() % 628) / 100.0;
    double vx = cos(angle) * dust_spd * dust_side_share - jdx * dust_spd * dust_back_share;
    double vy = sin(angle) * dust_spd * dust_side_share - jdy * dust_spd * dust_back_share;
    arr_set(dust, base, px);
    arr_set(dust, base + 1, py);
    arr_set(dust, base + 2, vx);
    arr_set(dust, base + 3, vy);
    arr_set(dust, base + 4, dust_life_max);
}

static void update_dust(void) {
    double i = 0;
    double base = 0;
    while (i < dust_max) {
        base = i * dust_fields;
        double lf = arr_get(dust, base + 4);
        if (lf > 0) {
            arr_set(dust, base, arr_get(dust, base) + arr_get(dust, base + 2) * dt);
            arr_set(dust, base + 1, arr_get(dust, base + 1) + arr_get(dust, base + 3) * dt);
            arr_set(dust, base + 2, arr_get(dust, base + 2) * 0.88);
            arr_set(dust, base + 3, arr_get(dust, base + 3) * 0.88);
            arr_set(dust, base + 4, lf - dt);
        }
        i = i + 1;
    }
}

static void draw_dust(void) {
    double i = 0;
    double base = 0;
    while (i < dust_max) {
        base = i * dust_fields;
        double lf = arr_get(dust, base + 4);
        if (lf > 0) {
            double t = lf / dust_life_max;
            double fade = clamp(t / 0.5, 0, 1);
            double r = dust_r_max * (0.45 + 0.55 * fade);
            double a = floor(fade * fade * 150);
            if (a > 0) {
                double grey = floor(120 + 100 * t);
                double c = ((((unsigned)(int)a) << 24) | (((unsigned)(int)grey) << 16) | (((unsigned)(int)grey) << 8) |
                            ((unsigned)(int)grey));
                circle(arr_get(dust, base), arr_get(dust, base + 1), r, c);
            }
        }
        i = i + 1;
    }
}

static void dust_trail(double px, double py, double jdx, double jdy) {
    dust_spawn(px, py, jdx, jdy);
    if (rand() % 2 == 0) {
        dust_spawn(px - jdx * dust_back_off * 0.4, py - jdy * dust_back_off * 0.4, jdx, jdy);
    }
}

static void tick_dust(double jdx, double jdy, double jmag) {
    if (finished != 0) {
        return;
    }
    if (jmag < 0.18) {
        return;
    }
    if (dust_cd > 0) {
        dust_cd = dust_cd - dt;
        return;
    }
    dust_cd = dust_spawn_cd;
    dust_trail(player->x - jdx * dust_back_off, player->y - jdy * dust_back_off, jdx, jdy);
}

static void tick_enemy_dust(double vx, double vy) {
    if (finished != 0 || enemy->hp <= 0) {
        return;
    }
    double mag = sqrt(vx * vx + vy * vy);
    if (mag < dust_enemy_min_speed) {
        return;
    }
    if (enemy_dust_cd > 0) {
        enemy_dust_cd = enemy_dust_cd - dt;
        return;
    }
    enemy_dust_cd = dust_spawn_cd;
    double dx = vx / mag;
    double dy = vy / mag;
    dust_trail(enemy->x - dx * dust_back_off, enemy->y - dy * dust_back_off, dx, dy);
}

static void tick_remote_dust(double s, double x, double y, double tx, double ty) {
    if (finished != 0) {
        return;
    }
    double vdx = tx - x;
    double vdy = ty - y;
    double est = sqrt(vdx * vdx + vdy * vdy) * remote_smooth;
    if (est < dust_enemy_min_speed || est > 900) {
        return;
    }
    if (arr_get(remote_dust_cd, s) > 0) {
        arr_set(remote_dust_cd, s, arr_get(remote_dust_cd, s) - dt);
        return;
    }
    arr_set(remote_dust_cd, s, dust_spawn_cd);
    double dl = sqrt(vdx * vdx + vdy * vdy);
    double dx = vdx / dl;
    double dy = vdy / dl;
    dust_trail(x - dx * dust_back_off, y - dy * dust_back_off, dx, dy);
}
