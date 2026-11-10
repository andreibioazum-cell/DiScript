static void clear_remote_punch(double s) {
    double rb = s * punch_fields;
    arr_set(remote_punches, rb, 0);
    arr_set(remote_punches, rb + 5, 0);
}

static void start_remote_punch(double s) {
    double rb = s * punch_fields;
    arr_set(remote_punches, rb, 1);
    arr_set(remote_punches, rb + 1, net_player_punch_x(s) * screen_w);
    arr_set(remote_punches, rb + 2, net_player_punch_y(s) * screen_h);
    arr_set(remote_punches, rb + 3, net_player_punch_dx(s));
    arr_set(remote_punches, rb + 4, net_player_punch_dy(s));
    arr_set(remote_punches, rb + 5, 0);
}

static double finite_or(double v, double fb) {
    if (v != v || v > 1000000000 || v < 0 - 1000000000) {
        return fb;
    }
    return v;
}

static void read_remotes(void) {
    if (net_slot() < 0) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        double b = s * remote_fields;
        if (net_player_online(s) == 1 && s != net_slot()) {
            double x = finite_or(net_player_x(s), 0.5) * screen_w;
            double y = finite_or(net_player_y(s), 0.5) * screen_h;
            double a = finite_or(net_player_angle(s), arr_get(remotes, b + 9));
            if (arr_get(remotes, b) == 0) {
                arr_set(remotes, b, 1);
                arr_set(remotes, b + 1, x);
                arr_set(remotes, b + 2, y);
                arr_set(remotes, b + 3, a);
                arr_set(remotes, b + 7, x);
                arr_set(remotes, b + 8, y);
                arr_set(remotes, b + 9, a);
                arr_set(remotes, b + 6, net_player_punch(s));
                reset_remote_smooth_slot((int)s);
                baseline_remote_slot(s);
                /* Snapshot class and skin on join. */
                double rcls = finite_or(net_player_class(s), CLASS_ORDINARY);
                double rskin = finite_or(net_player_skin(s), SKIN_NORMAL);
                if (rcls < CLASS_ORDINARY || rcls > CLASS_CODECRACKER) {
                    rcls = CLASS_ORDINARY;
                }
                if (rcls == 5) {
                    rcls = CLASS_ASTRA;
                }
                if (rskin < SKIN_NORMAL || rskin >= SKIN_COUNT) {
                    rskin = SKIN_NORMAL;
                }
                arr_set(remotes, b + 10, floor(rcls));
                arr_set(remotes, b + 11, floor(rskin));
            } else {
                arr_set(remotes, b + 7, x);
                arr_set(remotes, b + 8, y);
                arr_set(remotes, b + 9, a);
            }
            arr_set(remotes, b + 4, finite_or(net_player_hp(s), arr_get(remotes, b + 4)));
            arr_set(remotes, b + 5, net_player_alive(s) == 1);
            double net_p = finite_or(net_player_punch(s), arr_get(remotes, b + 6));
            double prev_p = arr_get(remotes, b + 6);
            if (net_p != prev_p) {
                arr_set(remotes, b + 6, net_p);
                start_remote_punch(s);
            }
        } else {
            arr_set(remotes, b, 0);
            arr_set(remotes, b + 5, 0);
            clear_remote_punch(s);
        }
        s = s + 1;
    }
}

static void tick_remote_punches(void) {
    double s = 0;
    while (s < max_players) {
        double rb = s * punch_fields;
        if (arr_get(remote_punches, rb) > 0) {
            double b = s * remote_fields;
            double t = arr_get(remote_punches, rb + 5) + dt;
            if (t > PUNCH_WINDUP + punch_time) {
                clear_remote_punch(s);
            } else {
                arr_set(remote_punches, rb + 5, t);
                if (arr_get(remotes, b) == 1 && arr_get(remotes, b + 5) == 1) {
                    double ra = arr_get(remotes, b + 3);
                    arr_set(remote_punches, rb + 1, arr_get(remotes, b + 1));
                    arr_set(remote_punches, rb + 2, arr_get(remotes, b + 2));
                    arr_set(remote_punches, rb + 3, cos(ra));
                    arr_set(remote_punches, rb + 4, sin(ra));
                }
            }
        }
        s = s + 1;
    }
}

static void update_remote_punches(void) {
    double s = 0;
    while (s < max_players) {
        double rb = s * punch_fields;
        if (arr_get(remote_punches, rb) > 0) {
            if (arr_get(remotes, s * remote_fields + 5) == 0) {
                clear_remote_punch(s);
            } else if (arr_get(remote_punches, rb) == 1 && arr_get(remote_punches, rb + 5) >= PUNCH_WINDUP - 1e-9) {
                double target = punch_resolve_target(arr_get(remote_punches, rb + 1), arr_get(remote_punches, rb + 2),
                                                     arr_get(remote_punches, rb + 3), arr_get(remote_punches, rb + 4),
                                                     punch_reach, punch_width);
                if (target == 0 || (target >= 1 && target <= turret_max)) {
                    double rcls = arr_get(remotes, s * remote_fields + 10);
                    /* Compute remote punch damage from synced class, level, and skin. */
                    double rdmg = astra_ability_punch_damage_net(rcls, net_player_level(s),
                                                            arr_get(remotes, s * remote_fields + 11));
                    double rlevel = net_player_level(s);
                    if (target == 0) {
                        if (revive_prot <= 0) {
                            take_damage_at(rdmg, arr_get(remotes, s * remote_fields + 1),
                                           arr_get(remotes, s * remote_fields + 2));
                            if (astra_parry_active() == 0 && rcls == CLASS_SANTA) {
                                if (player_poison <= 0) {
                                    player_poison_tick = santa_poison_tick_interval;
                                }
                                player_poison = santa_poison_time_for(rlevel);
                                player_poison_damage = santa_poison_damage_for(rlevel);
                            }
                        }
                    } else {
                        damage_turret(target - 1, rdmg);
                        thud_seq = thud_seq + 1;
                        net_publish_thud(thud_seq);
                        ds_log("remote punch hit turret %g, hp %g", target - 1, arr_get(turret_hp, target - 1));
                    }
                    arr_set(remote_punches, rb, 2);
                    if (finished != 0) {
                        return;
                    }
                }
            }
        }
        s = s + 1;
    }
}

static void update_remote_thuds(void) {
    if (net_slot() < 0) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        if (s == net_slot() || net_player_online(s) != 1) {
            arr_set(remote_thuds, s, 0);
        } else {
            double gen = net_player_thud(s);
            double prev = arr_get(remote_thuds, s);
            if (gen != prev) {
                arr_set(remote_thuds, s, gen);
                if (gen > 0 && revive_prot <= 0 && finished == 0) {
                    take_damage(turret_counter_damage);
                    ds_log("turret thud from %g, hp %g", s, player->hp);
                }
            }
        }
        s = s + 1;
    }
}

static void update_remote_snow(void) {
    if (net_slot() < 0) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        double b = s * snow_fields;
        if (s == net_slot() || net_player_online(s) != 1) {
            arr_set(remote_snow, b + 1, 0);
            arr_set(remote_snow, b + 5, 0);
        } else {
            double cnt = net_player_snow(s);
            double prev = arr_get(remote_snow, b);
            if (cnt != prev && cnt > 0) {
                double sx0 = net_player_snow_x(s) * screen_w;
                double sy0 = net_player_snow_y(s) * screen_h;
                arr_set(remote_snow, b, cnt);
                arr_set(remote_snow, b + 1, 1);
                arr_set(remote_snow, b + 2, 0);
                arr_set(remote_snow, b + 3, sx0);
                arr_set(remote_snow, b + 4, sy0);
                arr_set(remote_snow, b + 8, sx0);
                arr_set(remote_snow, b + 9, sy0);
            }
            if (arr_get(remote_snow, b + 1) == 1) {
                double t = arr_get(remote_snow, b + 2) + dt;
                double x = 0, y = 0;
                snow_step_pos(arr_get(remote_snow, b + 8), arr_get(remote_snow, b + 9), net_player_snow_dx(s),
                              net_player_snow_dy(s), t, arr_get(remote_snow, b), &x, &y);
                arr_set(remote_snow, b + 2, t);
                arr_set(remote_snow, b + 3, x);
                arr_set(remote_snow, b + 4, y);
                double hit = circle_hits_player(x, y, snow_radius_for(arr_get(remote_snow, b))) == 1 ||
                             t > super_range / super_speed ? 1 : 0;
                if (hit == 1) {
                    arr_set(remote_snow, b + 1, 0);
                    arr_set(remote_snow, b + 5, boom_time);
                    arr_set(remote_snow, b + 6, x);
                    arr_set(remote_snow, b + 7, y);
                    if (circle_hits_player(x, y, super_radius) == 1) {
                        double rcls = arr_get(remotes, s * remote_fields + 10);
                        double rlevel = net_player_level(s);
                        double freeze_time = super_freeze;
                        if (rcls == CLASS_SANTA) {
                            freeze_time = santa_freeze_time_for(rlevel);
                        }
                        if (astra_parry_active() == 0 && player_freeze < freeze_time) {
                            player_freeze = freeze_time;
                            player_freeze_slow = super_freeze_slow;
                        }
                    }
                }
            }
            if (arr_get(remote_snow, b + 5) > 0) {
                arr_set(remote_snow, b + 5, tick_down(arr_get(remote_snow, b + 5)));
            }
        }
        s = s + 1;
    }
}

static double count_living_remotes(void) {
    double count = 0;
    double s = 0;
    while (s < max_players) {
        if (s != net_slot() && arr_get(remotes, s * remote_fields + 5) == 1) {
            count = count + 1;
        }
        s = s + 1;
    }
    return count;
}

static double count_online_remotes(void) {
    if (net_slot() < 0) {
        return 0;
    }
    double count = 0;
    double s = 0;
    while (s < max_players) {
        if (s != net_slot() && net_player_online(s) == 1) {
            count = count + 1;
        }
        s = s + 1;
    }
    return count;
}

static void check_online_finish(void) {
    if (online_ready == 0 || finished != 0) {
        return;
    }
    double enemies_alive = count_living_remotes();
    if (enemies_alive > 0) {
        foes_seen_t = foes_seen_t + dt;
        if (foes_seen_t > foes_seen_min) {
            match_seen_foe = 1;
        }
        foes_gone_t = 0;
        alone_t = 0;
        return;
    }
    if (net_status() != net_st_playing) {
        foes_gone_t = 0;
        alone_t = 0;
        return;
    }
    if (count_online_remotes() > 0 && match_seen_foe == 1) {
        /* Finish match when all connected opponents are eliminated. */
        alone_t = 0;
        foes_gone_t = foes_gone_t + dt;
        if (foes_gone_t >= foes_dead_min) {
            finish_game(1);
        }
        return;
    }
    /* Wait alone_wait_time seconds for opponents before returning to room list. */
    foes_gone_t = 0;
    alone_t = alone_t + dt;
    if (alone_t >= alone_wait_time) {
        alone_t = 0;
        ds_log("online: alone in room for %g s, back to room list", alone_wait_time);
        start_transition(ST_ROOMS);
    }
}

static void draw_alone_wait(void) {
    if (online_ready == 0 || finished != 0 || alone_t <= 0) {
        return;
    }
    if (count_living_remotes() > 0) {
        return;
    }
    double left = alone_wait_time - alone_t;
    if (left < 0) {
        left = 0;
    }
    const char *s = ds_concat(ds_concat(language == 1 ? "Ожидание игроков: " : "Waiting for players: ",
                                        ds_num_to_string((double)(int)(left + 0.999))),
                              language == 1 ? " с" : " s");
    ctext_scaled(s, back_y + btn_h + 44, RGB(255,255,255), 0.6);
}

static void push_self(void) {
    if (net_slot() < 0) {
        return;
    }
    double alive = player->hp <= 0 ? 0 : 1;
    progress_push_to_net();
    /* Sync guard, abilities, and turrets before committing fighter state. */
    net_publish_block(player_block);
    net_publish_codecracker_super((codecracker_super_t > 0 || astra_parry_active() == 1) ? 1 : 0);
    net_publish_punch(punch->x / screen_w, punch->y / screen_h, punch->dx, punch->dy, punch->shot);
    if (player_class == CLASS_EBUC) {
        double x1 = 0, y1 = 0, h1 = 0;
        double x2 = 0, y2 = 0, h2 = 0;
        double x3 = 0, y3 = 0, h3 = 0;
        if (turret_alive(0) == 1) {
            x1 = arr_get(turret_x, 0) / screen_w;
            y1 = arr_get(turret_y, 0) / screen_h;
            h1 = arr_get(turret_hp, 0);
        }
        if (turret_alive(1) == 1) {
            x2 = arr_get(turret_x, 1) / screen_w;
            y2 = arr_get(turret_y, 1) / screen_h;
            h2 = arr_get(turret_hp, 1);
        }
        if (turret_alive(2) == 1) {
            x3 = arr_get(turret_x, 2) / screen_w;
            y3 = arr_get(turret_y, 2) / screen_h;
            h3 = arr_get(turret_hp, 2);
        }
        net_publish_turrets(x1, y1, h1, x2, y2, h2, x3, y3, h3, turret_count_alive());
        if (universe_active == 1) {
            net_publish_universe(universe_x / screen_w, universe_y / screen_h, universe_id);
        } else {
            net_publish_universe(0, 0, 0);
        }
    }
    push_astra_grab();
    if (player_class == CLASS_AZUM) {
        if (dash_active == 1 && dash_pub == 0) {
            net_publish_dash(dash_x0 / screen_w, dash_y0 / screen_h, dash_dx, dash_dy, dash_seq);
            dash_pub = 1;
        } else if (dash_active == 0 && dash_pub == 1) {
            net_publish_dash(0, 0, 0, 0, 0);
            dash_pub = 0;
        }
    }
    net_publish(player->x / screen_w, player->y / screen_h, player->angle, player->hp, alive);
}

static void update_online(void) {
    check_meet_eax229();
    if (net_banned() == 1) {
        banned_msg = tr_banned();
        start_transition(0);
        return;
    }
    if (hack_cinematic_lock() == 1) {
        update_event();
        drop_joy();
        player_block = 0;
        blk_id = -1;
        if (punch) {
            punch->active = 0;
        }
        return;
    }
    tick_alert_hold();
    update_banner();
    tick_revive();
    tick_freeze();
    tick_player_stun();
    tick_player_poison();
    tick_turrets();
    tick_universe();
    tick_astra();
    tick_status_fades();
    tick_hurt_fades();
    if (net_slot() >= 0) {
        if (online_ready == 0) {
            place_online();
        }
        read_remotes();
        smooth_remotes();
    }
    tick_remote_punches();
    tick_hitbox_fades();
    update_aim();
    ball_online_tick(); /* Tick ebuC's projectiles against current remote positions. */
    christmas_tick(); /* Christmas snowballs for every class. */
    autoblock_tick(); /* Admin auto block / auto punch. */
    if (finished != 0) {
        return;
    }
    if (player->hp <= 0) {
        if (try_revive() == 0) {
            finish_game(2);
            return;
        }
    }
    move_player();
    /* There is no local bot in an online battle. Separating from the stale
     * solo-enemy position created an invisible wall, so remote fighters stay
     * non-solid on this client. */
    ice_tick();
    tick_dash();
    punch_left = tick_down(punch_left);
    follow_punch();
    if (punch->active == 1 && punch_left <= 0) {
        punch->active = 0;
    }
    super_cd = tick_down(super_cd);
    if (admin_cheat_nocd() == 1) {
        super_cd = 0;
    }
    boom_t = tick_down(boom_t);
    tick_santa_follow();
    update_dust();
    update_gift();
    flush_queued_action();
    if (finished != 0) {
        return;
    }
    if (net_slot() >= 0) {
        codecracker_tick_online();
        if (finished != 0) {
            return;
        }
        update_remote_punches();
        update_remote_thuds();
        update_remote_snow();
        update_remote_dashes();
        update_remote_grabs();
        if (finished != 0) {
            return;
        }
        update_remote_station();
        update_remote_universe();
        check_online_finish();
        push_self();
    }
    update_event();
    chat_bubbles_watch();
    chat_update();
}
