/* Christmas snowballs, part 2: online hits, simulation, tick and drawing.
 * Included from game.c right after battle_christmas.inc. */

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
    xmas_ball.spin = xmas_ball.spin + dt * 6;
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
            xmas_ball.trail_a[(int)i] = xmas_ball.trail_a[(int)i] - dt / 0.45;
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
    double hw = 26 * 0.42;
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
            circle(xmas_ball.trail_x[(int)i], xmas_ball.trail_y[(int)i], XMAS_RADIUS * (0.12 + 0.45 * a),
                   ((unsigned)(int)(a * 170) << 24) | 0x00EAF7FFu);
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
    double scale = 0.55;
    uint32_t col = 0x7FC4E8u;
    if (christmas_snowballs_on() == 0 || (game_state != ST_SOLO && game_state != ST_ONLINE)) {
        return;
    }
    if (xmas_cd > 0) {
        label = ds_num_to_string((double)(floor(xmas_cd + 0.999)));
        col = 0x6E8CA8u;
        scale = 0.95;
    } else {
        label = tr_snowball();
    }
    draw_pad(christmas_pad_x(), christmas_pad_y(), christmas_pad_r(), col, label, scale);
}
