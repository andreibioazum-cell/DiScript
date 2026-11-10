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

static void xmas_step_local(XmasBall *b) {
    double left, piece, ox, oy;
    if (b->active == 0) {
        return;
    }
    b->t = b->t + dt;
    left = 1;
    while (left > 0 && b->active == 1) {
        ox = b->x;
        oy = b->y;
        piece = left < 0.125 ? left : 0.125;
        left = left - piece;
        b->x = b->x + b->vx * dt * piece;
        b->y = b->y + b->vy * dt * piece;
        b->bounces = b->bounces + xmas_bounce(&b->x, &b->y, &b->vx, &b->vy);
        b->bounces = b->bounces + ice_bounce_ball(&b->x, &b->y, XMAS_RADIUS, &b->vx, &b->vy);
        b->bounces = b->bounces + hack_bounce_ball(&b->x, &b->y, XMAS_RADIUS, &b->vx, &b->vy);
        if (b->bounces > XMAS_MAX_BOUNCES || b->t > XMAS_MAX_TIME) {
            xmas_burst(b);
            return;
        }
        if (game_state == ST_SOLO) {
            if (enemy != NULL && enemy->hp > 0 &&
                ball_seg_hits_fighter(ox, oy, b->x, b->y, XMAS_RADIUS, enemy->x, enemy->y, enemy->angle,
                                      enemy_scale) == 1) {
                xmas_burst(b);
                return;
            }
        } else if (game_state == ST_ONLINE) {
            if (xmas_seg_hits_online_foe(ox, oy, b->x, b->y) == 1) {
                xmas_burst(b);
                return;
            }
        }
    }
    xmas_trail_push(b);
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
        r->bounces = r->bounces + ice_bounce_ball(&r->x, &r->y, XMAS_RADIUS, &r->vx, &r->vy);
        r->bounces = r->bounces + hack_bounce_ball(&r->x, &r->y, XMAS_RADIUS, &r->vx, &r->vy);
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
    int s = 0, k = 0;
    double i = 0;
    xmas_cd = tick_down(xmas_cd);
    while (k < XMAS_MAX_BALLS) {
        XmasBall *b = &xmas_balls[k];
        i = 0;
        if (b->boom_t > 0) {
            b->boom_t = tick_down(b->boom_t);
        }
        while (i < XMAS_TRAIL) {
            if (b->trail_a[(int)i] > 0) {
                b->trail_a[(int)i] = b->trail_a[(int)i] - dt / 0.45;
                if (b->trail_a[(int)i] < 0) {
                    b->trail_a[(int)i] = 0;
                }
            }
            i = i + 1;
        }
        xmas_step_local(b);
        k = k + 1;
    }
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

static void xmas_draw_body(double x, double y) {
    double flake_scale = 0.72;
    double flake_half = 32 * flake_scale;
    /* Plain white ball with a soft halo, in the clean winter style of ebuC's
     * elf kit: no hard blue outline around the snowball. */
    circle(x, y, XMAS_RADIUS + 4, 0x24FFFFFFu);
    circle(x, y, XMAS_RADIUS, RGB(251,253,255));
    /* Full, clear snowflake; the red hitbox/debug outline is never rendered. */
    tex_tint(x - flake_half, y - flake_half, SNOWFLAKE_TEX, 0, flake_scale, RGB(255,255,255));
}

static void draw_christmas_snowballs(void) {
    double i = 0;
    int s = 0, k = 0;
    if (christmas_snowballs_on() == 0) {
        return;
    }
    while (k < XMAS_MAX_BALLS) {
        XmasBall *b = &xmas_balls[k];
        i = 0;
        while (i < XMAS_TRAIL) {
            if (b->trail_a[(int)i] > 0) {
                double a = b->trail_a[(int)i];
                circle(b->trail_x[(int)i], b->trail_y[(int)i], XMAS_RADIUS * (0.12 + 0.45 * a),
                       ((unsigned)(int)(a * 170) << 24) | 0x00EAF7FFu);
            }
            i = i + 1;
        }
        if (b->active == 1) {
            xmas_draw_body(b->x, b->y);
        }
        if (b->boom_t > 0) {
            draw_boom_at_r(b->boom_x, b->boom_y, b->boom_t, XMAS_BOOM_RADIUS);
        }
        k = k + 1;
    }
    while (s < NET_SLOTS) {
        if (xmas_remote[s].on == 1) {
            xmas_draw_body(xmas_remote[s].x, xmas_remote[s].y);
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
    uint32_t col = 0x6FA6C7u;
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
