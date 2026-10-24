#define PROMO_FILE "promo.dat"

/* Redemption state for fixed promo code 2027 (persisted in promo.dat and cloud). */
typedef struct {
    DSMutex lock;
    int loaded;
    int used2027;
} Promo;
static Promo pr = {.lock = DS_MUTEX_INIT};
static void pr_lock(void) { ds_mutex_lock(pr.lock); }
static void pr_unlock(void) { ds_mutex_unlock(pr.lock); }

static void promo_read(void) {
    char path[320], line[64];
    FILE *f;
    int used2027 = 0;
    pr_lock();
    if (pr.loaded) {
        pr_unlock();
        return;
    }
    pr_unlock();
    data_file_path(path, sizeof(path), PROMO_FILE);
    f = fopen(path, "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            int v = 0;
            if (sscanf(line, "used2027 %d", &v) == 1)
                used2027 = v ? 1 : 0;
        }
        fclose(f);
    }
    pr_lock();
    pr.used2027 = used2027;
    pr.loaded = 1;
    pr_unlock();
}

static void promo_write(void) {
    char path[320], text[64];
    int used2027;
    promo_read();
    pr_lock();
    used2027 = pr.used2027;
    pr_unlock();
    data_file_path(path, sizeof(path), PROMO_FILE);
    snprintf(text, sizeof(text), "used2027 %d\n", used2027);
    file_write_atomic(path, text);
}

double net_promo_2027_used(void) {
    double v;
    promo_read();
    pr_lock();
    v = (double)pr.used2027;
    pr_unlock();
    return v;
}

void net_promo_2027_mark_used(void) {
    char nick[LOGIN_NICK_MAX + 1] = "";
    promo_read();
    pr_lock();
    pr.used2027 = 1;
    pr_unlock();
    promo_write();
    lg_lock();
    if (lg.status == NET_LOGIN_OK && lg.session_nick[0])
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    lg_unlock();
    if (nick[0] && net.base[0]) {
        char url[URL];
        snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
        http_patch_async(url, "{\"promo_used\":1}");
    }
}

static void promo_sync_from_cloud(const char *resp) {
    if (num_int(resp, "promo_used", 0) == 1 && !net_promo_2027_used()) {
        pr_lock();
        pr.used2027 = 1;
        pr.loaded = 1;
        pr_unlock();
        promo_write();
    }
}

static void promo_reset_local(void) {
    char path[320];
    pr_lock();
    pr.loaded = 0;
    pr.used2027 = 0;
    pr_unlock();
    data_file_path(path, sizeof(path), PROMO_FILE);
    remove(path);
}
