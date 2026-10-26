static double fighter_look_of(double slot) { return arr_get(fighter_looks, slot); }

static double fighter_pose_of(double slot, double punching) {
    if (punching == 1 && arr_get(fighter_swings, slot) == 1) {
        return 1;
    }
    return 0;
}

static void resolve_fighter_look(double slot, double cls, double skin) {
    double wanted = class_look(cls, skin);
    if (look_ready(wanted) == 1) {
        /* Publish the matching idle/punch pair atomically. */
        arr_set(fighter_looks, slot, wanted);
        arr_set(fighter_swings, slot, 1);
        return;
    }
    /* Fall back to solid cube only when texture upload fails. */
    arr_set(fighter_looks, slot, LOOK_NONE);
    arr_set(fighter_swings, slot, 0);
}

static void resolve_fighter_looks(void) {
    resolve_fighter_look(FLOOK_ME, player_class, current_skin());
    if (game_state == ST_ONLINE) {
        double i = 0;
        while (i < max_players) {
            double b = i * remote_fields;
            resolve_fighter_look(FLOOK_REMOTE + i, arr_get(remotes, b + 10), arr_get(remotes, b + 11));
            i = i + 1;
        }
        return;
    }
    resolve_fighter_look(FLOOK_BOT, enemy_class, SKIN_NORMAL);
}

/* Draw one shadow blur tap; skip zero alpha since engine maps alpha 0 to opaque. */
static void shadow_tap_circle(double x, double y, double r, double a) {
    unsigned ai = (unsigned)(int)(a + 0.5);
    if (ai < 1) {
        return;
    }
    circle(x, y, r, ((uint32_t)ai) << 24);
}

static void shadow_tap_tex(double x, double y, double half_px, double scale, const char *name, double ang, double a) {
    unsigned ai = (unsigned)(int)(a + 0.5);
    if (ai < 1) {
        return;
    }
    tex_tint(x - half_px, y - half_px, name, ang, scale, ((uint32_t)ai) << 24);
}

/* Draw 5-tap blurred circular shadow. */
static void draw_shadow_blur_circle(double cx, double cy, double r, double alpha, double blur) {
    double rr = blur * 0.72 * 0.70710678;
    double tap = alpha * 0.18;
    shadow_tap_circle(cx, cy, r, alpha * 0.28);
    shadow_tap_circle(cx + rr, cy + rr, r, tap);
    shadow_tap_circle(cx - rr, cy + rr, r, tap);
    shadow_tap_circle(cx - rr, cy - rr, r, tap);
    shadow_tap_circle(cx + rr, cy - rr, r, tap);
}

/* Draw 5-tap blurred sprite shadow. */
static void draw_shadow_blur_tex(double cx, double cy, double half_px, double scale, const char *name, double ang,
                                 double alpha, double blur) {
    double rr = blur * 0.72 * 0.70710678;
    double tap = alpha * 0.18;
    shadow_tap_tex(cx, cy, half_px, scale, name, ang, alpha * 0.28);
    shadow_tap_tex(cx + rr, cy + rr, half_px, scale, name, ang, tap);
    shadow_tap_tex(cx - rr, cy + rr, half_px, scale, name, ang, tap);
    shadow_tap_tex(cx - rr, cy - rr, half_px, scale, name, ang, tap);
    shadow_tap_tex(cx + rr, cy - rr, half_px, scale, name, ang, tap);
}

/* Draw semi-transparent motion-blur sprites along recent movement vector. */
static void draw_motion_ghosts(double x, double y, double dx, double dy, double half_px, double scale,
                               const char *name, double ang) {
    double len = sqrt(dx * dx + dy * dy);
    double lim = half_px * 1.1;
    if (len < 2) {
        return;
    }
    if (len > lim) {
        dx = dx / len * lim;
        dy = dy / len * lim;
        len = lim;
    }
        /* Scale trail opacity with square root of speed. */
        double a = 0.40 * sqrt(len / lim);
    if (a < 0.02) {
        return;
    }
    int i = 1;
    while (i <= 4) {
        double t = i / 5.0;
        double ga = a * (1 - t);
        unsigned ai = (unsigned)(int)(ga * 255.0 + 0.5);
        if (ai >= 1) {
            tex_tint(x - dx * t - half_px, y - dy * t - half_px, name, ang, scale, ((uint32_t)ai) << 24);
        }
        i = i + 1;
    }
}

static void draw_fighter_shadow(double x, double y, double ang, double look, double punching) {
    double half_px = 25 * cube_scale * player_scale;
    double sprite_scale = cube_scale * player_scale;
    double sx = x - shadow_off;
    double sy = y + shadow_off;
    if (look == LOOK_NONE) {
        draw_shadow_blur_circle(sx, sy, half_px, shadow_alpha * 2.5, half_px * 0.07);
        return;
    }
    draw_shadow_blur_tex(sx, sy, half_px, sprite_scale, look_tex(look, punching), sprite_a(ang), shadow_alpha * 2.5,
                         half_px * 0.07);
}

static void draw_fighter_body(double x, double y, double ang, double look, double punching) {
    double half_px = 25 * cube_scale * player_scale;
    double sprite_scale = cube_scale * player_scale;
    if (look == LOOK_NONE) {
        rect(x - half_px, y - half_px, 2 * half_px, 2 * half_px, fighter_fallback_rgb);
        return;
    }
    tex(x - half_px, y - half_px, look_tex(look, punching), sprite_a(ang), sprite_scale);
}

static void draw_fighter_hurt(double x, double y, double ang, double look, double punching, double scale, double a) {
    if (a <= 0) {
        return;
    }
    double fa = floor(a * 255 * hurt_alpha);
    if (fa < 1) {
        return;
    }
    if (look == LOOK_NONE) {
        return;
    }
    double half_px = 25 * cube_scale * scale;
    tex_tint(x - half_px, y - half_px, look_tex(look, punching), sprite_a(ang), cube_scale * scale,
             (((unsigned)(int)fa) << 24) | ((unsigned)(int)hurt_rgb));
}

/* Draw fighter sprite with optional flat color tint. */
static void draw_fighter_tint(double x, double y, double ang, double look, double punching, double scale, double tint) {
    if (tint <= 0 || look == LOOK_NONE) {
        return;
    }
    double half_px = 25 * cube_scale * scale;
    tex_tint(x - half_px, y - half_px, look_tex(look, punching), sprite_a(ang), cube_scale * scale,
             (uint32_t)(int)tint);
}
