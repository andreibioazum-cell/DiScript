/* Root node Christmas: 1 gives snowballs to every online class without
 * changing the visual theme. With the flag off, the two holiday admins retain
 * their preview snowballs while their New Year theme is active. */
static double christmas_snowballs_on(void) {
    if (net_christmas() == 1) {
        return 1;
    }
    if (christmas_admin_allowed() == 1) {
        return active_theme_mode() == 1 ? 1 : 0;
    }
    return 0;
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
        tex_tint(x - hw, y - hw, SNOWFLAKE_TEX, ang, s, RGB(255,255,255));
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
            ds_log("christmas: no bundled winter_lobby.wav, the vibe stays visual-only");
        }
    }
    if (christmas_jingle_ok == 0) {
        return;
    }
    if (disco_music_blocks() == 1) {
        if (snd_playing(WINTER_JINGLE) == 1) snd_stop(WINTER_JINGLE);
        return;
    }
    if (christmas_music_active() == 1) {
        /* Stop either lobby loop when switching to the winter track. */
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

/* Re-apply the palette when the selected theme or the holiday flag changes
 * (the New Year theme is grey until the flag actually unlocks it). */
static double xmas_theme_sig = -1;
static void update_christmas(void) {
    double sig;
    /* Keep the public root flag polled even outside the shop. */
    (void)christmas_snowballs_on();
    sig = active_theme_mode() * 16 + winter_theme_on() * 8 + seasonal_content_allowed() * 4 +
          net_flags_reachable() * 2 + net_christmas();
    if (sig != xmas_theme_sig) {
        xmas_theme_sig = sig;
        apply_winter_theme();
        apply_theme();
    }
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
