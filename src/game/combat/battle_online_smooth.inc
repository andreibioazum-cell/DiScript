/* Dead-reckon and smooth remote fighter positions between network snapshots. */

#define RPRED_MAX_SPEED 1350.0
#define RPRED_LEAD_MAX 0.085

static double rpred_vx[8], rpred_vy[8];
static double rpred_lx[8], rpred_ly[8], rpred_dt[8];
static int rpred_init[8];

static void reset_remote_smooth_slot(int si) {
    if (si < 0 || si >= 8) {
        return;
    }
    rpred_vx[si] = 0;
    rpred_vy[si] = 0;
    rpred_lx[si] = 0;
    rpred_ly[si] = 0;
    rpred_dt[si] = 0;
    rpred_init[si] = 0;
}

static void reset_remote_smooth(void) {
    int i = 0;
    while (i < 8) {
        reset_remote_smooth_slot(i);
        i = i + 1;
    }
}

static void smooth_remotes(void) {
    double ph = player_half();
    double alpha = 1.0 - exp(0 - clamp(dt, 0, 0.1) * 22.0);
    double s = 0;
    while (s < max_players) {
        int si = (int)s;
        double b = s * remote_fields;
        if (arr_get(remotes, b + 5) == 1) {
            double rx = arr_get(remotes, b + 1);
            double ry = arr_get(remotes, b + 2);
            double ra = arr_get(remotes, b + 3);
            double nx = arr_get(remotes, b + 7);
            double ny = arr_get(remotes, b + 8);
            double na = finite_or(arr_get(remotes, b + 9), 0);
            ra = finite_or(ra, na);
            if (!rpred_init[si]) {
                rpred_init[si] = 1;
                rpred_lx[si] = nx;
                rpred_ly[si] = ny;
                rpred_vx[si] = 0;
                rpred_vy[si] = 0;
                rpred_dt[si] = 0;
                rx = nx;
                ry = ny;
                ra = na;
            } else if (nx != rpred_lx[si] || ny != rpred_ly[si]) {
                double dx = nx - rpred_lx[si];
                double dy = ny - rpred_ly[si];
                double jump = sqrt(dx * dx + dy * dy);
                if (jump > remote_snap) {
                    rpred_vx[si] = 0;
                    rpred_vy[si] = 0;
                    rx = nx;
                    ry = ny;
                    ra = na;
                } else {
                    double step = clamp(rpred_dt[si], 0.025, 0.35);
                    double ivx = dx / step;
                    double ivy = dy / step;
                    double spd = sqrt(ivx * ivx + ivy * ivy);
                    double blend = (ivx * rpred_vx[si] + ivy * rpred_vy[si] < 0) ? 0.85 : 0.6;
                    if (spd > RPRED_MAX_SPEED) {
                        ivx = ivx * (RPRED_MAX_SPEED / spd);
                        ivy = ivy * (RPRED_MAX_SPEED / spd);
                    }
                    rpred_vx[si] = lerp(rpred_vx[si], ivx, blend);
                    rpred_vy[si] = lerp(rpred_vy[si], ivy, blend);
                }
                rpred_lx[si] = nx;
                rpred_ly[si] = ny;
                rpred_dt[si] = 0;
            } else {
                rpred_dt[si] = rpred_dt[si] + dt;
                if (rpred_dt[si] > RPRED_LEAD_MAX) {
                    double decay = exp(0 - clamp(dt, 0, 0.1) * 18.0);
                    rpred_vx[si] = rpred_vx[si] * decay;
                    rpred_vy[si] = rpred_vy[si] * decay;
                }
            }
            double lead = clamp(rpred_dt[si], 0, RPRED_LEAD_MAX);
            double ex = nx + rpred_vx[si] * lead;
            double ey = ny + rpred_vy[si] * lead;
            if (screen_w > ph * 2) {
                ex = clamp(ex, ph, screen_w - ph);
            }
            if (screen_h > ph * 2) {
                ey = clamp(ey, ph, screen_h - ph);
            }
            if (dist(nx, ny, ex, ey) > remote_snap) {
                ex = nx;
                ey = ny;
            }
            if (dist(rx, ry, ex, ey) > remote_snap) {
                rx = ex;
                ry = ey;
                ra = na;
            } else {
                double step_dt = clamp(dt, 0, 0.1) * 0.65;
                rx = lerp(rx + rpred_vx[si] * step_dt, ex, alpha);
                ry = lerp(ry + rpred_vy[si] * step_dt, ey, alpha);
                double da = na - ra;
                if (da > pi) {
                    da = da - 2 * pi;
                }
                if (da < 0 - pi) {
                    da = da + 2 * pi;
                }
                ra = ra + da * alpha;
                tick_remote_dust(s, rx, ry, nx, ny);
            }
            arr_set(remotes, b + 1, rx);
            arr_set(remotes, b + 2, ry);
            arr_set(remotes, b + 3, ra);
        } else {
            reset_remote_smooth_slot(si);
        }
        s = s + 1;
    }
}
