/* Gift sticker inventory, shop purchases, sales, transfers, and admin grants.
 * Status: 0 idle, 1 running, 2 done, 3 error, 4 missing user, 5 low cups,
 * 6 none owned, 7 invalid target, 8 collection full (999). */

#define GIFT_N NET_GIFT_TYPE_COUNT

static volatile int s_gift_status = 0;
static char s_gift_recipient[24];
static int s_gift_id = 0, s_gift_grant = 0;
/* Cached sticker counts retained across refreshes. */
static volatile int s_gift_counts[GIFT_N] = {0, 0, 0, 0, 0, 0};
/* Account nick owning the cached sticker counts. */
static char s_gift_nick[24];
/* Generation counter used to discard stale refresh replies. */
static volatile int s_gift_gen = 0;
static volatile int s_gift_load_gen = -1;
static volatile int s_gift_cups = -1;

static const char *gift_id(double gift) {
    static const char *ids[GIFT_N] = {"poop", "cake", "bear", "rocket", "emerald", "pumpkin_cube"};
    int i = (int)gift;
    return i >= 0 && i < GIFT_N ? ids[i] : "";
}

static int gift_price(int i) {
    static const int prices[GIFT_N] = {25, 100, 250, 400, 500, 555};
    return i >= 0 && i < GIFT_N ? prices[i] : 0;
}

/* The admin-only pumpkin cube has a listed value but cannot be sold. */
static int gift_refund_for(int i) { return i == NET_GIFT_PUMPKIN_CUBE_ID ? 0 : gift_price(i) / 2; }

static int gift_nick_ok(const char *nick) {
    size_t i, n;
    if (!nick || !(n = strlen(nick)) || n > 16)
        return 0;
    for (i = 0; i < n; i++) {
        char c = nick[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            return 0;
    }
    return 1;
}

static int gift_nick_same(const char *a, const char *b) {
    size_t i;
    if (!a || !b)
        return 0;
    for (i = 0; a[i] && b[i]; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z')
            x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z')
            y = (char)(y - 'A' + 'a');
        if (x != y)
            return 0;
    }
    return a[i] == 0 && b[i] == 0;
}

/* Admins purchase stickers without spending cups. */
static int gift_nick_is_admin(const char *nick) {
    return gift_nick_same(nick, "Dimasi4ek229") == 1;
}

/* Pumpkin Cube can only ever be granted by the primary admin. */
static int gift_action_allowed(int id, const char *nick, int mode) {
    if (id != NET_GIFT_PUMPKIN_CUBE_ID)
        return 1;
    return mode == 1 && gift_nick_is_admin(nick);
}

/* Fetch sticker counters from users/<nick>/gifts.json. */
static int gift_get_counts(const char *nick, int out[GIFT_N]) {
    char url[URL], resp[2048];
    int i;
    for (i = 0; i < GIFT_N; i++)
        out[i] = 0;
    snprintf(url, sizeof(url), "%s/users/%s/gifts.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 0;
    out[0] = num_int(resp, "poop", 0);
    out[1] = num_int(resp, "cake", 0);
    out[2] = num_int(resp, "bear", 0);
    out[3] = num_int(resp, "rocket", 0);
    out[4] = num_int(resp, "emerald", 0);
    out[NET_GIFT_PUMPKIN_CUBE_ID] = num_int(resp, "pumpkin_cube", 0);
    for (i = 0; i < GIFT_N; i++) {
        if (out[i] < 0)
            out[i] = 0;
        if (out[i] > 999)
            out[i] = 999;
    }
    return 1;
}

/* Write one sticker counter without touching sibling keys. */
static int gift_put_one(const char *nick, int id, int value) {
    char url[URL], body[32];
    snprintf(url, sizeof(url), "%s/users/%s/gifts/%s.json", net.base, nick, gift_id(id));
    snprintf(body, sizeof(body), "%d", value);
    return fb_http("PUT", url, body, NULL, 0);
}

static int gift_recipient_exists(const char *nick) {
    char url[URL], resp[1024];
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 0;
    return resp[0] == '{';
}

/* Build PATCH payload updating one slash-delimited gift path and optional cup balance. */
static void gift_body(char *body, size_t cap, int cups, int id, int count) {
    if (cups < 0)
        snprintf(body, cap, "{\"gifts/%s\":%d}", gift_id(id), count);
    else
        snprintf(body, cap, "{\"cups\":%d,\"gifts/%s\":%d}", cups, gift_id(id), count);
    net_seal_body(body, cap);
}

static int gift_buy(const char *nick, int id) {
    char url[URL], body[BODY], resp[1024];
    int counts[GIFT_N], cups, code, price = gift_price(id);
    int free_buy = gift_nick_is_admin(nick);
    snprintf(url, sizeof(url), "%s/users/%s/cups.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 3;
    cups = num_int(resp, "", 0);
    if (free_buy == 0 && cups < price)
        return 5;
    if (!gift_get_counts(nick, counts))
        return 3;
    if (counts[id] >= 999)
        return 8;
    counts[id]++;
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    gift_body(body, sizeof(body), free_buy == 1 ? -1 : cups - price, id, counts[id]);
    code = fb_http("PATCH", url, body, NULL, 0);
    if (code != 200) {
        if (net_log_ok())
            LOGERR("gift buy for '%s': HTTP %d", nick, code);
        return 3;
    }
    /* Record resulting server cup balance for shop UI sync. */
    s_gift_cups = free_buy == 1 ? cups : cups - price;
    return 2;
}

static int gift_sell(const char *nick, int id) {
    char url[URL], body[BODY], resp[1024];
    int counts[GIFT_N], cups, code;
    snprintf(url, sizeof(url), "%s/users/%s/cups.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 3;
    cups = num_int(resp, "", 0);
    if (!gift_get_counts(nick, counts))
        return 3;
    if (counts[id] < 1)
        return 6;
    counts[id]--;
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    gift_body(body, sizeof(body), cups + gift_refund_for(id), id, counts[id]);
    code = fb_http("PATCH", url, body, NULL, 0);
    if (code != 200) {
        if (net_log_ok())
            LOGERR("gift sell for '%s': HTTP %d", nick, code);
        return 3;
    }
    s_gift_cups = cups + gift_refund_for(id);
    return 2;
}

static int gift_send(const char *sender, const char *recipient, int id) {
    int mine[GIFT_N], theirs[GIFT_N];
    if (!gift_nick_ok(recipient) || gift_nick_same(sender, recipient))
        return 7;
    if (!gift_recipient_exists(recipient))
        return 4;
    if (!gift_get_counts(sender, mine))
        return 3;
    if (mine[id] < 1)
        return 6;
    if (!gift_get_counts(recipient, theirs))
        return 3;
    if (theirs[id] >= 999)
        return 8;
    mine[id]--;
    if (gift_put_one(sender, id, mine[id]) != 200) {
        if (net_log_ok())
            LOGERR("gift send: cannot take the sticker from '%s'", sender);
        return 3;
    }
    theirs[id]++;
    if (gift_put_one(recipient, id, theirs[id]) != 200) {
        /* Restore sender count on failed delivery. */
        mine[id]++;
        gift_put_one(sender, id, mine[id]);
        if (net_log_ok())
            LOGERR("gift send: cannot deliver to '%s'", recipient);
        return 3;
    }
    return 2;
}

static int gift_grant(const char *recipient, int id) {
    int theirs[GIFT_N];
    if (!gift_nick_ok(recipient))
        return 4;
    if (!gift_recipient_exists(recipient))
        return 4;
    if (!gift_get_counts(recipient, theirs))
        return 3;
    if (theirs[id] >= 999)
        return 8;
    theirs[id]++;
    if (gift_put_one(recipient, id, theirs[id]) != 200) {
        if (net_log_ok())
            LOGERR("gift grant for '%s': write failed", recipient);
        return 3;
    }
    return 2;
}

static void *gift_action_job(void *arg) {
    char nick[24], sender[24], gift[16];
    int id, mode, result = 3;
    (void)arg;
    snprintf(nick, sizeof(nick), "%s", s_gift_recipient);
    lg_lock();
    snprintf(sender, sizeof(sender), "%s", lg.session_nick);
    lg_unlock();
    id = s_gift_id;
    mode = s_gift_grant;
    snprintf(gift, sizeof(gift), "%s", gift_id(id));
    if (!gift[0] || !sender[0] || !fb_enabled() || !fb_ensure_token())
        goto gift_done;
    if (!gift_action_allowed(id, sender, mode)) {
        result = 7;
        goto gift_done;
    }
    if (mode == 1)
        result = gift_grant(nick, id);
    else if (mode == 2)
        result = gift_buy(sender, id);
    else if (mode == 3)
        result = gift_sell(sender, id);
    else
        result = gift_send(sender, nick, id);
gift_done:
    s_gift_status = result;
    return NULL;
}

/* Start asynchronous gift action; returns 1 if started, 0 if rejected. */
double net_gift_action(double gift, const char *recipient, double grant) {
    DSThread t;
    int mode = (int)grant;
    if ((int)gift == NET_GIFT_PUMPKIN_CUBE_ID) {
        char sender[24];
        lg_lock();
        snprintf(sender, sizeof(sender), "%s", lg.session_nick);
        lg_unlock();
        if (!gift_action_allowed((int)gift, sender, mode)) return 0;
    }
    if (s_gift_status == 1 || gift < 0 || gift > GIFT_N - 1 || (mode != 1 && mode != 2 && mode != 3 && mode != 0) || !recipient ||
        strlen(recipient) >= sizeof(s_gift_recipient) || (mode == 0 && !*recipient))
        return 0;
    snprintf(s_gift_recipient, sizeof(s_gift_recipient), "%s", recipient);
    s_gift_id = (int)gift;
    s_gift_grant = mode;
    s_gift_status = 1;
    t = ds_thread_start(gift_action_job, NULL);
    if (t) {
        ds_thread_detach(t);
        return 1;
    }
    s_gift_status = 3;
    return 1;
}

double net_gift_action_status(void) {
    int s = s_gift_status;
    if (s >= 2)
        s_gift_status = 0;
    return (double)s;
}

/* Consume updated server cup balance from the last buy/sell (-1 if none). */
double net_gift_cups(void) {
    double v = (double)s_gift_cups;
    s_gift_cups = -1;
    return v;
}


/* Return number of sticker types. */
double net_gift_types(void) { return (double)GIFT_N; }

double net_gift_price(double gift) {
    return (double)gift_price((int)gift);
}

double net_gift_refund(double gift) {
    return (double)gift_refund_for((int)gift);
}

static void *gift_counts_job(void *arg) {
    char nick[24], plain[URL], url[FB_URL_MAX], resp[8192];
    int gen = (int)(uintptr_t)arg, ok = 0, n[GIFT_N] = {0, 0, 0, 0, 0}, i;
    lg_lock();
    snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    lg_unlock();
    if (nick[0] && fb_enabled() && fb_ensure_token()) {
        snprintf(plain, sizeof(plain), "%s/users/%s/gifts.json", net.base, nick);
        fb_url(url, sizeof(url), plain);
        if (http("GET", url, NULL, resp, sizeof(resp)) == 200) {
            n[0] = num_int(resp, "poop", 0);
            n[1] = num_int(resp, "cake", 0);
            n[2] = num_int(resp, "bear", 0);
            n[3] = num_int(resp, "rocket", 0);
            n[4] = num_int(resp, "emerald", 0);
            n[NET_GIFT_PUMPKIN_CUBE_ID] = num_int(resp, "pumpkin_cube", 0);
            ok = 1;
        }
    }
    if (gen != s_gift_gen) {
        /* Discard reply superseded by a newer local edit. */
        return NULL;
    }
    if (ok) {
        for (i = 0; i < GIFT_N; i++) {
            if (n[i] < 0)
                n[i] = 0;
            if (n[i] > 999)
                n[i] = 999;
            s_gift_counts[i] = n[i];
        }
    }
    return NULL;
}

/* Schedule asynchronous refresh of sticker counters. */
void net_gift_refresh(void) {
    char nick[24];
    lg_lock();
    snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    lg_unlock();
    if (strcmp(nick, s_gift_nick) != 0) {
        /* Reset cached counts on account switch. */
        memset((void *)s_gift_counts, 0, sizeof(s_gift_counts));
        snprintf(s_gift_nick, sizeof(s_gift_nick), "%s", nick);
    }
    s_gift_gen = s_gift_gen + 1;
}

/* Update cached count for a single sticker and invalidate in-flight loads. */
void net_gift_count_add(double gift, double delta) {
    int i = (int)gift;
    if (i < 0 || i >= GIFT_N)
        return;
    s_gift_counts[i] = s_gift_counts[i] + (int)delta;
    if (s_gift_counts[i] < 0)
        s_gift_counts[i] = 0;
    if (s_gift_counts[i] > 999)
        s_gift_counts[i] = 999;
    s_gift_gen = s_gift_gen + 1;
}

double net_gift_count(double gift) {
    if (s_gift_load_gen != s_gift_gen) {
        DSThread t;
        s_gift_load_gen = s_gift_gen;
        t = ds_thread_start(gift_counts_job, (void *)(uintptr_t)s_gift_gen);
        if (t)
            ds_thread_detach(t);
    }
    int i = (int)gift;
    return i >= 0 && i < GIFT_N ? s_gift_counts[i] : 0;
}
