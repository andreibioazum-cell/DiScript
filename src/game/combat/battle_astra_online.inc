/* Online sync for Astra's counter-strike. */

static void push_astra_grab(void) {
    if (class_is_astra(player_class) == 0 || astra_seq <= 0) {
        return;
    }
    if (astra_pub != astra_seq) {
        net_publish_grab(astra_ability_lunge_tx / screen_w, astra_ability_lunge_ty / screen_h, astra_dx, astra_dy,
                         astra_seq);
        astra_pub = astra_seq;
    }
}

static double remote_grab_get(double slot, double field) {
    return arr_get(remote_grab, slot * grab_fields + field);
}

static void remote_grab_set(double slot, double field, double value) {
    arr_set(remote_grab, slot * grab_fields + field, value);
}

static void remote_grab_finish(double slot) {
    remote_grab_set(slot, GRAB_ACTIVE, 0);
    remote_grab_set(slot, GRAB_CAUGHT, 0);
    remote_grab_set(slot, GRAB_HITS, 0);
    remote_grab_set(slot, GRAB_T, 0);
    remote_grab_set(slot, GRAB_ASTRA_KIT, 0);
    if (pgrab_active == 1 && pgrab_slot == slot) {
        pgrab_active = 0;
        pgrab_slot = -1;
    }
}

static void remote_grab_stop(double slot) {
    remote_grab_finish(slot);
    arr_set(rgrab_punch_t, slot, 0);
}

static void remote_grab_begin(double slot) {
    remote_grab_set(slot, GRAB_ACTIVE, 1);
    remote_grab_set(slot, GRAB_T, 0);
    remote_grab_set(slot, GRAB_CAUGHT, 0);
    remote_grab_set(slot, GRAB_HITS, 0);
    remote_grab_set(slot, GRAB_X, net_player_grab_x(slot) * screen_w);
    remote_grab_set(slot, GRAB_Y, net_player_grab_y(slot) * screen_h);
    remote_grab_set(slot, GRAB_DX, net_player_grab_dx(slot));
    remote_grab_set(slot, GRAB_DY, net_player_grab_dy(slot));
    remote_grab_set(slot, GRAB_ASTRA_KIT, astra_ability_of_slot(slot));
    arr_set(rgrab_box_a, slot, 0);
    remote_grab_swing(slot);
    punch_snd();
}

static double update_remote_grab(double slot) {
    double gen = net_player_grab(slot);
    if (gen != remote_grab_get(slot, GRAB_GEN)) {
        remote_grab_set(slot, GRAB_GEN, gen);
        if (gen > 0) {
            remote_grab_begin(slot);
        } else {
            remote_grab_stop(slot);
        }
    }
    if (remote_grab_get(slot, GRAB_ACTIVE) != 1) {
        return 0;
    }
    double t = remote_grab_get(slot, GRAB_T) + dt;
    remote_grab_set(slot, GRAB_T, t);
    if (t >= ASTRA_STRIKE_DELAY - 1e-9 && remote_grab_get(slot, GRAB_CAUGHT) == 0 && player->hp > 0) {
        double lv = net_player_level(slot);
        double gx = remote_grab_get(slot, GRAB_X);
        double gy = remote_grab_get(slot, GRAB_Y);
        double gdx = remote_grab_get(slot, GRAB_DX);
        double gdy = remote_grab_get(slot, GRAB_DY);
        double reach = astra_zone_reach(lv);
        double width = astra_zone_width(lv);
        double u = clamp((t - ASTRA_PULLBACK_TIME) / ASTRA_DASH_TIME, 0, 1);
        double p = astra_dash_progress(u);
        double sx = gx - gdx * astra_ability_lunge_dist;
        double sy = gy - gdy * astra_ability_lunge_dist;
        double swept = astra_ability_lunge_dist * p;
        double target = -1;
        if (strip_hits_player(sx, sy, gdx, gdy, astra_grab_forward_offset - ASTRA_PULLBACK_DIST,
                              swept + astra_grab_forward_offset + reach, width / 2) == 1) {
            target = 0;
        } else {
            double cx = sx + (gx - sx) * p;
            double cy = sy + (gy - sy) * p;
            target = punch_resolve_target(cx, cy, gdx, gdy, reach, width);
            if (target < 0) {
                target = punch_resolve_target(gx, gy, gdx, gdy, reach, width);
            }
        }
        if (target == 0) {
            remote_grab_set(slot, GRAB_CAUGHT, 1);
            arr_set(rgrab_box_a, slot, 1);
            if (revive_prot <= 0) {
                stun_player(astra_strike_stun_for(lv));
                if (take_damage_at(astra_strike_damage_for(lv), gx, gy) == 1) {
                    remote_grab_stop(slot);
                    return 1;
                }
            }
        } else if (target >= 1 && target <= turret_max) {
            remote_grab_set(slot, GRAB_CAUGHT, 1);
            arr_set(rgrab_box_a, slot, 1);
            damage_turret(target - 1, astra_strike_damage_for(lv));
            thud_seq = thud_seq + 1;
            net_publish_thud(thud_seq);
        }
    }
    if (t >= ASTRA_STRIKE_TIME - 1e-9) {
        remote_grab_finish(slot);
    }
    return 0;
}

static void update_remote_grabs(void) {
    if (net_slot() < 0) {
        return;
    }
    for (double slot = 0; slot <= max_players - 1; slot += 1) {
        if (slot == net_slot() || net_player_online(slot) != 1) {
            remote_grab_set(slot, GRAB_GEN, 0);
            remote_grab_stop(slot);
        } else if (update_remote_grab(slot) == 1) {
            return;
        }
    }
}

static double remote_grab_punching(double slot) {
    if (arr_get(rgrab_punch_t, slot) > 0) {
        return 1;
    }
    return 0;
}

static void remote_grab_swing(double slot) {
    arr_set(rgrab_punch_t, slot, fmax(punch_time, ASTRA_STRIKE_TIME));
}
