static void play_button_click(void) {
    if (button_click_ok == 1) {
        /* Play preloaded UI click sound on press. */
        snd_play(BUTTON_CLICK_SOUND);
    }
}

static void start_music(void) {
    music_ok = snd_load(LOBBY_MUSIC);
    if (music_ok == 1) {
        music_level = 1;
        snd_volume(LOBBY_MUSIC, music_volume / 100);
        snd_loop(LOBBY_MUSIC);
        ds_log("init: lobby music on");
    } else {
        ds_log("init: no lobby music");
    }
    seraphim_ok = snd_load(SERAPHIM_MUSIC);
    if (seraphim_ok == 1) {
        seraphim_level = 0;
        ds_log("init: seraphim battle music ready");
    } else {
        ds_log("init: no seraphim music");
    }
}

/* Azum vs Astra matchup music helpers. */
static int seraphim_is_astra(double cls) {
    return class_is_astra(cls) == 1 || cls == 5;
}

static int seraphim_should_play(void) {
    if (game_state != ST_SOLO && game_state != ST_ONLINE) {
        return 0;
    }
    if (game_state == ST_SOLO) {
        if ((player_class == CLASS_AZUM && seraphim_is_astra(enemy_class)) ||
            (seraphim_is_astra(player_class) && enemy_class == CLASS_AZUM)) {
            return 1;
        }
        return 0;
    }
    /* Check local and remote slots for an Azum-vs-Astra online matchup. */
    {
        int has_azum = 0;
        int has_astra = 0;
        if (player_class == CLASS_AZUM) has_azum = 1;
        if (seraphim_is_astra(player_class)) has_astra = 1;
        double s = 0;
        while (s < max_players) {
            if (net_player_online(s) == 1) {
                double c = net_player_class(s);
                if (c == CLASS_AZUM) has_azum = 1;
                if (seraphim_is_astra(c)) has_astra = 1;
            }
            if (arr_get(remotes, s * remote_fields) == 1) {
                double rc = arr_get(remotes, s * remote_fields + 10);
                if (rc == CLASS_AZUM) has_azum = 1;
                if (seraphim_is_astra(rc)) has_astra = 1;
            }
            s = s + 1;
        }
        if (has_azum == 1 && has_astra == 1) return 1;
        return 0;
    }
}

static void update_seraphim_music(void) {
    double target_vol = clamp(music_volume / 100 * 1.65, 0, 1);
    if (seraphim_should_play() == 1) {
        if (seraphim_ok == 0) {
            seraphim_ok = snd_load(SERAPHIM_MUSIC);
            if (seraphim_ok == 0) return;
        }
        if (snd_playing(LOBBY_MUSIC) == 1) {
            snd_stop(LOBBY_MUSIC);
            music_level = 0;
        }
        if (snd_playing(SERAPHIM_MUSIC) == 0) {
            seraphim_level = 0.25;
            snd_volume(SERAPHIM_MUSIC, target_vol * seraphim_level);
            snd_loop(SERAPHIM_MUSIC);
        }
        seraphim_level = clamp(seraphim_level + dt / seraphim_fade_in, 0, 1);
        snd_volume(SERAPHIM_MUSIC, target_vol * seraphim_level);
        return;
    }
    if (seraphim_ok == 0) return;
    seraphim_level = clamp(seraphim_level - dt / seraphim_fade_out, 0, 1);
    snd_volume(SERAPHIM_MUSIC, target_vol * seraphim_level);
    if (seraphim_level <= 0 && snd_playing(SERAPHIM_MUSIC) == 1) {
        snd_stop(SERAPHIM_MUSIC);
    }
}

static void logic_init(void) {
    ds_log("init: loading textures");
    load_textures();
    button_click_ok = snd_load(BUTTON_CLICK_SOUND);
    if (button_click_ok == 1) {
        snd_volume(BUTTON_CLICK_SOUND, 0.7);
    }
    ability_sounds_init();
    net_set_firebase_key(FB_KEY);
    net_autologin(FIREBASE);
    progress_from_storage();
    settings_from_storage();
    start_gate_init();
    apply_theme();
    apply_winter_theme();
    start_music();
    update_newyear_jingle();
    cube_npc_tex_ok = png_load(CUBE_NPC_TEX);
    cube_npc_d0 = "Привет! Я Куб-НПС!";
    cube_npc_d1 = "Добро пожаловать в Cubic Battle 4!";
    cube_npc_d2 = "Попробуй режим Solo или Online!";
    cube_npc_d3 = "В магазине можно купить персонажей!";
    cube_npc_d4 = "Удачи в битве!";
    ds_log("init: ready");
}

static void update_music(void) {
    if (music_ok == 0) {
        return;
    }
    if (game_state == ST_SOLO || game_state == ST_ONLINE || newyear_music_active() == 1) {
        music_level = clamp(music_level - dt / music_fade_out_time, 0, 1);
        snd_volume(LOBBY_MUSIC, music_volume / 100 * music_level);
        if (music_level <= 0 && snd_playing(LOBBY_MUSIC) == 1) {
            snd_stop(LOBBY_MUSIC);
        }
        return;
    }
    if (snd_playing(LOBBY_MUSIC) == 0) {
        music_level = 0;
        snd_loop(LOBBY_MUSIC);
    }
    music_level = clamp(music_level + dt / music_fade_in_time, 0, 1);
    snd_volume(LOBBY_MUSIC, music_volume / 100 * music_level);
}

static void logic_update(void) {
    update_warning();
    docs_tick();
    acct_tick();
    update_studio();
    update_transition();
    update_music();
    update_seraphim_music();
    reload_textures();
    tick_achievement_toast();
    gift_shimmer_tick();
    update_button_press();
    admin_tick();
    if (game_state == ST_LOBBY) {
        cube_npc_bob_t = cube_npc_bob_t + dt;
        cube_npc_tick();
        if (cube_npc_dialogue_active == 1) {
            double max_chars = utf8_len(cube_npc_dialogue_text());
            if (cube_npc_dialogue_chars < max_chars) {
                cube_npc_dialogue_chars = cube_npc_dialogue_chars + cube_npc_dialogue_speed * dt;
                if (cube_npc_dialogue_chars > max_chars) {
                    cube_npc_dialogue_chars = max_chars;
                }
            }
        }
    }
    if (game_state == ST_SOLO) {
        update_game();
    } else if (game_state == ST_ONLINE) {
        update_online();
    } else if (game_state == ST_LOGIN && acct_open == 0) {
        update_login();
    } else if (game_state == ST_PROMO) {
        update_promo();
    } else if (game_state == ST_ROOMS) {
        update_rooms();
    } else if (game_state == ST_ACHIEVEMENTS) {
        ach_scroll_tick();
    } else if (game_state == ST_CLASSES) {
        classes_scroll_tick();
    }
    update_newyear();
    update_weather();
}

static void draw_screen(void) {
    if (game_state == ST_LOBBY) {
        draw_lobby();
    } else if (game_state == ST_SOLO) {
        draw_game();
    } else if (game_state == ST_MODES) {
        draw_modes();
    } else if (game_state == ST_SETTINGS) {
        draw_settings();
    } else if (game_state == ST_LOGIN) {
        draw_login();
    } else if (game_state == ST_CLASSES) {
        draw_classes();
    } else if (game_state == ST_LEVELS) {
        draw_levels();
    } else if (game_state == ST_BPASS) {
        draw_bpass();
    } else if (game_state == ST_SKINS) {
        draw_skins();
    } else if (game_state == ST_LEADERBOARD) {
        draw_leaderboard();
    } else if (game_state == ST_ACHIEVEMENTS) {
        draw_achievements();
    } else if (game_state == ST_PROMO) {
        draw_promo();
    } else if (game_state == ST_ROOMS) {
        draw_rooms();
    } else {
        draw_online();
    }
}

static void logic_draw(void) {
    draw_screen();
    if (warn_open == 1) {
        draw_warning();
    }
    if (studio_open == 1) {
        draw_studio();
    }
    /* Draw winter snow above studio splash but below epilepsy warning. */
    draw_newyear_over_notices();
    /* Draw legal documents modal above notices and screens. */
    if (docs_open == 1) {
        draw_docs();
    }
    if (acct_open != 0) {
        draw_account();
    }
    draw_achievement_toast();
    if (t_fade > 0) {
        double a = t_fade * t_fade * (3 - 2 * t_fade);
        rect(0, 0, screen_w, screen_h, (1 + floor(a * 254)) * 0x1000000);
    }
    /* Draw admin orb and panel above all screens and transition curtains. */
    admin_draw();
    /* Clear unclaimed touch-press points at end of frame. */
    press_points_clear();
}

/* Post an exit notification on half of app exits. */
static void bye_notice_roll(void) {
    const char *nick;
    double pick;
    if (rand() % 100 >= 50) {
        return;
    }
    nick = net_login_nick();
    pick = rand() % 3;
    if (pick >= 2 && str_len(nick) == 0) {
        pick = rand() % 2;
    }
    if (pick >= 2) {
        bye_notice_show(ds_concat(ds_concat(tr_bye_nick_pre(), nick), tr_bye_nick_post()));
        return;
    }
    if (pick == 1) {
        bye_notice_show(tr_bye_cubes());
        return;
    }
    bye_notice_show(tr_bye_sorry());
}

static double logic_back(void) {
    /* Route back key to admin panel first when open. */
    if (admin_panel_open == 1) {
        admin_panel_close();
        return 1;
    }
    if (t_dir != 0) {
        return 1;
    }
    if (acct_open != 0) {
        start_action_transition(acct_back);
        return 1;
    }
    if (docs_open == 1) {
        start_action_transition(docs_close);
        return 1;
    }
    if (warn_open == 1) {
        return 0;
    }
    if (studio_open == 1) {
        return 1;
    }
    if (game_state == ST_ONLINE && chat_open == 1) {
        chat_set_open(0);
        return 1;
    }
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        if (finished != 0 && finish_t < alert_hold) {
            return 1;
        }
        start_transition(ST_LOBBY);
        return 1;
    }
    if (game_state == ST_LEVELS || game_state == ST_SKINS) {
        start_transition(ST_CLASSES);
        return 1;
    }
    if (game_state == ST_BPASS) {
        start_transition(ST_LOBBY);
        return 1;
    }
    if (game_state == ST_LEADERBOARD || game_state == ST_PROMO || game_state == ST_ROOMS) {
        keyboard_hide();
        start_transition(ST_MODES);
        return 1;
    }
    if (game_state == ST_LOGIN) {
        keyboard_hide();
        keyboard_clear();
        if (login_after == ST_ONLINE) {
            start_transition(ST_MODES);
        } else {
            start_transition(ST_LOBBY);
        }
        return 1;
    }
    if (game_state != ST_LOBBY) {
        start_transition(ST_LOBBY);
        return 1;
    }
    /* Close app when back is pressed in lobby. */
    return 0;
}

static void logic_touch(double x, double y, double action, double id) {
    /* Queue per-pointer touch coordinates for button press animations. */
    if (action == 0 || action == 2) {
        press_points_add(x, y, id);
    } else if (action == 1) {
        press_points_drop(id);
    } else if (action == 3) {
        press_points_drop(id);
    }
    /* Route touch to admin orb and panel first. */
    if (admin_touch(x, y, action, id) == 1) {
        if (action == 0) {
            press_points_drop(id);
        }
        return;
    }
    if (t_dir != 0) {
        press_points_drop(id);
        classes_swipe_reset();
        ach_scroll_reset();
        acct_gift_scroll_reset();
        return;
    }
    if (game_state != ST_CLASSES || acct_open != 0 || docs_open == 1 || warn_open == 1 || studio_open == 1) {
        classes_swipe_reset();
    }
    if (game_state != ST_ACHIEVEMENTS || acct_open != 0 || docs_open == 1 || warn_open == 1 || studio_open == 1) {
        ach_scroll_reset();
    }
    if (acct_open != 0) {
        touch_account(x, y, action, id);
        return;
    }
    if (docs_open == 1) {
        touch_docs(x, y, action);
        return;
    }
    if (warn_open == 1) {
        touch_warn(x, y, action);
        return;
    }
    if (studio_open == 1) {
        press_points_drop(id);
        return;
    }
    /* Forward full touch gestures to combat and shop; fire other menus on press. */
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        if (action == 0 && menu_button_hit(x, y) == 1) {
            play_button_click();
        }
        touch_game(x, y, action, id);
        return;
    }
    if (game_state == ST_CLASSES) {
        touch_classes(x, y, action, id);
        return;
    }
    if (game_state == ST_ACHIEVEMENTS) {
        touch_achievements(x, y, action, id);
        return;
    }
    if (action != 0) {
        return;
    }
    /* Advance active lobby NPC dialogue without triggering underlying menu rows. */
    double dialog_skip = game_state == ST_LOBBY && new_theme == 1 && cube_npc_dialogue_active == 1;
    if (dialog_skip == 1) {
        press_points_drop(id);
    }
    if (menu_button_hit(x, y) == 1 && dialog_skip == 0) {
        play_button_click();
    }
    if (game_state == ST_LOBBY) {
        touch_lobby(x, y, 0);
    } else {
        touch_menu(x, y, 0, id);
    }
}
