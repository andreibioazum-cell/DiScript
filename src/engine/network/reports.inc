/* Player reports. Any signed-in player can flag one chat message; the list is
 * read only by the owner (admin) from the "Reports" screen. Stored in the same
 * database under /reports/<id>, so nothing new has to be deployed. */

#define NET_REPORT_MAX 12
#define NET_REPORT_TEXT 110

typedef struct {
    char key[48];
    char from[24];
    char target[24];
    char text[NET_REPORT_TEXT];
    char room[24];
    double ts;
} NetReportItem;

static NetReportItem rp_items[NET_REPORT_MAX];
static volatile int rp_count;
static volatile int rp_status; /* 0 idle, 1 loading, 2 ready, 3 failed */
static int rp_seq;

typedef struct {
    char url[URL];
    char body[BODY];
} RpJob;

static void *rp_put_job(void *arg) {
    RpJob *j = (RpJob *)arg;
    if (j) {
        fb_http("PUT", j->url, j->body, NULL, 0);
        free(j);
    }
    return NULL;
}

/* File a complaint about one chat message. Returns 1 when the request is on
 * its way (the game never waits for the network here). */
double net_report_send(const char *target, const char *text, const char *room) {
    RpJob *j;
    DSThread t;
    char et[NET_REPORT_TEXT * 2], eu[40], ef[40], er[64], me[24], key[48];
    long now;
    if (!fb_enabled() || !target || !*target || !text || !*text)
        return 0;
    snprintf(me, sizeof(me), "%s", net_login_nick());
    if (!me[0])
        return 0;
    j = (RpJob *)malloc(sizeof(*j));
    if (!j)
        return 0;
    json_escape(text, et, sizeof(et));
    json_escape(target, eu, sizeof(eu));
    json_escape(me, ef, sizeof(ef));
    json_escape(room && *room ? room : "-", er, sizeof(er));
    now = (long)time(NULL);
    rp_seq++;
    snprintf(key, sizeof(key), "%ld_%d", now, rp_seq);
    snprintf(j->url, sizeof(j->url), "%s/reports/%s.json", net.base, key);
    snprintf(j->body, sizeof(j->body),
             "{\"from\":\"%s\",\"target\":\"%s\",\"text\":\"%s\",\"room\":\"%s\",\"ts\":%ld}", ef, eu, et, er,
             now);
    t = ds_thread_start(rp_put_job, j);
    if (t) {
        ds_thread_detach(t);
        return 1;
    }
    free(j);
    return 0;
}

/* Keep the newest reports first; the screen shows a short list. */
static void rp_insert(const NetReportItem *item) {
    int pos = rp_count < NET_REPORT_MAX ? rp_count : NET_REPORT_MAX - 1;
    if (rp_count < NET_REPORT_MAX)
        rp_count++;
    while (pos > 0 && rp_items[pos - 1].ts < item->ts) {
        rp_items[pos] = rp_items[pos - 1];
        pos--;
    }
    rp_items[pos] = *item;
}

static int rp_parse(const char *json) {
    const char *p;
    if (!json || !(p = skip_ws(json)) || *p != '{')
        return 0;
    rp_count = 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}';) {
        const char *kend = skip_str(p);
        const char *obj;
        NetReportItem item;
        if (!kend)
            break;
        memset(&item, 0, sizeof(item));
        {
            size_t kl = (size_t)(kend - p - 2);
            if (kl >= sizeof(item.key))
                kl = sizeof(item.key) - 1;
            memcpy(item.key, p + 1, kl);
            item.key[kl] = 0;
        }
        p = skip_ws(kend);
        if (*p != ':')
            break;
        p = skip_ws(p + 1);
        obj = p;
        p = skip_val(p);
        if (!p)
            break;
        if (*obj == '{') {
            strv(obj, "from", item.from, sizeof(item.from));
            strv(obj, "target", item.target, sizeof(item.target));
            strv(obj, "text", item.text, sizeof(item.text));
            strv(obj, "room", item.room, sizeof(item.room));
            item.ts = (double)num_int(obj, "ts", 0);
            if (item.key[0] && (item.target[0] || item.text[0]))
                rp_insert(&item);
        }
        p = skip_ws(p);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    return rp_count;
}

static void *rp_fetch_job(void *arg) {
    char url[URL];
    char *resp;
    int code;
    (void)arg;
    resp = (char *)malloc(65536);
    if (!resp) {
        rp_status = 3;
        return NULL;
    }
    snprintf(url, sizeof(url), "%s/reports.json", net.base);
    code = fb_http("GET", url, NULL, resp, 65536);
    if (code == 200)
        rp_parse(resp);
    free(resp);
    rp_status = code == 200 ? 2 : 3;
    return NULL;
}

/* Refresh the list; the screen shows a spinner while rp_status == 1. */
void net_reports_fetch(void) {
    DSThread t;
    if (rp_status == 1)
        return;
    rp_status = 1;
    t = ds_thread_start(rp_fetch_job, NULL);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    rp_status = 3;
}

double net_reports_status(void) { return (double)rp_status; }
double net_reports_count(void) { return (double)rp_count; }

static char s_rp_ret[NET_REPORT_TEXT];
static const char *rp_field(double idx, int which, size_t cap) {
    int i = (int)idx;
    s_rp_ret[0] = 0;
    if (i < 0 || i >= rp_count)
        return s_rp_ret;
    if (which == 0)
        snprintf(s_rp_ret, cap, "%s", rp_items[i].from);
    else if (which == 1)
        snprintf(s_rp_ret, cap, "%s", rp_items[i].target);
    else if (which == 2)
        snprintf(s_rp_ret, cap, "%s", rp_items[i].text);
    else if (which == 3)
        snprintf(s_rp_ret, cap, "%s", rp_items[i].room);
    else
        snprintf(s_rp_ret, cap, "%s", rp_items[i].key);
    return s_rp_ret;
}

const char *net_report_from(double idx) { return rp_field(idx, 0, sizeof(s_rp_ret)); }
const char *net_report_target(double idx) { return rp_field(idx, 1, sizeof(s_rp_ret)); }
const char *net_report_text(double idx) { return rp_field(idx, 2, sizeof(s_rp_ret)); }
const char *net_report_room(double idx) { return rp_field(idx, 3, sizeof(s_rp_ret)); }
const char *net_report_key(double idx) { return rp_field(idx, 4, sizeof(s_rp_ret)); }

static void *rp_delete_job(void *arg) {
    char *url = (char *)arg;
    if (url) {
        fb_http("DELETE", url, NULL, NULL, 0);
        free(url);
    }
    return NULL;
}

/* Drop a handled complaint from the database and from the local list. */
void net_report_delete(double idx) {
    int i = (int)idx;
    char key[48], *url;
    DSThread t;
    if (i < 0 || i >= rp_count)
        return;
    snprintf(key, sizeof(key), "%s", rp_items[i].key);
    for (; i + 1 < rp_count; i++)
        rp_items[i] = rp_items[i + 1];
    rp_count--;
    if (!key[0])
        return;
    url = (char *)malloc(URL);
    if (!url)
        return;
    snprintf(url, URL, "%s/reports/%s.json", net.base, key);
    t = ds_thread_start(rp_delete_job, url);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    free(url);
}
