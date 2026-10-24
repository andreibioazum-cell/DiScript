/* Merge locally bought progress into a freshly fetched cloud profile. */
static int apply_user_json_keep_local(const char *resp) {
    int had = 0, restored = 0, dirty = cloud_dirty_get();
    int local_azum = 0, local_santa = 0, local_ebuc = 0, local_astra = 0, local_cc = 0, local_cls = 0;
    int local_al = 0, local_au = 0, local_sl = 0, local_su = 0, local_el = 0, local_eu = 0;
    int local_atl = 0, local_atu = 0;
    int local_ccl = 0, local_ccu = 0;
    int local_cups = 0, local_candies = 0, local_ol = 0, local_ou = 0, local_sk = 0;
    int local_lv = 0, local_lu = 0, local_bp = 0;
    progress_read();
    pg_lock();
    if (pg.loaded) {
        had = 1;
        local_azum = pg.azum;
        local_santa = pg.santa;
        local_ebuc = pg.ebuc;
        local_astra = pg.astra;
        local_atl = pg.astra_level;
        local_atu = pg.astra_levels_unlocked;
        local_cc = pg.codecracker;
        local_ccl = pg.codecracker_level;
        local_ccu = pg.codecracker_levels_unlocked;
        local_cls = pg.cls;
        local_al = pg.azum_level;
        local_au = pg.azum_levels_unlocked;
        local_sl = pg.santa_level;
        local_su = pg.santa_levels_unlocked;
        local_el = pg.ebuc_level;
        local_eu = pg.ebuc_levels_unlocked;
        local_cups = pg.cups;
        local_candies = pg.candies;
        local_lv = pg.level;
        local_lu = pg.levels_unlocked;
        local_bp = pg.bp_level;
        local_ol = pg.ordinary_level;
        local_ou = pg.ordinary_levels_unlocked;
        local_sk = pg.azum_skin;
    }
    pg_unlock();
    apply_user_json(resp);
    if (!had)
        return 0;
    /* A device with nothing on it (a fresh install, or the zeroed profile a
     * sign-out leaves behind) must never overwrite the account: adopt the
     * cloud copy and drop any stale "waiting for the cloud" marker. */
    int local_has_progress = local_cups > 0 || local_candies > 0 || local_azum || local_santa || local_ebuc ||
                             local_astra || local_cc || local_cls != 0 || local_bp > 0 || local_lv > 0 ||
                             local_lu > 0 || local_ol > 0 || local_ou > 0 || local_al > 0 || local_au > 0 ||
                             local_sl > 0 || local_su > 0 || local_el > 0 || local_eu > 0 || local_atl > 0 ||
                             local_atu > 0 || local_ccl > 0 || local_ccu > 0 || local_sk != 0;
    if (!local_has_progress) {
        cloud_dirty_set(0);
        return 0;
    }
    int cloud_azum, cloud_santa, cloud_ebuc, cloud_astra, cloud_cc;
    pg_lock();
    cloud_azum = pg.azum;
    cloud_santa = pg.santa;
    cloud_ebuc = pg.ebuc;
    cloud_astra = pg.astra;
    cloud_cc = pg.codecracker;

    if (dirty) {
        /* Cups are only ever spent through the shop, which writes the new
         * balance straight to the database; a smaller local number is a stale
         * copy (or a wiped device), so it may not drag the account down. */
        if (local_cups > pg.cups) {
            pg.cups = local_cups;
            restored = 1;
        }
        if (pg.candies != local_candies) {
            pg.candies = local_candies;
            restored = 1;
        }
        if (local_ou > pg.ordinary_levels_unlocked) {
            pg.ordinary_levels_unlocked = local_ou;
            restored = 1;
        }
        if (local_ol > pg.ordinary_level) {
            pg.ordinary_level = local_ol;
            restored = 1;
        }
        if (pg.azum_skin != local_sk) {
            pg.azum_skin = local_sk;
            restored = 1;
        }
    }
    if (local_azum == 1 && pg.azum == 0) {
        pg.azum = 1;
        restored = 1;
    }
    if (local_santa == 1 && pg.santa == 0) {
        pg.santa = 1;
        restored = 1;
    }
    if (local_ebuc == 1 && pg.ebuc == 0) {
        pg.ebuc = 1;
        restored = 1;
    }
    if (local_al > pg.azum_level) {
        pg.azum_level = local_al;
        restored = 1;
    }
    if (local_au > pg.azum_levels_unlocked) {
        pg.azum_levels_unlocked = local_au;
        restored = 1;
    }
    if (local_sl > pg.santa_level) {
        pg.santa_level = local_sl;
        restored = 1;
    }
    if (local_su > pg.santa_levels_unlocked) {
        pg.santa_levels_unlocked = local_su;
        restored = 1;
    }
    if (local_el > pg.ebuc_level) {
        pg.ebuc_level = local_el;
        restored = 1;
    }
    if (local_eu > pg.ebuc_levels_unlocked) {
        pg.ebuc_levels_unlocked = local_eu;
        restored = 1;
    }
    if (local_astra == 1 && pg.astra == 0) {
        pg.astra = 1;
        restored = 1;
    }
    if (local_atl > pg.astra_level) {
        pg.astra_level = local_atl;
        restored = 1;
    }
    if (local_atu > pg.astra_levels_unlocked) {
        pg.astra_levels_unlocked = local_atu;
        restored = 1;
    }
    if (codecracker_merge_local(local_cc, local_ccl, local_ccu) == 1) {
        restored = 1;
    }

    if ((local_cls > 0 && local_cls <= 4) || local_cls == 6) {
        int local_owned = 0, was_in_cloud = 0;
        if (local_cls == 1) {
            local_owned = local_azum;
            was_in_cloud = cloud_azum;
        } else if (local_cls == 2) {
            local_owned = local_santa;
            was_in_cloud = cloud_santa;
        } else if (local_cls == 4) {
            local_owned = local_astra;
            was_in_cloud = cloud_astra;
        } else if (local_cls == 6) {
            local_owned = local_cc;
            was_in_cloud = cloud_cc;
        } else {
            local_owned = local_ebuc;
            was_in_cloud = cloud_ebuc;
        }
        if (local_owned && !was_in_cloud && pg.cls != local_cls) {
            pg.cls = local_cls;
            restored = 1;
        } else if (dirty && local_owned && pg.cls != local_cls) {
            pg.cls = local_cls;
            restored = 1;
        }
    } else if (dirty && local_cls == 0 && pg.cls != 0) {
        pg.cls = 0;
        restored = 1;
    }

    if (pg.cls == 0) {
        pg.level = pg.ordinary_level;
        pg.levels_unlocked = pg.ordinary_levels_unlocked;
    } else if (pg.cls == 6) {
        pg.level = pg.codecracker_level;
        pg.levels_unlocked = pg.codecracker_levels_unlocked;
    } else if (pg.cls == 1) {
        pg.level = pg.azum_level;
        pg.levels_unlocked = pg.azum_levels_unlocked;
    } else if (pg.cls == 2) {
        pg.level = pg.santa_level;
        pg.levels_unlocked = pg.santa_levels_unlocked;
    } else if (pg.cls == 4) {
        pg.level = pg.astra_level;
        pg.levels_unlocked = pg.astra_levels_unlocked;
    } else {
        pg.level = pg.ebuc_level;
        pg.levels_unlocked = pg.ebuc_levels_unlocked;
    }
    int cups = pg.cups, candies = pg.candies, cls = pg.cls;
    int azum = pg.azum, santa = pg.santa, ebuc = pg.ebuc, lv = pg.level, lu = pg.levels_unlocked;
    int at = pg.astra, atl = pg.astra_level, atu = pg.astra_levels_unlocked;
    int cc = pg.codecracker, ccl = pg.codecracker_level, ccu = pg.codecracker_levels_unlocked;
    int ol = pg.ordinary_level, ou = pg.ordinary_levels_unlocked;
    int al = pg.azum_level, au = pg.azum_levels_unlocked;
    int sl = pg.santa_level, su = pg.santa_levels_unlocked;
    int el = pg.ebuc_level, eul = pg.ebuc_levels_unlocked;
    int bp = pg.bp_level, sk = pg.azum_skin;
    pg_unlock();
    if (restored) {
        pending_cls = cls;
        pending_level = lv;
        pending_skin = SKIN_OF_CLASS(sk, cls);
        progress_write(cups, candies, cls, azum, santa, ebuc, lv, lu, ol, ou, al, au, sl, su, el, eul, bp, sk, at, atl,
                       atu, (int)net_load_bp_xp(), ccl, ccu, cc);
        LOG("restored locally bought progress over cloud (azum=%d santa=%d ebuc=%d astra=%d codecracker=%d cls=%d)",
            azum, santa, ebuc, at, cc, cls);
        return 1;
    }

    if (cloud_dirty_get()) {
        LOG("cloud profile marked stale on device - pushing local progress");
        return 1;
    }
    return 0;
}
