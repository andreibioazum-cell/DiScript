
static double punch_resolve_target(double px, double py, double pdx, double pdy, double reach, double width) {
    double best = -1;
    double bfw = 0;
    double i = 0;
    if (punch_hits_player(px, py, pdx, pdy, reach, width) == 1) {
        best = 0;
        bfw = along(px, py, pdx, pdy, player->x, player->y);
    }
    while (i < turret_max) {
        if (turret_alive(i) == 1) {
            double hx = arr_get(turret_x, i);
            double hy = arr_get(turret_y, i);
            if (punch_hits_turret(px, py, pdx, pdy, reach, width, hx, hy) == 1) {
                double tf = along(px, py, pdx, pdy, hx, hy);
                if (best == -1) {
                    best = i + 1;
                    bfw = tf;
                } else if (best == 0) {
                    if (tf <= bfw + 30) {
                        best = i + 1;
                        bfw = tf;
                    }
                } else if (tf < bfw - 1) {
                    best = i + 1;
                    bfw = tf;
                }
            }
        }
        i = i + 1;
    }
    return best;
}

static double dash_hit_radius_solo(void) { return player_half(); }

static double dash_hit_radius_remote(void) { return player_half(); }

static double dash_resolve_target(double sx, double sy, double dx, double dy, double len) {
    double best = -1;
    double bd = 0;
    double pr = dash_hit_radius_remote();
    if (strip_hits_player(sx, sy, dx, dy, 0, len, pr) == 1) {
        best = 0;
        bd = along(sx, sy, dx, dy, player->x, player->y);
    }
    double i = 0;
    while (i < turret_max) {
        if (turret_alive(i) == 1) {
            double hx = arr_get(turret_x, i);
            double hy = arr_get(turret_y, i);
            if (strip_hits_turret(sx, sy, dx, dy, 0, len, pr, hx, hy) == 1) {
                double td = along(sx, sy, dx, dy, hx, hy);
                if (best == -1) {
                    best = i + 1;
                    bd = td;
                } else if (best == 0) {
                    if (td <= bd + 20) {
                        best = i + 1;
                        bd = td;
                    }
                } else if (td < bd - 1) {
                    best = i + 1;
                    bd = td;
                }
            }
        }
        i = i + 1;
    }
    return best;
}
