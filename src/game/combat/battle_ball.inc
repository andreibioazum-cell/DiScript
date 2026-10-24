/* ebuC (буК) has two ults, picked by the small button above the pad: dispenser
 * and magic blue projectile. Pressing the ult pad in projectile mode fires the
 * ball immediately in the direction ebuC is currently facing (no manual aiming
 * or countdown), bouncing off the walls up to ten times before bursting. */

#define BALL_SPEED 620.0              /* initial px per second */
#define BALL_BOUNCE_ACCEL 1.04        /* +4% speed on every wall bounce */
#define BALL_RADIUS 24.0
#define BALL_BOOM_RADIUS 150.0        /* ~15% wider explosion than super_radius (130) */
#define BALL_BASE_DAMAGE 0.5          /* 0 bounces: barely scratches the target */
#define BALL_BOUNCE_DAMAGE 1.5        /* damage gained on the 1st wall bounce */
#define BALL_BOUNCE_DAMAGE_GROW 1.13  /* +13% bounce damage on every wall bounce */
#define BALL_MAX_BOUNCES 10.0
#define BALL_MAX_TIME 8.0             /* safety net: the ball always goes away */
#define BALL_TRAIL 12

typedef struct {
    double active;
    double x, y, vx, vy;
    double bounces, t, spin, shot;
    double boom_x, boom_y, boom_t;
    double trail_x[BALL_TRAIL], trail_y[BALL_TRAIL], trail_a[BALL_TRAIL];
} UltBall;

static UltBall ball;

/* The ult mode of a class that has two of them. ebuC (буК, genitive "буКа")
 * can throw either his dispenser or this magic blue projectile, and the small
 * button above the pad switches between them: 0 - dispenser (blue),
 * 1 - projectile (yellow indicator). */
static double ult_mode_r(void) { return 26; }
static double ult_mode_x(void) { return atk2_x; }
static double ult_mode_y(void) { return atk2_y - battle_pad_r() - ult_mode_r() - 10; }

static double ult_mode_hit(double x, double y) {
    double dx = x - ult_mode_x();
    double dy = y - ult_mode_y();
    return sqrt(dx * dx + dy * dy) <= ult_mode_r() + 14;
}

static void ult_mode_toggle(void) {
    ball_mode = ball_mode == 1 ? 0 : 1;
    play_button_click();
}

/* Shown only where both modes exist, so the pad keeps its meaning everywhere
 * else. No text - just color: yellow = magic blue projectile, blue = dispenser.
 * Only black outline, no white. */
static void draw_ult_mode(void) {
    double x = ult_mode_x();
    double y = ult_mode_y();
    double r = ult_mode_r();
    uint32_t fill;
    if (player_class != CLASS_EBUC || (game_state != ST_SOLO && game_state != ST_ONLINE)) {
        return;
    }
    fill = ball_mode == 1 ? 0xFFFFB300u : 0xFF2F6FEDu;
    circle(x, y, r, fill);
    ring(x, y, r, 3, 0xFF000000u);
}

static void ball_reset(void) {
    double i = 0;
    ball.active = 0;
    ball.x = 0;
    ball.y = 0;
    ball.vx = 0;
    ball.vy = 0;
    ball.bounces = 0;
    ball.t = 0;
    ball.boom_x = 0;
    ball.boom_y = 0;
    ball.boom_t = 0;
    ball.shot = 0;
    ball_mode = 0;
    remote_bricks_reset();
    while (i < BALL_TRAIL) {
        ball.trail_a[(int)i] = 0;
        i = i + 1;
    }
}

static void ball_trail_push(double x, double y) {
    double i = BALL_TRAIL - 1;
    while (i > 0) {
        ball.trail_x[(int)i] = ball.trail_x[(int)(i - 1)];
        ball.trail_y[(int)i] = ball.trail_y[(int)(i - 1)];
        ball.trail_a[(int)i] = ball.trail_a[(int)(i - 1)];
        i = i - 1;
    }
    /* Sparks spray a little to the sides, otherwise the ball hides its own
     * tail while it flies: at 620 px/s the trail sits inside the ball. */
    ball.trail_x[0] = x + (rand() % 21 - 10);
    ball.trail_y[0] = y + (rand() % 21 - 10);
    ball.trail_a[0] = 1;
}

static void ball_fire(void) {
    double dx, dy;
    if (finished != 0 || ball.active == 1 || player_block == 1 || player_grabbed() == 1) {
        return;
    }
    if (player_stun > 0 || player_freeze > 0 || super_cd > 0) {
        return;
    }
    dx = cos(player->angle);
    dy = sin(player->angle);
    ball.x = player->x;
    ball.y = player->y;
    ball.vx = dx * BALL_SPEED;
    ball.vy = dy * BALL_SPEED;
    ball.bounces = 0;
    ball.t = 0;
    ball.spin = 0;
    ball.active = 1;
    ball.shot = ball.shot + 1;
    if (game_state == ST_ONLINE) {
        /* Online the throw is published, not simulated for the others: every
         * client runs the same flight from the origin and the direction. */
        net_publish_brick(player->x / screen_w, player->y / screen_h, dx, dy, ball.shot);
    }
    super_cd = super_cd_time;
    ability_snd(STATION_SOUND);
    ds_log("magic projectile thrown");
}

static double ball_damage_for(double bounces) {
    double n = bounces < 0 ? 0 : floor(bounces);
    double dmg = BALL_BASE_DAMAGE;
    double step = BALL_BOUNCE_DAMAGE;
    double i = 0;
    while (i < n) {
        dmg = dmg + step;
        step = step * BALL_BOUNCE_DAMAGE_GROW;
        i = i + 1;
    }
    return dmg;
}

static void ball_burst(void) {
    double b = ball.bounces > BALL_MAX_BOUNCES ? BALL_MAX_BOUNCES : ball.bounces;
    ball.active = 0;
    ball.boom_x = ball.x;
    ball.boom_y = ball.y;
    ball.boom_t = boom_time;
    if (game_state != ST_ONLINE && enemy_revive_prot <= 0 &&
        circle_hits_enemy(ball.x, ball.y, BALL_BOOM_RADIUS) == 1) {
        enemy_apply_damage_at(ball_damage_for(b), ball.x, ball.y);
    }
}

static void ball_tick(void) {
    double left, piece;
    if (ball.active == 0) {
        return;
    }
    ball.t = ball.t + dt;
    ball.spin = ball.spin + dt * 6;
    /* The move is split into small pieces: on a slow frame the ball would
     * otherwise fly straight through the enemy without ever touching it. */
    left = 1;
    while (left > 0 && ball.active == 1) {
        piece = left < 0.2 ? left : 0.2;
        left = left - piece;
        ball.x = ball.x + ball.vx * dt * piece;
        ball.y = ball.y + ball.vy * dt * piece;
        if (ball.x < BALL_RADIUS) {
            ball.x = BALL_RADIUS;
            ball.vx = (0 - ball.vx) * BALL_BOUNCE_ACCEL;
            ball.vy = ball.vy * BALL_BOUNCE_ACCEL;
            ball.bounces = ball.bounces + 1;
        } else if (ball.x > screen_w - BALL_RADIUS) {
            ball.x = screen_w - BALL_RADIUS;
            ball.vx = (0 - ball.vx) * BALL_BOUNCE_ACCEL;
            ball.vy = ball.vy * BALL_BOUNCE_ACCEL;
            ball.bounces = ball.bounces + 1;
        }
        if (ball.y < BALL_RADIUS) {
            ball.y = BALL_RADIUS;
            ball.vx = ball.vx * BALL_BOUNCE_ACCEL;
            ball.vy = (0 - ball.vy) * BALL_BOUNCE_ACCEL;
            ball.bounces = ball.bounces + 1;
        } else if (ball.y > screen_h - BALL_RADIUS) {
            ball.y = screen_h - BALL_RADIUS;
            ball.vx = ball.vx * BALL_BOUNCE_ACCEL;
            ball.vy = (0 - ball.vy) * BALL_BOUNCE_ACCEL;
            ball.bounces = ball.bounces + 1;
        }
        if (ball.bounces > BALL_MAX_BOUNCES || ball.t > BALL_MAX_TIME) {
            /* Out of bounces or time: bursts with BALL_BOOM_RADIUS, hurting any enemy caught in the blast. */
            ds_log("charged ult: projectile used up after %g bounces", ball.bounces);
            ball_burst();
            return;
        }
        if (game_state != ST_ONLINE && circle_hits_enemy(ball.x, ball.y, BALL_RADIUS) == 1) {
            ball_burst();
            return;
        }
    }
    ball_trail_push(ball.x, ball.y);
}

static void draw_ball(void) {
    double i = 0;
    double a;
    draw_ult_mode();
    if (admin_cheat_ball_path() == 1) {
        if (ball.active == 1) {
            draw_ball_path_from(ball.x, ball.y, ball.vx, ball.vy, ball.bounces, ball.t, 0x0056CCF2u, 1);
        } else if (player_class == CLASS_EBUC && ball_mode == 1 && finished == 0) {
            draw_ball_path_from(player->x, player->y, cos(player->angle) * BALL_SPEED, sin(player->angle) * BALL_SPEED,
                                0, 0, 0x0056CCF2u, 1);
        }
    }
    while (i < BALL_TRAIL) {
        if (ball.trail_a[(int)i] > 0) {
            a = ball.trail_a[(int)i];
            circle(ball.trail_x[(int)i], ball.trail_y[(int)i], BALL_RADIUS * (0.12 + 0.45 * a),
                   ((unsigned)(int)(a * 170) << 24) | 0x003A8BFF);
        }
        i = i + 1;
    }
    if (ball.active == 1) {
        /* Pure ball + particles only: no outline, no hitbox ring. */
        circle(ball.x, ball.y, BALL_RADIUS, 0xFF3A8BFFu);
    }
    if (ball.boom_t > 0) {
        draw_boom_at_r(ball.boom_x, ball.boom_y, ball.boom_t, BALL_BOOM_RADIUS);
        if (show_hitboxes == 1) {
            draw_hit_circle(ball.boom_x, ball.boom_y, BALL_BOOM_RADIUS, ball.boom_t / boom_time);
        }
    }
}

/* Trail fade runs even between launches, so the dots left by the last ball do
 * not stay on screen forever. */
static void ball_fade_tick(void) {
    double i = 0;
    if (ball.boom_t > 0) {
        ball.boom_t = tick_down(ball.boom_t);
    }
    while (i < BALL_TRAIL) {
        if (ball.trail_a[(int)i] > 0) {
            ball.trail_a[(int)i] = ball.trail_a[(int)i] - dt / 0.45;
            if (ball.trail_a[(int)i] < 0) {
                ball.trail_a[(int)i] = 0;
            }
        }
        i = i + 1;
    }
}
