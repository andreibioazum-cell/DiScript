/* CodeCracker overclock ultimate, contact bite/heal, and spark trail FX. */

/* CodeCracker wears a random cube skin from the whole roster with a light,
 * slightly cool wash - never a white one. The old 0.42 white tint made the
 * class look brighter than every other cube on the field. */
static const double codecracker_tint_rgb = 0xBFD2F0;
static const double codecracker_tint_a = 0.16;
static double codecracker_look_pick = -1;

static double codecracker_random_look(void) {
    int try_count = 0;
    while (try_count < 16) {
        int cls = rand() % (int)class_count;
        double skin = (rand() % 5 == 0) ? SKIN_SPECIAL : SKIN_NORMAL;
        double look = class_look((double)cls, skin);
        if (look != LOOK_NONE && look_ready(look) == 1) {
            return look;
        }
        try_count = try_count + 1;
    }
    return LOOK_NONE;
}

/* One random skin per battle, so the silhouette does not flicker each frame. */
static double codecracker_match_look(void) {
    if (codecracker_look_pick >= 0 && look_ready(codecracker_look_pick) == 1) {
        return codecracker_look_pick;
    }
    codecracker_look_pick = codecracker_random_look();
    return codecracker_look_pick;
}

static void codecracker_look_reset(void) { codecracker_look_pick = -1; }

static void codecracker_refresh(void) { codecracker_ok = 1; }

static double codecracker_on(void) { return player_class == CLASS_CODECRACKER; }

static double codecracker_of_slot(double slot) {
    return arr_get(remotes, slot * remote_fields + 10) == CLASS_CODECRACKER;
}

static double codecracker_touching(double x, double y) {
    return dist(player->x, player->y, x, y) < player->size * 2;
}

static void codecracker_heal(void) {
    player->hp = clamp(player->hp + codecracker_touch_heal, 0, player->max_hp);
}

/* Tick active overclock timer and start super cooldown when it ends. */
static void codecracker_tick_super_timer(void) {
    double was = codecracker_super_t;
    codecracker_super_t = tick_down(codecracker_super_t);
    if (was > 0 && codecracker_super_t <= 0) {
        super_cd = admin_cheat_nocd() == 1 ? 0 : codecracker_super_cd_for(player_level);
    }
}

/* Check whether local CodeCracker overclock is active. */
static double codecracker_super_active(void) {
    return codecracker_on() == 1 && codecracker_super_t > 0;
}

static void codecracker_tick_solo(void) {
    codecracker_tick_super_timer();
    codecracker_touch_cd_left = tick_down(codecracker_touch_cd_left);
    if (codecracker_super_active() == 0 || finished != 0 || enemy->hp <= 0) {
        return;
    }
    if (codecracker_touch_cd_left > 0 || codecracker_touching(enemy->x, enemy->y) == 0) {
        return;
    }
    codecracker_touch_cd_left = codecracker_touch_cd;
    codecracker_heal();
    enemy_apply_damage_at(codecracker_bite_damage(player_level), player->x, player->y);
}

/* Resolve online CodeCracker contact heal and damage across synced slots. */
static void codecracker_tick_online(void) {
    codecracker_tick_super_timer();
    double s = 0;
    while (s < max_players) {
        double out_i = s;
        double in_i = max_players + s;
        arr_set(codecracker_touch_cds, out_i, tick_down(arr_get(codecracker_touch_cds, out_i)));
        arr_set(codecracker_touch_cds, in_i, tick_down(arr_get(codecracker_touch_cds, in_i)));
        s = s + 1;
    }
    if (finished != 0 || player->hp <= 0) {
        return;
    }
    s = 0;
    while (s < max_players) {
        double b = s * remote_fields;
        if (s != net_slot() && arr_get(remotes, b + 5) == 1 &&
            codecracker_touching(arr_get(remotes, b + 1), arr_get(remotes, b + 2)) == 1) {
            if (codecracker_super_active() == 1 && arr_get(codecracker_touch_cds, s) <= 0) {
                arr_set(codecracker_touch_cds, s, codecracker_touch_cd);
                codecracker_heal();
            }
            if (codecracker_of_slot(s) == 1 && net_player_codecracker_super(s) == 1 &&
                arr_get(codecracker_touch_cds, max_players + s) <= 0) {
                arr_set(codecracker_touch_cds, max_players + s, codecracker_touch_cd);
                if (revive_prot <= 0 &&
                    take_damage_at(codecracker_bite_damage(net_player_level(s)) *
                                   player_damage_mult_for(CLASS_CODECRACKER),
                                   arr_get(remotes, b + 1), arr_get(remotes, b + 2)) == 1) {
                    return;
                }
            }
        }
        s = s + 1;
    }
}

static double codecracker_tint(void) {
    return floor(codecracker_tint_a * 255) * 0x1000000 + codecracker_tint_rgb;
}

static double codecracker_tint_local(void) {
    if (codecracker_on() == 1) {
        return codecracker_tint();
    }
    return 0;
}

static double codecracker_tint_slot(double slot) {
    if (codecracker_of_slot(slot) == 1) {
        return codecracker_tint();
    }
    return 0;
}

/* Pack alpha and RGB for deterministic overclock spark particles. */
static double codecracker_fx_color(double alpha, double rgb) {
    double a = clamp(alpha, 0, 255);
    return (((unsigned)(int)a) << 24) | ((unsigned)(int)rgb);
}

static void draw_codecracker_fx_at(double cx, double cy, double angle, double phase) {
    double dx = cos(angle);
    double dy = sin(angle);
    double pulse = 0.5 + 0.5 * sin(phase * 8.5);
    double blue = 0x0068D8FF;
    double cyan = 0x0000E5FF;
    double ice = 0x00C8F7FF;

    /* Emit directional spark trail behind the fighter. */
    double i = 0;
    while (i < 24) {
        double h1 = hash01(i * 37 + 17);
        double h2 = hash01(i * 61 + 29);
        double h3 = hash01(i * 83 + 43);
        double h4 = hash01(i * 107 + 59);
        double cycle = phase * (1.25 + h1 * 0.8) + h2;
        cycle = cycle - floor(cycle);
        double fade = sin(cycle * pi);
        double back = 25 + cycle * (92 + h3 * 62);
        double side = sin(cycle * pi * (2.0 + h1) + h4 * 2 * pi) * (5 + h2 * 15);
        double px = cx - dx * back - dy * side;
        double py = cy - dy * back + dx * side;
        double tail = 6 + h3 * 15;
        double alpha = fade * (125 + 90 * h4);
        double size = 1.4 + h2 * 2.7 + pulse * 0.7;
        double rgb = h1 > 0.46 ? cyan : blue;
        if (alpha > 1) {
            line(px + dx * 3, py + dy * 3, px - dx * tail, py - dy * tail,
                 1.1 + h2 * 1.8, codecracker_fx_color(alpha * 0.62, rgb));
            circle(px, py, size * 2.4, codecracker_fx_color(alpha * 0.18, ice));
            circle(px, py, size, codecracker_fx_color(alpha, rgb));
            if (h4 > 0.62) {
                circle(px, py, size * 0.38, codecracker_fx_color(alpha + 70, 0x00FFFFFF));
            }
        }
        i = i + 1;
    }
}

static void draw_codecracker_fx_local(void) {
    if (codecracker_super_active() == 0) {
        return;
    }
    double elapsed = codecracker_super_time - codecracker_super_t;
    draw_codecracker_fx_at(player->x, player->y, player->angle, elapsed);
}

static void draw_codecracker_fx_slot(double slot, double x, double y, double angle) {
    if (codecracker_of_slot(slot) == 0 || net_player_codecracker_super(slot) == 0) {
        return;
    }
    draw_codecracker_fx_at(x, y, angle, event_t + slot * 0.31);
}
