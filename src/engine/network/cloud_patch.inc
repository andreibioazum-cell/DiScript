static int http_patch_once(HttpJob *j) {
    char sealed[BODY * 3];
    int code;
    snprintf(sealed, sizeof(sealed), "%s", j->body);
    if (fb_enabled()) {
        fb_ensure_token();
        fb_url(j->url, sizeof(j->url), j->plain);
        /* Attach write secret required by database rules. */
        net_seal_body(sealed, sizeof(sealed));
        code = http("PATCH", j->url, sealed, NULL, 0);
        if (code == 401 && fb_refresh_token_now()) {
            fb_url(j->url, sizeof(j->url), j->plain);
            code = http("PATCH", j->url, sealed, NULL, 0);
        }
    } else {
        code = http("PATCH", j->url, sealed, NULL, 0);
    }
    return code;
}

/* Advance cloud cups and candies in +1000 steps to satisfy per-write rule caps. */
static int http_patch_step_balances(HttpJob *j) {
    char resp[RESP];
    int cur_c, cur_cd, want_c, want_cd, guard = 0;
    if (!j->progress || fb_http("GET", j->plain, NULL, resp, sizeof(resp)) != 200)
        return 0;
    cur_c = num_int(resp, "cups", -1);
    cur_cd = num_int(resp, "candies", -1);
    want_c = num_int(j->body, "cups", 0);
    want_cd = num_int(j->body, "candies", 0);
    if (cur_c < 0 || cur_cd < 0 || (want_c <= cur_c + 1000 && want_cd <= cur_cd + 1000))
        return 0;
    while ((want_c > cur_c + 1000 || want_cd > cur_cd + 1000) && guard++ < 100) {
        HttpJob step;
        cur_c = want_c > cur_c + 1000 ? cur_c + 1000 : want_c;
        cur_cd = want_cd > cur_cd + 1000 ? cur_cd + 1000 : want_cd;
        snprintf(step.plain, sizeof(step.plain), "%s", j->plain);
        snprintf(step.url, sizeof(step.url), "%s", j->plain);
        snprintf(step.body, sizeof(step.body), "{\"cups\":%d,\"candies\":%d}", cur_c, cur_cd);
        step.progress = 0;
        if (http_patch_once(&step) != 200)
            return 0;
    }
    return http_patch_once(j) == 200;
}

#define PATCH_RETRIES 4

static DSMutex patch_seq_lock = DS_MUTEX_INIT;
static int patch_seq = 0;
static void *http_patch_job(void *arg) {
    HttpJob *j = (HttpJob *)arg;
    if (!j)
        return NULL;
    int code = 0, attempt = 0, wait = 1000;
    for (;;) {
        code = http_patch_once(j);
        if (code == 200)
            break;
        int transient = (code == 0 || code == 429 || code >= 500);
        if (!transient || ++attempt >= PATCH_RETRIES)
            break;
        sleep_ms(wait);
        if (wait < 4000)
            wait *= 2;
    }
    if (code != 200) {
        if (j->progress)
            cloud_dirty_set(1);
        if (net_log_ok())
            LOGERR("cloud PATCH failed: HTTP %d%s", code,
                   j->progress ? " (progress will be resent on next login)" : "");
    } else {

        if (j->progress) {
            int latest;
            ds_mutex_lock(patch_seq_lock);
            latest = (j->progress == patch_seq);
            ds_mutex_unlock(patch_seq_lock);
            if (latest)
                cloud_dirty_set(0);
        }
        if (attempt > 0)
            LOG("cloud PATCH ok after %d retry(ies)", attempt);
    }
    free(j);
    return NULL;
}
static void http_patch_async_ex(const char *url, const char *body, int progress) {
    HttpJob *j = (HttpJob *)malloc(sizeof(*j));
    DSThread t;
    if (!j)
        return;
    snprintf(j->plain, sizeof(j->plain), "%s", url);
    snprintf(j->url, sizeof(j->url), "%s", url);
    snprintf(j->body, sizeof(j->body), "%s", body);
    j->progress = 0;
    if (progress) {
        ds_mutex_lock(patch_seq_lock);
        j->progress = ++patch_seq;
        ds_mutex_unlock(patch_seq_lock);
    }
    t = ds_thread_start(http_patch_job, j);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    if (progress)
        cloud_dirty_set(1);
    free(j);
}
static void http_patch_async(const char *url, const char *body) {
    http_patch_async_ex(url, body, 0);
}

/* Serialize profile PATCH requests on one worker so latest snapshot always wins. */
static DSMutex progress_sync_lock = DS_MUTEX_INIT;
static char progress_sync_url[URL];
static char progress_sync_body[BODY * 3];
static int progress_sync_pending = 0;
static int progress_sync_running = 0;

static void *progress_sync_worker(void *arg) {
    (void)arg;
    for (;;) {
        char url[URL], body[BODY * 3];
        ds_mutex_lock(progress_sync_lock);
        if (!progress_sync_pending) {
            progress_sync_running = 0;
            ds_mutex_unlock(progress_sync_lock);
            break;
        }
        progress_sync_pending = 0;
        snprintf(url, sizeof(url), "%s", progress_sync_url);
        snprintf(body, sizeof(body), "%s", progress_sync_body);
        ds_mutex_unlock(progress_sync_lock);

        HttpJob j;
        snprintf(j.plain, sizeof(j.plain), "%s", url);
        snprintf(j.url, sizeof(j.url), "%s", url);
        snprintf(j.body, sizeof(j.body), "%s", body);
        j.progress = 1;

        int code = 0, attempt = 0, wait = 1000;
        for (;;) {
            code = http_patch_once(&j);
            if ((code == 400 || code == 403) && http_patch_step_balances(&j))
                code = 200;
            if (code == 200)
                break;
            int transient = (code == 0 || code == 429 || code >= 500);
            if (!transient || ++attempt >= PATCH_RETRIES)
                break;
            sleep_ms(wait);
            if (wait < 4000)
                wait *= 2;
        }

        ds_mutex_lock(progress_sync_lock);
        int superseded = progress_sync_pending;
        ds_mutex_unlock(progress_sync_lock);

        if (code != 200) {
            cloud_dirty_set(1);
            if (net_log_ok())
                LOGERR("cloud PATCH failed: HTTP %d (progress will be resent on next login)", code);
        } else if (!superseded) {
            /* Clear dirty flag only when no newer snapshot was queued mid-flight. */
            cloud_dirty_set(0);
        } else if (attempt > 0) {
            LOG("cloud PATCH ok after %d retry(ies)", attempt);
        }
    }
    return NULL;
}

static void queue_progress_patch(const char *url, const char *body) {
    int need_start;
    ds_mutex_lock(progress_sync_lock);
    snprintf(progress_sync_url, sizeof(progress_sync_url), "%s", url);
    snprintf(progress_sync_body, sizeof(progress_sync_body), "%s", body);
    progress_sync_pending = 1;
    need_start = !progress_sync_running;
    if (need_start)
        progress_sync_running = 1;
    ds_mutex_unlock(progress_sync_lock);
    if (!need_start)
        return;
    DSThread t = ds_thread_start(progress_sync_worker, NULL);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    ds_mutex_lock(progress_sync_lock);
    progress_sync_running = 0;
    ds_mutex_unlock(progress_sync_lock);
    cloud_dirty_set(1);
}

void net_progress_flush(void) {
    int waited = 0;
    for (;;) {
        int busy;
        ds_mutex_lock(progress_sync_lock);
        busy = progress_sync_running || progress_sync_pending;
        ds_mutex_unlock(progress_sync_lock);
        if (!busy || waited >= 1500)
            break;
        sleep_ms(15);
        waited += 15;
    }
}

static DSMutex ach_lock = DS_MUTEX_INIT;
static int ach_loaded = 0;
static unsigned int ach_flags = 0;
static unsigned int ach_revives = 0;

void net_save_progress_all(double cups, double candies, double cls, double azum, double santa, double ebuc,
                           double level, double levels_unlocked, double ordinary_level, double ordinary_levels_unlocked,
                           double azum_level, double azum_levels_unlocked, double santa_level,
                           double santa_levels_unlocked, double ebuc_level, double ebuc_levels_unlocked,
                           double bp_level, double azum_skin) {
    int c = (int)cups, cd = (int)candies, k = (int)cls, a = azum ? 1 : 0, sn = santa ? 1 : 0, eu = ebuc ? 1 : 0;
    int lv = (int)level, lu = (int)levels_unlocked;
    int ol = (int)ordinary_level, ou = (int)ordinary_levels_unlocked;
    int al = (int)azum_level, au = (int)azum_levels_unlocked;
    int sl = (int)santa_level, su = (int)santa_levels_unlocked;
    int el = (int)ebuc_level, eul = (int)ebuc_levels_unlocked;
    int bp = (int)bp_level, sk = (int)azum_skin;
    int wheel_spins, wheel_wins;

    int at = 0, atl = 0, atu = 0;
    int cc = 0, ccl = 0, ccu = 0;
    int legacy_astra_class = k == 5;
    if (sk < 0)
        sk = 0;
    if (sk > SKIN_FLAGS_MAX)
        sk = SKIN_FLAGS_MAX;
    if (c < 0)
        c = 0;
    if (cd < 0)
        cd = 0;
    if (k == 5)
        k = 4;
    if (legacy_astra_class) {
        at = 1;
        atl = lv;
        atu = lu;
    }
    if (k != 0 && k != 1 && k != 2 && k != 3 && k != 4 && k != 6)
        k = 0;

    progress_read();
    pg_lock();
    wheel_spins = pg.wheel_spins;
    wheel_wins = pg.wheel_wins;
    if (pg.loaded) {
        if (pg.azum == 1)
            a = 1;
        if (pg.santa == 1)
            sn = 1;
        if (pg.ebuc == 1)
            eu = 1;

        if (pg.ordinary_levels_unlocked > ou) {
            if (pg.ordinary_level > ol)
                ol = pg.ordinary_level;
            ou = pg.ordinary_levels_unlocked;
        }
        if (pg.azum_levels_unlocked > au) {
            if (pg.azum_level > al)
                al = pg.azum_level;
            au = pg.azum_levels_unlocked;
        }
        if (pg.santa_levels_unlocked > su) {
            if (pg.santa_level > sl)
                sl = pg.santa_level;
            su = pg.santa_levels_unlocked;
        }
        if (pg.ebuc_levels_unlocked > eul) {
            if (pg.ebuc_level > el)
                el = pg.ebuc_level;
            eul = pg.ebuc_levels_unlocked;
        }
        at = pg.astra ? 1 : 0;
        atl = pg.astra_level;
        atu = pg.astra_levels_unlocked;
        cc = pg.codecracker ? 1 : 0;
        ccl = pg.codecracker_level;
        ccu = pg.codecracker_levels_unlocked;
    }
    pg_unlock();
    normalize_level_pair(&atl, &atu);
    normalize_level_pair(&ccl, &ccu);
    if (k == 1 && !a)
        k = 0;
    if (k == 2 && !sn)
        k = 0;
    if (k == 3 && !eu)
        k = 0;
    if (k == 4 && !at)
        k = 0;
    if (k == 6 && !cc)
        k = 0;
    if (bp < 0)
        bp = 0;
    if (bp > BP_MAX)
        bp = BP_MAX;
    normalize_level_pair(&lv, &lu);
    normalize_level_pair(&ol, &ou);
    normalize_level_pair(&al, &au);
    normalize_level_pair(&sl, &su);
    normalize_level_pair(&el, &eul);
    if (k == 0) {
        lv = ol;
        lu = ou;
    } else if (k == 6) {
        lv = ccl;
        lu = ccu;
    } else if (k == 1) {
        lv = al;
        lu = au;
    } else if (k == 2) {
        lv = sl;
        lu = su;
    } else if (k == 4) {
        lv = atl;
        lu = atu;
    } else {
        lv = el;
        lu = eul;
    }
    pg_lock();
    pg.cups = c;
    pg.candies = cd;
    pg.cls = k;
    pg.azum = a;
    pg.santa = sn;
    pg.ebuc = eu;
    pg.level = lv;
    pg.levels_unlocked = lu;
    pg.loaded = 1;
    pg.ordinary_level = ol;
    pg.ordinary_levels_unlocked = ou;
    pg.azum_level = al;
    pg.azum_levels_unlocked = au;
    pg.santa_level = sl;
    pg.santa_levels_unlocked = su;
    pg.ebuc_level = el;
    pg.ebuc_levels_unlocked = eul;
    pg.bp_level = bp;
    if (wheel_spins < 0) wheel_spins = 0;
    if (wheel_spins > 999) wheel_spins = 999;
    if (wheel_wins < 0) wheel_wins = 0;
    if (wheel_wins > 5) wheel_wins = 5;
    pg.wheel_spins = wheel_spins;
    pg.wheel_wins = wheel_wins;
    pg.azum_skin = sk;
    pg.astra = at;
    pg.astra_level = atl;
    pg.astra_levels_unlocked = atu;
    pg.codecracker = cc;
    pg.codecracker_level = ccl;
    pg.codecracker_levels_unlocked = ccu;
    pg_unlock();
    pending_cls = k;
    pending_level = lv;
    pending_skin = SKIN_OF_CLASS(sk, k);
    progress_write(c, cd, k, a, sn, eu, lv, lu, ol, ou, al, au, sl, su, el, eul, bp, sk, at, atl, atu,
                   (int)net_load_bp_xp(), ccl, ccu, cc, wheel_spins, wheel_wins);

    char nick[LOGIN_NICK_MAX + 1] = "";
    lg_lock();
    if (lg.status == NET_LOGIN_OK && lg.session_nick[0]) {
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    }
    lg_unlock();
    if (nick[0] && net.base[0]) {
        char url[URL], body[BODY * 3];

        unsigned int af = 0, ar = 0;
        achievements_read();
        ds_mutex_lock(ach_lock);
        af = ach_flags;
        ar = ach_revives;
        ds_mutex_unlock(ach_lock);
        snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
        snprintf(body, sizeof(body),
                 "{\"cups\":%d,\"candies\":%d,\"cls\":%d,\"azum\":%d,\"santa\":%d,\"ebuc\":%d,\"astra\":%d,"
                 "\"codecracker\":%d,\"level\":%d,\"levels\":%d,\"ordinary_level\":%d,\"ordinary_levels\":%d,"
                 "\"azum_level\":%d,\"azum_levels\":%d,\"santa_level\":%d,\"santa_levels\":%d,\"ebuc_level\":%d,"
                 "\"ebuc_levels\":%d,\"astra_level\":%d,\"astra_levels\":%d,\"codecracker_level\":%d,"
                 "\"codecracker_levels\":%d,\"bp\":%d,\"bpx\":%d,\"azum_skin\":%d,\"wheel_spins\":%d,\"wheel_wins\":%d,\"ach_flags\":%u,\"ach_revives\":%u}",
                 c, cd, k, a, sn, eu, at, cc, lv, lu, ol, ou, al, au, sl, su, el, eul, atl, atu, ccl, ccu, bp,
                 (int)net_load_bp_xp(), sk, wheel_spins, wheel_wins, af, ar);

        cloud_dirty_set(1);
        queue_progress_patch(url, body);
    }
    /* Leave cloud_dirty unchanged when signed out. */
}
