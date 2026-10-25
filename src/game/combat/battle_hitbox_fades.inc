static double hb_live(double active) {
    if (finished != 0) {
        return 0;
    }
    return active;
}

static double hb_fade(double a, double live, double tin, double tout) {
    if (live == 1) {
        return fade_up_a(a, tin);
    }
    return fade_down_a(a, tout);
}

static void tick_hitbox_fades(void) {
    tick_astra_fades();
    dash_box_a = hb_fade(dash_box_a, hb_live(dash_active), hitbox_fade_in, hitbox_fade_out);
    snow_ball_a = hb_fade(snow_ball_a, hb_live(gift->active), hitbox_fade_in, hitbox_fade_out);
    snow_boom_a = hb_fade(snow_boom_a, hb_live(boom_t > 0), hitbox_fade_in, hitbox_fade_out);
    universe_box_a = hb_fade(universe_box_a, hb_live(universe_active), hitbox_fade_in, hitbox_fade_out);
    universe_fx_a = hb_fade(universe_fx_a, hb_live(universe_active), universe_fade_in, universe_fade_out);
    double i = 0;
    while (i < turret_max) {
        arr_set(turret_box_a, i,
                hb_fade(arr_get(turret_box_a, i), hb_live(turret_alive(i)), hitbox_fade_in, hitbox_fade_out));
        i = i + 1;
    }
    if (game_state == ST_SOLO) {
        double epunch = enemy->state == 2;
        enemy_punch_a = hb_fade(enemy_punch_a, hb_live(epunch), aim_fade_in, aim_fade_out);
        edash_box_a = hb_fade(edash_box_a, hb_live(enemy_dash_active), hitbox_fade_in, hitbox_fade_out);
        esnow_ball_a = hb_fade(esnow_ball_a, hb_live(enemy_gift->active), hitbox_fade_in, hitbox_fade_out);
        esnow_boom_a = hb_fade(esnow_boom_a, hb_live(enemy_boom_t > 0), hitbox_fade_in, hitbox_fade_out);
        i = 0;
        while (i < turret_max) {
            arr_set(enemy_turret_box_a, i,
                    hb_fade(arr_get(enemy_turret_box_a, i), hb_live(enemy_turret_alive(i)), hitbox_fade_in,
                            hitbox_fade_out));
            i = i + 1;
        }
    } else {
        enemy_punch_a = fade_down_a(enemy_punch_a, hitbox_fade_out);
    }
    if (game_state != ST_ONLINE) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        if (s != net_slot()) {
            arr_set(rpunch_a, s,
                    hb_fade(arr_get(rpunch_a, s), hb_live(arr_get(remote_punches, s * punch_fields) == 1), aim_fade_in,
                            aim_fade_out));
            arr_set(rdash_box_a, s,
                    hb_fade(arr_get(rdash_box_a, s), hb_live(remote_dash_get(s, DASH_ACTIVE)), hitbox_fade_in,
                            hitbox_fade_out));
            double b = s * snow_fields;
            arr_set(rsnow_ball_a, s,
                    hb_fade(arr_get(rsnow_ball_a, s), hb_live(arr_get(remote_snow, b + 1)), hitbox_fade_in,
                            hitbox_fade_out));
            arr_set(rsnow_boom_a, s,
                    hb_fade(arr_get(rsnow_boom_a, s), hb_live(arr_get(remote_snow, b + 5) > 0), hitbox_fade_in,
                            hitbox_fade_out));
            b = s * universe_fields;
            arr_set(runiverse_box_a, s,
                    hb_fade(arr_get(runiverse_box_a, s), hb_live(arr_get(remote_universe, b + 1)), hitbox_fade_in,
                            hitbox_fade_out));
            arr_set(runiverse_fx_a, s,
                    hb_fade(arr_get(runiverse_fx_a, s), hb_live(arr_get(remote_universe, b + 1)), universe_fade_in,
                            universe_fade_out));
            double k = 0;
            while (k < turret_max) {
                double stb = s * station_fields;
                arr_set(rstation_box_a, s * turret_max + k,
                        hb_fade(arr_get(rstation_box_a, s * turret_max + k),
                                hb_live(arr_get(remote_station, stb + 1 + k * 5)), hitbox_fade_in, hitbox_fade_out));
                k = k + 1;
            }
        }
        s = s + 1;
    }
}

static void tick_astra_fades(void) {
    double live = (astra_state != 0 && astra_t >= ASTRA_STRIKE_DELAY - 1e-9) ? 1 : 0;
    astra_box_a = hb_fade(astra_box_a, hb_live(live), hitbox_fade_in, hitbox_fade_out);
    double elive = (egrab_state != 0 && egrab_t >= ASTRA_STRIKE_DELAY - 1e-9) ? 1 : 0;
    egrab_box_a = hb_fade(egrab_box_a, hb_live(elive), hitbox_fade_in, hitbox_fade_out);
    if (game_state != ST_ONLINE) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        double rlive = (s != net_slot() && remote_grab_get(s, GRAB_ACTIVE) == 1 &&
                        remote_grab_get(s, GRAB_T) >= ASTRA_STRIKE_DELAY - 1e-9)
                           ? 1
                           : 0;
        arr_set(rgrab_box_a, s, hb_fade(arr_get(rgrab_box_a, s), hb_live(rlive), hitbox_fade_in, hitbox_fade_out));
        s = s + 1;
    }
}
