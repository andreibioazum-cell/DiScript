static void tick_freeze(void) {
    player_freeze = tick_down(player_freeze);
}

static void tick_player_stun(void) {
    player_stun = tick_down(player_stun);
}

static void clear_player_poison(void) {
    player_poison = 0;
    player_poison_tick = 0;
    player_poison_damage = 0;
}

static void tick_player_poison(void) {
    if (player_poison <= 0) {
        return;
    }
    if (player_poison_tick <= 0) {
        player_poison_tick = santa_poison_tick_interval;
    }
    player_poison = tick_down(player_poison);
    player_poison_tick = player_poison_tick - dt;
    if (player_poison_tick <= 0) {
        take_damage(player_poison_damage);
        clear_player_poison();
        return;
    }
    if (player_poison <= 0) {
        clear_player_poison();
    }
}

static void tick_status_fades(void) {
    double efreeze = 0;
    double epoison = 0;
    double estun = 0;
    if (game_state == ST_SOLO) {
        efreeze = enemy->freeze > 0;
        epoison = enemy->poison > 0;
        estun = enemy->stun > 0;
    }
    double pfreeze = player_freeze > 0;
    double ppoison = player_poison > 0;
    double pstun = player_stun > 0;
    if (enemy_grab_quiet > 0) {
        estun = 0;
    }
        /* Suppress orange stun ring while admin stun cheat or its residue is active. */
        if (cheat_estun == 1 || cheat_estun_mark == 1) {
        estun = 0;
    }
    if (player_grab_quiet > 0) {
        pstun = 0;
    }
    if (finished != 0) {
        efreeze = 0;
        epoison = 0;
        estun = 0;
        pfreeze = 0;
        ppoison = 0;
        pstun = 0;
    }
    freeze_a = hb_fade(freeze_a, efreeze, freeze_fade_in, freeze_fade_out);
    poison_a = hb_fade(poison_a, epoison, freeze_fade_in, freeze_fade_out);
    stun_a = hb_fade(stun_a, estun, freeze_fade_in, freeze_fade_out);
    pfreeze_a = hb_fade(pfreeze_a, pfreeze, freeze_fade_in, freeze_fade_out);
    ppoison_a = hb_fade(ppoison_a, ppoison, freeze_fade_in, freeze_fade_out);
    pstun_a = hb_fade(pstun_a, pstun, freeze_fade_in, freeze_fade_out);
}
