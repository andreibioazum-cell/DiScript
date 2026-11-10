/* Christmas snowballs use ebuC projectile movement with flat damage. */

#define XMAS_SPEED BALL_SPEED
#define XMAS_RADIUS BALL_RADIUS
#define XMAS_BOUNCE_ACCEL BALL_BOUNCE_ACCEL
#define XMAS_MAX_BOUNCES BALL_MAX_BOUNCES
#define XMAS_MAX_TIME BALL_MAX_TIME
#define XMAS_COOLDOWN 1.0
#define XMAS_DAMAGE 0.5               /* damage per snowball, same as it was before the cut */
#define XMAS_PUSH 7.0                 /* a tiny nudge, ebuC has none */
#define XMAS_BOOM_RADIUS BALL_BOOM_RADIUS
#define XMAS_TRAIL BALL_TRAIL
#define XMAS_MAX_BALLS 4    /* several snowballs may fly at the same time */

typedef struct {
    double active;
    double x, y, vx, vy;
    double bounces, t, shot;
    double boom_x, boom_y, boom_t;
    double trail_x[XMAS_TRAIL], trail_y[XMAS_TRAIL], trail_a[XMAS_TRAIL];
} XmasBall;

typedef struct {
    double on;
    double x, y, vx, vy;
    double bounces, t, shot;
    double boom_x, boom_y, boom_t;
} XmasRemote;

static XmasBall xmas_balls[XMAS_MAX_BALLS];
static double xmas_shot;
static XmasRemote xmas_remote[NET_SLOTS];
static double xmas_cd;

/* The snowball pad is as large as the block/ability pads, above the block pad. */
static double christmas_pad_x(void) { return blk_x; }
static double christmas_pad_y(void) { return blk_y - 2 * battle_pad_r() - 30; }
static double christmas_pad_r(void) { return battle_pad_r(); }

static double christmas_pad_hit(double x, double y) {
    if (christmas_snowballs_on() == 0) {
        return 0;
    }
    return dist(x, y, christmas_pad_x(), christmas_pad_y()) <= christmas_pad_r() + 14 ? 1 : 0;
}

static void xmas_ball_clear(XmasBall *b) {
    double i = 0;
    b->active = 0;
    b->x = 0;
    b->y = 0;
    b->vx = 0;
    b->vy = 0;
    b->bounces = 0;
    b->t = 0;
    b->boom_x = 0;
    b->boom_y = 0;
    b->boom_t = 0;
    while (i < XMAS_TRAIL) {
        b->trail_a[(int)i] = 0;
        i = i + 1;
    }
}

static void xmas_remote_clear(int s) {
    xmas_remote[s].on = 0;
    xmas_remote[s].boom_t = 0;
    xmas_remote[s].bounces = 0;
    xmas_remote[s].t = 0;
}

/* Baseline the shot counters so a projectile from a previous match does not
 * spawn again when a slot is (re)used. */
static void christmas_reset(void) {
    int s = 0, i = 0;
    while (i < XMAS_MAX_BALLS) {
        xmas_ball_clear(&xmas_balls[i]);
        i = i + 1;
    }
    xmas_cd = 0;
    while (s < NET_SLOTS) {
        xmas_remote_clear(s);
        xmas_remote[s].shot = net_player_xball((double)s);
        s = s + 1;
    }
}

static void christmas_clear_after_finish(void) {
    int s = 0, i = 0;
    while (i < XMAS_MAX_BALLS) {
        xmas_balls[i].active = 0;
        xmas_balls[i].boom_t = 0;
        i = i + 1;
    }
    while (s < NET_SLOTS) {
        xmas_remote_clear(s);
        s = s + 1;
    }
}

static double christmas_can_throw(void) {
    if (christmas_snowballs_on() == 0) {
        return 0;
    }
    if (game_state != ST_SOLO && game_state != ST_ONLINE) {
        return 0;
    }
    if (finished != 0 || xmas_cd > 0) {
        return 0;
    }
    if (player == NULL || player->hp <= 0) {
        return 0;
    }
    if (player_stun > 0 || player_freeze > 0 || player_block == 1 || player_grabbed() == 1) {
        return 0;
    }
    return 1;
}

/* dx/dy is any unit vector: the big pad lets you throw in any direction. */
static void christmas_throw_dir(double dx, double dy) {
    double d = sqrt(dx * dx + dy * dy);
    if (christmas_can_throw() == 0) {
        return;
    }
    if (d < 0.001) {
        dx = cos(player->angle);
        dy = sin(player->angle);
    } else {
        dx = dx / d;
        dy = dy / d;
    }
    {
        int slot = 0, i;
        for (i = 0; i < XMAS_MAX_BALLS; i++) {
            if (xmas_balls[i].active == 0) {
                slot = i;
                break;
            }
        }
        XmasBall *b = &xmas_balls[slot];
        b->active = 1;
        b->x = player->x;
        b->y = player->y;
        b->vx = dx * XMAS_SPEED;
        b->vy = dy * XMAS_SPEED;
        b->bounces = 0;
        b->t = 0;
        b->boom_t = 0;
        b->shot = xmas_shot;
        xmas_shot = xmas_shot + 1;
    }
    xmas_cd = XMAS_COOLDOWN;
    if (game_state == ST_ONLINE) {
        net_publish_xball(player->x / screen_w, player->y / screen_h, dx, dy, xmas_shot);
    }
    ability_snd(GIFT_SOUND);
    ds_log("christmas: snowball thrown");
}

/* Default launch: straight where the fighter is facing. */
static void christmas_throw(void) {
    christmas_throw_dir(cos(player->angle), sin(player->angle));
}

/* Press anywhere on the big pad: the offset from its centre IS the aim, so
 * the snowball can be thrown literally in any direction. A tap near the
 * centre keeps the ordinary facing-direction throw. */
static void christmas_pad_throw(double x, double y) {
    double dx = x - christmas_pad_x();
    double dy = y - christmas_pad_y();
    double d = sqrt(dx * dx + dy * dy);
    if (d > christmas_pad_r() * 0.35 && d > 0.001) {
        christmas_throw_dir(dx / d, dy / d);
        return;
    }
    christmas_throw();
}

/* Push a fighter a few pixels away from the impact point. */
static void xmas_push_away(double *px, double *py, double size, double sx, double sy) {
    double dx = *px - sx;
    double dy = *py - sy;
    double d = sqrt(dx * dx + dy * dy);
    double half = size * 0.5;
    if (d < 0.001) {
        dx = 1;
        dy = 0;
        d = 1;
    }
    *px = clamp(*px + dx / d * XMAS_PUSH, half, screen_w - half);
    *py = clamp(*py + dy / d * XMAS_PUSH, half, screen_h - half);
}

static void xmas_trail_push(XmasBall *b) {
    double i = XMAS_TRAIL - 1;
    while (i > 0) {
        b->trail_x[(int)i] = b->trail_x[(int)(i - 1)];
        b->trail_y[(int)i] = b->trail_y[(int)(i - 1)];
        b->trail_a[(int)i] = b->trail_a[(int)(i - 1)];
        i = i - 1;
    }
    b->trail_x[0] = b->x;
    b->trail_y[0] = b->y;
    b->trail_a[0] = 1;
}

/* ebuC's burst: an area hit around the endpoint. Damage is flat - bounces
 * are counted for the flight limit only, never for extra damage. */
static void xmas_burst(XmasBall *b) {
    b->active = 0;
    b->boom_x = b->x;
    b->boom_y = b->y;
    b->boom_t = boom_time;
    if (game_state != ST_ONLINE && enemy != NULL && enemy->hp > 0 && enemy_revive_prot <= 0 &&
        ball_hits_fighter_r(b->x, b->y, XMAS_BOOM_RADIUS, enemy->x, enemy->y, enemy->angle, enemy_scale) == 1) {
        enemy_apply_damage_at(XMAS_DAMAGE, b->x, b->y);
        xmas_push_away(&enemy->x, &enemy->y, enemy->size, b->x, b->y);
    }
}

static void xmas_remote_burst(int s) {
    XmasRemote *r = &xmas_remote[s];
    r->on = 0;
    r->boom_x = r->x;
    r->boom_y = r->y;
    r->boom_t = boom_time;
    if (player != NULL && player->hp > 0 && finished == 0 && revive_prot <= 0 &&
        ball_hits_fighter_r(r->x, r->y, XMAS_BOOM_RADIUS, player->x, player->y, player->angle,
                            player_scale) == 1) {
        take_damage_at(XMAS_DAMAGE, r->x, r->y);
        xmas_push_away(&player->x, &player->y, player->size, r->x, r->y);
    }
}

/* Reflect off the arena walls with ebuC's bounce acceleration. */
static double xmas_bounce(double *x, double *y, double *vx, double *vy) {
    double n = 0;
    if (*x < XMAS_RADIUS) {
        *x = XMAS_RADIUS;
        *vx = (0 - *vx) * XMAS_BOUNCE_ACCEL;
        *vy = *vy * XMAS_BOUNCE_ACCEL;
        n = n + 1;
    } else if (*x > screen_w - XMAS_RADIUS) {
        *x = screen_w - XMAS_RADIUS;
        *vx = (0 - *vx) * XMAS_BOUNCE_ACCEL;
        *vy = *vy * XMAS_BOUNCE_ACCEL;
        n = n + 1;
    }
    if (*y < XMAS_RADIUS) {
        *y = XMAS_RADIUS;
        *vx = *vx * XMAS_BOUNCE_ACCEL;
        *vy = (0 - *vy) * XMAS_BOUNCE_ACCEL;
        n = n + 1;
    } else if (*y > screen_h - XMAS_RADIUS) {
        *y = screen_h - XMAS_RADIUS;
        *vx = *vx * XMAS_BOUNCE_ACCEL;
        *vy = (0 - *vy) * XMAS_BOUNCE_ACCEL;
        n = n + 1;
    }
    return n;
}
