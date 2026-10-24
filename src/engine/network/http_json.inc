static int http(const char *method, const char *url, const char *body, char *out, size_t cap) {
    return http_ex(method, url, body, out, cap, NULL, NULL, NULL, 0);
}
static const char *skip_ws(const char *p) {
    while (p && *p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}
static const char *skip_str(const char *p) {
    if (!p || *p != '\"')
        return NULL;
    for (p++; *p; p++) {
        if (*p == '\\' && p[1]) {
            p++;
            continue;
        }
        if (*p == '\"')
            return p + 1;
    }
    return NULL;
}
static const char *skip_box(const char *p, char open, char close) {
    int d = 0;
    if (!p || *p != open)
        return NULL;
    for (; *p; p++) {
        if (*p == '\"') {
            p = skip_str(p);
            if (!p)
                return NULL;
            p--;
            continue;
        }
        if (*p == open)
            d++;
        else if (*p == close && --d == 0)
            return p + 1;
    }
    return NULL;
}
static const char *skip_val(const char *p) {
    p = skip_ws(p);
    if (!p || !*p)
        return NULL;
    if (*p == '\"')
        return skip_str(p);
    if (*p == '{')
        return skip_box(p, '{', '}');
    if (*p == '[')
        return skip_box(p, '[', ']');
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}
static const char *member(const char *o, const char *key) {
    size_t n = strlen(key);
    const char *p = skip_ws(o);
    if (!p || *p != '{')
        return NULL;
    for (p = skip_ws(p + 1); p && *p && *p != '}';) {
        const char *name = p, *end = skip_str(p);
        if (!end)
            return NULL;
        p = skip_ws(end);
        if (*p != ':')
            return NULL;
        p = skip_ws(p + 1);
        if ((size_t)(end - name - 2) == n && strncmp(name + 1, key, n) == 0)
            return p;
        p = skip_val(p);
        p = skip_ws(p);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    return NULL;
}
static const char *element(const char *a, size_t wanted) {
    const char *p = skip_ws(a);
    size_t index = 0;
    if (!p || *p != '[')
        return NULL;
    for (p = skip_ws(p + 1); p && *p && *p != ']'; index++) {
        if (index == wanted)
            return p;
        p = skip_val(p);
        p = skip_ws(p);
        if (p && *p == ',')
            p = skip_ws(p + 1);
        else
            break;
    }
    return NULL;
}
static const char *path_val(const char *json, const char *path) {
    char part[48], *end;
    const char *v = json;
    while (path && *path && v) {
        const char *slash = strchr(path, '/');
        size_t n = slash ? (size_t)(slash - path) : strlen(path), index;
        if (!n || n >= sizeof(part))
            return NULL;
        memcpy(part, path, n);
        part[n] = 0;
        v = skip_ws(v);
        if (*v == '[') {
            index = strtoul(part, &end, 10);
            if (!*part || *end)
                return NULL;
            v = element(v, index);
        } else
            v = member(v, part);
        path = slash ? slash + 1 : path + n;
    }
    return v;
}
/* Span of one object member's value: returns member key span too. */
static int member_span(const char *o, const char *key, const char **vs, const char **ve, const char **ms,
                       const char **me) {
    size_t n = strlen(key);
    const char *p = skip_ws(o);
    if (!p || *p != '{')
        return 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}';) {
        const char *name = p, *end = skip_str(p);
        const char *v, *vend;
        if (!end)
            return 0;
        p = skip_ws(end);
        if (*p != ':')
            return 0;
        v = skip_ws(p + 1);
        vend = skip_val(v);
        if (!vend)
            return 0;
        if ((size_t)(end - name - 2) == n && strncmp(name + 1, key, n) == 0) {
            *vs = v;
            *ve = vend;
            *ms = name;
            *me = vend;
            return 1;
        }
        p = skip_ws(vend);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    return 0;
}

/* Merge patch members into dst JSON object in place; null deletes keys. */
static int json_object_merge(char *dst, size_t cap, const char *patch) {
    const char *p = skip_ws(patch);
    if (!dst || !cap || !p || *p != '{' || !dst[0])
        return 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}';) {
        const char *name = p, *end = skip_str(p);
        const char *v, *vend, *ovs, *ove, *oms, *ome;
        char key[48];
        size_t kn;
        if (!end)
            return 0;
        p = skip_ws(end);
        if (*p != ':')
            return 0;
        v = skip_ws(p + 1);
        vend = skip_val(v);
        if (!vend)
            return 0;
        kn = (size_t)(end - name - 2);
        if (kn >= sizeof(key))
            return 0;
        memcpy(key, name + 1, kn);
        key[kn] = 0;
        int is_null = (size_t)(vend - v) == 4 && strncmp(v, "null", 4) == 0;
        if (member_span(dst, key, &ovs, &ove, &oms, &ome)) {
            if (is_null) {
                /* Delete the member plus one neighbouring comma. */
                const char *ds = oms, *de = ome;
                const char *q = skip_ws(de);
                if (*q == ',')
                    de = q + 1;
                else {
                    while (ds > dst && (ds[-1] == ' ' || ds[-1] == '\t'))
                        ds--;
                    if (ds > dst && ds[-1] == ',')
                        ds--;
                }
                memmove((char *)ds, de, strlen(de) + 1);
            } else {
                size_t old_len = (size_t)(ove - ovs), new_len = (size_t)(vend - v);
                size_t total = strlen(dst);
                if (total - old_len + new_len >= cap)
                    return 0;
                memmove((char *)ovs + new_len, ove, strlen(ove) + 1);
                memcpy((char *)ovs, v, new_len);
            }
        } else if (!is_null) {
            /* Append before the closing brace. */
            size_t total = strlen(dst);
            size_t add = (size_t)(end - name) + 1 + (size_t)(vend - v);
            const char *q = skip_ws(dst + 1);
            int empty = (*q == '}');
            if (total + add + (empty ? 0 : 1) >= cap)
                return 0;
            char *ins = dst + total - 1;
            if (!empty) {
                memmove(ins + 1, ins, 2);
                *ins++ = ',';
            }
            memcpy(ins, name, (size_t)(end - name));
            ins += (size_t)(end - name);
            *ins++ = ':';
            memcpy(ins, v, (size_t)(vend - v));
            ins += (size_t)(vend - v);
            *ins = '}';
            ins[1] = 0;
        }
        p = skip_ws(vend);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    return 1;
}

static double num(const char *json, const char *path, double fb) {
    const char *v = path_val(json, path);
    if (!v || !strncmp(v, "null", 4))
        return fb;
    if (*v == 't')
        return 1;
    if (*v == 'f')
        return 0;
    if (*v == '\"')
        v++;
    return atof(v);
}
static int num_int(const char *json, const char *path, int fb_value) {
    double v = num(json, path, fb_value);
    if (!isfinite(v) || v < (double)INT_MIN || v > (double)INT_MAX)
        return fb_value;
    return (int)v;
}
static long long num_ll(const char *json, const char *path, long long fb_value) {
    double v = num(json, path, (double)fb_value);
    double limit = ldexp(1.0, (int)(sizeof(long long) * CHAR_BIT - 1));
    if (!isfinite(v) || v < -limit || v >= limit)
        return fb_value;
    return (long long)v;
}
static unsigned long num_ulong(const char *json, const char *path, unsigned long fb_value) {
    double v = num(json, path, (double)fb_value);
    double limit = ldexp(1.0, (int)(sizeof(unsigned long) * CHAR_BIT));
    if (!isfinite(v) || v < 0 || v >= limit)
        return fb_value;
    return (unsigned long)v;
}
static void json_escape(const char *src, char *dst, size_t cap) {
    size_t o = 0;
    if (cap == 0)
        return;
    for (size_t i = 0; src[i] && o + 2 < cap; i++) {
        char c = src[i];
        if (c == '\"') {
            dst[o++] = '\\';
            dst[o++] = '\"';
        } else if (c == '\\') {
            dst[o++] = '\\';
            dst[o++] = '\\';
        } else if (c == '\n') {
            dst[o++] = '\\';
            dst[o++] = 'n';
        } else if (c == '\r') {
            dst[o++] = '\\';
            dst[o++] = 'r';
        } else if ((unsigned char)c < 0x20) {
        } else {
            dst[o++] = c;
        }
    }
    dst[o] = '\0';
}

#define FB_EMAIL_DOMAIN "@cb4.game"

/* Cached Firebase Auth token state. */
typedef struct {
    DSMutex lock;
    char key[128];
    char id_token[FB_ID_MAX];
    char refresh_token[FB_REFRESH_MAX];
    long long expires_at;
} FirebaseAuth;
static FirebaseAuth fb = {.lock = DS_MUTEX_INIT};

void net_set_firebase_key(const char *key) {
    size_t n;
    net_secret_prepare();
    if (!key)
        key = "";
    while (*key == ' ' || *key == '\t') key++;
    ds_mutex_lock(fb.lock);
    snprintf(fb.key, sizeof(fb.key), "%s", key);
    n = strlen(fb.key);
    while (n && (fb.key[n - 1] == ' ' || fb.key[n - 1] == '\t' || fb.key[n - 1] == '\r' || fb.key[n - 1] == '\n'))
        fb.key[--n] = 0;
    ds_mutex_unlock(fb.lock);
    if (fb.key[0])
        LOG("firebase auth enabled (key set)");
}
static int fb_enabled(void) {
    return fb.key[0] != 0;
}

static void fb_id_token(char *dst, size_t cap) {
    if (!dst || !cap)
        return;
    dst[0] = 0;
    ds_mutex_lock(fb.lock);
    snprintf(dst, cap, "%s", fb.id_token);
    ds_mutex_unlock(fb.lock);
}

static void fb_store_tokens(const char *id_token, const char *refresh_token, double expires_in_sec) {
    long long now, lifetime_ms;
    ds_mutex_lock(fb.lock);
    snprintf(fb.id_token, sizeof(fb.id_token), "%s", id_token ? id_token : "");
    snprintf(fb.refresh_token, sizeof(fb.refresh_token), "%s", refresh_token ? refresh_token : "");
    if (!isfinite(expires_in_sec) || expires_in_sec > 31536000)
        expires_in_sec = 3600;
    if (expires_in_sec < 60)
        expires_in_sec = 60;
    lifetime_ms = (long long)(expires_in_sec * 1000.0);
    now = now_ms();
    fb.expires_at = now > LLONG_MAX - lifetime_ms ? LLONG_MAX : now + lifetime_ms - 60000;
    ds_mutex_unlock(fb.lock);
}

static void fb_auth_endpoint(char *dst, size_t cap, const char *path) {
    if (net.base[0] && !strncmp(net.base, "http://", 7)) {
        snprintf(dst, cap, "%s%s?key=%s", net.base, path, fb.key);
        return;
    }
    if (!strcmp(path, "/v1/token"))
        snprintf(dst, cap, "https://securetoken.googleapis.com/v1/token?key=%s", fb.key);
    else

        snprintf(dst, cap, "https://identitytoolkit.googleapis.com%s?key=%s", path, fb.key);
}
