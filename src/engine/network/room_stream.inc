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

static void stream_mark_live(void) {
    lock();
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

static void sse_dispatch_players(void) {
    char path[32];
    const char *data, *dend;
    if (!sse.data_len)
        return;
    sse.data[sse.data_len] = 0;
    if (strcmp(sse.event, "keep-alive") == 0 || strcmp(sse.event, "cancel") == 0 ||
        strcmp(sse.event, "auth_revoked") == 0) {
        return;
    }
    strv(sse.data, "path", path, sizeof(path));
    data = member(sse.data, "data");
    if (!data)
        return;
    dend = skip_val(data);
    if (!dend || dend <= data)
        return;
    if (!stream_players_cache[0])
        snprintf(stream_players_cache, sizeof(stream_players_cache), "%s", "{}");
    if (strcmp(path, "/") == 0 || path[0] == 0) {
        if (strcmp(sse.event, "patch") == 0) {
            char patch[RESP];
            size_t n = (size_t)(dend - data);
            if (n >= sizeof(patch))
                return;
            memcpy(patch, data, n);
            patch[n] = 0;
            if (!json_object_merge(stream_players_cache, sizeof(stream_players_cache), patch))
                return;
        } else {
            size_t n = (size_t)(dend - data);
            if (n >= sizeof(stream_players_cache))
                return;
            memcpy(stream_players_cache, data, n);
            stream_players_cache[n] = 0;
            if (stream_players_cache[0] != '{')
                snprintf(stream_players_cache, sizeof(stream_players_cache), "%s", "{}");
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
        if (!json_object_merge(stream_players_cache, sizeof(stream_players_cache), patch))
            return;
    }
    if (net_gen_ok(stream_gen))
        read_players(stream_players_cache, stream_gen);
}

static void sse_line(const char *line, size_t len) {
    if (len == 0) {
        if (sse.data_len)
            sse_dispatch_players();
        sse.data_len = 0;
        sse.event[0] = 0;
        return;
    }
    if (line[0] == ':')
        return; /* comment / heartbeat */
    if (len > 6 && strncmp(line, "event:", 6) == 0) {
        const char *v = line + 6;
        while (*v == ' ')
            v++;
        snprintf(sse.event, sizeof(sse.event), "%s", v);
        return;
    }
    if (len > 5 && strncmp(line, "data:", 5) == 0) {
        const char *v = line + 5;
        size_t n;
        while (*v == ' ')
            v++;
        n = strlen(v);
        if (sse.data_len && sse.data_len + 1 < sizeof(sse.data))
            sse.data[sse.data_len++] = '\n';
        if (sse.data_len + n >= sizeof(sse.data))
            n = sizeof(sse.data) - sse.data_len - 1;
        memcpy(sse.data + sse.data_len, v, n);
        sse.data_len += n;
        return;
    }
}

/* Chunk callback for http_stream: reassembles lines across chunks. */
static int stream_players_chunk(void *ctx, const char *chunk, size_t len) {
    size_t i;
    (void)ctx;
    if (!net_gen_ok(stream_gen))
        return 0;
    stream_mark_live();
    for (i = 0; i < len; i++) {
        char c = chunk[i];
        if (c == '\n') {
            sse.line[sse.line_len] = 0;
            /* trim trailing CR */
            if (sse.line_len && sse.line[sse.line_len - 1] == '\r')
                sse.line[--sse.line_len] = 0;
            sse_line(sse.line, sse.line_len);
            sse.line_len = 0;
            if (!net_gen_ok(stream_gen))
                return 0;
        } else if (sse.line_len + 1 < sizeof(sse.line)) {
            sse.line[sse.line_len++] = c;
        } else {
            sse.line_len = 0; /* overlong line: drop it, resync on \n */
        }
    }
    return 1;
}
