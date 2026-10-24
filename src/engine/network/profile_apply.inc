static void apply_user_json(const char *resp) {
    int legacy_astra_class = 0;
    int cups = num_int(resp, "cups", 0);
    int candies = num_int(resp, "candies", 0);
    int cls = num_int(resp, "cls", 0);
    legacy_astra_class = cls == 5;
    int azum = num_int(resp, "azum", 0);
    int santa = num_int(resp, "santa", 0);
    int ebuc = num_int(resp, "ebuc", 0);
    int level = num_int(resp, "level", 0);
    int levels_unlocked = num_int(resp, "levels", -1);
    int ordinary_level = num_int(resp, "ordinary_level", -1);
    int ordinary_levels_unlocked = num_int(resp, "ordinary_levels", -1);
    int azum_level = num_int(resp, "azum_level", -1);
    int azum_levels_unlocked = num_int(resp, "azum_levels", -1);
    int santa_level = num_int(resp, "santa_level", -1);
    int santa_levels_unlocked = num_int(resp, "santa_levels", -1);
    int ebuc_level = num_int(resp, "ebuc_level", -1);
    int ebuc_levels_unlocked = num_int(resp, "ebuc_levels", -1);

    int astra = num_int(resp, "astra", 0);
    int astra_level = num_int(resp, "astra_level", -1);
    int astra_levels_unlocked = num_int(resp, "astra_levels", -1);
    int codecracker = num_int(resp, "codecracker", 0);
    int codecracker_level = num_int(resp, "codecracker_level", -1);
    int codecracker_levels_unlocked = num_int(resp, "codecracker_levels", -1);
    int bp_level = num_int(resp, "bp", 0);
    int bp_xp_v = num_int(resp, "bpx", -1);
    int azum_skin = num_int(resp, "azum_skin", 0);
    if (levels_unlocked < 0)
        levels_unlocked = level;
    if (cups < 0)
        cups = 0;
    if (candies < 0)
        candies = 0;
    if (cls == 5)
        cls = 4;
    if (cls != 0 && cls != 1 && cls != 2 && cls != 3 && cls != 4 && cls != 6)
        cls = 0;
    azum = azum ? 1 : 0;
    santa = santa ? 1 : 0;
    ebuc = ebuc ? 1 : 0;
    astra = astra ? 1 : 0;
    codecracker = codecracker ? 1 : 0;
    if (legacy_astra_class)
        astra = 1;
    if (cls == 1 && !azum)
        cls = 0;
    if (cls == 2 && !santa)
        cls = 0;
    if (cls == 3 && !ebuc)
        cls = 0;
    if (cls == 4 && !astra)
        cls = 0;
    if (cls == 6 && !codecracker)
        cls = 0;
    if (astra_level < 0)
        astra_level = 0;
    if (astra_levels_unlocked < 0)
        astra_levels_unlocked = astra_level;
    if (codecracker_level < 0)
        codecracker_level = 0;
    if (codecracker_levels_unlocked < 0)
        codecracker_levels_unlocked = codecracker_level;
    if (ordinary_level < 0 && ordinary_levels_unlocked < 0 && azum_level < 0 && azum_levels_unlocked < 0 &&
        santa_level < 0 && santa_levels_unlocked < 0 && ebuc_level < 0 && ebuc_levels_unlocked < 0) {
        ordinary_level = ordinary_levels_unlocked = 0;
        azum_level = azum_levels_unlocked = 0;
        santa_level = santa_levels_unlocked = 0;
        ebuc_level = ebuc_levels_unlocked = 0;
        if (legacy_astra_class) {
            astra_level = level;
            astra_levels_unlocked = levels_unlocked;
        } else if (cls == 1 && azum) {
            azum_level = level;
            azum_levels_unlocked = levels_unlocked;
        } else if (cls == 2 && santa) {
            santa_level = level;
            santa_levels_unlocked = levels_unlocked;
        } else if (cls == 3 && ebuc) {
            ebuc_level = level;
            ebuc_levels_unlocked = levels_unlocked;
        } else if (cls == 4 && astra) {
            astra_level = level;
            astra_levels_unlocked = levels_unlocked;
        } else if (cls == 6 && codecracker) {
            codecracker_level = level;
            codecracker_levels_unlocked = levels_unlocked;
        } else {
            ordinary_level = level;
            ordinary_levels_unlocked = levels_unlocked;
        }
    } else {
        if (ordinary_level < 0)
            ordinary_level = 0;
        if (ordinary_levels_unlocked < 0)
            ordinary_levels_unlocked = ordinary_level;
        if (azum_level < 0)
            azum_level = 0;
        if (azum_levels_unlocked < 0)
            azum_levels_unlocked = azum_level;
        if (santa_level < 0)
            santa_level = 0;
        if (santa_levels_unlocked < 0)
            santa_levels_unlocked = santa_level;
        if (ebuc_level < 0)
            ebuc_level = 0;
        if (ebuc_levels_unlocked < 0)
            ebuc_levels_unlocked = ebuc_level;
    }
    normalize_level_pair(&level, &levels_unlocked);
    normalize_level_pair(&ordinary_level, &ordinary_levels_unlocked);
    normalize_level_pair(&azum_level, &azum_levels_unlocked);
    normalize_level_pair(&santa_level, &santa_levels_unlocked);
    normalize_level_pair(&ebuc_level, &ebuc_levels_unlocked);
    normalize_level_pair(&astra_level, &astra_levels_unlocked);
    normalize_level_pair(&codecracker_level, &codecracker_levels_unlocked);
    if (cls == 0) {
        level = ordinary_level;
        levels_unlocked = ordinary_levels_unlocked;
    } else if (cls == 6) {
        level = codecracker_level;
        levels_unlocked = codecracker_levels_unlocked;
    } else if (cls == 1) {
        level = azum_level;
        levels_unlocked = azum_levels_unlocked;
    } else if (cls == 2) {
        level = santa_level;
        levels_unlocked = santa_levels_unlocked;
    } else if (cls == 4) {
        level = astra_level;
        levels_unlocked = astra_levels_unlocked;
    } else {
        level = ebuc_level;
        levels_unlocked = ebuc_levels_unlocked;
    }
    if (bp_level < 0)
        bp_level = 0;
    if (bp_level > BP_MAX)
        bp_level = BP_MAX;
    if (azum_skin < 0)
        azum_skin = 0;
    if (azum_skin > SKIN_FLAGS_MAX)
        azum_skin = SKIN_FLAGS_MAX;
    pg_lock();
    pg.cups = cups;
    pg.candies = candies;
    pg.cls = cls;
    pg.azum = azum;
    pg.santa = santa;
    pg.ebuc = ebuc;
    pg.level = level;
    pg.levels_unlocked = levels_unlocked;
    pg.ordinary_level = ordinary_level;
    pg.ordinary_levels_unlocked = ordinary_levels_unlocked;
    pg.azum_level = azum_level;
    pg.azum_levels_unlocked = azum_levels_unlocked;
    pg.santa_level = santa_level;
    pg.santa_levels_unlocked = santa_levels_unlocked;
    pg.ebuc_level = ebuc_level;
    pg.ebuc_levels_unlocked = ebuc_levels_unlocked;
    pg.bp_level = bp_level;
    if (bp_xp_v > pg.bp_xp)
        pg.bp_xp = bp_xp_v;
    pg.azum_skin = azum_skin;
    pg.astra = astra;
    pg.astra_level = astra_level;
    pg.astra_levels_unlocked = astra_levels_unlocked;
    pg.codecracker = codecracker;
    pg.codecracker_level = codecracker_level;
    pg.codecracker_levels_unlocked = codecracker_levels_unlocked;
    pg.loaded = 1;
    pg_unlock();
    pending_cls = cls;
    pending_level = level;
    pending_skin = SKIN_OF_CLASS(azum_skin, cls);
    {
        int bpx_keep = bp_xp_v > 0 ? bp_xp_v : 0;
        pg_lock();
        if (pg.bp_xp > bpx_keep)
            bpx_keep = pg.bp_xp;
        pg_unlock();
        progress_write(cups, candies, cls, azum, santa, ebuc, level, levels_unlocked, ordinary_level,
                       ordinary_levels_unlocked, azum_level, azum_levels_unlocked, santa_level, santa_levels_unlocked,
                       ebuc_level, ebuc_levels_unlocked, bp_level, azum_skin, astra, astra_level,
                       astra_levels_unlocked, bpx_keep, codecracker_level, codecracker_levels_unlocked, codecracker);
    }

    int cloud_ach_flags = num_int(resp, "ach_flags", -1);
    int cloud_ach_revives = num_int(resp, "ach_revives", -1);
    if (cloud_ach_flags >= 0) {
        ds_mutex_lock(ach_lock);

        unsigned int local_f = ach_flags;
        unsigned int local_r = ach_revives;
        int loaded_local = ach_loaded;
        ds_mutex_unlock(ach_lock);
        unsigned int cf = (unsigned int)cloud_ach_flags;
        unsigned int cr = cloud_ach_revives >= 0 ? (unsigned int)cloud_ach_revives : 0;
        int cloud_is_newer = !loaded_local || (cf | local_f) != local_f || cr > local_r;
        if (cloud_is_newer) {
            ds_mutex_lock(ach_lock);
            ach_flags = cf | ach_flags;
            if (cr > ach_revives)
                ach_revives = cr;
            unsigned int wf = ach_flags, wr = ach_revives;
            ach_loaded = 1;
            ds_mutex_unlock(ach_lock);
            achievements_write(wf, wr);
            LOG("cloud achievements merged: flags=%u revives=%u", wf, wr);
        }
    }

    settings_sync_with_cloud(resp);
}

static void push_pg_to_cloud(void) {
    int cups, candies, cls, azum, santa, ebuc, lv, lu;
    int ol, ou, al, au, sl, su, el, eul, bp, sk;
    pg_lock();
    cups = pg.cups;
    candies = pg.candies;
    cls = pg.cls;
    azum = pg.azum;
    santa = pg.santa;
    ebuc = pg.ebuc;
    lv = pg.level;
    lu = pg.levels_unlocked;
    ol = pg.ordinary_level;
    ou = pg.ordinary_levels_unlocked;
    al = pg.azum_level;
    au = pg.azum_levels_unlocked;
    sl = pg.santa_level;
    su = pg.santa_levels_unlocked;
    el = pg.ebuc_level;
    eul = pg.ebuc_levels_unlocked;
    bp = pg.bp_level;
    sk = pg.azum_skin;
    pg_unlock();
    net_save_progress_all(cups, candies, cls, azum, santa, ebuc, lv, lu, ol, ou, al, au, sl, su, el, eul, bp, sk);
}
