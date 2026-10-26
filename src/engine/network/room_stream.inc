/* Firebase REST SSE stream for room player slots with polling fallback. */
static char stream_players_cache[RESP];
static unsigned stream_gen;

typedef struct {
    char line[RESP];
    size_t line_len;
    char data[RESP];
    size_t data_len;
    char event[24];
} SseParser;
static SseParser sse;

typedef struct {
    SseParser sse;
    char cache[RESP];
    unsigned gen;
} SseStreamCtx;

static void stream_mark_live(unsigned gen) {
    lock();
    if (net_gen_ok(gen))
        net.stream_live_at = now_ms();
    unlock();
}

static int stream_is_live(void) {
    long long at;
    int active;
    lock();
    at = net.stream_live_at;
    active = net.stream_active;
    unlock();
    /* Treat SSE stream as live until keep-alive timeout expires. */
    return active && at && now_ms() - at < 75000;
}

static void sse_dispatch_players_ctx(SseParser *sp, char *cache, size_t cache_cap, unsigned gen) {
    char path[32];
    const char *data, *dend;
    if (!sp->data_len)
        return;
    sp->data[sp->data_len] = 0;
    if (strcmp(sp->event, "keep-alive") == 0 || strcmp(sp->event, "cancel") == 0 ||
        strcmp(sp->event, "auth_revoked") == 0) {
        return;
    }
    strv(sp->data, "path", path, sizeof(path));
    data = member(sp->data, "data");
    if (!data)
        return;
    dend = skip_val(data);
    if (!dend || dend <= data)
        return;
    if (!cache[0])
        snprintf(cache, cache_cap, "%s", "{}");
    if (strcmp(path, "/") == 0 || path[0] == 0) {
        if (strcmp(sp->event, "patch") == 0) {
            char patch[RESP];
            size_t n = (size_t)(dend - data);
            if (n >= sizeof(patch))
                return;
            memcpy(patch, data, n);
            patch[n] = 0;
            if (!json_object_merge(cache, cache_cap, patch))
                return;
        } else {
            size_t n = (size_t)(dend - data);
            if (n >= cache_cap)
                return;
            memcpy(cache, data, n);
            cache[n] = 0;
            if (cache[0] != '{')
                snprintf(cache, cache_cap, "%s", "{}");
        }
    } else {
        /* Convert SSE slot path/data payload into an object patch. */
        char patch[RESP];
        const char *slot = path + 1;
        int w = snprintf(patch, sizeof(patch), "{\"%s\":", slot);
        size_t n = (size_t)(dend - data);
        if (w < 0 || (size_t)w + n + 2 >= sizeof(patch))
            return;
        memcpy(patch + w, data, n);
        patch[w + (int)n] = '}';
        patch[w + (int)n + 1] = 0;
        if (!json_object_merge(cache, cache_cap, patch))
            return;
    }
    if (net_gen_ok(gen))
        read_players(cache, gen);
}

static void sse_line_ctx(SseParser *sp, char *cache, size_t cache_cap, unsigned gen, const char *line, size_t len) {
    if (len == 0) {
        if (sp->data_len)
            sse_dispatch_players_ctx(sp, cache, cache_cap, gen);
        sp->data_len = 0;
        sp->event[0] = 0;
        return;
    }
    if (line[0] == ':')
        return;
    if (len > 6 && strncmp(line, "event:", 6) == 0) {
        const char *v = line + 6;
        while (*v == ' ')
            v++;
        snprintf(sp->event, sizeof(sp->event), "%s", v);
        return;
    }
    if (len > 5 && strncmp(line, "data:", 5) == 0) {
        const char *v = line + 5;
        size_t n;
        while (*v == ' ')
            v++;
        n = strlen(v);
        if (sp->data_len && sp->data_len + 1 < sizeof(sp->data))
            sp->data[sp->data_len++] = '\n';
        if (sp->data_len + n >= sizeof(sp->data))
            n = sizeof(sp->data) - sp->data_len - 1;
        memcpy(sp->data + sp->data_len, v, n);
        sp->data_len += n;
        return;
    }
}

/* Reassemble SSE lines using isolated per-thread stream state when ctx is provided. */
static int stream_players_chunk(void *ctx, const char *chunk, size_t len) {
    SseStreamCtx *sc = (SseStreamCtx *)ctx;
    SseParser *sp = sc ? &sc->sse : &sse;
    char *cache = sc ? sc->cache : stream_players_cache;
    size_t cache_cap = sc ? sizeof(sc->cache) : sizeof(stream_players_cache);
    unsigned gen = sc ? sc->gen : stream_gen;
    size_t i;
    if (!net_gen_ok(gen))
        return 0;
    stream_mark_live(gen);
    for (i = 0; i < len; i++) {
        char c = chunk[i];
        if (c == '\n') {
            sp->line[sp->line_len] = 0;
            if (sp->line_len && sp->line[sp->line_len - 1] == '\r')
                sp->line[--sp->line_len] = 0;
            sse_line_ctx(sp, cache, cache_cap, gen, sp->line, sp->line_len);
            sp->line_len = 0;
            if (!net_gen_ok(gen))
                return 0;
        } else if (sp->line_len + 1 < sizeof(sp->line)) {
            sp->line[sp->line_len++] = c;
        } else {
            sp->line_len = 0;
        }
    }
    return 1;
}
