/* Ebuc (буК) has two ults, picked by the small button above the pad: dispenser
 * and magic blue projectile (renamed from brick). The projectile is aimed by
 * hand - the tap starts a 3-2-1 countdown over the cube, a dragged finger
 * points the throw (only when touching near the cube), and the projectile
 * leaves that way when the count hits zero and bounces off the walls (ten
 * bounces, then it just bursts). Lifting the finger early keeps the aim. Ebuc
 * only, and nothing in the game charges by holding. */

#define BALL_COUNT_FROM 3.0      /* countdown digits: 3, 2, 1 */
#define BALL_COUNT_STEP 0.7      /* seconds per digit */
#define BALL_SPEED 620.0         /* initial px per second */
#define BALL_BOUNCE_ACCEL 1.04   /* +4% speed on every wall bounce */
#define BALL_RADIUS 24.0
#define BALL_BASE_DAMAGE 0.5     /* 0 bounces: barely scratches the target */
#define BALL_BOUNCE_DAMAGE 1.5   /* extra damage gained per wall bounce */
#define BALL_MAX_BOUNCES 10.0
#define BALL_MAX_TIME 8.0        /* safety net: the ball always goes away */
#define BALL_TRAIL 12

typedef struct {
    double active;
    double x, y, vx, vy;
    double bounces, t, spin, shot;
    /* Aiming: the countdown runs in aim_t, the throw follows aim_dx/aim_dy. */
    double aim_on, aim_id, aim_t, aim_dx, aim_dy;
    double trail_x[BALL_TRAIL], trail_y[BALL_TRAIL], trail_a[BALL_TRAIL];
} UltBall;

static UltBall ball;

/* The ult mode of a class that has two of them. Ebuc (the cube called "буК",
 * genitive "буКа") can throw either his dispenser or this magic blue projectile,
 * and the small button above the pad switches between them: 0 - dispenser (blue),
 * 1 - projectile (yellow indicator). */
static double ball_mode;

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
 * else. No text – just color: yellow = magic blue projectile (former brick),
 * blue = dispenser. Only black outline, no white. */
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
    ball.aim_on = 0;
    ball.aim_id = -1;
    ball.aim_t = 0;
    ball.aim_dx = 1;
    ball.aim_dy = 0;
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

/* The pad was pressed in projectile mode: start the countdown. The finger that
 * started it owns the aim, so a second finger cannot drag the throw away. */
static void ball_aim_start(double id) {
    if (ball.aim_on == 1 || ball.active == 1) {
        return;
    }
    if (finished != 0 || player_block == 1 || player_grabbed() == 1) {
        return;
    }
    if (player_stun > 0 || player_freeze > 0 || super_cd > 0) {
        return;
    }
    ball.aim_on = 1;
    ball.aim_id = id;
    ball.aim_t = BALL_COUNT_FROM * BALL_COUNT_STEP;
    ball.aim_dx = cos(player->angle);
    ball.aim_dy = sin(player->angle);
}

/* The digit the countdown shows right now: 3, 2 or 1. */
static double ball_aim_digit(void) {
    double d = ceil(ball.aim_t / BALL_COUNT_STEP);
    if (d < 1) {
        d = 1;
    }
    if (d > BALL_COUNT_FROM) {
        d = BALL_COUNT_FROM;
    }
    return d;
}

static double ball_aim_near(double x, double y) {
    double d = dist(x, y, player->x, player->y);
    return d >= 14 && d <= 180 ? 1 : 0;
}

/* The finger moved: the projectile goes where it points, from the cube outwards.
 * Only touches directly around the player count – not any empty area without
 * buttons. A touch right on the cube means nothing yet. Hitbox direction is
 * shown when hitboxes are enabled. */
static void ball_aim_point(double x, double y) {
    double dx = x - player->x;
    double dy = y - player->y;
    double len = sqrt(dx * dx + dy * dy);
    if (ball.aim_on == 0 || len < 14 || len > 180) {
        return;
    }
    ball.aim_dx = dx / len;
    ball.aim_dy = dy / len;
}

static void ball_aim_cancel(void) {
    ball.aim_on = 0;
    ball.aim_id = -1;
    ball.aim_t = 0;
}

/* The countdown is over: the magic projectile is thrown. Lifting the finger early does
 * not stop it, the aim is already chosen. */
static void ball_aim_tick(void) {
    if (ball.aim_on == 0) {
        return;
    }
    if (finished != 0 || player_block == 1 || player_grabbed() == 1 || player_stun > 0 || player_freeze > 0) {
        ball_aim_cancel();
        return;
    }
    ball.aim_t = ball.aim_t - dt;
    if (ball.aim_t > 0) {
        return;
    }
    ball_aim_cancel();
    ball_fire();
}

static void ball_fire(void) {
    if (finished != 0 || ball.active == 1 || player_block == 1 || player_grabbed() == 1) {
        return;
    }
    if (player_stun > 0 || player_freeze > 0 || super_cd > 0) {
        return;
    }
    ball.x = player->x;
    ball.y = player->y;
    ball.vx = ball.aim_dx * BALL_SPEED;
    ball.vy = ball.aim_dy * BALL_SPEED;
    ball.bounces = 0;
    ball.t = 0;
    ball.spin = 0;
    ball.active = 1;
    ball.shot = ball.shot + 1;
    if (game_state == ST_ONLINE) {
        /* Online the throw is published, not simulated for the others: every
         * client runs the same flight from the origin and the direction. */
        net_publish_brick(player->x / screen_w, player->y / screen_h, ball.aim_dx, ball.aim_dy, ball.shot);
    }
    super_cd = super_cd_time;
    ability_snd(STATION_SOUND);
    ds_log("magic projectile thrown");
}

static double ball_damage_for(double bounces) {
    double b = bounces < 0 ? 0 : bounces;
    return BALL_BASE_DAMAGE + b * BALL_BOUNCE_DAMAGE;
}

static void ball_burst(double damage) {
    ball.active = 0;
    boom_x = ball.x;
    boom_y = ball.y;
    boom_t = boom_time;
    if (damage <= 0) {
        return;
    }
    if (enemy_revive_prot <= 0) {
        enemy_apply_damage_at(ball_damage_for(ball.bounces), ball.x, ball.y);
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
            /* Out of bounces: it bursts where it is and nobody gets hurt. */
            ds_log("charged ult: projectile used up after %g bounces", ball.bounces);
            ball_burst(0);
            return;
        }
        if (game_state != ST_ONLINE && circle_hits_enemy(ball.x, ball.y, BALL_RADIUS) == 1) {
            ball_burst(1);
            return;
        }
    }
    ball_trail_push(ball.x, ball.y);
}

/* Aim preview: dashed line + hitbox direction when hitboxes are shown. */
static void draw_ball_aim(void) {
    double i = 1;
    double len = punch_reach * 2.4;
    if (ball.aim_on == 0) {
        return;
    }
    while (i <= 5) {
        double k = i / 6;
        circle(player->x + ball.aim_dx * len * k, player->y + ball.aim_dy * len * k, 3 + 4 * (1 - k),
               ((unsigned)(int)(150 + 90 * k) << 24) | 0x00FFD54F);
        i = i + 1;
    }
    if (show_hitboxes == 1) {
        double hx = player->x + ball.aim_dx * 48;
        double hy = player->y + ball.aim_dy * 48;
        draw_hit_circle(hx, hy, BALL_RADIUS, 0.85);
    }
}

/* The digit of the countdown, drawn with the pads: the fight itself is drawn
 * under the colour correction, and a 3 lost under it is not a warning. */
static void draw_ball_count(void) {
    const char *digit;
    double d = ball_aim_digit();
    double sc = 1.7;
    if (ball.aim_on == 0) {
        return;
    }
    digit = d > 2 ? "3" : (d > 1 ? "2" : "1");
    text_scaled(digit, player->x - text_ink_width(digit) * sc / 2, player->y - 118,
                (unsigned)0xFFFFD54F, sc);
}

static void draw_ball(void) {
    double i = 0;
    double a;
    draw_ult_mode();
    draw_ball_aim();
    if (ball.aim_on == 1) {
        /* Armed: the countdown runs, the finger can still pull the aim. */
        ring(atk2_x, atk2_y, battle_pad_r() + 9, 5, 0xFFFFD54F);
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
        /* Magic blue projectile: blue stretched square, slightly rounded, no ring. */
        double hw = 26, hh = 16;
        roundrect(ball.x - hw, ball.y - hh, hw * 2, hh * 2, 6, 0xFF3A8BFFu);
        if (show_hitboxes == 1) {
            draw_hit_circle(ball.x, ball.y, BALL_RADIUS, 1);
        }
    }
}

/* Trail fade runs even between launches, so the dots left by the last ball do
 * not stay on screen forever. */
static void ball_fade_tick(void) {
    double i = 0;
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
