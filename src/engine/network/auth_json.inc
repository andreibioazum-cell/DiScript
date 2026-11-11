static void achievements_read(void) {
    char path[320];
    FILE *f;
    unsigned int flags = 0, revives = 0;
    ds_mutex_lock(ach_lock);
    if (ach_loaded) {
        ds_mutex_unlock(ach_lock);
        return;
    }
    ds_mutex_unlock(ach_lock);
    data_file_path(path, sizeof(path), ACHIEVEMENTS_FILE);
    f = fopen(path, "r");
    if (f) {

        char line[64];
        if (fgets(line, sizeof(line), f)) {
            int a = 0, b = 0;
            if (sscanf(line, "%d %d", &a, &b) == 2 && a <= 1 && b <= 1) {
                flags = (unsigned int)((b ? ACH_FLAG_ALL_CHARACTERS : 0));
            } else if (sscanf(line, "%u %u", &flags, &revives) < 1) {
                flags = 0;
            }
        } else {
            flags = 0;
        }
        fclose(f);
    }
    ds_mutex_lock(ach_lock);
    ach_flags = flags;
    ach_revives = revives;
    ach_loaded = 1;
    ds_mutex_unlock(ach_lock);
}

static void achievements_write(unsigned int flags, unsigned int revives) {
    char path[320], text[96];
    data_file_path(path, sizeof(path), ACHIEVEMENTS_FILE);
    snprintf(text, sizeof(text), "%u %u\n", flags, revives);
    file_write_atomic(path, text);
}

double net_load_achievement_flags(void) {
    double value;
    achievements_read();
    ds_mutex_lock(ach_lock);
    value = (double)ach_flags;
    ds_mutex_unlock(ach_lock);
    return value;
}

void net_mark_achievement_flag(double flag) {
    unsigned int f, r;
    achievements_read();
    ds_mutex_lock(ach_lock);
    ach_flags |= (unsigned int)flag;
    f = ach_flags;
    r = ach_revives;
    ds_mutex_unlock(ach_lock);
    achievements_write(f, r);

    char nick[LOGIN_NICK_MAX + 1] = "";
    lg_lock();
    if (lg.status == NET_LOGIN_OK && lg.session_nick[0]) {
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    }
    lg_unlock();
    if (nick[0] && net.base[0]) {
        char url[URL], body[BODY];
        snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
        snprintf(body, sizeof(body), "{\"ach_flags\":%u,\"ach_revives\":%u}", f, r);
        http_patch_async(url, body);
    }
}

double net_load_azum_revives(void) {
    double value;
    achievements_read();
    ds_mutex_lock(ach_lock);
    value = (double)ach_revives;
    ds_mutex_unlock(ach_lock);
    return value;
}

void net_save_azum_revives(double revives) {
    unsigned int f, r = (unsigned int)revives;
    achievements_read();
    ds_mutex_lock(ach_lock);
    f = ach_flags;
    ach_revives = r;
    ach_loaded = 1;
    ds_mutex_unlock(ach_lock);
    achievements_write(f, r);

    char nick[LOGIN_NICK_MAX + 1] = "";
    lg_lock();
    if (lg.status == NET_LOGIN_OK && lg.session_nick[0]) {
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    }
    lg_unlock();
    if (nick[0] && net.base[0]) {
        char url[URL], body[BODY];
        snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
        snprintf(body, sizeof(body), "{\"ach_flags\":%u,\"ach_revives\":%u}", f, r);
        http_patch_async(url, body);
    }
}

void net_set_class(double cls) {
    int k = (int)cls;
    if (k == 5)
        k = 4;
    if (k != 0 && k != 1 && k != 2 && k != 3 && k != 4 && k != 6)
        k = 0;
    pending_cls = k;
    lock();
    net.me.cls = (double)k;
    if (net.slot >= 0)
        net.players[net.slot].cls = (double)k;
    unlock();
}
void net_set_level(double level) {
    int lv = (int)level;
    if (lv < 0)
        lv = 0;
    if (lv > LEVEL_MAX)
        lv = LEVEL_MAX;
    pending_level = lv;
    lock();
    net.me.level = (double)lv;
    if (net.slot >= 0)
        net.players[net.slot].level = (double)lv;
    unlock();
}
void net_set_skin(double skin) {
    int sk = (int)skin;
    if (sk < 0)
        sk = 0;
    if (sk > SKIN_MAX)
        sk = SKIN_MAX;
    pending_skin = sk;
    lock();
    net.me.skin = (double)sk;
    if (net.slot >= 0)
        net.players[net.slot].skin = (double)sk;
    unlock();
}

static int nick_valid(const char *n) {
    size_t i = 0, bytes = n ? strlen(n) : 0, chars = 0;
    if (bytes < 1 || bytes > LOGIN_NICK_MAX)
        return 0;
    while (i < bytes) {
        unsigned char c = (unsigned char)n[i];
        unsigned long cp = 0;
        size_t need = 0;
        if (c < 0x80) {
            cp = c;
            need = 1;
        } else if ((c & 0xE0) == 0xC0) {
            cp = c & 0x1F;
            need = 2;
        } else if ((c & 0xF0) == 0xE0) {
            cp = c & 0x0F;
            need = 3;
        } else if ((c & 0xF8) == 0xF0) {
            cp = c & 0x07;
            need = 4;
        } else
            return 0;
        if (i + need > bytes)
            return 0;
        size_t k;
        for (k = 1; k < need; k++) {
            unsigned char q = (unsigned char)n[i + k];
            if ((q & 0xC0) != 0x80)
                return 0;
            cp = (cp << 6) | (q & 0x3F);
        }
        if ((need == 2 && cp < 0x80) || (need == 3 && cp < 0x800) || (need == 4 && cp < 0x10000) || cp > 0x10FFFF)
            return 0;
        if ((cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') || cp == '_' ||
            (cp >= 0x0400 && cp <= 0x04FF))
            chars++;
        else
            return 0;
        i += need;
    }
    return chars >= 1 && chars <= 16;
}

static int pass_valid(const char *p) {
    if (!p)
        return 0;
    size_t len = strlen(p);
    if (len < 1 || len > 32)
        return 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)p[i];
        if (c < 0x20 || c == 0x7F || c == '\"' || c == '\\' || c == ' ')
            return 0;
    }
    return 1;
}

static void session_save(const char *nick, const char *pass) {
    char path[320], text[256];
    if (lg.path[0])
        snprintf(path, sizeof(path), "%s/%s", lg.path, SESSION_FILE);
    else
        snprintf(path, sizeof(path), "%s", SESSION_FILE);
    if (pass && *pass)
        snprintf(text, sizeof(text), "%s %s\n", nick, pass);
    else
        snprintf(text, sizeof(text), "%s\n", nick);
    file_write_atomic(path, text);
}
static long long now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}
static void sleep_ms(int ms) {
    struct timespec t = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&t, NULL);
}
static double safe(double v) {
    if (v != v)
        return 0;
    if (v > 1e300 || v < -1e300)
        return 0;
    return v;
}
static void lock(void) {
    ds_mutex_lock(net.lock);
}
static void unlock(void) {
    ds_mutex_unlock(net.lock);
}
static void status(int s) {
    lock();
    net.status = s;
    unlock();
}

static DSMutex net_log_lock = DS_MUTEX_INIT;
static int net_log_ok(void) {
    static long long last = 0;
    long long now = now_ms();
    int ok = 0;
    ds_mutex_lock(net_log_lock);
    if (now - last >= 2000) {
        last = now;
        ok = 1;
    }
    ds_mutex_unlock(net_log_lock);
    return ok;
}
