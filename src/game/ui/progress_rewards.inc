static double has_achievement(double bit) {
    double shifted = floor(achievement_mask / bit);
    return shifted - floor(shifted * 0.5) * 2;
}

static void show_achievement_toast(double bit) {
    ach_toast_bit = bit;
    ach_toast_t = ACH_TOAST_DURATION;
}

static double grant_achievement(double bit) {
    if (has_achievement(bit) == 1) {
        return 0;
    }
    achievement_mask = achievement_mask + bit;
    net_mark_achievement_flag(bit);
    show_achievement_toast(bit);
    return 1;
}

static void check_achievements(void) {
    if (class_owned_of(CLASS_AZUM) == 1 && class_owned_of(CLASS_SANTA) == 1 && class_owned_of(CLASS_EBUC) == 1) {
        grant_achievement(ACH_ALL_CHARACTERS);
    }
    check_meet_eax229();
    check_buk_elf_achievement();
}

static void check_buk_elf_achievement(void) {
    if (has_achievement(ACH_BUK_GIFT) == 1) {
        return;
    }
    if (game_state != ST_ONLINE) {
        return;
    }
    if (event_mode != 3) {
        return;
    }
    if (player_class != CLASS_SANTA) {
        return;
    }
    if (online_ready != 1) {
        return;
    }
    grant_achievement(ACH_BUK_GIFT);
}

static double nick_is_dimasi229(const char *nick) {
    const char *want = ADMIN_NICK;
    if (!nick) {
        return 0;
    }
    while (*want && *nick) {
        char a = *nick, b = *want;
        if (a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if (a != b)
            return 0;
        nick++;
        want++;
    }
    return *want == 0 && *nick == 0 ? 1 : 0;
}

/* mov eax, 0x229: be in one online room with Dimasi4ek229. For
 * Dimasi4ek229 himself it is granted automatically. */
static void check_meet_eax229(void) {
    if (has_achievement(ACH_EAX229) == 1) {
        return;
    }
    if (nick_is_dimasi229(net_login_nick()) == 1) {
        grant_achievement(ACH_EAX229);
        return;
    }
    if (game_state != ST_ONLINE) {
        return;
    }
    double s = 0;
    while (s < NET_SLOTS) {
        if (s != net_slot() && net_player_online(s) == 1 && nick_is_dimasi229(net_player_nick(s)) == 1) {
            grant_achievement(ACH_EAX229);
            return;
        }
        s = s + 1;
    }
}

static void save_progress(void) {
    net_set_bp_xp(bp_xp);
    double bp_lv = bp_claimed;
    net_save_codecracker(class_owned_of(CLASS_CODECRACKER), class_level_of(CLASS_CODECRACKER),
                         class_unlocked_of(CLASS_CODECRACKER));
    net_save_astra(class_owned_of(CLASS_ASTRA), class_level_of(CLASS_ASTRA), class_unlocked_of(CLASS_ASTRA));
    net_save_progress_all(cups, candies, player_class, class_owned_of(CLASS_AZUM), class_owned_of(CLASS_SANTA),
                          class_owned_of(CLASS_EBUC), player_level, levels_unlocked, class_level_of(CLASS_ORDINARY),
                          class_unlocked_of(CLASS_ORDINARY), class_level_of(CLASS_AZUM), class_unlocked_of(CLASS_AZUM),
                          class_level_of(CLASS_SANTA), class_unlocked_of(CLASS_SANTA), class_level_of(CLASS_EBUC),
                          class_unlocked_of(CLASS_EBUC), bp_lv, skin_flags());
    progress_push_to_net();
}

static double skin_flags(void) {
    double f = 0;
    if (azum_skin == SKIN_SPECIAL) {
        f = f + 1;
    }
    if (ebuc_skin == SKIN_SPECIAL) {
        f = f + 2;
    }
    return f;
}

static void progress_push_to_net(void) {
    net_set_class(player_class);
    net_set_level(player_level);
    net_set_skin(current_skin());
}

static void progress_normalize(void) {
    astra_ability_refresh();
    codecracker_refresh();
    /* ID 5 was the removed Astra variant in older saves. Fold it into the
     * ordinary Astra class before validating the selected class. */
    if (player_class == 5) {
        player_class = CLASS_ASTRA;
    }
    if (azum_skin < SKIN_NORMAL || azum_skin >= SKIN_COUNT) {
        azum_skin = SKIN_NORMAL;
    }
    if (ebuc_skin < SKIN_NORMAL || ebuc_skin >= SKIN_COUNT) {
        ebuc_skin = SKIN_NORMAL;
    }
    if (azum_skin == SKIN_ZOMBIE && has_achievement(ACH_LEGENDS) == 0) {
        azum_skin = SKIN_NORMAL;
    }
    double order = 0;
    while (order < class_count) {
        double cls = class_at_order(order);
        double un = clamp(class_unlocked_of(cls), 0, max_level);
        double lv = class_level_of(cls);
        if (lv < 0 || lv > un) {
            lv = 0;
        }
        set_class_progress(cls, lv, un);
        order = order + 1;
    }
    if (player_class == CLASS_SANTA && dev_access() == 0 && (is_admin() == 0 || bp_claimed < 7)) {
        /* Santa is developer-only until the admin battle-pass tier 7 is claimed;
         * the ownership itself is kept either way. */
        player_class = CLASS_ORDINARY;
    }
    if (class_id_valid(player_class) == 0 || class_owned_of(player_class) == 0) {
        player_class = CLASS_ORDINARY;
    }
    levels_unlocked = clamp(levels_unlocked, 0, max_level);
    if (player_level < 0 || player_level > levels_unlocked) {
        player_level = 0;
    }
}

static void progress_from_storage(void) {
    bp_xp = net_load_bp_xp();
    bp_claimed = net_load_bp_level();
    if (bp_claimed < 0) bp_claimed = 0;
    if (bp_claimed > 7) bp_claimed = 7;
    cups = net_load_cups();
    candies = net_load_candies();
    achievement_mask = net_load_achievement_flags();
    azum_revive_count = net_load_azum_revives();
    player_class = net_load_class();
    set_class_owned(CLASS_AZUM, net_load_azum());
    set_class_owned(CLASS_SANTA, net_load_santa());
    set_class_owned(CLASS_EBUC, net_load_ebuc());
    set_class_owned(CLASS_ASTRA, net_load_astra());
    set_class_owned(CLASS_CODECRACKER, net_load_codecracker());
    player_level = net_load_level();
    levels_unlocked = net_load_levels_unlocked();
    set_class_progress(CLASS_ORDINARY, net_load_ordinary_level(), net_load_ordinary_levels_unlocked());
    set_class_progress(CLASS_AZUM, net_load_azum_level(), net_load_azum_levels_unlocked());
    set_class_progress(CLASS_SANTA, net_load_santa_level(), net_load_santa_levels_unlocked());
    set_class_progress(CLASS_EBUC, net_load_ebuc_level(), net_load_ebuc_levels_unlocked());
    set_class_progress(CLASS_ASTRA, net_load_astra_level(), net_load_astra_levels_unlocked());
    set_class_progress(CLASS_CODECRACKER, net_load_codecracker_level(), net_load_codecracker_levels_unlocked());
    astra_ability_refresh();
    codecracker_refresh();
    double skins = net_load_azum_skin();
    ebuc_skin = SKIN_NORMAL;
    if (skins >= 2) {
        ebuc_skin = SKIN_SPECIAL;
        skins = skins - 2;
    }
    azum_skin = skins;
    if (azum_skin == SKIN_ZOMBIE && has_achievement(ACH_LEGENDS) == 0) {
        grant_achievement(ACH_LEGENDS);
    }
    progress_normalize();
    sync_selected_class();
    progress_push_to_net();
    check_achievements();
}

/* Sign-out wipes the local progress back to a fresh guest: cups, candies,
 * bought classes, levels, skins and achievements all go to zero, and the
 * zeroed state is saved on the device so a restart does not resurrect the
 * previous account's numbers. The cloud profile is NOT touched: callers
 * run this after net_logout(), when save_progress() can only write the
 * local file, and logging back in restores the account from the cloud. */
static void progress_reset_on_signout(void) {
    bp_xp = 0;
    bp_claimed = 0;
    cups = 0;
    candies = 0;
    set_class_owned(CLASS_AZUM, 0);
    set_class_owned(CLASS_SANTA, 0);
    set_class_owned(CLASS_EBUC, 0);
    set_class_owned(CLASS_ASTRA, 0);
    set_class_owned(CLASS_CODECRACKER, 0);
    double i = 0;
    while (i < class_count) {
        set_class_progress(class_at_order(i), 0, 0);
        i = i + 1;
    }
    player_class = CLASS_ORDINARY;
    player_level = 0;
    levels_unlocked = 0;
    azum_skin = SKIN_NORMAL;
    ebuc_skin = SKIN_NORMAL;
    achievement_mask = 0;
    azum_revive_count = 0;
    cheat_god = 0;
    cheat_damage = 0;
    cheat_speed = 0;
    cheat_nocd = 0;
    cheat_cc = 0;
    cheat_estun = 0;
    cheat_chaos = 0;
    cheat_stain = 0;
    cheat_estun_mark = 0;
    admin_panel_open = 0;
    sync_selected_class();
    save_progress();
    net_mark_progress_synced();
}

static void settings_from_storage(void) {
    language = net_load_language();
    show_hitboxes = net_load_hitboxes();
    /* There is no music toggle any more: the lobby music always plays at
     * the fixed default volume (music_volume in the state defaults). */
    winter_theme = net_load_winter_theme();
    new_theme = net_load_new_theme();
    apply_theme();
}

static void save_settings(void) {
    net_save_settings(language, show_hitboxes);
    net_save_winter_theme(winter_theme);
    net_save_new_theme(new_theme);
}
