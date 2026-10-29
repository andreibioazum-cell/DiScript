static double winter_theme_on(void) {
    /* Online New Year is available to every account. Explicit ordinary/night
     * choices affect the visuals only; offline flags fall back to ordinary. */
    if (active_theme_mode() != 1) {
        return 0;
    }
    return seasonal_content_allowed();
}

/* Night theme: sleepy colours and slowly falling little stars. */
static double night_theme_on(void) { return active_theme_mode() == 2 ? 1 : 0; }

static const char *arena_ground_tex(void) {
    if (winter_theme_on() == 1 && snow_tex_ok == 1) {
        return SNOW_TEX;
    }
    return GRASS;
}

static double arena_ground_rgb(void) {
    if (night_theme_on() == 1) {
        return 0x0B1030;
    }
    if (winter_theme_on() == 1 && snow_tex_ok == 1) {
        return snow_ground_rgb;
    }
    return grass_ground_rgb;
}

static void draw_arena_background(void) {
    const char *ground = arena_ground_tex();
    rect(0, 0, screen_w, screen_h, arena_ground_rgb());
    if (night_theme_on() == 1) {
        /* A plain dark arena: the falling stars are drawn by draw_snow(). */
        return;
    }
    if (tex_ready(ground) == 0) {
        return;
    }
    double y = 0;
    while (y < screen_h) {
        double x = 0;
        while (x < screen_w) {
            tex(x, y, ground, 0, 1);
            x = x + arena_tile_size;
        }
        y = y + arena_tile_size;
    }
}

static double snow_active(void) {
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        return winter_theme_on();
    }
    return 0;
}

static void update_weather(void) {
    if (snow_active() == 0 && night_theme_on() == 0) {
        return;
    }
    snow_t = snow_t + dt;
    if (snow_t > 100000) {
        snow_t = 0;
    }
}

static double hash01(double i) {
    double v = sin(i * 127.1 + 11.7) * 43758.5453;
    return v - floor(v);
}

static double frac01(double v) {
    return v - floor(v);
}

static void flake_grow(double count) {
    while (flake_cached < count) {
        double k = 0;
        while (k < flake_fields) {
            arr_push(flake_cache, -1);
            k = k + 1;
        }
        flake_cached = flake_cached + 1;
    }
}

static void flake_refresh(double b, double i, double cyc) {
    arr_set(flake_cache, b, cyc);
    arr_set(flake_cache, b + 3, hash01(i * 7 + cyc * 131 + 1));
    arr_set(flake_cache, b + 4, hash01(i * 13 + cyc * 197 + 2));
    arr_set(flake_cache, b + 5, hash01(i * 29 + cyc * 57 + 3));
    arr_set(flake_cache, b + 6, hash01(i * 17 + cyc * 83 + 4));
}

static void draw_snow(void) {
    draw_night_stars();
    if (snow_active() == 0) {
        return;
    }
    flake_grow(event_flakes);
    double i = 0;
    while (i < event_flakes) {
        double b = i * flake_fields;
        double h1 = arr_get(flake_cache, b + 1);
        double h2 = arr_get(flake_cache, b + 2);
        if (h1 < 0) {
            h1 = hash01(i);
            h2 = hash01(i + 91);
            arr_set(flake_cache, b + 1, h1);
            arr_set(flake_cache, b + 2, h2);
        }
        double life = 2.2 + 2.6 * h1;
        double cyc = floor(snow_t / life + h2);
        if (arr_get(flake_cache, b) != cyc) {
            flake_refresh(b, i, cyc);
        }
        double ph = frac01(snow_t / life + h2);
        double hx = arr_get(flake_cache, b + 3);
        double hy = arr_get(flake_cache, b + 4);
        double hs = arr_get(flake_cache, b + 5);
        double hr = arr_get(flake_cache, b + 6);
        double s0 = 0.3 + 0.7 * hs;
        double s = s0 * (1 - ph);
        if (s > 0.02) {
            double xx = 36 + hx * (screen_w - 72);
            double yy = 36 + hy * (screen_h - 72);
            double hw = 32 * s;
            tex(xx - hw, yy - hw, SNOWFLAKE_TEX, hr * 2 * pi, s);
        }
        i = i + 1;
    }
}

/* Falling sleepy stars for the night theme. Drawn with primitives only, so
 * no extra texture has to be shipped. Uses the same clock as the snow. */
#define NIGHT_STARS 30
static void draw_night_stars(void) {
    double i = 0;
    if (night_theme_on() == 0) {
        return;
    }
    while (i < NIGHT_STARS) {
        double hx = hash01(i * 3 + 17);
        double hy = hash01(i * 7 + 91);
        double hs = hash01(i * 13 + 5);
        double fall = 3.5 + 4.5 * hs;
        double sz = 0.5 + 0.9 * hs;
        double x = 20 + hx * (screen_w - 40) + 12 * sin(snow_t * 0.5 + i);
        double y = -30 + frac01(snow_t / fall + hy) * (screen_h + 60);
        double tw = 0.55 + 0.45 * sin(snow_t * 2.2 + i * 1.7);
        double rgb = 0xB9C6FF;
        double arm = (3 + 4 * sz) * (0.7 + 0.3 * tw);
        uint32_t col;
        if (ds_mod(i, 3) == 1) {
            rgb = 0xFFE9B0;
        } else if (ds_mod(i, 3) == 2) {
            rgb = 0x9FB4FF;
        }
        col = (uint32_t)((unsigned)(int)(110 + 140 * tw) * 0x1000000u + rgb);
        line(x - arm, y, x + arm, y, 1.6, col);
        line(x, y - arm, x, y + arm, 1.6, col);
        circle(x, y, 1.4 + 1.4 * sz, col);
        i = i + 1;
    }
}
