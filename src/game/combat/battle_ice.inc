/* New Year ice blocks: light blue semi-transparent cubes that appear on their
 * own during the winter theme. They are solid for both fighters, steer the bot
 * around them and bounce ebuC's projectiles and the snowballs. */

#define ICE_MAX 4
#define ICE_HALF 30.0        /* half size of one ice cube, in pixels */
#define ICE_LIFE_MIN 7.0
#define ICE_LIFE_MAX 12.0
#define ICE_SPAWN_MIN 3.0
#define ICE_SPAWN_MAX 6.0
#define ICE_MARGIN 120.0     /* keep ice away from the arena border */
#define ICE_CONTROLS 230.0   /* do not cover the joystick and the pads */

typedef struct {
    double on;
    double x, y, r;
    double t;      /* age */
    double life;   /* full lifetime */
    double a;      /* current alpha 0..1 */
} IceBlock;

static IceBlock ice_blocks[ICE_MAX];
static double ice_spawn_cd;

static double ice_active(void) {
    if (game_state != ST_SOLO && game_state != ST_ONLINE) {
        return 0;
    }
    if (finished != 0) {
        return 0;
    }
    return winter_theme_on();
}

/* Ice only exists while the winter theme is on, so any other mode clears it. */
static void ice_reset(void) {
    int i;
    for (i = 0; i < ICE_MAX; i++) {
        ice_blocks[i].on = 0;
        ice_blocks[i].a = 0;
    }
    ice_spawn_cd = 1.0;
}

double ice_near_fighter(double x, double y, double gap) {
    if (dist(x, y, player->x, player->y) < gap) {
        return 1;
    }
    if (game_state != ST_ONLINE && dist(x, y, enemy->x, enemy->y) < gap) {
        return 1;
    }
    return 0;
}

/* Return whether a real ice block is in front of the punch target.  This is
 * deliberately separate from ice_near_fighter(), which only keeps randomly
 * spawned blocks away from fighters. */
static double ice_blocks_punch(double px, double py, double pdx, double pdy, double reach, double width,
                               double tx, double ty) {
    double target_at = along(px, py, pdx, pdy, tx, ty);
    int i;
    for (i = 0; i < ICE_MAX; i++) {
        IceBlock *b = &ice_blocks[i];
        if (b->on == 0 || target_is_forward(px, py, pdx, pdy, b->x, b->y) == 0) {
            continue;
        }
        if (strip_hits_box(px, py, pdx, pdy, punch_forward_offset, punch_forward_offset + reach, width / 2,
                           b->x, b->y, 0, b->r) == 1 &&
            along(px, py, pdx, pdy, b->x, b->y) <= target_at) {
            return 1;
        }
    }
    return 0;
}

static int ice_free_slot(void) {
    int i;
    for (i = 0; i < ICE_MAX; i++) {
        if (ice_blocks[i].on == 0) {
            return i;
        }
    }
    return -1;
}

static void ice_spawn_one(void) {
    int slot = ice_free_slot();
    int tries = 0;
    if (slot < 0) {
        return;
    }
    while (tries < 20) {
        double x = ICE_MARGIN + (rand() % 1000) / 1000.0 * (screen_w - 2 * ICE_MARGIN);
        double y = ICE_MARGIN + (rand() % 1000) / 1000.0 * (screen_h - ICE_CONTROLS - 2 * ICE_MARGIN);
        tries++;
        if (ice_near_fighter(x, y, 140) == 1) {
            continue;
        }
        ice_blocks[slot].on = 1;
        ice_blocks[slot].x = x;
        ice_blocks[slot].y = y;
        ice_blocks[slot].r = ICE_HALF;
        ice_blocks[slot].t = 0;
        ice_blocks[slot].life = ICE_LIFE_MIN + (rand() % 1000) / 1000.0 * (ICE_LIFE_MAX - ICE_LIFE_MIN);
        ice_blocks[slot].a = 0;
        return;
    }
}

/* Spawn, age and fade the blocks. Called once per battle frame. */
static void ice_tick(void) {
    int i;
    if (ice_active() == 0) {
        if (ice_blocks[0].on != 0 || ice_spawn_cd > 0) {
            ice_reset();
        }
        return;
    }
    ice_spawn_cd = ice_spawn_cd - dt;
    if (ice_spawn_cd <= 0) {
        ice_spawn_one();
        ice_spawn_cd = ICE_SPAWN_MIN + (rand() % 1000) / 1000.0 * (ICE_SPAWN_MAX - ICE_SPAWN_MIN);
    }
    for (i = 0; i < ICE_MAX; i++) {
        IceBlock *b = &ice_blocks[i];
        if (b->on == 0) {
            continue;
        }
        b->t = b->t + dt;
        if (b->t >= b->life) {
            b->on = 0;
            b->a = 0;
            continue;
        }
        /* Fade in during the first 0.6 s and out during the last 1.2 s. */
        double a = 1;
        if (b->t < 0.6) {
            a = b->t / 0.6;
        } else if (b->life - b->t < 1.2) {
            a = (b->life - b->t) / 1.2;
        }
        b->a = clamp(a, 0, 1);
    }
}

/* True when the point is inside a block; used by the projectile bounce. */
static double ice_block_at(double x, double y, int *out) {
    int i;
    for (i = 0; i < ICE_MAX; i++) {
        IceBlock *b = &ice_blocks[i];
        if (b->on == 0) {
            continue;
        }
        if (fabs(x - b->x) < b->r && fabs(y - b->y) < b->r) {
            *out = i;
            return 1;
        }
    }
    return 0;
}

/* Push a round body out of every ice block it touches. */
static void ice_push_point(double *px, double *py, double half) {
    int i;
    for (i = 0; i < ICE_MAX; i++) {
        IceBlock *b = &ice_blocks[i];
        double cx, cy, dx, dy, d, ox, oy, ax, ay;
        if (b->on == 0) {
            continue;
        }
        cx = clamp(*px, b->x - b->r, b->x + b->r);
        cy = clamp(*py, b->y - b->r, b->y + b->r);
        dx = *px - cx;
        dy = *py - cy;
        d = sqrt(dx * dx + dy * dy);
        if (d >= half) {
            continue;
        }
        if (d > 0.001) {
            double k = (half - d) / d;
            *px = *px + dx * k;
            *py = *py + dy * k;
        } else {
            /* Center inside the block: leave through the nearest face. */
            ox = *px - b->x;
            oy = *py - b->y;
            ax = b->r + half - fabs(ox);
            ay = b->r + half - fabs(oy);
            if (ax < ay) {
                *px = *px + (ox < 0 ? -ax : ax);
            } else {
                *py = *py + (oy < 0 ? -ay : ay);
            }
        }
        *px = clamp(*px, half, screen_w - half);
        *py = clamp(*py, half, screen_h - half);
    }
}

/* Steer a walking bot around the ice instead of into it. */
static void ice_avoid_vector(double x, double y, double *vx, double *vy) {
    int i;
    for (i = 0; i < ICE_MAX; i++) {
        IceBlock *b = &ice_blocks[i];
        double dx = x - b->x;
        double dy = y - b->y;
        double d = sqrt(dx * dx + dy * dy);
        double reach = b->r + 86;
        if (b->on == 0 || d > reach || d < 0.001) {
            continue;
        }
        double k = (reach - d) / reach;
        /* Away from the block plus a sideways slide so the bot goes around. */
        *vx = *vx + (dx / d) * k * 120 - (dy / d) * k * 90;
        *vy = *vy + (dy / d) * k * 120 + (dx / d) * k * 90;
    }
}

/* Bounce a projectile off a block. Returns 1 when it hit something. */
static double ice_bounce_ball(double *x, double *y, double rad, double *vx, double *vy) {
    int i, hit = 0;
    int slot = 0;
    if (ice_block_at(*x, *y, &slot) == 0) {
        return 0;
    }
    for (i = 0; i < ICE_MAX; i++) {
        IceBlock *b = &ice_blocks[i];
        double ox, oy, px, py;
        if (b->on == 0) {
            continue;
        }
        ox = *x - b->x;
        oy = *y - b->y;
        px = b->r + rad - fabs(ox);
        py = b->r + rad - fabs(oy);
        if (px <= 0 || py <= 0) {
            continue;
        }
        if (px < py) {
            *x = *x + (ox < 0 ? -px : px);
            *vx = -*vx;
        } else {
            *y = *y + (oy < 0 ? -py : py);
            *vy = -*vy;
        }
        *vx = *vx * 0.98;
        *vy = *vy * 0.98;
        hit = 1;
    }
    return hit;
}

/* Light blue, semi-transparent cube with a pale edge highlight. */
static void draw_ice(void) {
    int i;
    for (i = 0; i < ICE_MAX; i++) {
        IceBlock *b = &ice_blocks[i];
        unsigned a, edge, core;
        double inset;
        if (b->on == 0 || b->a <= 0.01) {
            continue;
        }
        a = (unsigned)(int)(0.42 * b->a * 255.0 + 0.5);
        edge = (unsigned)(int)(0.85 * b->a * 255.0 + 0.5);
        core = (unsigned)(int)(0.26 * b->a * 255.0 + 0.5);
        if (a < 1) {
            continue;
        }
        roundrect(b->x - b->r, b->y - b->r, b->r * 2, b->r * 2, 10, (a << 24) | 0x9BD8F7u);
        roundrect_outline(b->x - b->r, b->y - b->r, b->r * 2, b->r * 2, 10, 3, (edge << 24) | 0xE8F6FFu);
        inset = b->r * 0.45;
        rect(b->x - inset, b->y - inset, inset, inset, (core << 24) | 0xFFFFFFu);
        rect(b->x + inset * 0.2, b->y + inset * 0.2, inset * 0.7, inset * 0.7, (core << 24) | 0xFFFFFFu);
    }
}
