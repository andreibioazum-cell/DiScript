static void universe_collapse(double dmg) {
    if (finished != 0) {
        return;
    }
    if (game_state != ST_ONLINE) {
        enemy_apply_damage_at(dmg, universe_x, universe_y);
    }
    take_damage(dmg);
}

static void universe_collapse_remote(double dmg, double sx, double sy) {
    if (finished != 0) {
        return;
    }
    take_damage_at(dmg, sx, sy);
}

static void tick_universe(void) {
    if (universe_active == 0) {
        return;
    }
    universe_t = universe_t + dt;
    if (universe_t >= ebuc_universe_collapse_time) {
        universe_active = 0;
        universe_collapse(ebuc_universe_damage_for(player_level));
    }
}

static void update_remote_station(void) {
    if (net_slot() < 0) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        double b = s * station_fields;
        if (s == net_slot() || net_player_online(s) != 1) {
            double f = 0;
            while (f < station_fields) {
                arr_set(remote_station, b + f, 0);
                f = f + 1;
            }
        } else {
            double i = 0;
            while (i < turret_max) {
                double o = b + 1 + i * 5;
                double hx = 0;
                double hy = 0;
                double hhp = 0;
                if (i == 0) {
                    hx = net_player_station_x(s) * screen_w;
                    hy = net_player_station_y(s) * screen_h;
                    hhp = net_player_station_hp(s);
                } else if (i == 1) {
                    hx = net_player_station2_x(s) * screen_w;
                    hy = net_player_station2_y(s) * screen_h;
                    hhp = net_player_station2_hp(s);
                } else {
                    hx = net_player_station3_x(s) * screen_w;
                    hy = net_player_station3_y(s) * screen_h;
                    hhp = net_player_station3_hp(s);
                }
                double st = arr_get(remote_station, o);
                if (hhp > 0) {
                    arr_set(remote_station, o, 1);
                    arr_set(remote_station, o + 1, hx);
                    arr_set(remote_station, o + 2, hy);
                    arr_set(remote_station, o + 3, hhp);
                    arr_set(remote_station, o + 4, 0);
                    if (st != 1) {
                        arr_set(remote_station, b, i + 1);
                    }
                } else {
                    if (st == 1) {
                        arr_set(remote_station, o, 2);
                        arr_set(remote_station, o + 4, station_boom_time);
                    } else if (st == 2) {
                        double bt = arr_get(remote_station, o + 4) - dt;
                        if (bt < 0) {
                            bt = 0;
                        }
                        arr_set(remote_station, o + 4, bt);
                        if (bt <= 0) {
                            arr_set(remote_station, o, 0);
                        }
                    }
                }
                i = i + 1;
            }
        }
        s = s + 1;
    }
}

static void update_remote_universe(void) {
    if (net_slot() < 0) {
        return;
    }
    double s = 0;
    while (s < max_players) {
        double b = s * universe_fields;
        if (s == net_slot() || net_player_online(s) != 1) {
            arr_set(remote_universe, b, 0);
            arr_set(remote_universe, b + 1, 0);
        } else {
            double ucnt = net_player_universe(s);
            double prev = arr_get(remote_universe, b);
            if (ucnt != prev && ucnt > 0) {
                arr_set(remote_universe, b, ucnt);
                arr_set(remote_universe, b + 1, 1);
                arr_set(remote_universe, b + 2, net_player_universe_x(s) * screen_w);
                arr_set(remote_universe, b + 3, net_player_universe_y(s) * screen_h);
                arr_set(remote_universe, b + 4, 0);
            }
            if (arr_get(remote_universe, b + 1) == 1) {
                double t = arr_get(remote_universe, b + 4) + dt;
                arr_set(remote_universe, b + 4, t);
                if (t >= ebuc_universe_collapse_time) {
                    double ux = arr_get(remote_universe, b + 2);
                    double uy = arr_get(remote_universe, b + 3);
                    arr_set(remote_universe, b + 1, 0);
                    double rlevel = net_player_level(s);
                    universe_collapse_remote(ebuc_universe_damage_for(rlevel), ux, uy);
                    if (finished != 0) {
                        return;
                    }
                }
            }
        }
        s = s + 1;
    }
}

static double remote_dash_get(double slot, double field) {
    return arr_get(remote_dash, slot * dash_fields + field);
}

static void remote_dash_set(double slot, double field, double value) {
    arr_set(remote_dash, slot * dash_fields + field, value);
}

static void remote_dash_begin(double slot) {
    remote_dash_set(slot, DASH_ACTIVE, 1);
    remote_dash_set(slot, DASH_TIME_LEFT, azum_dash_time);
    remote_dash_set(slot, DASH_HIT_DONE, 0);
}

static void remote_dash_stop(double slot) {
    remote_dash_set(slot, DASH_ACTIVE, 0);
    remote_dash_set(slot, DASH_HIT_DONE, 0);
}

static double remote_dash_track(double slot) {
    double ox = net_player_dash_x(slot) * screen_w;
    double oy = net_player_dash_y(slot) * screen_h;
    double vx = net_player_x(slot) * screen_w - ox;
    double vy = net_player_y(slot) * screen_h - oy;
    double leg = sqrt(vx * vx + vy * vy);
    if (leg < 1) {
        leg = 0;
        remote_dash_set(slot, DASH_DIR_X, 0);
        remote_dash_set(slot, DASH_DIR_Y, 0);
    } else {
        remote_dash_set(slot, DASH_DIR_X, vx / leg);
        remote_dash_set(slot, DASH_DIR_Y, vy / leg);
    }
    remote_dash_set(slot, DASH_X, ox);
    remote_dash_set(slot, DASH_Y, oy);
    remote_dash_set(slot, DASH_TRAVELLED, leg);
    return leg;
}

static double remote_dash_apply_hit(double slot, double leg) {
    double target = dash_resolve_target(remote_dash_get(slot, DASH_X), remote_dash_get(slot, DASH_Y),
                                        remote_dash_get(slot, DASH_DIR_X), remote_dash_get(slot, DASH_DIR_Y), leg);
    if (target == 0) {
        remote_dash_set(slot, DASH_HIT_DONE, 1);
        if (revive_prot <= 0) {
            take_damage_at(azum_dash_damage_for(), remote_dash_get(slot, DASH_X), remote_dash_get(slot, DASH_Y));
            stun_player(azum_dash_stun);
            ds_log("remote dash hit player");
            if (finished != 0) {
                return 1;
            }
        }
    } else if (target >= 1 && target <= turret_max) {
        remote_dash_set(slot, DASH_HIT_DONE, 1);
        damage_turret(target - 1, azum_dash_damage_for());
    }
    return 0;
}

static double update_remote_dash(double slot) {
    double gen = net_player_dash(slot);
    if (gen != remote_dash_get(slot, DASH_GEN)) {
        remote_dash_set(slot, DASH_GEN, gen);
        if (gen > 0) {
            remote_dash_begin(slot);
        } else {
            remote_dash_set(slot, DASH_ACTIVE, 0);
        }
    }
    if (remote_dash_get(slot, DASH_ACTIVE) != 1) {
        return 0;
    }
    double leg = remote_dash_track(slot);
    double time_left = remote_dash_get(slot, DASH_TIME_LEFT) - dt;
    if (time_left < 0) {
        time_left = 0;
    }
    remote_dash_set(slot, DASH_TIME_LEFT, time_left);
    if (time_left <= 0) {
        remote_dash_stop(slot);
        return 0;
    }
    if (remote_dash_get(slot, DASH_HIT_DONE) == 0 && finished == 0 && leg > 0) {
        return remote_dash_apply_hit(slot, leg);
    }
    return 0;
}

static void update_remote_dashes(void) {
    if (net_slot() < 0) {
        return;
    }
    for (double slot = 0; slot <= max_players - 1; slot += 1) {
        if (slot == net_slot() || net_player_online(slot) != 1) {
            remote_dash_set(slot, DASH_GEN, 0);
            remote_dash_set(slot, DASH_ACTIVE, 0);
        } else if (update_remote_dash(slot) == 1) {
            return;
        }
    }
}

static void flush_queued_action(void) {
    if (queued_punch == 0 && queued_super == 0) {
        return;
    }
    if (punch_left > 0 || punch_cd > 0 || ultimate_visual_busy() == 1) {
        return;
    }
    if (queued_super == 1) {
        if (class_is_astra(player_class) == 1) {
            if (astra_state == 0 &&
                (astra_grab_ready_t > 0 || (astra_cd <= 0 && astra_parry_t <= 0 && astra_strike_unlock_t <= 0))) {
                queued_super = 0;
                fire_super();
                return;
            }
        } else if (player_class == CLASS_AZUM) {
            if (dash_cd <= 0) {
                queued_super = 0;
                fire_super();
                return;
            }
        } else {
            if (super_cd <= 0) {
                queued_super = 0;
                fire_super();
                return;
            }
        }
    }
    if (queued_punch == 1) {
        queued_punch = 0;
        fire_punch();
    }
}

static void tick_dash(void) {
    if (dash_active > 0) {
        dash_t = dash_t - dt;
        if (dash_t <= 0) {
            dash_t = 0;
            dash_active = 0;
            dash_hit = 0;
        }
    }
    if (dash_cd > 0) {
        dash_cd = dash_cd - dt;
        if (dash_cd < 0) {
            dash_cd = 0;
        }
    }
}

static void tick_candies(void) {
    if (candies_on() == 1) {
        double ci = 0;
        while (ci < candy_count) {
            double cp = arr_get(candy_pick, ci);
            if (cp > 0) {
                cp = 0;
                candy_rand_pos(ci);
                arr_set(candy_pick, ci, 0);
            } else {
                arr_set(candy_t, ci, arr_get(candy_t, ci) + dt);
                if (dist(arr_get(candy_x, ci), arr_get(candy_y, ci), player->x, player->y) < player->size * 0.65 + 18) {
                    candies = candies + candy_give;
                    save_progress();
                    ds_log("candy picked -> %g", candies);
                    candy_rand_pos(ci);
                    arr_set(candy_pick, ci, 0);
                    arr_set(candy_t, ci, 0);
                }
            }
            ci = ci + 1;
        }
    }
}

static void update_game(void) {
    tick_alert_hold();
    tick_revive();
    tick_freeze();
    tick_player_stun();
    tick_player_poison();
    tick_turrets();
    tick_universe();
    tick_astra();
    tick_enemy_class();
    tick_hitbox_fades();
    tick_status_fades();
    tick_hurt_fades();
    update_aim();
    /* Solo battles run events too: the server event when online, or the one
     * launched locally from the CCP admin commands tab while offline. The
     * raid's cane rain keeps ticking after the round ends, like online. */
    update_event();
    if (finished != 0) {
        return;
    }
    /* The ban blackout is theatre, not a free kill: freeze the whole fight. */
    if (hack_cinematic_lock() == 1) {
        drop_joy();
        player_block = 0;
        blk_id = -1;
        if (punch) {
            punch->active = 0;
        }
        return;
    }
    if (player->hp <= 0) {
        if (try_revive() == 0) {
            finish_game(2);
            return;
        }
    }
    move_player();
    codecracker_tick_solo();
    if (finished != 0) {
        return;
    }
    tick_dash_solo_hit();
    tick_dash();
    tick_candies();
    follow_punch();
    ball_tick();
    christmas_tick();
    autoblock_tick();
    ball_fade_tick();
    super_cd = tick_down(super_cd);
    if (admin_cheat_nocd() == 1) {
        super_cd = 0;
    }
    boom_t = tick_down(boom_t);
    tick_santa_follow();
    punch_left = tick_down(punch_left);
    if (punch->active == 1 && punch->hit == 0 && punch_left <= punch_time + 1e-9) {
        double enemy_turret_target = enemy_punch_turret_target(punch->x, punch->y, punch->dx, punch->dy);
        if (enemy_turret_target >= 1) {
            punch->hit = 1;
            enemy_damage_turret(enemy_turret_target - 1, astra_ability_self_damage());
            if (revive_prot <= 0) {
                if (take_damage(turret_counter_damage * enemy_damage_mult_for(enemy_class)) == 1) {
                    return;
                }
            }
            heal_after_attack();
        } else if (enemy_in_punch() == 1) {
            /* Block punch if it overlaps with an ice block. */
            if (ice_near_fighter(punch->x, punch->y, 30.0 + punch_reach) == 0) {
                punch->hit = 1;
            }
            if (enemy_revive_prot <= 0) {
                if (player_class == 2 && santa_follow > 0) {
                    if (player_level >= 3) {
                        if (enemy->poison <= 0) {
                            enemy->poison_tick = santa_poison_tick_interval;
                            enemy->poison_damage = santa_poison_damage_for(player_level);
                        } else {
                            enemy->poison_damage = enemy->poison_damage + santa_poison_damage_for(player_level);
                        }
                        enemy->poison = santa_poison_time_for(player_level);
                    } else {
                        if (enemy->poison <= 0) {
                            enemy->poison_tick = santa_poison_tick_interval;
                        }
                        enemy->poison = santa_poison_time_for(player_level);
                    }
                }
                heal_after_attack();
                if (enemy_apply_damage_at(astra_ability_self_damage(), player->x, player->y) == 1) {
                    return;
                }
            }
        }
    }
    if (punch->active == 1 && punch_left <= 0) {
        punch->active = 0;
    }
    update_dust();
    update_gift();
    flush_queued_action();
    if (finished != 0) {
        return;
    }
    update_enemy();
    separate_fighters(1);
    ice_push_point(&enemy->x, &enemy->y, enemy_half());
    hack_button_push(&enemy->x, &enemy->y, enemy_half());
    ice_tick();
    /* update_event() above already ticked the raid; do not double-advance it. */
}
