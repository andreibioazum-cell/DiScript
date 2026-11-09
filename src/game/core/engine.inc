static void play_button_click(void) {
    if (button_click_ok == 1) {
        /* Play preloaded UI click sound on press. */
        snd_play(BUTTON_CLICK_SOUND);
    }
}

static const char *lobby_music_for_theme(void) {
    return LOBBY_MUSIC;
}

static void select_lobby_music(void) {
    const char *wanted = lobby_music_for_theme();
    if (lobby_music_current != NULL && strcmp(lobby_music_current, wanted) == 0) {
        return;
    }
    if (lobby_music_current != NULL && snd_playing(lobby_music_current) == 1) {
        snd_stop(lobby_music_current);
    }
    lobby_music_current = wanted;
    music_level = 0;
    music_ok = snd_load(lobby_music_current);
}

static void start_music(void) {
    lobby_music_current = NULL;
    select_lobby_music();
    if (music_ok == 1) {
        music_level = 1;
        snd_volume(lobby_music_current, music_volume / 100);
        snd_loop(lobby_music_current);
        ds_log("init: lobby music on (%s)", lobby_music_current);
    } else {
        ds_log("init: no lobby music (%s)", lobby_music_current);
    }
}

static void logic_init(void) {
    /* Texture pack first: every later asset load picks pack files over built-in ones. */
    packs_init();
    /* Device-only parental mode has to be known before the first frame. */
    parent_load();
    ds_log("init: loading textures");
    load_textures();
    button_click_ok = snd_load(BUTTON_CLICK_SOUND);
    if (button_click_ok != 1)
        ds_log("init: no button click sound (%s)", BUTTON_CLICK_SOUND);
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
    update_christmas_jingle();
    cube_npc_tex_ok = png_load(CUBE_NPC_TEX);
    helpy_npc_tex_ok = png_load(HELPY_NPC_TEX);
    cube_npc_d0 = "Привет! Я Куб-НПС!";
    cube_npc_d1 = "Добро пожаловать в Cubic Battle 4!";
    cube_npc_d2 = "Попробуй режим Solo или Online!";
    cube_npc_d3 = "Кстати, Азум ушёл к Астре и теперь дерётся за неё!";
    cube_npc_d4 = "Удачи в битве!";
    ds_log("init: ready");
}

/* Quiet ambient bed under the menus: the ordinary bed on the normal theme, the
 * winter bed on the New Year theme. It fades out for a fight, for the winter
 * jingle (which is the New Year lobby's own music) and for the disco loop. */
static const char *ambient_for_theme(void) {
    return winter_theme_on() == 1 ? AMBIENT_WINTER : AMBIENT_ORDINARY;
}

static int ambient_allowed(void) {
    if (game_state == ST_SOLO || game_state == ST_ONLINE) {
        return 0;
    }
    if (disco_music_blocks() == 1) {
        return 0;
    }
    if (winter_theme_on() == 1 && christmas_music_active() == 1) {
        return 0;
    }
    return 1;
}

static void update_ambient(void) {
    double target = clamp(music_volume / 100.0 * ambient_volume_pct / 100.0, 0, 1);
    if (ambient_allowed() == 1) {
        const char *want = ambient_for_theme();
        if (ambient_current == NULL || strcmp(ambient_current, want) != 0) {
            if (ambient_current != NULL && snd_playing(ambient_current) == 1) {
                snd_stop(ambient_current);
            }
            ambient_current = want;
            ambient_level = 0;
            ambient_ok = snd_load(ambient_current);
            if (ambient_ok != 1) {
                ds_log("ambient: no bed (%s)", ambient_current);
                return;
            }
            ds_log("ambient: bed ready (%s)", ambient_current);
        }
        if (ambient_ok != 1) {
            return;
        }
        if (snd_playing(ambient_current) == 0) {
            ambient_level = 0;
            snd_volume(ambient_current, 0);
            if (snd_loop(ambient_current) == 0) {
                ambient_ok = 0;
                return;
            }
        }
        ambient_level = clamp(ambient_level + dt / 1.2, 0, 1);
        snd_volume(ambient_current, target * ambient_level);
        return;
    }
    if (ambient_ok != 1 || ambient_level <= 0 || ambient_current == NULL) {
        return;
    }
    ambient_level = clamp(ambient_level - dt / 1.2, 0, 1);
    snd_volume(ambient_current, target * ambient_level);
    if (ambient_level <= 0 && snd_playing(ambient_current) == 1) {
        snd_stop(ambient_current);
    }
}

static void update_music(void) {
    if (disco_music_blocks() == 1) {
        return;
    }
    select_lobby_music();
    if (music_ok == 0) {
        return;
    }
    if (game_state == ST_SOLO || game_state == ST_ONLINE || christmas_music_active() == 1) {
        music_level = clamp(music_level - dt / music_fade_out_time, 0, 1);
        snd_volume(lobby_music_current, music_volume / 100 * music_level);
        if (music_level <= 0 && snd_playing(lobby_music_current) == 1) {
            snd_stop(lobby_music_current);
        }
        return;
    }
    if (snd_playing(lobby_music_current) == 0) {
        music_level = 0;
        snd_loop(lobby_music_current);
    }
    music_level = clamp(music_level + dt / music_fade_in_time, 0, 1);
    snd_volume(lobby_music_current, music_volume / 100 * music_level);
}

static void logic_update(void) {
    update_warning();
    docs_tick();
    acct_tick();
    update_studio();
    update_transition();
    update_disco_music();
    update_music();
    update_ambient();
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
    } else if (game_state == ST_WHEEL) {
        update_wheel();
    } else if (game_state == ST_ACHIEVEMENTS) {
        ach_scroll_tick();
    } else if (game_state == ST_CLASSES) {
        classes_scroll_tick();
    }
    update_christmas();
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
    } else if (game_state == ST_WHEEL) {
        draw_wheel();
    } else {
        draw_online();
    }
}

static void logic_draw(void) {
    draw_screen();
    draw_countdown();
    if (packs_open == 1) {
        if (packs_fs_open == 1)
            draw_fs_picker();
        else
            draw_packs();
    }
    if (warn_open == 1) {
        draw_warning();
    }
    if (studio_open == 1) {
        draw_studio();
    }
    /* Draw winter snow above studio splash but below epilepsy warning. */
    draw_christmas_over_notices();
    /* Draw legal documents modal above notices and screens. */
    if (docs_open == 1) {
        draw_docs();
    }
    if (parent_open == 1) {
        draw_parental();
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
    if (game_state == ST_BPASS || game_state == ST_WHEEL) {
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
    /* Queue touch-down coordinates for button press animations; ignore drag-over. */
    if (action == 0) {
        press_points_add(x, y, id);
    } else if (action == 1 || action == 3) {
        press_points_drop(id);
    }
    /* Route touch to admin orb and panel first. */
    if (admin_touch(x, y, action, id) == 1) {
        press_points_drop(id);
        return;
    }
    /* Texture pack button sits above the lobby screens and takes its touches first. */
    if (game_state == ST_LOBBY && acct_open == 0 && docs_open == 0 && warn_open == 0 && studio_open == 0 &&
        hit_packs_lobby_button(x, y) == 1) {
        play_button_click();
        packs_open_screen();
        press_points_drop(id);
        return;
    }
    if (t_dir != 0) {
        press_points_drop(id);
        classes_swipe_reset();
        ach_scroll_reset();
        rooms_scroll_reset();
        acct_gift_scroll_reset();
        return;
    }
    if (game_state != ST_CLASSES || acct_open != 0 || docs_open == 1 || warn_open == 1 || studio_open == 1) {
        classes_swipe_reset();
    }
    if (game_state != ST_ACHIEVEMENTS || acct_open != 0 || docs_open == 1 || warn_open == 1 || studio_open == 1) {
        ach_scroll_reset();
    }
    if (game_state != ST_ROOMS || acct_open != 0 || docs_open == 1 || warn_open == 1 || studio_open == 1) {
        rooms_scroll_reset();
    }
    if (parent_open == 1) {
        touch_parental(x, y, action, id);
        return;
    }
    if (packs_open == 1) {
        if (packs_fs_open == 1)
            touch_fs_picker(x, y, action, id);
        else
            touch_packs(x, y, action, id);
        return;
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
    if (game_state == ST_ROOMS) {
        touch_rooms(x, y, action, id);
        return;
    }
    if (game_state == ST_LOBBY && action != 0) {
        touch_lobby(x, y, action, id);
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
        touch_lobby(x, y, 0, id);
    } else {
        touch_menu(x, y, 0, id);
    }
}
