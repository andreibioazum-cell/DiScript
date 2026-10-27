/* Rows asked from the database: a little more than the ten shown, so ties and
 * profiles without a "cups" field do not leave the table short. */
#define LB_QUERY_ROWS 25
#define LB_BODY_MAX 262144

static int parse_leaderboard(const char *json, LeaderboardEntry *out, int max_out) {
    const char *p;
    LeaderboardEntry top[64];
    int count = 0, cap;
    if (!json || !out || max_out <= 0)
        return 0;
    cap = max_out > 64 ? 64 : max_out;
    p = skip_ws(json);
    if (!p || *p != '{')
        return 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}';) {
        const char *kend = skip_str(p);
        if (!kend)
            break;
        char key[32] = {0};
        size_t kl = (size_t)(kend - p - 2);
        if (kl >= sizeof(key))
            kl = sizeof(key) - 1;
        memcpy(key, p + 1, kl);
        key[kl] = 0;
        p = skip_ws(kend);
        if (*p != ':')
            break;
        p = skip_ws(p + 1);
        const char *obj = p;
        p = skip_val(p);
        if (!p)
            break;
        if (*obj == '{') {
            char nick[24] = {0};
            strv(obj, "nick", nick, sizeof(nick));
            if (!nick[0])
                snprintf(nick, sizeof(nick), "%s", key);
            int cups = num_int(obj, "cups", 0);
            if (cups < 0)
                cups = 0;
            if (count < cap || cups > top[count - 1].cups) {
                int pos = count < cap ? count++ : cap - 1;
                while (pos > 0 && top[pos - 1].cups < cups) {
                    top[pos] = top[pos - 1];
                    pos--;
                }
                snprintf(top[pos].nick, sizeof(top[pos].nick), "%s", nick);
                top[pos].cups = cups;
            }
        }
        p = skip_ws(p);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    for (int i = 0; i < count; i++)
        out[i] = top[i];
    return count;
}

static void *lb_fetch_job(void *arg) {
    char *arg_base = (char *)arg;
    char base[URL], url[URL];
    char *resp;
    int code;
    snprintf(base, sizeof(base), "%s", arg_base ? arg_base : "");
    free(arg_base);
    resp = (char *)malloc(LB_BODY_MAX);
    if (!resp) {
        ds_mutex_lock(lb.lock);
        lb.status = 4;
        ds_mutex_unlock(lb.lock);
        return NULL;
    }
    /* Only the cup leaders: the whole users node grows without a limit and a
     * full read may simply not arrive in time over a slow link. The rules
     * index "cups", so the database does the sorting. */
    snprintf(url, sizeof(url), "%s/users.json?orderBy=%%22cups%%22&limitToLast=%d", base, LB_QUERY_ROWS);
    code = fb_http("GET", url, NULL, resp, LB_BODY_MAX);
    if (code == 400 || code == 404) {
        /* A database without the cups index refuses the query: read it whole.
         * Auth errors are not retried this way - the whole node is just as
         * closed as the query and a second request would only waste a login. */
        snprintf(url, sizeof(url), "%s/users.json", base);
        code = fb_http("GET", url, NULL, resp, LB_BODY_MAX);
    }
    if (code != 200) {
        if (net_log_ok())
            LOGERR("leaderboard: HTTP %d for %.160s", code, url);
        free(resp);
        ds_mutex_lock(lb.lock);
        lb.status = 4;
        ds_mutex_unlock(lb.lock);
        return NULL;
    }
    LeaderboardEntry tmp[64];
    int cnt = parse_leaderboard(resp, tmp, 10);
    free(resp);
    ds_mutex_lock(lb.lock);
    lb.count = cnt;
    for (int i = 0; i < cnt; i++) lb.entries[i] = tmp[i];
    lb.status = 2;
    ds_mutex_unlock(lb.lock);
    return NULL;
}
void net_leaderboard_fetch(const char *url) {
    const char *base = (url && *url) ? url : net.base;
    if (!base || !*base)
        return;
    ds_mutex_lock(lb.lock);
    lb.status = 1;
    lb.count = 0;
    ds_mutex_unlock(lb.lock);
    char *arg = (char *)malloc(URL);
    if (!arg)
        return;
    snprintf(arg, URL, "%s", base);
    DSThread t;
    t = ds_thread_start(lb_fetch_job, arg);
    if (t)
        ds_thread_detach(t);
    else {
        ds_mutex_lock(lb.lock);
        lb.status = 4;
        ds_mutex_unlock(lb.lock);
        free(arg);
    }
}
double net_leaderboard_status(void) {
    double s;
    ds_mutex_lock(lb.lock);
    s = (double)lb.status;
    ds_mutex_unlock(lb.lock);
    return s;
}
double net_leaderboard_count(void) {
    double c;
    ds_mutex_lock(lb.lock);
    c = (double)lb.count;
    ds_mutex_unlock(lb.lock);
    return c;
}
static char s_lb_nick_ret[24];
const char *net_leaderboard_nick(double idx) {
    int i = (int)idx;
    ds_mutex_lock(lb.lock);
    if (i >= 0 && i < lb.count)
        snprintf(s_lb_nick_ret, sizeof(s_lb_nick_ret), "%s", lb.entries[i].nick);
    else
        s_lb_nick_ret[0] = 0;
    ds_mutex_unlock(lb.lock);
    return s_lb_nick_ret;
}
double net_leaderboard_cups(double idx) {
    int i = (int)idx;
    double cups = 0;
    ds_mutex_lock(lb.lock);
    if (i >= 0 && i < lb.count)
        cups = (double)lb.entries[i].cups;
    ds_mutex_unlock(lb.lock);
    return cups;
}

static void *http_post_job(void *arg) {
    HttpJob *j = (HttpJob *)arg;
    if (j) {
        char sealed[BODY * 3];
        snprintf(sealed, sizeof(sealed), "%s", j->body);
    /* Attach write secret required by database rules. */
    if (fb_enabled())
            net_seal_body(sealed, sizeof(sealed));
        fb_http("POST", j->plain, sealed, NULL, 0);
        free(j);
    }
    return NULL;
}
static void http_post_async(const char *url, const char *body) {
    HttpJob *j = (HttpJob *)malloc(sizeof(*j));
    DSThread t;
    if (!j)
        return;
    snprintf(j->plain, sizeof(j->plain), "%s", url);
    snprintf(j->url, sizeof(j->url), "%s", url);
    snprintf(j->body, sizeof(j->body), "%s", body);
    t = ds_thread_start(http_post_job, j);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    free(j);
}
static void *http_delete_job(void *arg) {
    char *url = (char *)arg;
    if (url) {
                /* fb_http adds a fresh token itself and retries the request on 401. */
        fb_http("DELETE", url, NULL, NULL, 0);
        free(url);
    }
    return NULL;
}
static void chat_delete_key_async(const char *key) {
    char *url;
    DSThread t;
    if (!key || !*key)
        return;
    if (strchr(key, '/') || strchr(key, '.') || strchr(key, '#'))
        return;
    url = (char *)malloc(FB_URL_MAX);
    if (!url)
        return;
    {
        char plain[URL];
        snprintf(plain, sizeof(plain), "%s/rooms/%s/chat/%s.json", net.base, net.room, key);
        snprintf(url, FB_URL_MAX, "%s", plain);
    }
    t = ds_thread_start(http_delete_job, url);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    free(url);
}

static int actor_same(const Actor *x, const Actor *y) {
    return x->x == y->x && x->y == y->y && x->a == y->a && x->hp == y->hp && x->alive == y->alive &&
           x->punch_x == y->punch_x && x->punch_y == y->punch_y && x->punch_dx == y->punch_dx &&
           x->punch_dy == y->punch_dy && x->punch == y->punch && x->snow_x == y->snow_x && x->snow_y == y->snow_y &&
           x->snow_dx == y->snow_dx && x->snow_dy == y->snow_dy && x->snow == y->snow && x->station_x == y->station_x &&
           x->station_y == y->station_y && x->station_hp == y->station_hp && x->station == y->station &&
           x->station2_x == y->station2_x && x->station2_y == y->station2_y && x->station2_hp == y->station2_hp &&
           x->station3_x == y->station3_x && x->station3_y == y->station3_y && x->station3_hp == y->station3_hp &&
           x->dash_x == y->dash_x && x->dash_y == y->dash_y && x->dash_dx == y->dash_dx && x->dash_dy == y->dash_dy &&
           x->dash == y->dash && x->grab_x == y->grab_x && x->grab_y == y->grab_y && x->grab_dx == y->grab_dx &&
           x->grab_dy == y->grab_dy && x->grab == y->grab && x->universe_x == y->universe_x &&
           x->universe_y == y->universe_y && x->universe == y->universe && x->thud == y->thud && x->block == y->block && x->ccu == y->ccu && x->brick_x == y->brick_x && x->brick_y == y->brick_y &&
           x->brick_dx == y->brick_dx && x->brick_dy == y->brick_dy && x->brick == y->brick && x->cls == y->cls &&
           x->level == y->level && x->skin == y->skin && !strcmp(x->nick, y->nick);
}

static double clamp01(double v) {
    return v < 0 ? 0 : (v > 1 ? 1 : safe(v));
}
static double clamp11(double v) {
    v = safe(v);
    return v < -1 ? -1 : (v > 1 ? 1 : v);
}
