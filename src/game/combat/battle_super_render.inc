static double station_beam_visible(double owner_hp) {
    if (finished != 0) {
        return 0;
    }
    if (owner_hp <= 0) {
        return 0;
    }
    return 1;
}

static double ebuc_elf_local(void) {
    return player_class == CLASS_EBUC && current_skin() == SKIN_SPECIAL ? 1 : 0;
}

static double ebuc_elf_slot(double slot) {
    double rb = slot * remote_fields;
    return arr_get(remotes, rb + 10) == CLASS_EBUC && arr_get(remotes, rb + 11) == SKIN_SPECIAL ? 1 : 0;
}

static const char *dispenser_tex_of(double is_elf) {
    if (is_elf == 1 && tex_ready(EBUC_ELF_DESPENSER_TEX) == 1) {
        return EBUC_ELF_DESPENSER_TEX;
    }
    return DESPENSER_TEX;
}

static void draw_station_beam_skin(double sx, double sy, double px, double py, double is_elf) {
    double dx = px - sx;
    double dy = py - sy;
    double d = sqrt(dx * dx + dy * dy);
    if (d < station_beam_min) {
        return;
    }
    double nx = dx / d;
    double ny = dy / d;
    double pull = station_beam_pull;
    if (pull > d * 0.5) {
        pull = d * 0.5;
    }
    double x1 = sx;
    double y1 = sy;
    double x2 = px - nx * pull;
    double y2 = py - ny * pull;
    uint32_t rgb = is_elf == 1 ? 0x009E9E9Eu : (unsigned)(int)station_beam_rgb;
    uint32_t core_rgb = is_elf == 1 ? 0x00FFFFFFu : (unsigned)(int)station_beam_core_rgb;
    uint32_t outer = (((unsigned)(int)station_beam_alpha) << 24) | rgb;
    uint32_t core = (((unsigned)(int)station_beam_core_alpha) << 24) | core_rgb;
    line(x1, y1, x2, y2, station_beam_w, outer);
    line(x1, y1, x2, y2, station_beam_core_w, core);
}

static void draw_station_beam(double sx, double sy, double px, double py) {
    draw_station_beam_skin(sx, sy, px, py, 0);
}

static void draw_turret_shadow_skin(double x, double y, double spawn, double is_elf) {
    double scale = station_scale;
    if (spawn > 0) {
        double sp = clamp(spawn / station_spawn_time, 0, 1);
        scale = station_scale * (1.0 + 0.25 * sp);
    }
    double hw = 25 * scale;
    tex_tint(x - hw - shadow_off, y - hw + shadow_off, dispenser_tex_of(is_elf), 0, scale,
             (((unsigned)(int)shadow_alpha) << 24));
}

static void draw_turret_shadow_at(double x, double y, double spawn) {
    draw_turret_shadow_skin(x, y, spawn, 0);
}

static void draw_turret_shadows(void) {
    double i = 0;
    double local_elf = ebuc_elf_local();
    while (i < turret_max) {
        if (turret_alive(i) == 1) {
            draw_turret_shadow_skin(arr_get(turret_x, i), arr_get(turret_y, i), arr_get(turret_spawn_t, i), local_elf);
        }
        i = i + 1;
    }
    if (game_state == ST_SOLO) {
        i = 0;
        while (i < turret_max) {
            if (enemy_turret_alive(i) == 1) {
                draw_turret_shadow_at(arr_get(enemy_turret_x, i), arr_get(enemy_turret_y, i),
                                      arr_get(enemy_turret_spawn_t, i));
            }
            i = i + 1;
        }
    }
    if (game_state != ST_ONLINE) {
        return;
    }
    if (net_slot() < 0) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        if (s != net_slot()) {
            double rem_elf = ebuc_elf_slot(s);
            double b = s * station_fields;
            double o = b + 1;
            double k = 0;
            while (k < turret_max) {
                if (arr_get(remote_station, o) == 1) {
                    draw_turret_shadow_skin(arr_get(remote_station, o + 1), arr_get(remote_station, o + 2), 0, rem_elf);
                }
                o = o + 5;
                k = k + 1;
            }
        }
        s = s + 1;
    }
}

static void draw_turret_at_skin(double x, double y, double hp, double max_hp, double spawn, double is_elf) {
    double scale = station_scale;
    if (spawn > 0) {
        double sp = clamp(spawn / station_spawn_time, 0, 1);
        scale = station_scale * (1.0 + 0.25 * sp);
    }
    double hw = 25 * scale;
    tex(x - hw, y - hw, dispenser_tex_of(is_elf), 0, scale);
    double bw = 46 * scale;
    double bh = 7;
    hud_bar(x - bw / 2, y - hw - 12, bw, bh, hp, max_hp, 0x7ED321, 0);
}

static void draw_turret_at(double x, double y, double hp, double max_hp, double spawn) {
    draw_turret_at_skin(x, y, hp, max_hp, spawn, 0);
}

static void draw_turret_boom_skin(double x, double y, double bt, double is_elf) {
    if (bt <= 0) {
        return;
    }
    double p = 1 - bt / station_boom_time;
    if (p < 0) {
        p = 0;
    }
    if (p > 1) {
        p = 1;
    }
    double fade_in = clamp(p / boom_fade_in, 0, 1);
    double fade_out = clamp(bt / (station_boom_time * boom_fade_out), 0, 1);
    double a = floor(fade_in * fade_out * 240);
    if (a <= 0) {
        return;
    }
    uint32_t c1 = is_elf == 1 ? 0x009E9E9Eu : 0x0000E676u;
    uint32_t c2 = is_elf == 1 ? 0x00FFFFFFu : 0x00B9F6CAu;
    uint32_t c3 = is_elf == 1 ? 0x00CFCFCFu : 0x0076FF03u;
    double r = 30 + 95 * p;
    ring(x, y, r, 9, (((unsigned)(int)a) << 24) | c1);
    double a85 = floor(a * 0.85);
    if (a85 > 0) {
        ring(x, y, r * 0.66, 6, (((unsigned)(int)a85) << 24) | c2);
    }
    double a65 = floor(a * 0.65);
    if (a65 > 0) {
        ring(x, y, r * 0.36, 5, (((unsigned)(int)a65) << 24) | c3);
    }
    double a70 = floor(a * 0.7);
    if (a70 > 0) {
        circle(x, y, r * 0.22 + 6 * (1 - p), (((unsigned)(int)a70) << 24) | c1);
    }
    double i = 0;
    while (i < 18) {
        double h1 = hash01(i * 37 + 11);
        double h2 = hash01(i * 61 + 7);
        double ang = h1 * 2 * pi;
        double rr = r * (0.5 + 0.5 * h2);
        double px = x + cos(ang) * rr;
        double py = y + sin(ang) * rr;
        double sz = 2 + 4 * (1 - p);
        double aa = floor(a * (0.7 + 0.3 * h2));
        if (aa > 0) {
            circle(px, py, sz, (((unsigned)(int)aa) << 24) | c1);
        }
        i = i + 1;
    }
}

static void draw_station(void) {
    double beam = -1;
    if (station_beam_visible(player->hp) == 1) {
        if (turret_alive(last_turret_slot) == 1) {
            beam = last_turret_slot;
        } else {
            double j = 0;
            while (j < turret_max) {
                if (turret_alive(j) == 1) {
                    beam = j;
                    j = turret_max;
                }
                j = j + 1;
            }
        }
    }
    double local_elf = ebuc_elf_local();
    if (beam >= 0) {
        draw_station_beam_skin(arr_get(turret_x, beam), arr_get(turret_y, beam), player->x, player->y, local_elf);
    }
    double i = 0;
    while (i < turret_max) {
        if (turret_alive(i) == 1) {
            draw_turret_at_skin(arr_get(turret_x, i), arr_get(turret_y, i), arr_get(turret_hp, i),
                                arr_get(turret_maxhp, i), arr_get(turret_spawn_t, i), local_elf);
        }
        i = i + 1;
    }
    double bi = 0;
    while (bi < arr_len(station_boom_ts)) {
        double bt = arr_get(station_boom_ts, bi);
        if (bt > 0) {
            draw_turret_boom_skin(arr_get(station_boom_xs, bi), arr_get(station_boom_ys, bi), bt, local_elf);
        }
        bi = bi + 1;
    }
}

static double universe_particle_color(double i, double a) {
    double m = (int)i % 4;
    if (m == 0) {
        return (((unsigned)(int)a) << 24) | 0x0000E676;
    }
    if (m == 1) {
        return (((unsigned)(int)a) << 24) | 0x0000E5FF;
    }
    if (m == 2) {
        return (((unsigned)(int)a) << 24) | 0x00FFFFFF;
    }
    return (((unsigned)(int)a) << 24) | 0x0076FF03;
}

static void draw_universe_at(double cx, double cy, double seed, double t, double fade) {
    if (t <= 0 || fade <= 0) {
        return;
    }
    double p = clamp(t / ebuc_universe_collapse_time, 0, 1);
    double collapsed = 0;
    if (p >= 1) {
        collapsed = 1;
    }
    if (collapsed == 0) {
        double i = 0;
        while (i < ebuc_universe_particles) {
            double h1 = hash01(seed * 13 + i * 91 + 7);
            double h2 = hash01(seed * 17 + i * 131 + 19);
            double h3 = hash01(seed * 23 + i * 173 + 31);
            double h4 = hash01(seed * 29 + i * 211 + 47);
            double sx = 40 + h1 * (screen_w - 80);
            double sy = 40 + h2 * (screen_h - 80);
            double k = p * p;
            double px = lerp(sx, cx, k);
            double py = lerp(sy, cy, k);
            double size = 2 + 6 * h3 * (1 - p);
            double a = floor((120 + 120 * p) * fade);
            if (a > 0) {
                circle(px, py, size, universe_particle_color(i, a));
            }
            if (h4 > 0.86) {
                double trail = floor(a * 0.55);
                if (trail > 0) {
                    circle(px, py, size * 2.1, universe_particle_color(i + 1, trail));
                }
            }
            i = i + 1;
        }
    }
    if (collapsed == 1) {
        double core = floor(240 * fade);
        if (core > 0) {
            circle(cx, cy, 10, universe_particle_color(2, core));
        }
    }
    double ep = (p - 0.78) / 0.22;
    if (ep > 1) {
        ep = 1;
    }
    if (ep > 0) {
        double r = (80 + 120 * ep) * 1.0;
        double af = floor(240 * ep * fade);
        if (af > 0) {
            ring(cx, cy, r, 10, (((unsigned)(int)af) << 24) | 0x0000E676);
        }
        if (ep > 0.45) {
            double a2 = floor(160 * (ep - 0.45) / 0.55 * fade);
            if (a2 > 0) {
                ring(cx, cy, r * 0.62, 8, (((unsigned)(int)a2) << 24) | 0x0076FF03);
            }
        }
    }
}

static void draw_universe(void) {
    if (universe_fx_a <= 0) {
        return;
    }
    draw_universe_at(universe_x, universe_y, universe_id, universe_t, universe_fx_a);
}

static void draw_remote_station(void) {
    if (net_slot() < 0) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        double b = s * station_fields;
        if (s != net_slot()) {
            double rb = s * remote_fields;
            double owner_alive = arr_get(remotes, rb + 5);
            double ox = arr_get(remotes, rb + 1);
            double oy = arr_get(remotes, rb + 2);
            double beam = -1;
            double i = 0;
            if (station_beam_visible(arr_get(remotes, rb + 4)) == 1 && owner_alive == 1) {
                double lslot = arr_get(remote_station, b);
                if (lslot >= 1 && lslot <= turret_max) {
                    if (arr_get(remote_station, b + 1 + (lslot - 1) * 5) == 1) {
                        beam = lslot - 1;
                    }
                }
                if (beam < 0) {
                    while (i < turret_max) {
                        if (arr_get(remote_station, b + 1 + i * 5) == 1) {
                            beam = i;
                            i = turret_max;
                        }
                        i = i + 1;
                    }
                }
            }
            double rem_elf = ebuc_elf_slot(s);
            if (beam >= 0) {
                double bo = b + 1 + beam * 5;
                draw_station_beam_skin(arr_get(remote_station, bo + 1), arr_get(remote_station, bo + 2), ox, oy, rem_elf);
            }
            double o = b + 1;
            double st = 0;
            i = 0;
            while (i < turret_max) {
                st = arr_get(remote_station, o);
                if (st == 1) {
                    double rl = net_player_level(s);
                    draw_turret_at_skin(arr_get(remote_station, o + 1), arr_get(remote_station, o + 2),
                                        arr_get(remote_station, o + 3), ebuc_station_hp_for(rl), 0, rem_elf);
                } else if (st == 2) {
                    draw_turret_boom_skin(arr_get(remote_station, o + 1), arr_get(remote_station, o + 2),
                                          arr_get(remote_station, o + 4), rem_elf);
                }
                o = o + 5;
                i = i + 1;
            }
        }
        s = s + 1;
    }
}

static void draw_remote_universe(void) {
    if (net_slot() < 0) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        double b = s * universe_fields;
        if (s != net_slot() && arr_get(runiverse_fx_a, s) > 0) {
            draw_universe_at(arr_get(remote_universe, b + 2), arr_get(remote_universe, b + 3),
                             arr_get(remote_universe, b), arr_get(remote_universe, b + 4), arr_get(runiverse_fx_a, s));
        }
        s = s + 1;
    }
}
