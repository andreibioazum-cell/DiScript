/* Christmas snowballs: a small, weak version of ebuC's bouncing projectile.
 * With Christmas: 1 in the Firebase root every class can throw one; the two
 * holiday admins always can, Sanch never can. Three bounces at most, one
 * second cooldown, tiny damage and a tiny push - all weaker than elf ebuC's. */

#define XMAS_SPEED 520.0
#define XMAS_RADIUS 15.0          /* smaller than ebuC's 24.0 ball */
#define XMAS_MAX_BOUNCES 3.0
#define XMAS_MAX_TIME 4.0
#define XMAS_COOLDOWN 1.0
#define XMAS_DAMAGE 0.25          /* ebuC starts at 0.5 and grows per bounce */
#define XMAS_PUSH 7.0             /* very small knockback */
#define XMAS_BOOM_RADIUS 30.0
#define XMAS_TRAIL 8

typedef struct {
    double active;
    double x, y, vx, vy;
    double bounces, t, spin, shot;
    double boom_x, boom_y, boom_t;
    double trail_x[XMAS_TRAIL], trail_y[XMAS_TRAIL], trail_a[XMAS_TRAIL];
} XmasBall;

typedef struct {
    double on;
    double x, y, vx, vy;
    double bounces, t, shot;
    double boom_x, boom_y, boom_t;
} XmasRemote;

static XmasBall xmas_ball;
static XmasRemote xmas_remote[NET_SLOTS];
static double xmas_cd;

/* The snowball pad sits above the block pad, clear of the super pad. */
static double christmas_pad_x(void) { return blk_x; }
static double christmas_pad_y(void) { return blk_y - 2 * battle_pad_r() - 30; }
static double christmas_pad_r(void) { return 32; }

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
    b->spin = 0;
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
    int s = 0;
    xmas_ball_clear(&xmas_ball);
    xmas_cd = 0;
    while (s < NET_SLOTS) {
        xmas_remote_clear(s);
        xmas_remote[s].shot = net_player_xball((double)s);
        s = s + 1;
    }
}

static void christmas_clear_after_finish(void) {
    int s = 0;
    xmas_ball.active = 0;
    xmas_ball.boom_t = 0;
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
    if (finished != 0 || xmas_cd > 0 || xmas_ball.active == 1) {
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

static void christmas_throw(void) {
    double dx, dy;
    if (christmas_can_throw() == 0) {
        return;
    }
    dx = cos(player->angle);
    dy = sin(player->angle);
    xmas_ball.active = 1;
    xmas_ball.x = player->x;
    xmas_ball.y = player->y;
    xmas_ball.vx = dx * XMAS_SPEED;
    xmas_ball.vy = dy * XMAS_SPEED;
    xmas_ball.bounces = 0;
    xmas_ball.t = 0;
    xmas_ball.spin = 0;
    xmas_ball.boom_t = 0;
    xmas_ball.shot = xmas_ball.shot + 1;
    xmas_cd = XMAS_COOLDOWN;
    if (game_state == ST_ONLINE) {
        net_publish_xball(player->x / screen_w, player->y / screen_h, dx, dy, xmas_ball.shot);
    }
    ability_snd(GIFT_SOUND);
    ds_log("christmas: snowball thrown");
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

static void xmas_burst(XmasBall *b) {
    b->active = 0;
    b->boom_x = b->x;
    b->boom_y = b->y;
    b->boom_t = boom_time;
}

static void xmas_remote_burst(int s) {
    xmas_remote[s].on = 0;
    xmas_remote[s].boom_x = xmas_remote[s].x;
    xmas_remote[s].boom_y = xmas_remote[s].y;
    xmas_remote[s].boom_t = boom_time;
}

/* Reflect off the arena walls, counting every bounce. */
static double xmas_bounce(double *x, double *y, double *vx, double *vy) {
    double n = 0;
    if (*x < XMAS_RADIUS) {
        *x = XMAS_RADIUS;
        *vx = 0 - *vx;
        n = n + 1;
    } else if (*x > screen_w - XMAS_RADIUS) {
        *x = screen_w - XMAS_RADIUS;
        *vx = 0 - *vx;
        n = n + 1;
    }
    if (*y < XMAS_RADIUS) {
        *y = XMAS_RADIUS;
        *vy = 0 - *vy;
        n = n + 1;
    } else if (*y > screen_h - XMAS_RADIUS) {
        *y = screen_h - XMAS_RADIUS;
        *vy = 0 - *vy;
        n = n + 1;
    }
    return n;
}

static double xmas_seg_hits_online_foe(double ox, double oy, double nx, double ny) {
    double s = 0;
    while (s < max_players) {
        double b = s * remote_fields;
        if (s != net_slot() && arr_get(remotes, b + 5) == 1 && arr_get(remotes, b + 4) > 0) {
            if (ball_seg_hits_fighter(ox, oy, nx, ny, XMAS_RADIUS, arr_get(remotes, b + 1),
                                      arr_get(remotes, b + 2), arr_get(remotes, b + 3), player_scale) == 1) {
                return 1;
            }
        }
        s = s + 1;
    }
    return 0;
}

static double xmas_remote_hits_other(int shooter, double ox, double oy, double nx, double ny) {
    double s = 0;
    while (s < max_players) {
        double b = s * remote_fields;
        if (s != (double)shooter && s != net_slot() && arr_get(remotes, b + 5) == 1 &&
            arr_get(remotes, b + 4) > 0) {
            if (ball_seg_hits_fighter(ox, oy, nx, ny, XMAS_RADIUS, arr_get(remotes, b + 1),
                                      arr_get(remotes, b + 2), arr_get(remotes, b + 3), player_scale) == 1) {
                return 1;
            }
        }
        s = s + 1;
    }
    return 0;
}

static void xmas_step_local(void) {
    double left, piece, ox, oy;
    if (xmas_ball.active == 0) {
        return;
    }
    xmas_ball.t = xmas_ball.t + dt;
    xmas_ball.spin = xmas_ball.spin + dt * 4;
    left = 1;
    while (left > 0 && xmas_ball.active == 1) {
        ox = xmas_ball.x;
        oy = xmas_ball.y;
        piece = left < 0.125 ? left : 0.125;
        left = left - piece;
        xmas_ball.x = xmas_ball.x + xmas_ball.vx * dt * piece;
        xmas_ball.y = xmas_ball.y + xmas_ball.vy * dt * piece;
        xmas_ball.bounces = xmas_ball.bounces +
                            xmas_bounce(&xmas_ball.x, &xmas_ball.y, &xmas_ball.vx, &xmas_ball.vy);
        if (xmas_ball.bounces > XMAS_MAX_BOUNCES || xmas_ball.t > XMAS_MAX_TIME) {
            xmas_burst(&xmas_ball);
            return;
        }
        if (game_state == ST_SOLO) {
            if (enemy != NULL && enemy->hp > 0 &&
                ball_seg_hits_fighter(ox, oy, xmas_ball.x, xmas_ball.y, XMAS_RADIUS, enemy->x, enemy->y,
                                      enemy->angle, enemy_scale) == 1) {
                enemy_apply_damage_at(XMAS_DAMAGE, xmas_ball.x, xmas_ball.y);
                xmas_push_away(&enemy->x, &enemy->y, enemy->size, xmas_ball.x, xmas_ball.y);
                xmas_burst(&xmas_ball);
                return;
            }
        } else if (game_state == ST_ONLINE) {
            if (xmas_seg_hits_online_foe(ox, oy, xmas_ball.x, xmas_ball.y) == 1) {
                xmas_burst(&xmas_ball);
                return;
            }
        }
    }
    xmas_trail_push(&xmas_ball);
}

static void xmas_step_remote(int s) {
    double left, piece, ox, oy;
    XmasRemote *r = &xmas_remote[s];
    if (r->on == 0) {
        return;
    }
    r->t = r->t + dt;
    left = 1;
    while (left > 0 && r->on == 1) {
        ox = r->x;
        oy = r->y;
        piece = left < 0.125 ? left : 0.125;
        left = left - piece;
        r->x = r->x + r->vx * dt * piece;
        r->y = r->y + r->vy * dt * piece;
        r->bounces = r->bounces + xmas_bounce(&r->x, &r->y, &r->vx, &r->vy);
        if (r->bounces > XMAS_MAX_BOUNCES || r->t > XMAS_MAX_TIME) {
            xmas_remote_burst(s);
            return;
        }
        if (player != NULL && player->hp > 0 && revive_prot <= 0 &&
            ball_seg_hits_fighter(ox, oy, r->x, r->y, XMAS_RADIUS, player->x, player->y, player->angle,
                                  player_scale) == 1) {
            take_damage_at(XMAS_DAMAGE, r->x, r->y);
            xmas_push_away(&player->x, &player->y, player->size, r->x, r->y);
            xmas_remote_burst(s);
            return;
        }
        if (xmas_remote_hits_other(s, ox, oy, r->x, r->y) == 1) {
            xmas_remote_burst(s);
            return;
        }
    }
}

static void christmas_tick(void) {
    int s = 0;
    double i = 0;
    xmas_cd = tick_down(xmas_cd);
    if (xmas_ball.boom_t > 0) {
        xmas_ball.boom_t = tick_down(xmas_ball.boom_t);
    }
    while (i < XMAS_TRAIL) {
        if (xmas_ball.trail_a[(int)i] > 0) {
            xmas_ball.trail_a[(int)i] = xmas_ball.trail_a[(int)i] - dt / 0.4;
            if (xmas_ball.trail_a[(int)i] < 0) {
                xmas_ball.trail_a[(int)i] = 0;
            }
        }
        i = i + 1;
    }
    xmas_step_local();
    if (game_state != ST_ONLINE) {
        return;
    }
    while (s < NET_SLOTS) {
        double shot = net_player_xball((double)s);
        if (net_player_online((double)s) != 1 || s == net_slot()) {
            xmas_remote_clear(s);
            xmas_remote[s].shot = shot;
        } else if (shot != xmas_remote[s].shot) {
            xmas_remote[s].shot = shot;
            xmas_remote_clear(s);
            if (shot > 0) {
                xmas_remote[s].on = 1;
                xmas_remote[s].x = net_player_xball_x((double)s) * screen_w;
                xmas_remote[s].y = net_player_xball_y((double)s) * screen_h;
                xmas_remote[s].vx = net_player_xball_dx((double)s) * XMAS_SPEED;
                xmas_remote[s].vy = net_player_xball_dy((double)s) * XMAS_SPEED;
                xmas_remote[s].t = 0;
            }
        }
        if (xmas_remote[s].on == 1) {
            xmas_step_remote(s);
        }
        if (xmas_remote[s].boom_t > 0) {
            xmas_remote[s].boom_t = tick_down(xmas_remote[s].boom_t);
        }
        s = s + 1;
    }
}

static void xmas_draw_body(double x, double y, double spin) {
    double hw = 16 * 0.42;
    circle(x, y, XMAS_RADIUS, 0xFFEAF7FFu);
    ring(x, y, XMAS_RADIUS, 3, 0xFF9FD3F0u);
    tex(x - hw, y - hw, SNOWFLAKE_TEX, spin, 0.42);
    if (show_hitboxes == 1) {
        ring(x, y, XMAS_RADIUS, 2, 0xFFFF2D2Du);
    }
}

static void draw_christmas_snowballs(void) {
    double i = 0;
    int s = 0;
    if (christmas_snowballs_on() == 0) {
        return;
    }
    while (i < XMAS_TRAIL) {
        if (xmas_ball.trail_a[(int)i] > 0) {
            double a = xmas_ball.trail_a[(int)i];
            circle(xmas_ball.trail_x[(int)i], xmas_ball.trail_y[(int)i], XMAS_RADIUS * (0.15 + 0.4 * a),
                   ((unsigned)(int)(a * 150) << 24) | 0x00FFFFFFu);
        }
        i = i + 1;
    }
    if (xmas_ball.active == 1) {
        xmas_draw_body(xmas_ball.x, xmas_ball.y, xmas_ball.spin);
    }
    if (xmas_ball.boom_t > 0) {
        draw_boom_at_r(xmas_ball.boom_x, xmas_ball.boom_y, xmas_ball.boom_t, XMAS_BOOM_RADIUS);
    }
    while (s < NET_SLOTS) {
        if (xmas_remote[s].on == 1) {
            xmas_draw_body(xmas_remote[s].x, xmas_remote[s].y, xmas_remote[s].t * 4);
        }
        if (xmas_remote[s].boom_t > 0) {
            draw_boom_at_r(xmas_remote[s].boom_x, xmas_remote[s].boom_y, xmas_remote[s].boom_t,
                           XMAS_BOOM_RADIUS);
        }
        s = s + 1;
    }
}

static void draw_christmas_pad(void) {
    const char *label;
    double scale = 0.42;
    uint32_t col = 0x7FC4E8u;
    if (christmas_snowballs_on() == 0 || (game_state != ST_SOLO && game_state != ST_ONLINE)) {
        return;
    }
    if (xmas_cd > 0) {
        label = ds_num_to_string((double)(floor(xmas_cd + 0.999)));
        col = 0x6E8CA8u;
        scale = 0.8;
    } else {
        label = tr_snowball();
    }
    draw_pad(christmas_pad_x(), christmas_pad_y(), christmas_pad_r(), col, label, scale);
}
