/* Extrapolate and smooth remote fighter positions between network snapshots. */

static double rpred_vx[8], rpred_vy[8];
static double rpred_lx[8], rpred_ly[8], rpred_dt[8];

static void smooth_remotes(void) {
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
            if (nx != rpred_lx[si] || ny != rpred_ly[si]) {
                /* A fresh snapshot: derive the speed it implies. */
                double step = rpred_dt[si];
                if (step < 0.05) {
                    step = 0.05;
                }
                if (step > 0.5) {
                    step = 0.5;
                }
                rpred_vx[si] = lerp(rpred_vx[si], (nx - rpred_lx[si]) / step, 0.6);
                rpred_vy[si] = lerp(rpred_vy[si], (ny - rpred_ly[si]) / step, 0.6);
                rpred_lx[si] = nx;
                rpred_ly[si] = ny;
                rpred_dt[si] = 0;
            } else {
                rpred_dt[si] = rpred_dt[si] + dt;
            }
            double ex = nx + rpred_vx[si] * 0.12;
            double ey = ny + rpred_vy[si] * 0.12;
            /* Clamp extrapolated position within remote_snap radius. */
            if (dist(nx, ny, ex, ey) > remote_snap) {
                ex = nx;
                ey = ny;
            }
            if (dist(rx, ry, ex, ey) > remote_snap) {
                rx = ex;
                ry = ey;
                ra = na;
            } else {
                rx = lerp(rx, ex, clamp(dt * remote_smooth, 0, 1));
                ry = lerp(ry, ey, clamp(dt * remote_smooth, 0, 1));
                double da = na - ra;
                if (da > pi) {
                    da = da - 2 * pi;
                }
                if (da < 0 - pi) {
                    da = da + 2 * pi;
                }
                ra = ra + da * clamp(dt * remote_smooth, 0, 1);
                tick_remote_dust(s, rx, ry, nx, ny);
            }
            arr_set(remotes, b + 1, rx);
            arr_set(remotes, b + 2, ry);
            arr_set(remotes, b + 3, ra);
        }
        s = s + 1;
    }
}
