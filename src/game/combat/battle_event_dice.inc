/* Event 4 (Cube): central cube spawns collectible character cubes worth candies. */

#define DICE_CUBE_ORDINARY 0
#define DICE_CUBE_AZUM 1
#define DICE_CUBE_ASTRA 2
#define DICE_CUBE_EBUC 3
#define DICE_CUBE_SANTA 4
#define DICE_CUBE_CODECRACKER 5
#define DICE_CUBE_LEGENDARY 6

static void dice_event_reset(void) {
    dice_active = 1;
    dice_off_t = 0;
    dice_throw_cd = 0.6;
    dice_next = 0;
    arr_clear(dice_cubes);
    double i = 0;
    while (i < dice_cubes_max * dice_fields) {
        arr_push(dice_cubes, 0);
        i = i + 1;
    }
}

static double dice_prize_of(double cube) {
    if (cube == DICE_CUBE_LEGENDARY) {
        return 95;
    }
    if (cube == DICE_CUBE_CODECRACKER) {
        return 60;
    }
    if (cube == DICE_CUBE_SANTA) {
        return 30;
    }
    if (cube == DICE_CUBE_EBUC) {
        return 15;
    }
    if (cube == DICE_CUBE_ASTRA) {
        return 9;
    }
    if (cube == DICE_CUBE_AZUM) {
        return 5;
    }
    return 2;
}

static double dice_roll_cube(void) {
    double roll = (double)(rand() % 100000); /* 0.001% units */
    if (roll < 80) {
        return DICE_CUBE_LEGENDARY; /* 0.08% -> 95 */
    }
    roll = roll - 80;
    if (roll < 1500) {
        return DICE_CUBE_CODECRACKER; /* 1.5% -> 60, everyone */
    }
    roll = roll - 1500;
    if (roll < 3000) {
        return DICE_CUBE_SANTA; /* 3% -> 30, everyone */
    }
    roll = roll - 3000;
    if (roll < 7000) {
        return DICE_CUBE_EBUC; /* 7% -> 15 */
    }
    roll = roll - 7000;
    if (roll < 14000) {
        return DICE_CUBE_ASTRA; /* 14% -> 9 */
    }
    roll = roll - 14000;
    if (roll < 25000) {
        return DICE_CUBE_AZUM; /* 25% -> 5 */
    }
    return DICE_CUBE_ORDINARY; /* the rest -> 2 */
}

static const char *dice_cube_tex(double cube) {
    if (cube == DICE_CUBE_LEGENDARY) {
        return AZUM_ZOMBIE_CARD_TEX;
    }
    if (cube == DICE_CUBE_SANTA) {
        return SANTA_CARD_TEX;
    }
    if (cube == DICE_CUBE_EBUC) {
        return EBUC_CARD_TEX;
    }
    if (cube == DICE_CUBE_ASTRA) {
        return ASTRA_CARD_TEX;
    }
    if (cube == DICE_CUBE_AZUM) {
        return AZUM_CARD_TEX;
    }
    if (cube == DICE_CUBE_CODECRACKER) {
        return CODECRACKER_CARD_TEX;
    }
    return ORDINARY_CARD_TEX; /* Ordinary */
}

static const char *dice_cube_name(double cube) {
    if (cube == DICE_CUBE_LEGENDARY) {
        return language == 1 ? "Легендарный Азум" : "Legendary Azum";
    }
    if (cube == DICE_CUBE_CODECRACKER) {
        return class_name_of(CLASS_CODECRACKER);
    }
    if (cube == DICE_CUBE_SANTA) {
        return class_name_of(CLASS_SANTA);
    }
    if (cube == DICE_CUBE_EBUC) {
        return class_name_of(CLASS_EBUC);
    }
    if (cube == DICE_CUBE_ASTRA) {
        return class_name_of(CLASS_ASTRA);
    }
    if (cube == DICE_CUBE_AZUM) {
        return class_name_of(CLASS_AZUM);
    }
    return class_name_of(CLASS_ORDINARY);
}

static void dice_throw_cube(void) {
    if (dice_next >= dice_cubes_max) {
        return;
    }
    double b = dice_next * dice_fields;
    double m = 70;
    arr_set(dice_cubes, b, 1);
    arr_set(dice_cubes, b + 1, dice_roll_cube());
    arr_set(dice_cubes, b + 2, 0);
    arr_set(dice_cubes, b + 3, screen_w / 2);
    arr_set(dice_cubes, b + 4, screen_h / 2);
    arr_set(dice_cubes, b + 5, m + rand() % (int)(screen_w - 2 * m));
    arr_set(dice_cubes, b + 6, m + rand() % (int)(screen_h - 2 * m));
    arr_set(dice_cubes, b + 7, 0);
    dice_next = dice_next + 1;
    ds_log("dice event: threw %s (+%g)", dice_cube_name(arr_get(dice_cubes, b + 1)),
           dice_prize_of(arr_get(dice_cubes, b + 1)));
}

/* Grace period before clearing active cubes when event mode drops off. */
#define DICE_OFF_GRACE 1.0

static void update_dice_event(void) {
    if (event_mode != 4) {
        if (dice_active == 0) {
            dice_off_t = 0;
            return;
        }
        dice_off_t = dice_off_t + dt;
        if (dice_off_t < DICE_OFF_GRACE) {
            return;
        }
        dice_off_t = 0;
        dice_active = 0;
        return;
    }
    dice_off_t = 0;
    if (dice_active == 0) {
        dice_event_reset();
    }
    dice_throw_cd = dice_throw_cd - dt;
    if (dice_throw_cd <= 0) {
        dice_throw_cd = dice_throw_interval;
        dice_throw_cube();
    }
    double i = 0;
    while (i < dice_cubes_max) {
        double b = i * dice_fields;
        if (arr_get(dice_cubes, b) == 1) {
            double t = arr_get(dice_cubes, b + 2);
            if (t < dice_flight) {
                arr_set(dice_cubes, b + 2, t + dt);
            } else {
                double v = arr_get(dice_cubes, b + 7) + dt;
                arr_set(dice_cubes, b + 7, v);
                if (v >= dice_cube_lifetime) {
                    arr_set(dice_cubes, b, 0);
                } else if (dist(arr_get(dice_cubes, b + 5), arr_get(dice_cubes, b + 6), player->x, player->y) <
                           player->size * 0.65 + 26) {
                    double prize = dice_prize_of(arr_get(dice_cubes, b + 1));
                    candies = candies + prize;
                    save_progress();
                    ds_log("dice event: collected %s +%g candies -> %g",
                           dice_cube_name(arr_get(dice_cubes, b + 1)), prize, candies);
                    arr_set(dice_cubes, b, 0);
                }
            }
        }
        i = i + 1;
    }
}

static void dice_draw_cube(double cx, double cy, double size, double cube, double ang) {
    double half = size / 2;
    roundrect(cx - half - 3, cy - half - 3, size + 6, size + 6, 14, 0xFF292944);
    roundrect(cx - half, cy - half, size, size, 12, 0xFFFFFFFF);
    if (tex_ready(dice_cube_tex(cube)) == 1) {
        tex(cx - half + 6, cy - half + 6, dice_cube_tex(cube), ang, (size - 12) / CARD_RENDER_PX);
    } else {
        circle(cx, cy, half - 10, 0xFFB0BEC5);
    }
}

static void draw_dice_cubes(void) {
    if (event_mode != 4) {
        return;
    }
    double i = 0;
    while (i < dice_cubes_max) {
        double b = i * dice_fields;
        if (arr_get(dice_cubes, b) == 1) {
            double cube = arr_get(dice_cubes, b + 1);
            double t = arr_get(dice_cubes, b + 2);
            double k = clamp(t / dice_flight, 0, 1);
            double e = 1 - (1 - k) * (1 - k);
            double xx = arr_get(dice_cubes, b + 3) + (arr_get(dice_cubes, b + 5) - arr_get(dice_cubes, b + 3)) * e;
            double yy = arr_get(dice_cubes, b + 4) + (arr_get(dice_cubes, b + 6) - arr_get(dice_cubes, b + 4)) * e;
            double v = arr_get(dice_cubes, b + 7);
            double shrink = 1;
            if (v > dice_cube_lifetime - 0.7) {
                shrink = clamp((dice_cube_lifetime - v) / 0.7, 0, 1);
            }
            double bob = 0;
            if (k >= 1) {
                bob = 3 * sin(event_t * 4 + i);
            }
            double size = 64 * shrink * (0.5 + 0.5 * e);
            if (size > 4) {
                dice_draw_cube(xx, yy + bob, size, cube, (1 - e) * 2.4);
                if (k >= 1) {
                    const char *prize = ds_concat("+", ds_num_to_string(dice_prize_of(cube)));
                    double pw = text_ink_width(prize) * 0.5;
                    text_scaled(prize, xx - pw / 2, yy + bob + 36, 0xFFFFD54A, 0.5);
                    const char *nm = dice_cube_name(cube);
                    double nw = text_ink_width(nm) * 0.38;
                    text_scaled(nm, xx - nw / 2, yy + bob - 58, 0xFFFFFFFF, 0.38);
                }
            }
        }
        i = i + 1;
    }
}

static void draw_dice_event(void) {
    if (event_mode != 4) {
        return;
    }
    /* Match Event 3 Santa: standard player fighter scale, blurred shadow, wobble, and overhead nick. */
    double cx = screen_w / 2;
    double cy = screen_h / 2;
    double pop = 1;
    if (event_t < 0.4) {
        double p = event_t / 0.4;
        pop = 1 - (1 - p) * (1 - p);
    }
    double sc = cube_scale * player_scale * pop;
    double hw = 25 * sc;
    if (sc <= 0.01) {
        return;
    }
    double wob = 6 * pop * sin(event_t * 3);
    double ang = event_t * 1.2;
    draw_shadow_blur_tex(cx - shadow_off, cy + shadow_off, hw, sc, ORDINARY_TEX, ang,
                         shadow_alpha * 2.5, hw * 0.07);
    tex(cx - hw, cy - hw + wob, ORDINARY_TEX, ang, sc);
    if (pop > 0.35) {
        draw_overhead_nick(language == 1 ? "Кубик" : "Cube", cx, cy + wob - hw, 0xFFFFFF);
    }
}
