/* A candy appears by alpha only: it stands at its place at full size from the
 * start, only the alpha changes - no growth and no fly-in. */
static void draw_candies(void) {
    if (candies_on() == 0) {
        return;
    }
    double i = 0;
    double cp = 0;
    double p = 0;
    double a = 0;
    double hw = 0;
    while (i < candy_count) {
        cp = arr_get(candy_pick, i);
        if (cp > 0) {
        } else {
            p = clamp(arr_get(candy_t, i) / candy_pop_time, 0, 1);
            a = 1 - (1 - p) * (1 - p);
            if (a > 0.02) {
                double ang = arr_get(candy_ang, i);
                double cx = arr_get(candy_x, i);
                double cy = arr_get(candy_y, i);
                hw = 25 * candy_scale;
                if (candy_tex_ok == 1) {
                    tex_alpha(cx - hw, cy - hw, CANDY_TEX, ang, candy_scale, a);
                } else {
                    circle(cx, cy, hw, (((unsigned)(int)floor(a * 254) + 1u) << 24) | 0x00FF4081);
                }
            }
        }
        i = i + 1;
    }
}

static void draw_status_aura(double x, double y, double size, double rgb, double rim, double a) {
    if (a <= 0) {
        return;
    }
    double fill = floor(a * 255 * 0.3);
    if (fill <= 0) {
        return;
    }
    double half = size + 14;
    circle(x, y, half, (((unsigned)(int)fill) << 24) | ((unsigned)(int)rgb));
    /* pack_c() maps alpha 0 to opaque, so a ring whose alpha has faded to
     * zero must be skipped instead of flashing opaque for a frame. */
    double ring_a = floor(a * 255 * 0.7);
    if (ring_a >= 1) {
        ring(x, y, half, 2, (((unsigned)(int)ring_a) << 24) | ((unsigned)(int)rim));
    }
}

static void draw_remote_snow(void) {
    double s = 0;
    while (s < max_players) {
        double b = s * snow_fields;
        if (arr_get(remote_snow, b + 1) == 1) {
            double x = arr_get(remote_snow, b + 3);
            double y = arr_get(remote_snow, b + 4);
            double t = arr_get(remote_snow, b + 2);
            double hw = 16;
            tex(x - hw, y - hw, SNOWFLAKE_TEX, t * 3, 0.5);
        }
        if (arr_get(remote_snow, b + 5) > 0) {
            draw_boom_at(arr_get(remote_snow, b + 6), arr_get(remote_snow, b + 7), arr_get(remote_snow, b + 5));
        }
        s = s + 1;
    }
}

static void finish_game(double result) {
    finished = result;
    punch->active = 0;
    punch->hit = 0;
    punch_left = 0;
    super_windup = 0;
    santa_follow = 0;
    drop_joy();
    gift->active = 0;
    boom_t = 0;
    dash_active = 0;
    dash_t = 0;
    dash_hit = 0;
    dash_pub = 0;
    enemy_gift->active = 0;
    enemy_boom_t = 0;
    enemy_dash_active = 0;
    enemy_dash_t = 0;
    enemy_dash_hit = 0;
    enemy_super_windup = 0;
    enemy_super_kind = 0;
    enemy_grab_clear();
    /* A projectile still in the air would freeze on the result screen: the fight is
     * over, so it bursts where it is and does nothing. */
    ball_clear_after_finish();
    turret_heal_t = 0;
    universe_active = 0;
    keyboard_hide();
    double i = 0;
    while (i < max_players) {
        double sb = i * snow_fields;
        arr_set(remote_snow, sb, 0);
        arr_set(remote_snow, sb + 1, 0);
        arr_set(remote_snow, sb + 5, 0);
        double tb = i * station_fields;
        double k = 0;
        while (k < turret_max) {
            arr_set(remote_station, tb + 1 + k * 5, 0);
            k = k + 1;
        }
        double ub = i * universe_fields;
        arr_set(remote_universe, ub, 0);
        arr_set(remote_universe, ub + 1, 0);
        remote_dash_set(i, DASH_GEN, 0);
        remote_dash_set(i, DASH_ACTIVE, 0);
        clear_remote_punch(i);
        arr_set(rpunch_a, i, 0);
        i = i + 1;
    }
    if ((result == 1 || result == 2) && cups_awarded == 0) {
        cups_awarded = 1;
        award_battle_result(result);
    }
}

/* Battle payout. If any admin panel switch was on at least once during this
 * fight, the fight changes nothing in the wallet: a win pays no cups (and no
 * win achievements), a loss takes none. With all panel switches off, wins and
 * losses change cups normally even for an admin. */
static void award_battle_result(double result) {
    if (result == 1) {
        if (admin_cups_allowed() == 0) {
            ds_log("win: no cups, a cheat was used in this battle");
            return;
        }
        cups = cups + cup_win;
        if (candies_on() == 1) {
            candies = candies + candy_win;
        }
        if (is_admin() == 1) {
            bp_xp = bp_xp + 10;
        }
        save_progress();
        grant_achievement(ACH_FIRST_WIN);
        /* Flawless win: keep full health and take no hits during the battle.
         * player_was_hit includes damage intercepted by ebuc's turrets. */
        if (player->hp >= player->max_hp && player_was_hit == 0) {
            grant_achievement(ACH_LAST_HP);
        }
        ds_log("win +%g cups +%g candies", cup_win, candy_win);
        return;
    }
    if (admin_cups_allowed() == 0) {
        ds_log("lose: no cups taken away, a cheat was used in this battle");
        return;
    }
    cups = cups - cup_lose;
    if (cups < 0) {
        cups = 0;
    }
    save_progress();
    ds_log("lose -%g cups", cup_lose);
}

static void draw_punch_box(double x, double y, double dx, double dy, double reach, double width, double alpha) {
    draw_hit_strip(x, y, dx, dy, punch_forward_offset, punch_forward_offset + reach, width / 2, alpha);
}

static void draw_field(void) { rect(0, 0, screen_w, screen_h, 0x33000000); }

static double player_punching_now(void) {
    if (punch_left > 0) {
        return 1;
    }
    if (astra_punch_t > 0) {
        return 1;
    }
    return 0;
}

static void draw_player_shadow_layer(void) {
    draw_fighter_shadow(player->x, player->y, player->angle, fighter_look_of(FLOOK_ME),
                        fighter_pose_of(FLOOK_ME, player_punching_now()));
}

static void draw_player_hitbox_layer(void) {
    if (show_hitboxes == 0) {
        return;
    }
    if (aim_a > 0) {
        draw_punch_box(punch->x, punch->y, punch->dx, punch->dy, punch_reach, punch_width, aim_a);
    }
}

static void draw_guard(double x, double y, double angle, double on, double scale) {
    (void)angle;
    if (on <= 0) {
        return;
    }
        /* Block is just a blue circle on the player itself, nothing else. */
    double r = 25 * cube_scale * scale * 1.75;
    circle(x, y, r, 0x2E4FC3F7);
    ring(x, y, r, clamp(r * 0.13, 3, 5), 0xCC4FC3F7);
}

static void draw_player_body_layer(void) {
    double punching = fighter_pose_of(FLOOK_ME, player_punching_now());
    double look = fighter_look_of(FLOOK_ME);
    if (look != LOOK_NONE) {
        draw_motion_ghosts(player->x, player->y, mb_dx, mb_dy, 25 * cube_scale * player_scale,
                           cube_scale * player_scale, look_tex(look, punching), sprite_a(player->angle));
    }
    draw_fighter_body(player->x, player->y, player->angle, look, punching);
    double self_tint = codecracker_tint_local();
    draw_fighter_tint(player->x, player->y, player->angle, look, punching, player_scale, self_tint);
    draw_fighter_hurt(player->x, player->y, player->angle, look, punching, player_scale, player_hurt_a);
    draw_guard(player->x, player->y, player->angle, player_block, player_scale);
}

static double enemy_punching_now(void) {
    if (enemy->state == 1 || enemy->state == 2) {
        return 1;
    }
    if (enemy_super_windup > 0) {
        return 1;
    }
    if (enemy_grab_punching() == 1) {
        return 1;
    }
    return 0;
}

static void draw_enemy_shadow(void) {
    double ehw = 25 * cube_scale * enemy_scale;
    double es = cube_scale * enemy_scale;
    double look = fighter_look_of(FLOOK_BOT);
    double sx = enemy->x - shadow_off;
    double sy = enemy->y + shadow_off;
    if (look == LOOK_NONE) {
        draw_shadow_blur_circle(sx, sy, ehw, shadow_alpha * 2.5, ehw * 0.07);
        return;
    }
    draw_shadow_blur_tex(sx, sy, ehw, es, look_tex(look, fighter_pose_of(FLOOK_BOT, enemy_punching_now())),
                         sprite_a(enemy->angle), shadow_alpha * 2.5, ehw * 0.07);
}

static void draw_enemy_hitbox(void) {
    if (show_hitboxes == 0) {
        return;
    }
    if (enemy_punch_a > 0) {
        draw_punch_box(enemy_punch_x, enemy_punch_y, enemy_punch_dx, enemy_punch_dy, enemy_punch_reach,
                       enemy_punch_width, enemy_punch_a);
    }
}

static void draw_enemy_body(void) {
    double ehw = 25 * cube_scale * enemy_scale;
    double es = cube_scale * enemy_scale;
    double punching = fighter_pose_of(FLOOK_BOT, enemy_punching_now());
    double look = fighter_look_of(FLOOK_BOT);
    if (look != LOOK_NONE) {
        draw_motion_ghosts(enemy->x, enemy->y, mb_edx, mb_edy, ehw, es, look_tex(look, punching),
                           sprite_a(enemy->angle));
    }
    if (look == LOOK_NONE) {
        rect(enemy->x - ehw, enemy->y - ehw, 2 * ehw, 2 * ehw, fighter_fallback_rgb);
    } else {
        tex(enemy->x - ehw, enemy->y - ehw, look_tex(look, punching), sprite_a(enemy->angle), es);
    }
    draw_fighter_hurt(enemy->x, enemy->y, enemy->angle, look, punching, enemy_scale, enemy_hurt_a);
    draw_guard(enemy->x, enemy->y, enemy->angle, enemy_block_left > 0, enemy_scale);
}

/* A combat pad answers a press the way a menu button does: it shrinks a
 * little and darkens, so a punch that goes off on the touch still reads as a
 * press and not as a misfire. */
/* Battle button radius: it was 55, made noticeably bigger on request, so a
 * finger cannot miss. */
static double battle_pad_r(void) {
    return 60;
}
static void draw_pad(double cx, double cy, double r, uint32_t col, const char *label, double scale) {
    /* The squeeze is claimed by the circle itself - the same circle the
     * touch handler fires on - not by its square bounding box. */
    double p = button_press_k_round(cx - r, cy - r, r * 2, r * 2, r);
    double s = 1 - PRESS_SQUEEZE * p;
    double rr = r * s;
    circle(cx, cy, rr, shade(col, 1 - PRESS_SHADE * p));
    ring(cx, cy, rr, 4, 0x000000);
    text_in_box(label, cx - rr, cy - rr, rr * 2, rr * 2, 0xFFFFFF, scale * s);
}

static void draw_controls(void) {
    ring(joy.x, joy.y, joy.r, 4, 0x000000);
    /* The projectile countdown belongs to the interface, above the fight. */
    draw_ball_count();
    circle(joy.x + joy.ox, joy.y + joy.oy, 34, 0x000000);
        /* All three battle buttons are the same size and much bigger than before:
         * block is no smaller than punch and super. */
    draw_pad(atk_x, atk_y, battle_pad_r(), 0xE94560, tr_punch(), 0.55);
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
                /* Block sits between punch and super, to the left of both. */
        draw_pad(blk_x, blk_y, battle_pad_r(), player_block == 1 ? 0x2E6FBF : 0x3A5A8C, tr_block(), 0.55);
    }
    if (class_has_super(player_class) == 1 && (game_state == ST_SOLO || game_state == ST_ONLINE)) {
        const char *sl = tr_super();
        if (player_class == CLASS_EBUC) {
            sl = tr_super_ebuc();
        } else if (player_class == CLASS_AZUM) {
            sl = tr_super_azum();
        } else if (class_is_astra(player_class) == 1) {
            sl = tr_super_astra();
        } else if (player_class == CLASS_CODECRACKER) {
            sl = tr_super_codecracker();
        }
        double sls = 0.55;
        double left = super_cd;
        if (player_class == CLASS_CODECRACKER && codecracker_super_t > 0) {
            /* While the overclock runs, the pad counts the ult itself down. */
            left = codecracker_super_t;
        }
        if (player_class == CLASS_AZUM) {
            left = dash_cd;
        } else if (class_is_astra(player_class) == 1) {
            left = astra_cd;
        }
        if (left > 0) {
            sl = ds_concat(ds_num_to_string((double)(floor(left + 0.999))), "");
            sls = 0.95;
        }
        draw_pad(atk2_x, atk2_y, battle_pad_r(), 0x5F10A0, sl, sls);
    }
    draw_battle_back();
}

/* Battle result screen: purple in the new theme, pink in the old one (in its
 * button style), for a win and for a loss. The fill matches the window face:
 * dark purple or dark pink. */
#define RESULT_CURTAIN 0xCC1A1A2E
/* Dark pink fill of the old theme under its pink defeat window. */
#define RESULT_ROSE_CURTAIN 0xCC3A1530

static void draw_result(const char *title) {
    double face = result_face_color();
    rect(0, 0, screen_w, screen_h, face == RESULT_PURPLE ? RESULT_CURTAIN : RESULT_ROSE_CURTAIN);
    double w = 430;
    double h = 250;
    double x = (screen_w - w) / 2;
    double y = screen_h / 2 - h / 2;
    roundrect(x, y, w, h, 30, face);
    double reward = 0;
    if (finished == 1) {
        reward = cup_win;
    } else if (finished == 2) {
        reward = 0 - cup_lose;
    }
    ctext_scaled(title, y + 36, 0xFFFFFF, 1.12);
    if (admin_cups_allowed() == 0) {
        ctext_scaled(finished == 2 ? tr_cups_admin() : tr_cups_cheated(), y + 126, 0xFFFFFF, 0.9);
    } else if (finished == 2) {
        ctext_scaled(tr_cups_lose(reward), y + 126, 0xFFFFFF, 0.9);
    } else {
        ctext_scaled(tr_cups_reward(reward), y + 126, 0xFFFFFF, 0.9);
    }
    ctext_scaled(tr_tap_to_return(), y + 188, 0xFFFFFF, 0.62);
}

static void draw_game(void) {
    resolve_fighter_looks();
    motion_blur_track();
    draw_arena_background();
    draw_field();
    draw_dust();
    draw_player_shadow_layer();
    draw_enemy_shadow();
    draw_turret_shadows();
    draw_candies();
    draw_player_hitbox_layer();
    draw_enemy_hitbox();
    draw_ability_hitboxes();
    draw_gift();
    draw_boom();
    draw_enemy_gift();
    draw_enemy_boom();
    draw_universe();
    draw_enemy_turrets();
    chaos_draw();
    draw_ball();
    draw_codecracker_fx_local();
    draw_player_body_layer();
    draw_status_aura(player->x, player->y, player->size, freeze_rgb, freeze_rim, pfreeze_a);
    draw_status_aura(player->x, player->y, player->size, poison_rgb, poison_rim, ppoison_a);
    draw_status_aura(player->x, player->y, player->size, stun_rgb, stun_rim, pstun_a);
    draw_station();
    draw_enemy_body();
    draw_status_aura(enemy->x, enemy->y, enemy->size, freeze_rgb, freeze_rim, freeze_a);
    draw_status_aura(enemy->x, enemy->y, enemy->size, poison_rgb, poison_rim, poison_a);
    draw_status_aura(enemy->x, enemy->y, enemy->size, stun_rgb, stun_rim, stun_a);
    draw_snow();
        /* Colour correction and vignette cover the fight, not the interface: the
         * health bars and buttons stay clean. */
    draw_screen_fx();
    double w = screen_w / 4;
    hud_bar(20, 20, w, 28, player->hp, player->max_hp, 0x7ED321, 0);
    hud_bar(screen_w - w - 20, 20, w, 28, enemy->hp, enemy->max_hp, 0xE94560, 1);
    draw_controls();
        /* Red tint on damage - over everything, but very weak. */
    draw_hurt_flash();
    draw_fps_meter();
    if (finished == 1) {
        draw_result(tr_victory());
    } else if (finished == 2) {
        draw_result(tr_defeat());
    }
}
