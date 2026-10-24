/* Per-class level state accessors and CodeCracker level persistence. */

double net_load_astra(void) {
    double v;
    progress_read();
    pg_lock();
    v = (double)pg.astra;
    pg_unlock();
    return v;
}
double net_load_astra_level(void) {
    double v;
    progress_read();
    pg_lock();
    v = (double)pg.astra_level;
    pg_unlock();
    return v;
}
double net_load_astra_levels_unlocked(void) {
    double v;
    progress_read();
    pg_lock();
    v = (double)pg.astra_levels_unlocked;
    pg_unlock();
    return v;
}

void net_save_astra(double owned, double level, double levels_unlocked) {
    int own = owned ? 1 : 0, lv = (int)level, lu = (int)levels_unlocked;
    int c, cd, k, a, sn, eu, plv, plu, ol, ou, al, au, sl, su, el, eul, bp, sk, cc, ccl, ccu;
    normalize_level_pair(&lv, &lu);
    progress_read();
    pg_lock();
    if (pg.astra == 1)
        own = 1;
    if (pg.astra_levels_unlocked > lu) {
        if (pg.astra_level > lv)
            lv = pg.astra_level;
        lu = pg.astra_levels_unlocked;
    }
    normalize_level_pair(&lv, &lu);
    pg.astra = own;
    pg.astra_level = lv;
    pg.astra_levels_unlocked = lu;
    if (pg.cls == 4) {
        if (!own)
            pg.cls = 0;
        else {
            pg.level = lv;
            pg.levels_unlocked = lu;
        }
    }
    c = pg.cups;
    cd = pg.candies;
    k = pg.cls;
    a = pg.azum;
    sn = pg.santa;
    eu = pg.ebuc;
    plv = pg.level;
    plu = pg.levels_unlocked;
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
    cc = pg.codecracker;
    ccl = pg.codecracker_level;
    ccu = pg.codecracker_levels_unlocked;
    pg.loaded = 1;
    pg_unlock();
    if (k == 4) {
        pending_cls = k;
        pending_level = plv;
        pending_skin = 0;
    }
    progress_write(c, cd, k, a, sn, eu, plv, plu, ol, ou, al, au, sl, su, el, eul, bp, sk, own, lv, lu,
                   (int)net_load_bp_xp(), ccl, ccu, cc);
}

/* CodeCracker ownership and level accessors. */

double net_load_codecracker(void) {
    double v;
    progress_read();
    pg_lock();
    v = (double)pg.codecracker;
    pg_unlock();
    return v;
}

double net_load_codecracker_level(void) {
    double v;
    progress_read();
    pg_lock();
    v = (double)pg.codecracker_level;
    pg_unlock();
    return v;
}

double net_load_codecracker_levels_unlocked(void) {
    double v;
    progress_read();
    pg_lock();
    v = (double)pg.codecracker_levels_unlocked;
    pg_unlock();
    return v;
}

/* Save CodeCracker ownership and level state without rolling back unlocked tiers. */
void net_save_codecracker(double owned, double level, double levels_unlocked) {
    int own = owned ? 1 : 0, lv = (int)level, lu = (int)levels_unlocked;
    int c, cd, k, a, sn, eu, plv, plu, ol, ou, al, au, sl, su, el, eul, bp, sk, at, atl, atu;
    normalize_level_pair(&lv, &lu);
    progress_read();
    pg_lock();
    if (pg.codecracker == 1)
        own = 1;
    if (pg.codecracker_levels_unlocked > lu) {
        if (pg.codecracker_level > lv)
            lv = pg.codecracker_level;
        lu = pg.codecracker_levels_unlocked;
    }
    normalize_level_pair(&lv, &lu);
    pg.codecracker = own;
    pg.codecracker_level = lv;
    pg.codecracker_levels_unlocked = lu;
    if (pg.cls == 6) {
        if (!own)
            pg.cls = 0;
        else {
            pg.level = lv;
            pg.levels_unlocked = lu;
        }
    }
    c = pg.cups;
    cd = pg.candies;
    k = pg.cls;
    a = pg.azum;
    sn = pg.santa;
    eu = pg.ebuc;
    plv = pg.level;
    plu = pg.levels_unlocked;
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
    at = pg.astra;
    atl = pg.astra_level;
    atu = pg.astra_levels_unlocked;
    pg.loaded = 1;
    pg_unlock();
    if (k == 6) {
        pending_cls = k;
        pending_level = plv;
        pending_skin = 0;
    }
    progress_write(c, cd, k, a, sn, eu, plv, plu, ol, ou, al, au, sl, su, el, eul, bp, sk, at, atl, atu,
                   (int)net_load_bp_xp(), lv, lu, own);
}

/* Merge local CodeCracker ownership and unlocked levels under pg lock. */
int codecracker_merge_local(int local_owned, int local_level, int local_unlocked) {
    int changed = 0;
    if (local_owned == 1 && pg.codecracker == 0) {
        pg.codecracker = 1;
        changed = 1;
    }
    if (local_level > pg.codecracker_level) {
        pg.codecracker_level = local_level;
        changed = 1;
    }
    if (local_unlocked > pg.codecracker_levels_unlocked) {
        pg.codecracker_levels_unlocked = local_unlocked;
        changed = 1;
    }
    return changed;
}
