static double fighter_half(double scale) {
    return body_px_half * cube_scale * scale;
}

static double player_half(void) {
    return fighter_half(player_scale);
}

static double enemy_half(void) {
    return fighter_half(enemy_scale);
}

static double turret_half(void) {
    return turret_body_px_half * station_scale;
}

static double sat_axis_ok(double nx, double ny, double tx, double ty, double aux, double auy, double ae0, double ae1,
                          double bux, double buy, double be0, double be1) {
    double ra = ae0 * fabs(aux * nx + auy * ny) + ae1 * fabs(auy * nx - aux * ny);
    double rb = be0 * fabs(bux * nx + buy * ny) + be1 * fabs(buy * nx - bux * ny);
    if (fabs(tx * nx + ty * ny) > ra + rb) {
        return 0;
    }
    return 1;
}

static double rects_overlap(double ax, double ay, double aux, double auy, double ae0, double ae1, double bx, double by,
                            double bux, double buy, double be0, double be1) {
    double tx = bx - ax;
    double ty = by - ay;
    if (sat_axis_ok(aux, auy, tx, ty, aux, auy, ae0, ae1, bux, buy, be0, be1) == 0) {
        return 0;
    }
    if (sat_axis_ok(0 - auy, aux, tx, ty, aux, auy, ae0, ae1, bux, buy, be0, be1) == 0) {
        return 0;
    }
    if (sat_axis_ok(bux, buy, tx, ty, aux, auy, ae0, ae1, bux, buy, be0, be1) == 0) {
        return 0;
    }
    if (sat_axis_ok(0 - buy, bux, tx, ty, aux, auy, ae0, ae1, bux, buy, be0, be1) == 0) {
        return 0;
    }
    return 1;
}

static double strip_hits_box(double sx, double sy, double dx, double dy, double off0, double off1, double half_w,
                             double bx, double by, double bang, double bh) {
    double len = off1 - off0;
    if (len <= 0 || half_w <= 0 || bh <= 0) {
        return 0;
    }
    double mid = (off0 + off1) / 2;
    return rects_overlap(sx + dx * mid, sy + dy * mid, dx, dy, len / 2, half_w, bx, by, cos(bang), sin(bang), bh, bh);
}

static double circle_hits_rect(double cx, double cy, double r, double bx, double by, double bang, double hu,
                               double hv) {
    if (r <= 0 || hu <= 0 || hv <= 0) {
        return 0;
    }
    double ux = cos(bang);
    double uy = sin(bang);
    double rx = cx - bx;
    double ry = cy - by;
    double lu = clamp(rx * ux + ry * uy, 0 - hu, hu);
    double lv = clamp(ry * ux - rx * uy, 0 - hv, hv);
    double px = bx + ux * lu - uy * lv;
    double py = by + uy * lu + ux * lv;
    double ex = cx - px;
    double ey = cy - py;
    if (ex * ex + ey * ey <= r * r) {
        return 1;
    }
    return 0;
}

static double circle_hits_box(double cx, double cy, double r, double bx, double by, double bang, double bh) {
    return circle_hits_rect(cx, cy, r, bx, by, bang, bh, bh);
}

/* Test circle against fighter cube body and side hands matching 50x50 sprite pixels. */
static double ball_hits_fighter_r(double cx, double cy, double r, double fx, double fy, double fang, double scale) {
    double s = cube_scale * scale;
    double body_h = 16.5 * s;
    double hand_u = 4.5 * s;
    double hand_v = 23.0 * s;
    if (circle_hits_rect(cx, cy, r, fx, fy, fang, body_h, body_h) == 1) {
        return 1;
    }
    return circle_hits_rect(cx, cy, r, fx, fy, fang, hand_u, hand_v);
}

/* Test swept projectile segment against fighter cube body and side hands. */
static double ball_seg_hits_fighter(double ox, double oy, double nx, double ny, double r, double fx, double fy,
                                    double fang, double scale) {
    if (ball_hits_fighter_r(ox, oy, r, fx, fy, fang, scale) == 1 ||
        ball_hits_fighter_r(nx, ny, r, fx, fy, fang, scale) == 1) {
        return 1;
    }
    double dx = nx - ox;
    double dy = ny - oy;
    double len = sqrt(dx * dx + dy * dy);
    if (len <= 1e-6 || r <= 0) {
        return 0;
    }
    double ux = dx / len;
    double uy = dy / len;
    double mx = (ox + nx) * 0.5;
    double my = (oy + ny) * 0.5;
    double s = cube_scale * scale;
    double fu = cos(fang);
    double fv = sin(fang);
    if (rects_overlap(mx, my, ux, uy, len * 0.5, r, fx, fy, fu, fv, 16.5 * s, 16.5 * s) == 1) {
        return 1;
    }
    return rects_overlap(mx, my, ux, uy, len * 0.5, r, fx, fy, fu, fv, 4.5 * s, 23.0 * s);
}

static double strip_hits_player(double sx, double sy, double dx, double dy, double off0, double off1, double half_w) {
    return strip_hits_box(sx, sy, dx, dy, off0, off1, half_w, player->x, player->y, player->angle, player_half());
}

static double strip_hits_enemy(double sx, double sy, double dx, double dy, double off0, double off1, double half_w) {
    return strip_hits_box(sx, sy, dx, dy, off0, off1, half_w, enemy->x, enemy->y, enemy->angle, enemy_half());
}

static double strip_hits_turret(double sx, double sy, double dx, double dy, double off0, double off1, double half_w,
                                double tx, double ty) {
    return strip_hits_box(sx, sy, dx, dy, off0, off1, half_w, tx, ty, 0, turret_half());
}

static double circle_hits_player(double cx, double cy, double r) {
    return circle_hits_box(cx, cy, r, player->x, player->y, player->angle, player_half());
}

static double circle_hits_enemy(double cx, double cy, double r) {
    return circle_hits_box(cx, cy, r, enemy->x, enemy->y, enemy->angle, enemy_half());
}

static double circle_hits_turret(double cx, double cy, double r, double tx, double ty) {
    return circle_hits_box(cx, cy, r, tx, ty, 0, turret_half());
}

static double target_is_forward(double px, double py, double pdx, double pdy, double tx, double ty) {
    return (tx - px) * pdx + (ty - py) * pdy > 0;
}

static double punch_hits_player(double px, double py, double pdx, double pdy, double reach, double width) {
    if (target_is_forward(px, py, pdx, pdy, player->x, player->y) == 0) {
        return 0;
    }
    return strip_hits_player(px, py, pdx, pdy, punch_forward_offset, punch_forward_offset + reach, width / 2);
}

static double punch_hits_enemy(double px, double py, double pdx, double pdy, double reach, double width) {
    if (target_is_forward(px, py, pdx, pdy, enemy->x, enemy->y) == 0) {
        return 0;
    }
    return strip_hits_enemy(px, py, pdx, pdy, punch_forward_offset, punch_forward_offset + reach, width / 2);
}

static double punch_hits_turret(double px, double py, double pdx, double pdy, double reach, double width, double tx,
                                double ty) {
    if (target_is_forward(px, py, pdx, pdy, tx, ty) == 0) {
        return 0;
    }
    return strip_hits_turret(px, py, pdx, pdy, punch_forward_offset, punch_forward_offset + reach, width / 2, tx, ty);
}

static double along(double px, double py, double pdx, double pdy, double tx, double ty) {
    return (tx - px) * pdx + (ty - py) * pdy;
}
