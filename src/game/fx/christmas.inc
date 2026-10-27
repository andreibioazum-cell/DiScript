/* Root node Christmas: 1 gives snowballs to every class without changing
 * the visuals. The two holiday admins always have them; Sanch never does
 * (he keeps earned rewards only). Regular players see no winter theme. */
static double christmas_snowballs_on(void) {
    if (sanch_no_christmas() == 1) {
        return 0;
    }
    if (christmas_admin_allowed() == 1) {
        return 1;
    }
    return net_christmas() == 1;
}

/* Menu snow, jingle and the lobby banner follow the visual theme. */
static double christmas_active(void) {
    return winter_theme_on() == 1;
}

/* Return 1 when winter snow animation is active on menu screens. */
static double christmas_menu_active(void) {
    if (christmas_active() == 0) {
        return 0;
    }
    return game_state == ST_LOBBY || game_state == ST_MODES || game_state == ST_SETTINGS;
}

static void update_christmas_snow(void) {
    if (christmas_menu_active() == 0) {
        return;
    }
    christmas_snow_t = christmas_snow_t + dt;
    if (christmas_snow_t > 100000) {
        christmas_snow_t = 0;
    }
}

static void christmas_flake_grow(double count) {
    while (christmas_flake_cached < count) {
        double k = 0;
        while (k < flake_fields) {
            arr_push(christmas_flake_cache, -1);
            k = k + 1;
        }
        christmas_flake_cached = christmas_flake_cached + 1;
    }
}

static void christmas_flake_refresh(double b, double i, double cyc) {
    arr_set(christmas_flake_cache, b, cyc);
    arr_set(christmas_flake_cache, b + 3, hash01(i * 7 + cyc * 131 + 5));
    arr_set(christmas_flake_cache, b + 4, hash01(i * 13 + cyc * 197 + 6));
    arr_set(christmas_flake_cache, b + 5, hash01(i * 29 + cyc * 57 + 7));
    arr_set(christmas_flake_cache, b + 6, hash01(i * 17 + cyc * 83 + 8));
}

static void draw_christmas_snow(void) {
    if (christmas_menu_active() == 0) {
        return;
    }
    christmas_flake_grow(christmas_menu_flakes);
    double i = 0;
    while (i < christmas_menu_flakes) {
        double b = i * flake_fields;
        double h1 = arr_get(christmas_flake_cache, b + 1);
        double h2 = arr_get(christmas_flake_cache, b + 2);
        if (h1 < 0) {
            h1 = hash01(i + 51);
            h2 = hash01(i + 142);
            arr_set(christmas_flake_cache, b + 1, h1);
            arr_set(christmas_flake_cache, b + 2, h2);
        }
        double fall_time = 3.2 + 3.6 * h1;
        double cyc = floor(christmas_snow_t / fall_time + h2);
        if (arr_get(christmas_flake_cache, b) != cyc) {
            christmas_flake_refresh(b, i, cyc);
        }
        double ph = frac01(christmas_snow_t / fall_time + h2);
        double hx = arr_get(christmas_flake_cache, b + 3);
        double hs = arr_get(christmas_flake_cache, b + 5);
        double hr = arr_get(christmas_flake_cache, b + 6);
        double sway = 14 * sin(christmas_snow_t * 0.6 + i * 2.1);
        double x = hx * screen_w + sway;
        double y = -40 + ph * (screen_h + 80);
        double s = 0.35 + 0.5 * hs;
        double hw = 26 * s;
        double spin_dir = 1;
        if (ds_mod(i, 2) == 0) {
            spin_dir = -1;
        }
        double spin = spin_dir * (0.9 + 1.4 * hr);
        double ang = (hr + christmas_snow_t * spin) * 2 * pi;
        /* Tint snowflake sprite pure white across all themes. */
        tex_tint(x - hw, y - hw, SNOWFLAKE_TEX, ang, s, 0xFFFFFFFF);
        i = i + 1;
    }
}

static double christmas_music_active(void) {
    if (christmas_jingle_ok != 1) {
        return 0;
    }
    if (christmas_active() == 0) {
        return 0;
    }
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        return 0;
    }
    return 1;
}

static void update_christmas_jingle(void) {
    if (christmas_jingle_tried == 0) {
        christmas_jingle_tried = 1;
        christmas_jingle_ok = snd_load(WINTER_JINGLE);
        if (christmas_jingle_ok == 0) {
            ds_log("christmas: no bundled winter_jingle.wav, the vibe stays visual-only");
        }
    }
    if (christmas_jingle_ok == 0) {
        return;
    }
    if (christmas_music_active() == 1) {
        /* Stop standard lobby music when switching to winter track. */
        if (snd_playing(LOBBY_MUSIC) == 1) {
            snd_stop(LOBBY_MUSIC);
        }
        music_level = 0;
        snd_volume(WINTER_JINGLE, music_volume / 100 * winter_jingle_volume);
        if (snd_playing(WINTER_JINGLE) == 0) {
            snd_loop(WINTER_JINGLE);
        }
        return;
    }
    if (snd_playing(WINTER_JINGLE) == 1) {
        snd_stop(WINTER_JINGLE);
    }
}

static void update_christmas(void) {
    /* Keep the public root flag polled even outside the shop. */
    (void)christmas_snowballs_on();
    update_christmas_snow();
    update_christmas_jingle();
}

/* Return 1 when backdrop snow pass should render behind UI and fading notices. */
static double christmas_snow_under_notice_active(void) {
    if (christmas_menu_active() == 0 || studio_open == 1) {
        return 0;
    }
    /* Skip snow draw while warning modal is fully opaque. */
    if (warn_open == 1 && warn_a >= 1) {
        return 0;
    }
    return 1;
}

static double christmas_snow_over_notice_active(void) {
    return christmas_menu_active() == 1 && studio_open == 1 && t_fade <= 0;
}

static void draw_christmas_under_notices(void) {
    if (christmas_snow_under_notice_active() == 1) {
        draw_christmas_snow();
    }
}

static void draw_christmas_over_notices(void) {
    if (christmas_snow_over_notice_active() == 1) {
        draw_christmas_snow();
    }
}
