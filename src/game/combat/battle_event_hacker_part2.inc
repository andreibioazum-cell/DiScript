/* Event 7 part 2: the raid's world junk - falling menu buttons as solid
 * cover, the 45-degree candy-cane compensation, and the scene tick that
 * drives everything from glitch to clean victory. */

/* Menu junk the avalanche drops: real menu rows plus the hacker's fantasies. */
static const char *hack_junk_ru[10] = {"ВЫЙТИ В МЕНЮ",   "КУПИТЬ ПРЕМИУМ",  "НАСТРОЙКИ ЗВУКА",
                                       "ВЫБОР КАРТЫ",   "УДАЛИТЬ КУБИК БАТЛ", "ПОЛУЧИТЬ 1000000 ЗВЕЗД",
                                       "НАСТРОЙКИ",     "МАГАЗИН",         "ДОСТИЖЕНИЯ",
                                       "АККАУНТ"};
static const char *hack_junk_en[10] = {"QUIT TO MENU",    "BUY PREMIUM",     "SOUND SETTINGS",
                                       "MAP SELECT",      "DELETE CUBIC BATTLE", "GET 1000000 STARS",
                                       "SETTINGS",        "SHOP",            "ACHIEVEMENTS",
                                       "ACCOUNT"};

static const char *hack_junk_label(double idx) {
    int i = (int)idx;
    if (i < 0 || i > 9) {
        i = 0;
    }
    if (i >= 6) {
        /* Rows 6..9 reuse the live menu strings so the junk looks native. */
        if (i == 6) {
            return tr_settings();
        }
        if (i == 7) {
            return tr_shop();
        }
        if (i == 8) {
            return tr_achievements();
        }
        return tr_account();
    }
    return language == 1 ? hack_junk_ru[i] : hack_junk_en[i];
}

static void hack_reset(void) {
    hack_active = 0;
    hack_t = 0;
    hack_glitch = 0;
    hack_platform = 0;
    hack_payload_ok = 0;
    hack_payload_bytes = 0;
    hack_cmd_idx = 0;
    hack_stub_disco_t = 0;
    hack_stub_plates_t = 0;
    hack_cloud_on = 0;
    hack_cloud_t = 0;
    hack_btn_cd = 0;
    hack_btn_made = 0;
    hack_ban_t = 0;
    hack_cane_made = 0;
    hack_cane_got = 0;
    hack_cane_cd = 0;
    hack_cane_pop_t = 0;
    hack_finish_t = 0;
    hack_boomed = 0;
    hack_cmd_n = 0;
    hack_btn_total = HACK_BTN_MAX;
    hack_cane_total = HACK_CANE_MAX;
    hack_cane_candy = 3;
    arr_clear(hack_buttons);
    arr_clear(hack_canes);
    /* Compact defaults: two corners and a lower slot, never over the arena. */
    hack_term_x[0] = 8;
    hack_term_y[0] = hack_term_head() + 6;
    hack_term_x[1] = screen_w - hack_term_w() - 8;
    hack_term_y[1] = screen_h * 0.30;
    hack_term_x[2] = 8;
    hack_term_y[2] = screen_h * 0.60;
    hack_drag_win = -1;
    hack_drag_id = -1;
}

static void hack_spawn_button(void) {
    if (hack_btn_made >= HACK_BTN_MAX) {
        return;
    }
    double b = hack_btn_made * HB_FIELDS;
    double w = clamp(screen_w * 0.30, 150, 300);
    double h = clamp(screen_h * 0.08, 40, 64);
    double x = 12 + (rand() % (int)(screen_w - w - 24));
    double rest = screen_h * 0.34 + (rand() % (int)(screen_h * 0.52));
    if (rest > screen_h - h - 96) {
        rest = screen_h - h - 96;
    }
    while (arr_len(hack_buttons) < b + HB_FIELDS) {
        arr_push(hack_buttons, 0);
    }
    arr_set(hack_buttons, b + HB_ON, 1);
    arr_set(hack_buttons, b + HB_LABEL, (double)(rand() % 10));
    arr_set(hack_buttons, b + HB_X, x);
    arr_set(hack_buttons, b + HB_Y, 0 - h - 20);
    arr_set(hack_buttons, b + HB_W, w);
    arr_set(hack_buttons, b + HB_H, h);
    arr_set(hack_buttons, b + HB_VY, 60);
    arr_set(hack_buttons, b + HB_REST, rest);
    arr_set(hack_buttons, b + HB_LAND, 0);
    arr_set(hack_buttons, b + HB_PRESS, 0);
    hack_btn_made = hack_btn_made + 1;
}

static void hack_tick_buttons(void) {
    double i = 0;
    while (i < HACK_BTN_MAX) {
        double b = i * HB_FIELDS;
        if (arr_get(hack_buttons, b + HB_ON) != 1) {
            i = i + 1;
            continue;
        }
        double press = arr_get(hack_buttons, b + HB_PRESS);
        if (press > 0) {
            arr_set(hack_buttons, b + HB_PRESS, press - dt);
        }
        if (arr_get(hack_buttons, b + HB_LAND) == 1) {
            i = i + 1;
            continue;
        }
        double vy = arr_get(hack_buttons, b + HB_VY) + HACK_BTN_GRAVITY * dt;
        double y = arr_get(hack_buttons, b + HB_Y) + vy * dt;
        double rest = arr_get(hack_buttons, b + HB_REST);
        if (y >= rest) {
            y = rest;
            vy = 0 - vy * 0.3;
            if (vy > -70) {
                vy = 0;
                arr_set(hack_buttons, b + HB_LAND, 1);
                ds_gfx_shake(2.5, 0.12);
            }
        }
        arr_set(hack_buttons, b + HB_Y, y);
        arr_set(hack_buttons, b + HB_VY, vy);
        i = i + 1;
    }
}

/* Solid cover: push a round fighter out of every landed junk button. */
static void hack_button_push(double *px, double *py, double half) {
    double i = 0;
    while (i < HACK_BTN_MAX) {
        double b = i * HB_FIELDS;
        double cx, cy, dx, dy, d, ox, oy, ax, ay;
        if (arr_get(hack_buttons, b + HB_ON) != 1 || arr_get(hack_buttons, b + HB_LAND) != 1) {
            i = i + 1;
            continue;
        }
        double x = arr_get(hack_buttons, b + HB_X);
        double y = arr_get(hack_buttons, b + HB_Y);
        double w = arr_get(hack_buttons, b + HB_W);
        double h = arr_get(hack_buttons, b + HB_H);
        cx = clamp(*px, x, x + w);
        cy = clamp(*py, y, y + h);
        dx = *px - cx;
        dy = *py - cy;
        d = sqrt(dx * dx + dy * dy);
        if (d >= half) {
            i = i + 1;
            continue;
        }
        if (d > 0.001) {
            double k = (half - d) / d;
            *px = *px + dx * k;
            *py = *py + dy * k;
        } else {
            ox = *px - (x + w / 2);
            oy = *py - (y + h / 2);
            ax = w / 2 + half - fabs(ox);
            ay = h / 2 + half - fabs(oy);
            if (ax < ay) {
                *px = *px + (ox < 0 ? -ax : ax);
            } else {
                *py = *py + (oy < 0 ? -ay : ay);
            }
        }
        *px = clamp(*px, half, screen_w - half);
        *py = clamp(*py, half, screen_h - half);
        i = i + 1;
    }
}

/* Steer the solo bot around the junk pile instead of into it. */
static void hack_avoid_vector(double x, double y, double *vx, double *vy) {
    double i = 0;
    while (i < HACK_BTN_MAX) {
        double b = i * HB_FIELDS;
        double dx, dy, d, k, reach;
        if (arr_get(hack_buttons, b + HB_ON) != 1 || arr_get(hack_buttons, b + HB_LAND) != 1) {
            i = i + 1;
            continue;
        }
        double cx = arr_get(hack_buttons, b + HB_X) + arr_get(hack_buttons, b + HB_W) / 2;
        double cy = arr_get(hack_buttons, b + HB_Y) + arr_get(hack_buttons, b + HB_H) / 2;
        dx = x - cx;
        dy = y - cy;
        d = sqrt(dx * dx + dy * dy);
        reach = arr_get(hack_buttons, b + HB_W) / 2 + 70;
        if (d > reach || d < 0.001) {
            i = i + 1;
            continue;
        }
        k = (reach - d) / reach;
        *vx = *vx + (dx / d) * k * 120 - (dy / d) * k * 90;
        *vy = *vy + (dy / d) * k * 120 + (dx / d) * k * 90;
        i = i + 1;
    }
}

/* Bounce ebuC's ball and the snowballs off the junk pile, like ice. */
static double hack_bounce_ball(double *x, double *y, double rad, double *vx, double *vy) {
    double i = 0;
    double hit = 0;
    while (i < HACK_BTN_MAX) {
        double b = i * HB_FIELDS;
        double ox, oy, px, py;
        if (arr_get(hack_buttons, b + HB_ON) != 1 || arr_get(hack_buttons, b + HB_LAND) != 1) {
            i = i + 1;
            continue;
        }
        double bx = arr_get(hack_buttons, b + HB_X);
        double by = arr_get(hack_buttons, b + HB_Y);
        double w = arr_get(hack_buttons, b + HB_W);
        double h = arr_get(hack_buttons, b + HB_H);
        ox = *x - (bx + w / 2);
        oy = *y - (by + h / 2);
        px = w / 2 + rad - fabs(ox);
        py = h / 2 + rad - fabs(oy);
        if (px <= 0 || py <= 0) {
            i = i + 1;
            continue;
        }
        if (px < py) {
            *x = *x + (ox < 0 ? -px : px);
            *vx = -*vx;
        } else {
            *y = *y + (oy < 0 ? -py : py);
            *vy = -*vy;
        }
        *vx = *vx * 0.98;
        *vy = *vy * 0.98;
        hit = 1;
        i = i + 1;
    }
    return hit;
}

/* Junk buttons click with their native sound and do absolutely nothing. */
static double hack_touch(double x, double y, double action, double id) {
    double i;
    if (hack_drag_touch(x, y, action, id) == 1) {
        return 1;
    }
    if (action != 0 || hack_active == 0 || finished != 0 || hack_t < HACK_BTN_START) {
        return 0;
    }
    i = HACK_BTN_MAX - 1;
    while (i >= 0) {
        double b = i * HB_FIELDS;
        if (arr_get(hack_buttons, b + HB_ON) == 1 &&
            hit_rect(x, y, arr_get(hack_buttons, b + HB_X), arr_get(hack_buttons, b + HB_Y),
                     arr_get(hack_buttons, b + HB_W), arr_get(hack_buttons, b + HB_H)) == 1) {
            arr_set(hack_buttons, b + HB_PRESS, 0.16);
            play_button_click();
            return 1;
        }
        i = i - 1;
    }
    return 0;
}

static void hack_spawn_cane(void) {
    if (hack_cane_made >= hack_cane_total) {
        return;
    }
    double b = hack_cane_made * HC_FIELDS;
    double diag = HACK_CANE_SPEED * 0.70710678;
    double dir = (ds_mod(hack_cane_made, 2) == 0) ? 1 : -1;
    while (arr_len(hack_canes) < b + HC_FIELDS) {
        arr_push(hack_canes, 0);
    }
    arr_set(hack_canes, b + HC_ON, 1);
    arr_set(hack_canes, b + HC_X, 40 + (rand() % (int)(screen_w - 80)));
    arr_set(hack_canes, b + HC_Y, -40);
    arr_set(hack_canes, b + HC_DX, dir * diag);
    arr_set(hack_canes, b + HC_LAND, 0);
    arr_set(hack_canes, b + HC_POP, 0);
    hack_cane_made = hack_cane_made + 1;
}

static void hack_tick_canes(void) {
    double diag = HACK_CANE_SPEED * 0.70710678;
    double ground = screen_h - 70;
    double i = 0;
    while (i < HACK_CANE_MAX) {
        double b = i * HC_FIELDS;
        if (arr_get(hack_canes, b + HC_ON) != 1) {
            i = i + 1;
            continue;
        }
        double pop = arr_get(hack_canes, b + HC_POP);
        if (pop > 0) {
            pop = pop + dt;
            if (pop > 0.5) {
                arr_set(hack_canes, b + HC_ON, 0);
            } else {
                arr_set(hack_canes, b + HC_POP, pop);
            }
            i = i + 1;
            continue;
        }
        if (arr_get(hack_canes, b + HC_LAND) == 0) {
            double x = arr_get(hack_canes, b + HC_X) + arr_get(hack_canes, b + HC_DX) * dt;
            double y = arr_get(hack_canes, b + HC_Y) + diag * dt;
            if (x < 24) {
                x = 24;
                arr_set(hack_canes, b + HC_DX, fabs(arr_get(hack_canes, b + HC_DX)));
            }
            if (x > screen_w - 24) {
                x = screen_w - 24;
                arr_set(hack_canes, b + HC_DX, 0 - fabs(arr_get(hack_canes, b + HC_DX)));
            }
            if (y >= ground) {
                y = ground;
                arr_set(hack_canes, b + HC_LAND, 0.0001);
                arr_set(hack_canes, b + HC_DX, 0);
            }
            arr_set(hack_canes, b + HC_X, x);
            arr_set(hack_canes, b + HC_Y, y);
        } else {
            /* Landed canes wait a while, then pop empty-handed. */
            double land_t = arr_get(hack_canes, b + HC_LAND) + dt;
            arr_set(hack_canes, b + HC_LAND, land_t);
            if (land_t > HACK_CANE_LIFE) {
                arr_set(hack_canes, b + HC_POP, 0.0001);
                i = i + 1;
                continue;
            }
        }
        if (finished == 0 && dist(arr_get(hack_canes, b + HC_X), arr_get(hack_canes, b + HC_Y), player->x,
                                  player->y) < player->size * 0.6 + 24) {
            hack_cane_got = hack_cane_got + 1;
            candies = candies + hack_cane_candy;
            save_progress();
            snd_play(GIFT_SOUND);
            ds_log("h4c4k0r: cane collected %g/%g (+%g candies)", hack_cane_got, hack_cane_total,
                   hack_cane_candy);
            arr_set(hack_canes, b + HC_POP, 0.0001);
        }
        i = i + 1;
    }
}

static double hack_canes_left(void) {
    double i = 0;
    double n = 0;
    while (i < HACK_CANE_MAX) {
        if (arr_get(hack_canes, i * HC_FIELDS + HC_ON) == 1) {
            n = n + 1;
        }
        i = i + 1;
    }
    return n;
}

static void update_hacker_event(void) {
    if (hack_enabled() == 0) {
        if (hack_active == 1) {
            hack_reset();
        }
        return;
    }
    if (hack_active == 0) {
        hack_reset();
        hack_active = 1;
        hack_glitch = HACK_GLITCH_TIME;
        hack_payload_ok = hack_load_payload();
        ds_gfx_shake(6, 0.4);
        ds_log("h4c4k0r: raid started (platform %g, payload %s)", hack_platform,
               hack_payload_ok == 1 ? "ok" : "broken");
    }
    if (finished != 0) {
        /* Round already over: let the last pops play out, nothing else. */
        hack_tick_canes();
        return;
    }
    hack_t = hack_t + dt;
    if (hack_glitch > 0) {
        hack_glitch = hack_glitch - dt;
    }
    if (hack_stub_disco_t > 0) {
        hack_stub_disco_t = hack_stub_disco_t - dt;
    }
    if (hack_stub_plates_t > 0) {
        hack_stub_plates_t = hack_stub_plates_t - dt;
    }
    if (hack_cloud_on == 1) {
        hack_cloud_t = hack_cloud_t + dt;
    }
    while (hack_cmd_idx < hack_cmd_n && hack_t >= hack_cmd_at[(int)hack_cmd_idx]) {
        hack_fire_cmd(hack_cmd_id[(int)hack_cmd_idx], hack_cmd_kind[(int)hack_cmd_idx],
                      hack_cmd_life[(int)hack_cmd_idx]);
        hack_cmd_idx = hack_cmd_idx + 1;
    }
    if (hack_ban_t == 0 && hack_t >= HACK_BTN_START && hack_btn_made < hack_btn_total) {
        hack_btn_cd = hack_btn_cd - dt;
        if (hack_btn_cd <= 0) {
            hack_btn_cd = HACK_BTN_INTERVAL;
            hack_spawn_button();
        }
    }
    hack_tick_buttons();
    if (hack_ban_t == 0 && hack_t >= HACK_BAN_AT) {
        hack_ban_t = 0.0001;
        ds_log("h4c4k0r: Azum's ban landed, terminals wiped");
    }
    if (hack_ban_t > 0) {
        hack_ban_t = hack_ban_t + dt;
    }
    if (hack_t >= HACK_RAIN_AT) {
        if (hack_boomed == 0) {
            /* Terminals and the whole junk pile explode at once. */
            hack_boomed = 1;
            hack_cloud_on = 0;
            hack_stub_disco_t = 0;
            hack_stub_plates_t = 0;
            arr_clear(hack_buttons);
            hack_btn_made = 0;
            ds_gfx_shake(7, 0.5);
            snd_play(STATION_SOUND);
            ds_log("h4c4k0r: junk exploded, compensation rain starts");
        }
        if (hack_cane_made < hack_cane_total) {
            hack_cane_cd = hack_cane_cd - dt;
            if (hack_cane_cd <= 0) {
                hack_cane_cd = HACK_CANE_INTERVAL;
                hack_spawn_cane();
            }
        }
        hack_tick_canes();
        if (hack_cane_made >= hack_cane_total && hack_canes_left() == 0) {
            hack_finish_t = hack_finish_t + dt;
            if (hack_finish_t >= HACK_FINISH_GRACE) {
                ds_log("h4c4k0r: canes over (%g collected), clean victory", hack_cane_got);
                finish_game(1);
            }
        }
    }
}
