static void plates_event_reset(void) {
    if (plates_dirty == 1) {
        plates_dirty = 0;
        save_progress();
    }
    plates_save_cd = 0;
    plates_phase = 0;
    /* Reset completion guard for a fresh plates round. */
    plates_finished = 0;
    plates_t = 0;
    plate_a_lit = 0;
    plate_b_lit = 0;
    plates_santa_t = 0;
    plates_santa_angle = 0;
    plates_throw_cd = 0;
    plates_next_candy = 0;
    plates_thrown = 0;
    plates_santa_out = 0;
    plates_santa_gone = 0;
    plates_collect_t = 0;
    plates_vanish_cd = 0;
    plates_santa_dy = 0;
    plates_santa_vy = 0;
    plates_santa_ang0 = 0;
    plates_santa_up = 0;
    plates_lock_me = 0;
    arr_clear(plates_candies);
    double i = 0;
    while (i < plates_candy_max * plates_candy_fields) {
        arr_push(plates_candies, 0);
        i = i + 1;
    }
}

static double plate_ax(void) {
    return screen_w * plates_xa;
}

static double plate_ay(void) {
    return screen_h * plates_yy;
}

static double plate_bx(void) {
    return screen_w * plates_xb;
}

static double plate_by(void) {
    return screen_h * plates_yy;
}

static double me_on_plate(double px, double py) {
    if (player->hp > 0 && dist(player->x, player->y, px, py) < plates_stand_r) {
        return 1;
    }
    return 0;
}

static double fighter_on_plate(double px, double py) {
    if (me_on_plate(px, py) == 1) {
        return 1;
    }
    double s = 0;
    while (s < max_players) {
        double b = s * remote_fields;
        if (s != net_slot() && arr_get(remotes, b + 5) == 1) {
            if (dist(arr_get(remotes, b + 1), arr_get(remotes, b + 2), px, py) < plates_stand_r) {
                return 1;
            }
        }
        s = s + 1;
    }
    return 0;
}

static void plates_throw_candy(void) {
    if (plates_next_candy >= plates_candy_max) {
        return;
    }
    double b = plates_next_candy * plates_candy_fields;
    double m = candy_margin + 16;
    double tx = m + rand() % (int)(screen_w - 2 * m);
    double ty = m + rand() % (int)(screen_h - 2 * m);
    arr_set(plates_candies, b, 1);
    arr_set(plates_candies, b + 1, 0);
    arr_set(plates_candies, b + 2, screen_w / 2);
    arr_set(plates_candies, b + 3, screen_h / 2);
    arr_set(plates_candies, b + 4, tx);
    arr_set(plates_candies, b + 5, ty);
    arr_set(plates_candies, b + 6, 0);
    arr_set(plates_candies, b + 7, 0);
    plates_next_candy = plates_next_candy + 1;
    plates_thrown = plates_thrown + 1;
}

static double plates_candies_left(void) {
    double n = 0;
    double i = 0;
    while (i < plates_candy_max) {
        if (arr_get(plates_candies, i * plates_candy_fields) == 1) {
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

static void plates_collect_candies(void) {
    double i = 0;
    while (i < plates_candy_max) {
        double b = i * plates_candy_fields;
        if (arr_get(plates_candies, b) == 1) {
            double t = arr_get(plates_candies, b + 1);
            double v = arr_get(plates_candies, b + 6);
            double pt = arr_get(plates_candies, b + 7);
            if (pt > 0) {
                arr_set(plates_candies, b, 0);
            } else if (t < plates_candy_flight) {
                arr_set(plates_candies, b + 1, t + dt);
            } else if (v > 0) {
                v = v + dt;
                arr_set(plates_candies, b + 6, v);
                if (v >= plates_candy_vanish_time) {
                    arr_set(plates_candies, b, 0);
                }
            } else if (dist(arr_get(plates_candies, b + 4), arr_get(plates_candies, b + 5), player->x, player->y) <
                       player->size * 0.65 + 18) {
                if (candies_on() == 1) {
                    candies = candies + candy_give;
                }
                plates_dirty = 1;
                arr_set(plates_candies, b, 0);
                ds_log("event candy picked -> %g", candies);
            }
        }
        i = i + 1;
    }
}

static double plates_start_vanish_one(void) {
    double i = 0;
    while (i < plates_candy_max) {
        double b = i * plates_candy_fields;
        if (arr_get(plates_candies, b) == 1 && arr_get(plates_candies, b + 6) <= 0 &&
            arr_get(plates_candies, b + 7) <= 0 && arr_get(plates_candies, b + 1) >= plates_candy_flight) {
            arr_set(plates_candies, b + 6, 0.0001);
            return 1;
        }
        i = i + 1;
    }
    return 0;
}

static double plates_time_left(void) {
    double left = plates_collect_time - plates_collect_t;
    if (left < 0) {
        left = 0;
    }
    return left;
}

static double plates_wrap_angle(double a) {
    double w = a;
    while (w > pi) {
        w = w - 2 * pi;
    }
    while (w < 0 - pi) {
        w = w + 2 * pi;
    }
    return w;
}

static double plates_santa_exit_dy(void) {
    return screen_h / 2 + plates_santa_size;
}

static void tick_event_santa(void) {
    plates_santa_t = plates_santa_t + dt;
    plates_collect_t = plates_collect_t + dt;
    if (plates_santa_gone == 0) {
        if (plates_santa_out > 0) {
            plates_santa_out = plates_santa_out + dt;
            plates_santa_up = clamp(plates_santa_out / plates_santa_turn_time, 0, 1);
            plates_santa_angle = plates_santa_ang0 * (1 - plates_santa_up);
            plates_santa_vy = plates_santa_vy + plates_santa_fly_acc * dt;
            if (plates_santa_vy > plates_santa_fly_speed) {
                plates_santa_vy = plates_santa_fly_speed;
            }
            plates_santa_dy = plates_santa_dy - plates_santa_vy * dt;
            if (plates_santa_dy <= 0 - plates_santa_exit_dy() || plates_santa_out >= plates_santa_out_time) {
                plates_santa_gone = 1;
                ds_log("plates event: Santa flew up and left");
            }
        } else {
            plates_santa_angle = plates_santa_angle + dt * (plates_santa_spin + 0.9 * sin(plates_santa_t * 1.7));
            if (plates_thrown < plates_candy_total) {
                plates_throw_cd = plates_throw_cd - dt;
                if (plates_throw_cd <= 0) {
                    plates_throw_cd = plates_throw_interval;
                    plates_throw_candy();
                }
            } else {
                plates_santa_out = plates_santa_out + dt;
                plates_santa_ang0 = plates_wrap_angle(plates_santa_angle);
            }
        }
    }
    if (plates_collect_t >= plates_collect_time) {
        plates_vanish_cd = plates_vanish_cd - dt;
        if (plates_vanish_cd <= 0) {
            plates_vanish_cd = plates_candy_vanish_stagger;
            plates_start_vanish_one();
        }
    }
    plates_collect_candies();
    plates_save_cd = plates_save_cd - dt;
    if (plates_dirty == 1 && plates_save_cd <= 0) {
        plates_dirty = 0;
        plates_save_cd = plates_save_interval;
        save_progress();
    }
    if (plates_santa_gone == 1 && plates_candies_left() == 0) {
        ds_log("plates event: round over, finished one-time");
        plates_finished = 1;
        plates_phase = 4;
        if (plates_dirty == 1) {
            plates_dirty = 0;
            save_progress();
        }
    }
}

/* Return 1 when plates (2) or standalone Santa (3) candy round is active. */
static double plates_round_on(void) {
    return event_mode == 2 || event_mode == 3 ? 1 : 0;
}

/* Re-arm candy round only after event mode turns off. */
static void plates_watch_cloud(void) {
    if (plates_round_on() == 1 || net_status() != net_st_playing) {
        plates_off_t = 0;
        return;
    }
    plates_off_t = plates_off_t + dt;
    if (plates_off_t >= plates_rearm_time) {
        plates_off_t = 0;
        plates_finished = 0;
        plates_phase = 0;
    }
}

static void tick_plates_event(void) {
    plates_watch_cloud();
    if (plates_round_on() == 0) {
        if (plates_prev_mode == 2 || plates_prev_mode == 3) {
            plates_event_reset();
        }
        plates_prev_mode = event_mode;
        return;
    }
    if (plates_prev_mode != event_mode) {
        plates_event_reset();
    }
    plates_prev_mode = event_mode;
    if (plates_finished == 1) {
        plates_phase = 4;
        plates_lock_me = 0;
        return;
    }
    if (event_mode == 3) {
        tick_santa_event();
        return;
    }
    if (plates_phase == 0) {
        plate_a_lit = fighter_on_plate(plate_ax(), plate_ay());
        plate_b_lit = fighter_on_plate(plate_bx(), plate_by());
        if (plate_a_lit == 1 && plate_b_lit == 1) {
            plates_phase = 1;
            plates_t = 0;
            if (me_on_plate(plate_ax(), plate_ay()) == 1 || me_on_plate(plate_bx(), plate_by()) == 1) {
                plates_lock_me = 1;
                drop_joy();
            }
            ds_log("plates event: both plates occupied, charging");
        }
    } else if (plates_phase == 1) {
        plates_t = plates_t + dt;
        if (plates_t >= plates_charge_time) {
            plates_phase = 2;
            plates_t = 0;
            plates_lock_me = 0;
            ds_log("plates event: charge done, shrinking");
        }
    } else if (plates_phase == 2) {
        plates_t = plates_t + dt;
        if (plates_t >= plates_shrink_time) {
            santa_event_begin();
            ds_log("plates event: Santa appeared");
        }
    } else {
        tick_event_santa();
    }
}

static void draw_plate(double cx, double cy, double k, double lit) {
    if (k <= 0) {
        return;
    }
    double half = plates_size / 2 * k;
    double r = 20 * k;
    double pulse = 0.5 + 0.5 * sin(event_t * 5);
    double aa = floor(clamp(k * 1.4, 0, 1) * 235);
    if (aa < 1) {
        return;
    }
    double tr = 0x00E53935;
    if (lit == 1) {
        tr = 0x00FF6E63;
    }
    double a85 = floor(aa * 0.85);
    if (a85 > 0) {
        roundrect(cx - half - 5 * k, cy - half - 5 * k, half * 2 + 10 * k, half * 2 + 10 * k, r + 5 * k,
                  (((unsigned)(int)a85) << 24) | 0x007F1414);
    }
    roundrect(cx - half, cy - half, half * 2, half * 2, r, (((unsigned)(int)aa) << 24) | ((unsigned)(int)tr));
    double rr = half * 0.45 + 2 * pulse * k;
    double a55 = floor(aa * 0.55);
    if (a55 > 0) {
        ring(cx, cy, rr, 3 * k, (((unsigned)(int)a55) << 24) | 0x00FFFFFF);
    }
}

static double weak_hit_plate_active(void) {
    if (game_state != ST_ONLINE || event_mode != 6) return 0;
    return fighter_on_plate(plate_ax(), plate_ay()) == 1 || fighter_on_plate(plate_bx(), plate_by()) == 1;
}

static double weak_hit_damage_mult(void) {
    return weak_hit_plate_active() == 1 ? (1.0 / 3.0) : 1.0;
}

static void draw_event_plates(void) {
    if (event_mode == 6) {
        plate_a_lit = fighter_on_plate(plate_ax(), plate_ay());
        plate_b_lit = fighter_on_plate(plate_bx(), plate_by());
        draw_plate(plate_ax(), plate_ay(), 1, plate_a_lit);
        draw_plate(plate_bx(), plate_by(), 1, plate_b_lit);
        const char *hint = language == 1 ? "Stand on a plate: hits deal one third damage" : "Stand on a plate: hits deal one third damage";
        double tw = text_ink_width(hint) * 0.46;
        text_scaled(hint, (screen_w - tw) / 2, screen_h * 0.14, RGB(255,255,255), 0.46);
        return;
    }
    if (event_mode != 2 || plates_phase >= 3 || plates_finished == 1) {
        return;
    }
    double k = 1;
    if (plates_phase == 2) {
        k = 1 - plates_t / plates_shrink_time;
    }
    draw_plate(plate_ax(), plate_ay(), k, plate_a_lit);
    draw_plate(plate_bx(), plate_by(), k, plate_b_lit);
}
