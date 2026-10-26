static double winter_theme_on(void) {
    return winter_theme == 1 && seasonal_admin_allowed() == 1;
}

static const char *arena_ground_tex(void) {
    if (winter_theme_on() == 1 && snow_tex_ok == 1) {
        return SNOW_TEX;
    }
    return GRASS;
}

static double arena_ground_rgb(void) {
    if (winter_theme_on() == 1 && snow_tex_ok == 1) {
        return snow_ground_rgb;
    }
    return grass_ground_rgb;
}

static void draw_arena_background(void) {
    const char *ground = arena_ground_tex();
    rect(0, 0, screen_w, screen_h, arena_ground_rgb());
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
    if (snow_active() == 0) {
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
