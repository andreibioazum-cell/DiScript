/* Projectiles thrown by other players in an online fight. Every client flies them
 * from the published origin and direction, so the bounces happen in the same
 * places on every screen, and the hit lands where the local player stands. */
#define REMOTE_BRICKS 4

static double rb_on[REMOTE_BRICKS], rb_shot[REMOTE_BRICKS];
static double rb_x[REMOTE_BRICKS], rb_y[REMOTE_BRICKS], rb_vx[REMOTE_BRICKS], rb_vy[REMOTE_BRICKS];
static double rb_bounces[REMOTE_BRICKS], rb_t[REMOTE_BRICKS], rb_spin[REMOTE_BRICKS];

static void remote_bricks_reset(void) {
    double i = 0;
    while (i < REMOTE_BRICKS) {
        rb_on[(int)i] = 0;
        rb_shot[(int)i] = -1;
        i = i + 1;
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
            rb_shot[(int)s] = -1;
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
                    rb_on[(int)s] = 0;
                } else if (circle_hits_player(x, y, BALL_RADIUS) == 1) {
                    take_damage_at(ball_damage_for(rb_bounces[(int)s]), x, y);
                    rb_on[(int)s] = 0;
                }
            }
        }
        s = s + 1;
    }
}

static void draw_remote_bricks(void) {
    double s = 0;
    while (s < REMOTE_BRICKS) {
        if (rb_on[(int)s] == 1) {
            double hw = 26, hh = 16;
            roundrect(rb_x[(int)s] - hw, rb_y[(int)s] - hh, hw * 2, hh * 2, 6, 0xFF3A8BFFu);
            if (show_hitboxes == 1) {
                draw_hit_circle(rb_x[(int)s], rb_y[(int)s], BALL_RADIUS, 1);
            }
        }
        s = s + 1;
    }
}

#ifdef CB_PC
/* How many bricks of other players are in the air right now (harness only). */
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

/* The round is over: the projectile goes away and so do the ones in the air. */
static void ball_clear_after_finish(void) {
    ball.active = 0;
    ball.aim_on = 0;
    ball.aim_id = -1;
    ball.aim_t = 0;
    remote_bricks_reset();
}

/* Everything the projectile needs per frame in an online fight: the local aim and
 * flight plus the projectiles of the others. */
static void ball_online_tick(void) {
    ball_aim_tick();
    ball_tick();
    ball_fade_tick();
    remote_bricks_tick();
}
