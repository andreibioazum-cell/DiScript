/* ---- Gifts without Cloud Functions ----
 * Inventory and purchases are plain Realtime Database requests:
 *   users/<nick>/gifts/<sticker> - counter 0..999;
 *   users/<nick>/cups           - cups.
 * Five stickers: poop, cake, bear, rocket and the most expensive one,
 * emerald (500 cups); the old rose counters stay in the database but are
 * no longer shown.
 * Buying is one write to users/<nick>.json: cups minus the price and +1
 * sticker, so a purchase never splits in two.
 * Gifting first takes -1 from you, then +1 to the receiver; if the second
 * write fails, your counter goes back. Selling is one write: -1 sticker and
 * +half the price in cups.
 * firebase.rules.json lets another player only add +1 to gifts (and keeps
 * them out of the rest of the profile and away from deleting stickers); the
 * owner may write their own counters freely, so selling passes the same
 * rules as buying.
 * A sticker counter is written as a path ("gifts/poop"), not as a nested
 * object: REST PATCH replaces the named subtree whole, and the plain
 * "gifts":{"poop":0} form wiped the other counters, the emerald with them.
 * Statuses: 0 - nothing, 1 - running, 2 - done, 3 - network or server,
 * 4 - receiver not found, 5 - not enough cups, 6 - no such sticker in the
 * inventory, 7 - bad nick or gifting yourself, 8 - collection is full (999).
 *
  * The counters on screen do not reset on refresh: the collection stays
  * visible while fresh numbers travel, and a bought, sold or sent sticker
  * changes its own counter right away (net_gift_count_add). */

#define GIFT_N 5
/* How many emerald hues the admin can pick (0 is the plain rainbow). */
#define GIFT_HUE_MAX 7

static volatile int s_gift_status = 0;
static char s_gift_recipient[24];
static int s_gift_id = 0, s_gift_grant = 0;
/* Collection counters: the last known numbers. A refresh no longer zeroes
 * them - the collection stays on screen while fresh data travels. It used to
 * wipe every counter, so right after selling one sticker the collection looked
 * empty for a moment, as if the emerald went with the poop. */
static volatile int s_gift_counts[GIFT_N] = {0, 0, 0, 0, 0};
/* Nick whose counters are on screen right now: another account has its own. */
static char s_gift_nick[24];
/* Data generation: a load answer that arrives after a newer edit is stale and
 * must not overwrite the fresher numbers. */
static volatile int s_gift_gen = 0;
static volatile int s_gift_load_gen = -1; /* поколение последней загрузки, -1 - её не было */
static volatile int s_gift_cups = -1; /* balance the last buy/sell left in the DB */
/* The player's emerald hue: 0 is the plain rainbow, 1..7 a custom one picked
 * by the admin. It lives next to the counters, in the gifts node. */
static volatile int s_gift_hue = 0;

static const char *gift_id(double gift) {
    static const char *ids[GIFT_N] = {"poop", "cake", "bear", "rocket", "emerald"};
    int i = (int)gift;
    return i >= 0 && i < GIFT_N ? ids[i] : "";
}

static int gift_price(int i) {
    static const int prices[GIFT_N] = {25, 100, 250, 400, 500};
    return i >= 0 && i < GIFT_N ? prices[i] : 0;
}

/* Selling returns half of the shop price: the shop stays the cheaper way to
 * get a sticker, while a duplicate from an event is still worth something. */
static int gift_refund_for(int i) { return gift_price(i) / 2; }

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

/* Admins (same list as game/core/config.inc, but the engine cannot see its
 * statics): their sticker purchase is free and the cups in the database are
 * left alone - "admins never lose cups". */
static int gift_nick_is_admin(const char *nick) {
    return gift_nick_same(nick, "Dimasi4ek229") == 1;
}

/* Counters from users/<nick>/gifts.json; a missing node ("null") means all
 * zeroes. */
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
    for (i = 0; i < GIFT_N; i++) {
        if (out[i] < 0)
            out[i] = 0;
        if (out[i] > 999)
            out[i] = 999;
    }
    return 1;
}

/* One sticker is one write: other counters are not overwritten and old keys
 * (rose, for one) do not get in the way. */
static int gift_put_one(const char *nick, int id, int value) {
    char url[URL], body[32];
    snprintf(url, sizeof(url), "%s/users/%s/gifts/%s.json", net.base, nick, gift_id(id));
    snprintf(body, sizeof(body), "%d", value);
    return fb_http("PUT", url, body, NULL, 0);
}

/* Write the emerald hue for a player. Written as a path
 * ("gifts/emerald_hue") so the other counters are not wiped. */
static int gift_put_hue(const char *nick, int hue) {
    char url[URL], body[32];
    if (hue < 0)
        hue = 0;
    if (hue > GIFT_HUE_MAX)
        hue = GIFT_HUE_MAX;
    snprintf(url, sizeof(url), "%s/users/%s/gifts/emerald_hue.json", net.base, nick);
    snprintf(body, sizeof(body), "%d", hue);
    return fb_http("PUT", url, body, NULL, 0);
}

static int gift_recipient_exists(const char *nick) {
    char url[URL], resp[1024];
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    if (fb_http("GET", url, NULL, resp, sizeof(resp)) != 200)
        return 0;
    return resp[0] == '{';
}

/* Body of a buy or sell write: the balance and one sticker counter change in
 * one request. The counter key is a path (with a slash): REST PATCH replaces
 * the named subtree whole, so the plain "gifts":{"poop":0} form used to wipe
 * the other counters together with the emerald. With "gifts/poop" only that
 * counter is written, and cups and the sticker still change in one request.
 * cubes < 0 means "leave the cups alone" - that is how the admin buys free. */
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
    /* The purchase wrote the balance itself; the shop takes this exact number
     * instead of subtracting from a local copy that may be stale. For the
     * free admin buy this is the untouched server balance. */
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
                /* Could not write to the receiver - give the sticker back to us. */
        mine[id]++;
        gift_put_one(sender, id, mine[id]);
        if (net_log_ok())
            LOGERR("gift send: cannot deliver to '%s'", recipient);
        return 3;
    }
    return 2;
}

static int gift_grant(const char *recipient, int id, int hue) {
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
        /* The emerald arrives with the hue the admin picked. */
    if (id == 4) {
        gift_put_hue(recipient, hue);
        s_gift_hue = hue;
    }
    return 2;
}

static volatile int s_gift_hue_pick = 0; /* the hue the admin picked */

static void *gift_action_job(void *arg) {
    char nick[24], sender[24], gift[16];
    int id, mode, hue, result = 3;
    (void)arg;
    snprintf(nick, sizeof(nick), "%s", s_gift_recipient);
    lg_lock();
    snprintf(sender, sizeof(sender), "%s", lg.session_nick);
    lg_unlock();
    id = s_gift_id;
    mode = s_gift_grant;
    hue = s_gift_hue_pick;
    snprintf(gift, sizeof(gift), "%s", gift_id(id));
    if (!gift[0] || !sender[0] || !fb_enabled() || !fb_ensure_token())
        goto gift_done;
    if (mode == 1)
        result = gift_grant(nick, id, hue);
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

/* Start a gift action: 1 - started, 0 - refused (another action is running or
 * a check failed). The caller needs the refusal: otherwise the Sell button
 * would sit on "Selling..." forever. */
double net_gift_action(double gift, const char *recipient, double grant, double hue) {
    DSThread t;
    int mode = (int)grant;
    int h = (int)hue;
    if (s_gift_status == 1 || gift < 0 || gift > GIFT_N - 1 || (mode != 1 && mode != 2 && mode != 3 && mode != 0) || !recipient ||
        strlen(recipient) >= sizeof(s_gift_recipient) || (mode == 0 && !*recipient))
        return 0;
    if (h < 0)
        h = 0;
    if (h > GIFT_HUE_MAX)
        h = GIFT_HUE_MAX;
    snprintf(s_gift_recipient, sizeof(s_gift_recipient), "%s", recipient);
    s_gift_id = (int)gift;
    s_gift_grant = mode;
    s_gift_hue_pick = h;
    s_gift_status = 1;
    t = ds_thread_start(gift_action_job, NULL);
    if (t) {
        ds_thread_detach(t);
        return 1;
    }
    s_gift_status = 3; /* the thread did not start: the caller sees the error */
    return 1;
}

double net_gift_action_status(void) {
    int s = s_gift_status;
    if (s >= 2)
        s_gift_status = 0;
    return (double)s;
}

/* Balance the database holds after the last successful buy/sell, or -1 when
 * the current save is not one. The shop never guesses this number locally. */
double net_gift_cups(void) {
    double v = (double)s_gift_cups;
    s_gift_cups = -1;
    return v;
}

/* The current player's emerald hue: 0 is the plain rainbow, 1..7 a custom one. */
double net_gift_hue(void) { return (double)s_gift_hue; }

/* How many sticker kinds the catalogue has (poop, cake, bear, rocket,
 * emerald). The shop and the collection both lay themselves out from this. */
double net_gift_types(void) { return (double)GIFT_N; }

double net_gift_price(double gift) {
    return (double)gift_price((int)gift);
}

double net_gift_refund(double gift) {
    return (double)gift_refund_for((int)gift);
}

static void *gift_counts_job(void *arg) {
    char nick[24], plain[URL], url[FB_URL_MAX], resp[8192];
    int gen = (int)(uintptr_t)arg, ok = 0, n[GIFT_N] = {0, 0, 0, 0, 0}, i, hue = 0;
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
            hue = num_int(resp, "emerald_hue", 0);
            ok = 1;
        }
    }
    if (gen != s_gift_gen) {
                /* The collection changed while the numbers were in flight (a sticker
                 * was sold or bought): this answer is stale, a fresh one is coming. */
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
        if (hue < 0)
            hue = 0;
        if (hue > GIFT_HUE_MAX)
            hue = GIFT_HUE_MAX;
        s_gift_hue = hue;
    }
    return NULL;
}

/* Reload the counters from the database. Old numbers stay on screen until new
 * ones arrive: the collection must not vanish for a moment (after selling a
 * poop an empty list looks like the emerald went too). */
void net_gift_refresh(void) {
    char nick[24];
    lg_lock();
    snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    lg_unlock();
    if (strcmp(nick, s_gift_nick) != 0) {
                /* Another account: the previous player's collection is not theirs. */
        memset((void *)s_gift_counts, 0, sizeof(s_gift_counts));
        snprintf(s_gift_nick, sizeof(s_gift_nick), "%s", nick);
    }
    s_gift_gen = s_gift_gen + 1; /* ответ прежней загрузки больше не подходит */
}

/* Local counter edit: a sticker was bought, sold or sent. The numbers on
 * screen change at once, the other counters stay as they are. */
void net_gift_count_add(double gift, double delta) {
    int i = (int)gift;
    if (i < 0 || i >= GIFT_N)
        return;
    s_gift_counts[i] = s_gift_counts[i] + (int)delta;
    if (s_gift_counts[i] < 0)
        s_gift_counts[i] = 0;
    if (s_gift_counts[i] > 999)
        s_gift_counts[i] = 999;
        /* The numbers on screen changed: the answer of the old load no longer
         * describes them, and the next load brings them from the database. */
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
