/* Remote ebuC's projectile simulation and admin bounce-path preview. */
#define REMOTE_BRICKS 4

static double rb_on[REMOTE_BRICKS], rb_shot[REMOTE_BRICKS];
static double rb_x[REMOTE_BRICKS], rb_y[REMOTE_BRICKS], rb_vx[REMOTE_BRICKS], rb_vy[REMOTE_BRICKS];
static double rb_bounces[REMOTE_BRICKS], rb_t[REMOTE_BRICKS], rb_spin[REMOTE_BRICKS];
static double rb_boom_x[REMOTE_BRICKS], rb_boom_y[REMOTE_BRICKS], rb_boom_t[REMOTE_BRICKS];
static double rb_trail_x[REMOTE_BRICKS][BALL_TRAIL], rb_trail_y[REMOTE_BRICKS][BALL_TRAIL];
static double rb_trail_a[REMOTE_BRICKS][BALL_TRAIL];

/* True when a bounce segment [x,y -> nx,ny] (or its terminal blast) touches a target. */
static double ball_seg_hits_target(double x, double y, double nx, double ny, double end_r, double check_foe) {
    double slen = sqrt((nx - x) * (nx - x) + (ny - y) * (ny - y));
    double ux = slen > 1e-6 ? (nx - x) / slen : 1;
    double uy = slen > 1e-6 ? (ny - y) / slen : 0;
    if (finished != 0) {
        return 0;
    }
    if (check_foe == 0) {
        if (player->hp <= 0) {
            return 0;
        }
        if (slen > 0 && strip_hits_player(x, y, ux, uy, 0, slen, BALL_RADIUS) == 1) {
            return 1;
        }
        return circle_hits_player(x, y, BALL_RADIUS) == 1 || circle_hits_player(nx, ny, end_r) == 1;
    }
    if (game_state == ST_SOLO) {
        if (enemy->hp <= 0) {
            return 0;
        }
        if (slen > 0 && strip_hits_enemy(x, y, ux, uy, 0, slen, BALL_RADIUS) == 1) {
            return 1;
        }
        return circle_hits_enemy(x, y, BALL_RADIUS) == 1 || circle_hits_enemy(nx, ny, end_r) == 1;
    }
    if (game_state == ST_ONLINE) {
        double i = 0;
        double rh = player_half();
        while (i < max_players) {
            double b = i * remote_fields;
            if (i != net_slot() && arr_get(remotes, b + 5) == 1 && net_player_hp(i) > 0) {
                double rx = arr_get(remotes, b + 1);
                double ry = arr_get(remotes, b + 2);
                double ra = arr_get(remotes, b + 3);
                if ((slen > 0 && strip_hits_box(x, y, ux, uy, 0, slen, BALL_RADIUS, rx, ry, ra, rh) == 1) ||
                    circle_hits_box(x, y, BALL_RADIUS, rx, ry, ra, rh) == 1 ||
                    circle_hits_box(nx, ny, end_r, rx, ry, ra, rh) == 1) {
                    return 1;
                }
            }
            i = i + 1;
        }
    }
    return 0;
}

/* Draw predicted bounce segments; highlight enemy-intersecting segments in red. */
static void draw_ball_path_from(double x0, double y0, double vx0, double vy0, double bounces0, double t0,
                                uint32_t rgb, double check_foe) {
    double min_x = BALL_RADIUS, max_x = screen_w - BALL_RADIUS;
    double min_y = BALL_RADIUS, max_y = screen_h - BALL_RADIUS;
    double x, y, vx = vx0, vy = vy0, b = bounces0, t = t0, seg = 0;
    uint32_t last_rgb = rgb & 0x00FFFFFFu;
    if (max_x <= min_x || max_y <= min_y || (fabs(vx) < 1e-6 && fabs(vy) < 1e-6)) {
        return;
    }
    x = clamp(x0, min_x, max_x);
    y = clamp(y0, min_y, max_y);
    while (b <= BALL_MAX_BOUNCES && t < BALL_MAX_TIME && seg < 16) {
        double tx = vx > 1e-6 ? (max_x - x) / vx : (vx < -1e-6 ? (min_x - x) / vx : 1e9);
        double ty = vy > 1e-6 ? (max_y - y) / vy : (vy < -1e-6 ? (min_y - y) / vy : 1e9);
        double dt_wall = tx < ty ? tx : ty;
        double dt_rem = BALL_MAX_TIME - t;
        double dt_seg, nx, ny, slen, next_b, end_r, hits;
        int hit_cap = 0;
        int alpha = 220 - (int)(seg * 9);
        uint32_t seg_rgb, col;
        if (dt_wall < 0) {
            dt_wall = 0;
        }
        dt_seg = dt_wall;
        if (dt_rem < dt_seg) {
            dt_seg = dt_rem;
            hit_cap = 1;
        }
        nx = clamp(x + vx * dt_seg, min_x, max_x);
        ny = clamp(y + vy * dt_seg, min_y, max_y);
        next_b = b;
        if (hit_cap == 0) {
            if (fabs(dt_wall - tx) <= 1e-6) {
                next_b = next_b + 1;
            }
            if (fabs(dt_wall - ty) <= 1e-6) {
                next_b = next_b + 1;
            }
        }
        end_r = (hit_cap == 1 || next_b > BALL_MAX_BOUNCES) ? BALL_BOOM_RADIUS : BALL_RADIUS;
        hits = ball_seg_hits_target(x, y, nx, ny, end_r, check_foe);
        if (alpha < 110) {
            alpha = 110;
        }
        if (hits == 1) {
            seg_rgb = 0x00FF2D2Du;
            alpha = 245;
        } else {
            seg_rgb = rgb & 0x00FFFFFFu;
        }
        last_rgb = seg_rgb;
        col = ((uint32_t)alpha << 24) | seg_rgb;
        line(x, y, nx, ny, 2.5, col);
        slen = sqrt((nx - x) * (nx - x) + (ny - y) * (ny - y));
        if (slen >= 36) {
            double ux = (nx - x) / slen;
            double uy = (ny - y) / slen;
            double mx = x + (nx - x) * 0.56;
            double my = y + (ny - y) * 0.56;
            line(mx, my, mx - ux * 11 - uy * 6.5, my - uy * 11 + ux * 6.5, 2.5, col);
            line(mx, my, mx - ux * 11 + uy * 6.5, my - uy * 11 - ux * 6.5, 2.5, col);
        }
        circle(nx, ny, 4, col);
        x = nx;
        y = ny;
        t = t + dt_seg;
        seg = seg + 1;
        if (hit_cap == 1) {
            break;
        }
        if (fabs(dt_wall - tx) <= 1e-6) {
            vx = (0 - vx) * BALL_BOUNCE_ACCEL;
            vy = vy * BALL_BOUNCE_ACCEL;
            b = b + 1;
        }
        if (fabs(dt_wall - ty) <= 1e-6) {
            vx = vx * BALL_BOUNCE_ACCEL;
            vy = (0 - vy) * BALL_BOUNCE_ACCEL;
            b = b + 1;
        }
    }
    ring(x, y, 8, 2, (190u << 24) | last_rgb);
}

static void remote_bricks_reset(void) {
    int i = 0, k;
    while (i < REMOTE_BRICKS) {
        rb_on[i] = 0;
        rb_shot[i] = 0;
        rb_boom_x[i] = 0;
        rb_boom_y[i] = 0;
        rb_boom_t[i] = 0;
        for (k = 0; k < BALL_TRAIL; k++) {
            rb_trail_a[i][k] = 0;
        }
        i = i + 1;
    }
}

static void baseline_remote_slot(double s) {
    int idx = (int)s;
    arr_set(remote_snow, s * snow_fields, net_player_snow(s));
    arr_set(remote_universe, s * universe_fields, net_player_universe(s));
    remote_dash_set(s, DASH_GEN, net_player_dash(s));
    arr_set(remote_thuds, s, net_player_thud(s));
    if (idx >= 0 && idx < REMOTE_BRICKS) {
        rb_shot[idx] = net_player_brick(s);
    }
}

static void rb_trail_push(int s, double x, double y) {
    int k = BALL_TRAIL - 1;
    while (k > 0) {
        rb_trail_x[s][k] = rb_trail_x[s][k - 1];
        rb_trail_y[s][k] = rb_trail_y[s][k - 1];
        rb_trail_a[s][k] = rb_trail_a[s][k - 1];
        k = k - 1;
    }
    rb_trail_x[s][0] = x + (rand() % 21 - 10);
    rb_trail_y[s][0] = y + (rand() % 21 - 10);
    rb_trail_a[s][0] = 1;
}

static void rb_burst(int s, double x, double y) {
    double b = rb_bounces[s] > BALL_MAX_BOUNCES ? BALL_MAX_BOUNCES : rb_bounces[s];
    rb_on[s] = 0;
    rb_boom_x[s] = x;
    rb_boom_y[s] = y;
    rb_boom_t[s] = boom_time;
    if (finished == 0 && revive_prot <= 0 && circle_hits_player(x, y, BALL_BOOM_RADIUS) == 1) {
        take_damage_at(ball_damage_for(b), x, y);
    }
}

static void remote_bricks_tick(void) {
    double s = 0;
    if (game_state != ST_ONLINE) {
        return;
    }
    while (s < max_players && s < REMOTE_BRICKS) {
        double shot = net_player_brick(s);
        if (net_player_online(s) != 1 || s == net_slot()) {
            rb_on[(int)s] = 0;
            rb_shot[(int)s] = 0;
            rb_boom_t[(int)s] = 0;
            s = s + 1;
            continue;
        }
        if (shot != rb_shot[(int)s]) {
            rb_shot[(int)s] = shot;
            if (shot > 0) {
                rb_on[(int)s] = 1;
                rb_x[(int)s] = net_player_brick_x(s) * screen_w;
                rb_y[(int)s] = net_player_brick_y(s) * screen_h;
                rb_vx[(int)s] = net_player_brick_dx(s) * BALL_SPEED;
                rb_vy[(int)s] = net_player_brick_dy(s) * BALL_SPEED;
                rb_bounces[(int)s] = 0;
                rb_t[(int)s] = 0;
                rb_spin[(int)s] = 0;
            }
        }
        if (rb_on[(int)s] == 1) {
            double left = 1;
            rb_t[(int)s] = rb_t[(int)s] + dt;
            rb_spin[(int)s] = rb_spin[(int)s] + dt * 6;
            while (left > 0 && rb_on[(int)s] == 1) {
                double piece = left < 0.2 ? left : 0.2;
                double x = rb_x[(int)s] + rb_vx[(int)s] * dt * piece;
                double y = rb_y[(int)s] + rb_vy[(int)s] * dt * piece;
                double vx = rb_vx[(int)s], vy = rb_vy[(int)s];
                left = left - piece;
                if (x < BALL_RADIUS) {
                    x = BALL_RADIUS;
                    vx = (0 - vx) * BALL_BOUNCE_ACCEL;
                    vy = vy * BALL_BOUNCE_ACCEL;
                    rb_bounces[(int)s] = rb_bounces[(int)s] + 1;
                } else if (x > screen_w - BALL_RADIUS) {
                    x = screen_w - BALL_RADIUS;
                    vx = (0 - vx) * BALL_BOUNCE_ACCEL;
                    vy = vy * BALL_BOUNCE_ACCEL;
                    rb_bounces[(int)s] = rb_bounces[(int)s] + 1;
                }
                if (y < BALL_RADIUS) {
                    y = BALL_RADIUS;
                    vx = vx * BALL_BOUNCE_ACCEL;
                    vy = (0 - vy) * BALL_BOUNCE_ACCEL;
                    rb_bounces[(int)s] = rb_bounces[(int)s] + 1;
                } else if (y > screen_h - BALL_RADIUS) {
                    y = screen_h - BALL_RADIUS;
                    vx = vx * BALL_BOUNCE_ACCEL;
                    vy = (0 - vy) * BALL_BOUNCE_ACCEL;
                    rb_bounces[(int)s] = rb_bounces[(int)s] + 1;
                }
                rb_x[(int)s] = x;
                rb_y[(int)s] = y;
                rb_vx[(int)s] = vx;
                rb_vy[(int)s] = vy;
                if (rb_bounces[(int)s] > BALL_MAX_BOUNCES || rb_t[(int)s] > BALL_MAX_TIME) {
                    rb_burst((int)s, x, y);
                } else if (circle_hits_player(x, y, BALL_RADIUS) == 1) {
                    rb_burst((int)s, x, y);
                }
            }
            if (rb_on[(int)s] == 1) {
                rb_trail_push((int)s, rb_x[(int)s], rb_y[(int)s]);
            }
        }
        if (rb_boom_t[(int)s] > 0) {
            rb_boom_t[(int)s] = tick_down(rb_boom_t[(int)s]);
        }
        {
            int k = 0;
            while (k < BALL_TRAIL) {
                if (rb_trail_a[(int)s][k] > 0) {
                    rb_trail_a[(int)s][k] = rb_trail_a[(int)s][k] - dt / 0.45;
                    if (rb_trail_a[(int)s][k] < 0) {
                        rb_trail_a[(int)s][k] = 0;
                    }
                }
                k = k + 1;
            }
        }
        s = s + 1;
    }
}

static void draw_remote_bricks(void) {
    int s = 0, k;
    while (s < REMOTE_BRICKS) {
        if (rb_on[s] == 1 && admin_cheat_ball_path() == 1) {
            draw_ball_path_from(rb_x[s], rb_y[s], rb_vx[s], rb_vy[s], rb_bounces[s], rb_t[s], 0x00FFD54Fu, 0);
        }
        for (k = 0; k < BALL_TRAIL; k++) {
            if (rb_trail_a[s][k] > 0) {
                double a = rb_trail_a[s][k];
                circle(rb_trail_x[s][k], rb_trail_y[s][k], BALL_RADIUS * (0.12 + 0.45 * a),
                       ((unsigned)(int)(a * 170) << 24) | 0x003A8BFF);
            }
        }
        if (rb_on[s] == 1) {
            circle(rb_x[s], rb_y[s], BALL_RADIUS, 0xFF3A8BFFu);
        }
        if (rb_boom_t[s] > 0) {
            draw_boom_at_r(rb_boom_x[s], rb_boom_y[s], rb_boom_t[s], BALL_BOOM_RADIUS);
            if (show_hitboxes == 1) {
                draw_hit_circle(rb_boom_x[s], rb_boom_y[s], BALL_BOOM_RADIUS, rb_boom_t[s] / boom_time);
            }
        }
        s = s + 1;
    }
}

#ifdef CB_PC
/* Count active remote projectiles (test helper). */
static double remote_bricks_live(void) {
    double s = 0, n = 0;
    while (s < REMOTE_BRICKS) {
        if (rb_on[(int)s] == 1) {
            n = n + 1;
        }
        s = s + 1;
    }
    return n;
}
#endif

/* Clear local and remote projectiles after match finish. */
static void ball_clear_after_finish(void) {
    ball.active = 0;
    ball.boom_t = 0;
    remote_bricks_reset();
}

/* Tick local and remote projectiles during an online match. */
static void ball_online_tick(void) {
    ball_tick();
    ball_fade_tick();
    remote_bricks_tick();
}
