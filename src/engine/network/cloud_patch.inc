static int http_patch_once(HttpJob *j) {
    int code;
    if (fb_enabled()) {
        fb_ensure_token();
        fb_url(j->url, sizeof(j->url), j->plain);
                /* The database accepts a write only with the "w" field (see
                 * firebase.rules.json). */
        net_seal_body(j->body, sizeof(j->body));
        code = http("PATCH", j->url, j->body, NULL, 0);
        if (code == 401 && fb_refresh_token_now()) {
            fb_url(j->url, sizeof(j->url), j->plain);
            code = http("PATCH", j->url, j->body, NULL, 0);
        }
    } else {
        code = http("PATCH", j->url, j->body, NULL, 0);
    }
    return code;
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

/*
 * Progress (cups/profile) writes used to be fired as independent detached
 * threads, each with its own retry/backoff loop. When several progress
 * updates happened in quick succession (e.g. a quest reward immediately
 * followed by a battle win), the resulting PATCH requests raced each other
 * over the network. Retries/backoff meant they could finish out of order,
 * so an *older*, smaller cups value could land on the server *after* a
 * newer one - leaving the leaderboard showing a stale (lower) cups count
 * even though the local profile already had the correct, higher value.
 *
 * To fix this we serialize progress PATCHes through a single worker that
 * always sends the most recently queued snapshot. If a newer update is
 * queued while a request is in flight, the worker simply sends that newer
 * snapshot next - it never lets an older write "win" over a newer one.
 */
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
            /* Only clear the dirty flag if nothing newer was queued while
             * this request was in flight; otherwise the loop above will
             * pick up the newer snapshot on its next iteration. */
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
    if (pg.loaded) {
        if (pg.azum == 1)
            a = 1;
        if (pg.santa == 1)
            sn = 1;
        if (pg.ebuc == 1)
            eu = 1;

        if (pg.ordinary_levels_unlocked > ou)
            ou = pg.ordinary_levels_unlocked;
        if (pg.azum_levels_unlocked > au)
            au = pg.azum_levels_unlocked;
        if (pg.santa_levels_unlocked > su)
            su = pg.santa_levels_unlocked;
        if (pg.ebuc_levels_unlocked > eul)
            eul = pg.ebuc_levels_unlocked;
        if (pg.ordinary_level > ol)
            ol = pg.ordinary_level;
        if (pg.azum_level > al)
            al = pg.azum_level;
        if (pg.santa_level > sl)
            sl = pg.santa_level;
        if (pg.ebuc_level > el)
            el = pg.ebuc_level;
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
                   (int)net_load_bp_xp(), ccl, ccu, cc);

    char nick[LOGIN_NICK_MAX + 1] = "";
    lg_lock();
    if (lg.status == NET_LOGIN_OK && lg.session_nick[0]) {
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    }
    lg_unlock();
    if (nick[0] && net.base[0]) {
        char url[URL], body[BODY * 3];

        unsigned int af = 0, ar = 0;
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
                 "\"codecracker_levels\":%d,\"bp\":%d,\"bpx\":%d,\"azum_skin\":%d,\"ach_flags\":%u,\"ach_revives\":%u}",
                 c, cd, k, a, sn, eu, at, cc, lv, lu, ol, ou, al, au, sl, su, el, eul, atl, atu, ccl, ccu, bp,
                 (int)net_load_bp_xp(), sk, af, ar);

        cloud_dirty_set(1);
        queue_progress_patch(url, body);
    }
    /* No session: the save above only touched the device. Marking it dirty
     * here is what used to wipe accounts: a sign-out zeroes the local profile,
     * the marker survives on disk, and the next login pushes those zeros into
     * the cloud (the merge below trusts a "dirty" device). */
}
