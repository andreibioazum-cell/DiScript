/* ebuC's dual-mode ultimate: dispenser turret or bouncing projectile. */

#define BALL_SPEED 620.0
#define BALL_BOUNCE_ACCEL 1.04
#define BALL_RADIUS 24.0
#define BALL_BOOM_RADIUS 150.0
#define BALL_BASE_DAMAGE 0.15
#define BALL_BOUNCE_DAMAGE 2.8
#define BALL_BOUNCE_DAMAGE_GROW 1.28
#define BALL_MAX_BOUNCES 10.0
#define BALL_MAX_TIME 8.0
#define BALL_TRAIL 12

typedef struct {
    double active;
    double x, y, vx, vy;
    double bounces, t, spin, shot;
    double boom_x, boom_y, boom_t;
    double trail_x[BALL_TRAIL], trail_y[BALL_TRAIL], trail_a[BALL_TRAIL];
} UltBall;

static UltBall ball;
static UltBall enemy_ball;

/* Ultimate mode toggle button geometry (0 = dispenser, 1 = projectile). */
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

/* Draw ebuC's ultimate mode toggle button. */
static void draw_ult_mode(void) {
    double x = ult_mode_x();
    double y = ult_mode_y();
    double r = ult_mode_r();
    uint32_t fill;
    if (player_class != CLASS_EBUC || (game_state != ST_SOLO && game_state != ST_ONLINE)) {
        return;
    }
    fill = ball_mode == 1 ? RGB(255,179,0) : RGB(47,111,237);
    circle(x, y, r, fill);
    ring(x, y, r, 3, RGB(0,0,0));
}

static void ult_ball_clear(UltBall *b) {
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
    b->shot = 0;
    while (i < BALL_TRAIL) {
        b->trail_a[(int)i] = 0;
        i = i + 1;
    }
}

static void enemy_ball_reset(void) {
    ult_ball_clear(&enemy_ball);
    enemy_ball_mode = 0;
}

static double enemy_ball_active(void) { return enemy_ball.active; }

static void ball_reset(void) {
    ult_ball_clear(&ball);
    ball_mode = 0;
    enemy_ball_reset();
    remote_bricks_reset();
}

static void ult_ball_trail_push(UltBall *b, double x, double y) {
    double i = BALL_TRAIL - 1;
    while (i > 0) {
        b->trail_x[(int)i] = b->trail_x[(int)(i - 1)];
        b->trail_y[(int)i] = b->trail_y[(int)(i - 1)];
        b->trail_a[(int)i] = b->trail_a[(int)(i - 1)];
        i = i - 1;
    }
    /* Offset trail sparks slightly so projectile does not occlude its own trail. */
    b->trail_x[0] = x + (rand() % 21 - 10);
    b->trail_y[0] = y + (rand() % 21 - 10);
    b->trail_a[0] = 1;
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
        /* Publish projectile launch parameters for remote clients. */
        net_publish_brick(player->x / screen_w, player->y / screen_h, dx, dy, ball.shot);
    }
    super_cd = admin_cheat_nocd() == 1 ? 0 : super_cd_time;
    ability_snd(STATION_SOUND);
    ds_log("magic projectile thrown");
}

/* Launch bot ebuC's bouncing projectile toward the player. */
static void enemy_ball_fire(void) {
    double a;
    if (finished != 0 || enemy_ball.active == 1 || game_state != ST_SOLO) {
        return;
    }
    a = atan2(player->y - enemy->y, player->x - enemy->x);
    enemy->angle = a;
    enemy_ball.x = enemy->x;
    enemy_ball.y = enemy->y;
    enemy_ball.vx = cos(a) * BALL_SPEED;
    enemy_ball.vy = sin(a) * BALL_SPEED;
    enemy_ball.bounces = 0;
    enemy_ball.t = 0;
    enemy_ball.spin = 0;
    enemy_ball.active = 1;
    enemy_ball.shot = enemy_ball.shot + 1;
    ability_snd(STATION_SOUND);
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

static void ult_ball_burst(UltBall *b, int is_enemy) {
    double n = b->bounces > BALL_MAX_BOUNCES ? BALL_MAX_BOUNCES : b->bounces;
    b->active = 0;
    b->boom_x = b->x;
    b->boom_y = b->y;
    b->boom_t = boom_time;
    if (is_enemy) {
        if (finished == 0 && revive_prot <= 0 &&
            ball_hits_fighter_r(b->x, b->y, BALL_BOOM_RADIUS, player->x, player->y, player->angle, player_scale) == 1) {
            take_damage_at(ball_damage_for(n) * enemy_damage_mult_for(enemy_class), b->x, b->y);
        }
        return;
    }
    if (game_state != ST_ONLINE && enemy_revive_prot <= 0 &&
        ball_hits_fighter_r(b->x, b->y, BALL_BOOM_RADIUS, enemy->x, enemy->y, enemy->angle, enemy_scale) == 1) {
        enemy_apply_damage_at(ball_damage_for(n), b->x, b->y);
    }
}

/* Check swept projectile segment against living remote fighters. */
static double ball_seg_hits_online_foe(double ox, double oy, double nx, double ny, double r) {
    double s = 0;
    while (s < max_players) {
        double rb = s * remote_fields;
        if (s != net_slot() && arr_get(remotes, rb + 5) == 1 && arr_get(remotes, rb + 4) > 0) {
            double rx = arr_get(remotes, rb + 1);
            double ry = arr_get(remotes, rb + 2);
            double ra = arr_get(remotes, rb + 3);
            if (ball_seg_hits_fighter(ox, oy, nx, ny, r, rx, ry, ra, player_scale) == 1) {
                return 1;
            }
        }
        s = s + 1;
    }
    return 0;
}

static void ult_ball_step(UltBall *b, int is_enemy) {
    double left, piece;
    if (b->active == 0) {
        return;
    }
    b->t = b->t + dt;
    b->spin = b->spin + dt * 6;
    /* Substep projectile motion and test swept segments against fighters. */
    left = 1;
    while (left > 0 && b->active == 1) {
        double ox = b->x;
        double oy = b->y;
        piece = left < 0.125 ? left : 0.125;
        left = left - piece;
        b->x = b->x + b->vx * dt * piece;
        b->y = b->y + b->vy * dt * piece;
        if (ice_bounce_ball(&b->x, &b->y, BALL_RADIUS, &b->vx, &b->vy) == 1) {
            b->bounces = b->bounces + 1;
        }
        if (hack_bounce_ball(&b->x, &b->y, BALL_RADIUS, &b->vx, &b->vy) == 1) {
            b->bounces = b->bounces + 1;
        }
        if (b->x < BALL_RADIUS) {
            b->x = BALL_RADIUS;
            b->vx = (0 - b->vx) * BALL_BOUNCE_ACCEL;
            b->vy = b->vy * BALL_BOUNCE_ACCEL;
            b->bounces = b->bounces + 1;
        } else if (b->x > screen_w - BALL_RADIUS) {
            b->x = screen_w - BALL_RADIUS;
            b->vx = (0 - b->vx) * BALL_BOUNCE_ACCEL;
            b->vy = b->vy * BALL_BOUNCE_ACCEL;
            b->bounces = b->bounces + 1;
        }
        if (b->y < BALL_RADIUS) {
            b->y = BALL_RADIUS;
            b->vx = b->vx * BALL_BOUNCE_ACCEL;
            b->vy = (0 - b->vy) * BALL_BOUNCE_ACCEL;
            b->bounces = b->bounces + 1;
        } else if (b->y > screen_h - BALL_RADIUS) {
            b->y = screen_h - BALL_RADIUS;
            b->vx = b->vx * BALL_BOUNCE_ACCEL;
            b->vy = (0 - b->vy) * BALL_BOUNCE_ACCEL;
            b->bounces = b->bounces + 1;
        }
        if (b->bounces > BALL_MAX_BOUNCES || b->t > BALL_MAX_TIME) {
            ult_ball_burst(b, is_enemy);
            return;
        }
        if (is_enemy) {
            if (player->hp > 0 &&
                ball_seg_hits_fighter(ox, oy, b->x, b->y, BALL_RADIUS, player->x, player->y, player->angle,
                                      player_scale) == 1) {
                ult_ball_burst(b, 1);
                return;
            }
        } else if (game_state == ST_ONLINE) {
            if (ball_seg_hits_online_foe(ox, oy, b->x, b->y, BALL_RADIUS) == 1) {
                ult_ball_burst(b, 0);
                return;
            }
        } else if (enemy->hp > 0 &&
                   ball_seg_hits_fighter(ox, oy, b->x, b->y, BALL_RADIUS, enemy->x, enemy->y, enemy->angle,
                                         enemy_scale) == 1) {
            ult_ball_burst(b, 0);
            return;
        }
    }
    ult_ball_trail_push(b, b->x, b->y);
}

static void ball_tick(void) { ult_ball_step(&ball, 0); }

static void enemy_ball_tick(void) { ult_ball_step(&enemy_ball, 1); }

static void ult_ball_draw_body(UltBall *b) {
    double i = 0;
    while (i < BALL_TRAIL) {
        if (b->trail_a[(int)i] > 0) {
            double a = b->trail_a[(int)i];
            circle(b->trail_x[(int)i], b->trail_y[(int)i], BALL_RADIUS * (0.12 + 0.45 * a),
                   ((unsigned)(int)(a * 170) << 24) | 0x003A8BFF);
        }
        i = i + 1;
    }
    if (b->active == 1) {
        circle(b->x, b->y, BALL_RADIUS, RGB(58,139,255));
        if (show_hitboxes == 1) {
            draw_hit_circle(b->x, b->y, BALL_RADIUS, 1);
        }
    }
    if (b->boom_t > 0) {
        draw_boom_at_r(b->boom_x, b->boom_y, b->boom_t, BALL_BOOM_RADIUS);
        if (show_hitboxes == 1) {
            draw_hit_circle(b->boom_x, b->boom_y, BALL_BOOM_RADIUS, b->boom_t / boom_time);
        }
    }
}

static void draw_ball(void) {
    if (admin_cheat_ball_path() == 1) {
        if (ball.active == 1) {
            draw_ball_path_from(ball.x, ball.y, ball.vx, ball.vy, ball.bounces, ball.t, 0x0056CCF2u, 1, 0);
        } else if (player_class == CLASS_EBUC && ball_mode == 1 && finished == 0) {
            draw_ball_path_from(player->x, player->y, cos(player->angle) * BALL_SPEED, sin(player->angle) * BALL_SPEED,
                                0, 0, 0x0056CCF2u, 1, 26 * player_scale);
        }
    }
    ult_ball_draw_body(&ball);
}

static void draw_enemy_ball(void) {
    if (game_state != ST_SOLO) {
        return;
    }
    /* The "ebuC's bounces" cheat predicts the PLAYER's own ball only: the
     * enemy's bounce path is never revealed, so the foe keeps its surprise.
     * Only the ball body and its trail are drawn here. */
    ult_ball_draw_body(&enemy_ball);
}

static void ult_ball_fade(UltBall *b) {
    double i = 0;
    if (b->boom_t > 0) {
        b->boom_t = tick_down(b->boom_t);
    }
    while (i < BALL_TRAIL) {
        if (b->trail_a[(int)i] > 0) {
            b->trail_a[(int)i] = b->trail_a[(int)i] - dt / 0.45;
            if (b->trail_a[(int)i] < 0) {
                b->trail_a[(int)i] = 0;
            }
        }
        i = i + 1;
    }
}

/* Fade projectile trail particles between casts. */
static void ball_fade_tick(void) { ult_ball_fade(&ball); }

static void enemy_ball_fade_tick(void) { ult_ball_fade(&enemy_ball); }
