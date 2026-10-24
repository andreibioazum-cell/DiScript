/* PC Firebase HTTP/SSE transport wrapper (WinHTTP on Windows, POSIX sockets on Linux). */
typedef int (*DSStreamChunkFn)(void *ctx, const char *data, size_t len);

typedef struct {
    char host[256];
    char path[2048];
    int port;
    int https;
} PcUrl;

static int pc_url_parse(const char *url, PcUrl *u) {
    const char *p, *host_end, *path_start;
    size_t n;
    if (!url || !u)
        return 0;
    memset(u, 0, sizeof(*u));
    if (strncmp(url, "https://", 8) == 0) {
        u->https = 1;
        u->port = 443;
        p = url + 8;
    } else if (strncmp(url, "http://", 7) == 0) {
        u->port = 80;
        p = url + 7;
    } else
        return 0;
    host_end = p;
    while (*host_end && *host_end != '/' && *host_end != ':' && *host_end != '?')
        host_end++;
    n = (size_t)(host_end - p);
    if (n == 0 || n >= sizeof(u->host))
        return 0;
    memcpy(u->host, p, n);
    u->host[n] = '\0';
    if (*host_end == ':') {
        u->port = atoi(host_end + 1);
        while (*host_end && *host_end != '/')
            host_end++;
    }
    path_start = (*host_end == '/') ? host_end : "/";
    snprintf(u->path, sizeof(u->path), "%s", path_start);
    return u->port > 0 && u->port < 65536;
}

#ifdef _WIN32
#include "platform/pc/net_winhttp.inc"
#else
#include "platform/pc/net_posix.inc"
#endif

static int http_ex(const char *method, const char *url, const char *body, char *out, size_t cap, const char *header,
                   const char *value, char *etag, size_t etag_cap) {
    PcUrl u;
    const char *m = method ? method : "GET";
    const char *override_method = NULL;
    if (!pc_url_parse(url, &u)) {
        ds_log_err("http: bad url '%s'", url ? url : "(null)");
        return 0;
    }
    if (strcmp(m, "PATCH") == 0) {
        m = "POST";
        override_method = "PATCH";
    }
    return pc_http_request(m, &u, body, out, cap, header, value, override_method, etag, etag_cap);
}

static int http_stream(const char *url, const char *accept, DSStreamChunkFn cb, void *ctx) {
    PcUrl u;
    if (!pc_url_parse(url, &u)) {
        ds_log_err("http: bad stream url '%s'", url ? url : "(null)");
        return 0;
    }
    if (!cb)
        return 0;
    return pc_http_stream(&u, accept ? accept : "text/event-stream", cb, ctx);
}
