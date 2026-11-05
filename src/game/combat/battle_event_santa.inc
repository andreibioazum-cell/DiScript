/* Event 3 (Ded Moroz): standalone Santa candy throw sharing plates candy state. */

static void santa_event_begin(void) {
    plates_phase = 3;
    plates_santa_t = 0;
    plates_santa_angle = 0;
    plates_throw_cd = 0.3;
    plates_collect_t = 0;
    plates_thrown = 0;
    plates_next_candy = 0;
    plates_santa_out = 0;
    plates_santa_gone = 0;
    plates_santa_dy = 0;
    plates_santa_vy = 0;
    plates_santa_ang0 = 0;
    plates_santa_up = 0;
    plates_vanish_cd = 0;
}

static void tick_santa_event(void) {
    if (plates_phase != 3) {
        santa_event_begin();
        ds_log("santa event: Ded Moroz appeared");
    }
    tick_event_santa();
}

static void draw_event_candies(void) {
    if (plates_round_on() == 0 || plates_phase != 3) {
        return;
    }
    double i = 0;
    double t = 0;
    double k = 0;
    double v = 1;
    double e = 0;
    double xx = 0;
    double yy = 0;
    double pt = 0;
    double p = 0;
    double pop = 0;
    double sc = 0;
    double hw = 0;
    while (i < plates_candy_max) {
        double b = i * plates_candy_fields;
        if (arr_get(plates_candies, b) == 1) {
            t = arr_get(plates_candies, b + 1);
            k = t / plates_candy_flight;
            if (k > 1) {
                k = 1;
            }
            v = 1;
            if (arr_get(plates_candies, b + 6) > 0) {
                v = 1 - arr_get(plates_candies, b + 6) / plates_candy_vanish_time;
            }
            e = 1 - (1 - k) * (1 - k);
            xx = arr_get(plates_candies, b + 2) + (arr_get(plates_candies, b + 4) - arr_get(plates_candies, b + 2)) * e;
            yy = arr_get(plates_candies, b + 3) + (arr_get(plates_candies, b + 5) - arr_get(plates_candies, b + 3)) * e;
            pt = arr_get(plates_candies, b + 7);
            if (pt > 0) {
            } else if (v > 0.02) {
                p = clamp(t / plates_candy_pop_time, 0, 1);
                pop = 1 - (1 - p) * (1 - p);
                                /* Appears by alpha, still disappears by shrinking. */
                sc = candy_scale * v;
                if (sc > 0.02 && pop > 0.02) {
                    hw = 25 * sc;
                    if (candy_tex_ok == 1) {
                        tex_alpha(xx - hw, yy - hw, CANDY_TEX, 0, sc, pop);
                    } else {
                        circle(xx, yy, hw, (((unsigned)(int)floor(pop * 254) + 1u) << 24) | 0x00FF4081);
                    }
                }
            }
        }
        i = i + 1;
    }
}

static void draw_event_santa(void) {
    if (plates_round_on() == 0 || plates_phase != 3 || plates_santa_gone == 1) {
        return;
    }
    double cx = screen_w / 2;
    double cy = screen_h / 2 + plates_santa_dy;
    double pop = 1;
    if (plates_santa_t < 0.4) {
        double p = plates_santa_t / 0.4;
        pop = 1 - (1 - p) * (1 - p);
    }
        /* Draw Santa at standard fighter scale with blurred shadow. */
        double half_px = 25 * cube_scale * player_scale * pop;
    double sc = cube_scale * player_scale * pop;
    if (sc <= 0.01) {
        return;
    }
    double wob = 6 * pop * sin(plates_santa_t * 3) * (1 - plates_santa_up);
    draw_shadow_blur_tex(cx - shadow_off, cy + shadow_off, half_px, sc, SANTA_TEX, plates_santa_angle,
                         shadow_alpha * 2.5, half_px * 0.07);
    tex(cx - half_px, cy - half_px + wob, SANTA_TEX, plates_santa_angle, sc);
    if (pop > 0.35) {
        draw_overhead_nick(tr_class_santa(), cx, cy + wob - half_px, 0xFFFFFF);
    }
}

static void draw_plates_hud(void) {
    if (plates_round_on() == 0 || plates_phase != 3) {
        return;
    }
    double left = plates_candies_left();
    if (left == 0 && plates_santa_gone == 1) {
        return;
    }
    double secs = floor(plates_time_left() + 0.999);
    const char *s = ds_concat(ds_concat(tr_candy_timer(), ": "), ds_num_to_string((double)(secs)));
    uint32_t c = RGB(255,255,255);
    if (secs <= 10) {
        c = RGB(255,110,99);
    }
    ctext_scaled(s, back_y + btn_h + 6, c, 0.5);
}
