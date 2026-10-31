/* Poll global server flags (AzumBuff, FlashSale, Christmas) with TTL retention and timeout recovery. */
#define LIVE_FLAG_POLL_MS 3000
#define LIVE_FLAG_TTL_MS 30000
#define LIVE_FLAG_FETCH_TIMEOUT_MS 10000

typedef struct {
    const char *path;
    int busy, enabled;
    unsigned epoch, fetch_epoch;
    long long next_poll, valid_until, busy_since;
} LiveFlag;

static struct {
    DSMutex lock;
    LiveFlag azum;
    LiveFlag sale;
    LiveFlag xmas;
    long long link_at;   /* last time any Firebase flag answer arrived */
    int link_last_ok;    /* 1 if that last answer was HTTP 200 */
/* Initialize all fields explicitly to satisfy -Wmissing-field-initializers. */
} live_flags = {.lock = DS_MUTEX_INIT,
                .azum = {"/AzumBuff.json", 0, 0, 0, 0, 0, 0, 0},
                .sale = {"/FlashSale.json", 0, 0, 0, 0, 0, 0, 0},
                .xmas = {"/Christmas.json", 0, 0, 0, 0, 0, 0, 0},
                .link_at = 0, .link_last_ok = 0};

static int live_flag_value(const char *json) {
    char *end;
    double value;
    if (!json)
        return 0;
    while (*json == ' ' || *json == '\t' || *json == '\r' || *json == '\n')
        json++;
    if (!strcmp(json, "true") || !strcmp(json, "\"true\""))
        return 1;
    if (!strcmp(json, "false") || !strcmp(json, "\"false\"") || !strcmp(json, "null"))
        return 0;
    if (*json == '{') {
        /* Accept single-field object wrappers around scalar flag values. */
        json = strchr(json, ':');
        if (!json)
            return 0;
        return live_flag_value(json + 1);
    }
    if (*json == '"')
        json++;
    value = strtod(json, &end);
    if (end == json)
        return 0;
    if (*end == '"')
        end++;
    while (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')
        end++;
    return value == 1 && !*end;
}

static void *live_flag_fetch(void *arg) {
    LiveFlag *flag = (LiveFlag *)arg;
    unsigned epoch;
    char url[160], resp[64] = "";
    int code;
    ds_mutex_lock(live_flags.lock);
    epoch = flag->fetch_epoch;
    ds_mutex_unlock(live_flags.lock);
    snprintf(url, sizeof(url), "%s%s", NET_FIREBASE_URL, flag->path);
    code = fb_http("GET", url, NULL, resp, sizeof(resp));
    ds_mutex_lock(live_flags.lock);
    if (epoch == flag->epoch) {
        if (code == 200) {
            flag->enabled = live_flag_value(resp);
            flag->valid_until = now_ms() + LIVE_FLAG_TTL_MS;
            live_flags.link_at = now_ms();
            live_flags.link_last_ok = 1;
        } else {
            /* Offline (or server down) means the ordinary game: no New Year,
             * no AzumBuff, no FlashSale. A cached 1 must not survive a
             * failed request, otherwise the holiday sticks for 30 seconds. */
            flag->enabled = 0;
            flag->valid_until = 0;
            live_flags.link_at = now_ms();
            live_flags.link_last_ok = 0;
        }
        flag->busy = 0;
    }
    ds_mutex_unlock(live_flags.lock);
    return NULL;
}

/* Caller holds live_flags.lock. Starts a fetch when one is due (or forced). */
static void live_flag_poll(LiveFlag *flag, long long now, int force) {
    DSThread t;
    if (flag->busy && now - flag->busy_since > LIVE_FLAG_FETCH_TIMEOUT_MS) {
        /* The old thread is stuck; its late reply must not count. */
        flag->epoch++;
        flag->busy = 0;
    }
    if (flag->busy || (!force && now < flag->next_poll))
        return;
    flag->busy = 1;
    flag->busy_since = now;
    flag->next_poll = now + LIVE_FLAG_POLL_MS;
    flag->fetch_epoch = flag->epoch;
    t = ds_thread_start(live_flag_fetch, flag);
    if (t)
        ds_thread_detach(t);
    else {
        flag->busy = 0;
        flag->enabled = 0;
        flag->valid_until = 0;
    }
}

static double live_flag_get(LiveFlag *flag, int force) {
    long long now = now_ms();
    double active;
    /* Read publicly accessible root flags regardless of login state. */
    ds_mutex_lock(live_flags.lock);
    if (force)
        flag->valid_until = 0;
    live_flag_poll(flag, now, force);
    active = flag->enabled && now < flag->valid_until;
    ds_mutex_unlock(live_flags.lock);
    return active;
}

double net_azum_buff(void) { return live_flag_get(&live_flags.azum, 0); }

double net_flash_sale(void) { return live_flag_get(&live_flags.sale, 0); }

/* Christmas: 1 enables the holiday theme and snowballs for everyone. */
double net_christmas(void) { return live_flag_get(&live_flags.xmas, 0); }

/* 1 while the flag poller still hears from Firebase. Everything driven by
 * server-side switches (holiday, buffs, sales, events, banner) falls back to
 * its ordinary state when the Internet is switched off. */
double net_flags_reachable(void) {
    long long now = now_ms(), at;
    int ok;
    ds_mutex_lock(live_flags.lock);
    at = live_flags.link_at;
    ok = live_flags.link_last_ok;
    ds_mutex_unlock(live_flags.lock);
    /* The most recent poll must have succeeded and be fresh: one failed
     * request (no Internet) is enough to fall back to the ordinary game. */
    return ok == 1 && at > 0 && now - at <= LIVE_FLAG_TTL_MS ? 1 : 0;
}

/* Invalidate cached flags and trigger immediate server refresh. */
void net_live_flags_refresh(void) {
    (void)live_flag_get(&live_flags.azum, 1);
    (void)live_flag_get(&live_flags.sale, 1);
    (void)live_flag_get(&live_flags.xmas, 1);
}
