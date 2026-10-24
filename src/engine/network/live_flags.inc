/* Live operator flags at the database root: AzumBuff and FlashSale.
 * Account-independent global events: read by every client, logged in or
 * not; the rules make the two nodes publicly readable (admins keep the
 * only write access). Temporary: never persisted to the profile or disk.
 * Each flag is enabled only by a fresh successful read of its own node
 * (number 1; true and "1" are also accepted, since those are what the
 * Firebase console tends to store). One failed read no longer kills a flag
 * that was on: the last good value survives until its TTL runs out, so a
 * single dropped request cannot switch the Azum buff off mid-fight or make
 * shop prices flicker. A hung request cannot wedge a flag either: after
 * LIVE_FLAG_FETCH_TIMEOUT_MS its reply is ignored and a new fetch starts. */
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
/* Every field is listed on purpose: clang -Wmissing-field-initializers
 * (in -Wextra, promoted by -Werror in CI) rejects partial initializers. */
} live_flags = {.lock = DS_MUTEX_INIT,
                .azum = {"/AzumBuff.json", 0, 0, 0, 0, 0, 0, 0},
                .sale = {"/FlashSale.json", 0, 0, 0, 0, 0, 0, 0}};

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
        /* Tolerate {"FlashSale":1}-shaped values some consoles produce. */
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
        }
        /* On an error the previous value and its deadline stay untouched. */
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
    /* Deliberately NOT gated on login: these are global event flags, and
     * gating them on the session made them silently vanish for guests and
     * for accounts whose session was not (yet) in NET_LOGIN_OK state -
     * while admins, who can read the whole database root, always saw
     * them. The Firebase nodes are publicly readable for the same reason;
     * only writing them stays admin-only. */
    ds_mutex_lock(live_flags.lock);
    if (force)
        flag->valid_until = 0; /* only the fresh reply may re-enable it */
    live_flag_poll(flag, now, force);
    active = flag->enabled && now < flag->valid_until;
    ds_mutex_unlock(live_flags.lock);
    return active;
}

double net_azum_buff(void) { return live_flag_get(&live_flags.azum, 0); }

double net_flash_sale(void) { return live_flag_get(&live_flags.sale, 0); }

/* Entering the shop: re-read both flags from Firebase right now instead of
 * trusting whatever the periodic poll last saw, AzumBuff included. The
 * getters keep returning 0 until the fresh replies land, so a price shown
 * or charged in the shop always comes from this visit's read. */
void net_live_flags_refresh(void) {
    (void)live_flag_get(&live_flags.azum, 1);
    (void)live_flag_get(&live_flags.sale, 1);
}
