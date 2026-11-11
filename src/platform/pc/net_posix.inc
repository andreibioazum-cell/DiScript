/* Plain-HTTP POSIX socket transport for the Linux test harness. */
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <strings.h>

static int pc_sock_connect(PcUrl *u) {
    struct addrinfo hints, *res = NULL, *it;
    char port[16];
    int fd = -1;
    if (u->https) {
        ds_log_err("http: TLS is not available in the Linux harness (%s)", u->host);
        return -1;
    }
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(port, sizeof(port), "%d", u->port);
    if (getaddrinfo(u->host, port, &hints, &res) != 0)
        return -1;
    for (it = res; it; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, it->ai_addr, it->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    if (fd >= 0) {
        int one = 1;
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    }
    return fd;
}

static int pc_send_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        ssize_t n = send(fd, data, len, 0);
        if (n <= 0) {
            if (errno == EINTR)
                continue;
            return 0;
        }
        data += (size_t)n;
        len -= (size_t)n;
    }
    return 1;
}

static int pc_read_some(int fd, char *buf, size_t cap) {
    for (;;) {
        ssize_t n = recv(fd, buf, cap, 0);
        if (n < 0 && errno == EINTR)
            continue;
        return n > 0 ? (int)n : 0;
    }
}

/* Read HTTP status line and response headers; return status code. */
static int pc_read_headers(int fd, char *hdrs, size_t cap, long *content_length, int *chunked, char *etag,
                           size_t etag_cap) {
    size_t used = 0;
    int code = 0;
    const char *p;
    hdrs[0] = '\0';
    *content_length = -1;
    *chunked = 0;
    if (etag && etag_cap)
        etag[0] = '\0';
    while (used + 1 < cap) {
        char c;
        if (pc_read_some(fd, &c, 1) != 1)
            return 0;
        hdrs[used++] = c;
        hdrs[used] = '\0';
        if (used >= 4 && memcmp(hdrs + used - 4, "\r\n\r\n", 4) == 0)
            break;
    }
    if (strncmp(hdrs, "HTTP/1.", 7) == 0) {
        const char *sp = strchr(hdrs, ' ');
        if (sp)
            code = atoi(sp + 1);
    }
    for (p = strstr(hdrs, "\r\n"); p; p = strstr(p, "\r\n")) {
        p += 2;
        if (!*p)
            break;
        if (strncasecmp(p, "content-length:", 15) == 0)
            *content_length = atol(p + 15);
        else if (strncasecmp(p, "transfer-encoding:", 18) == 0 && strstr(p, "chunked"))
            *chunked = 1;
        else if (etag && etag_cap && strncasecmp(p, "etag:", 5) == 0) {
            const char *v = p + 5, *e;
            while (*v == ' ')
                v++;
            e = strstr(v, "\r\n");
            if (e)
                snprintf(etag, etag_cap, "%.*s", (int)(e - v), v);
        }
    }
    return code;
}

typedef struct {
    char *out;
    size_t cap;
    size_t used;
} PcOutBuf;

static int pc_out_sink(void *ctx, const char *data, size_t len) {
    PcOutBuf *b = (PcOutBuf *)ctx;
    if (!b->out || b->cap == 0)
        return 1;
    /* Copy prefix up to buffer capacity and discard overflow tail. */
    if (b->used + 1 >= b->cap)
        return 1;
    if (b->used + len > b->cap - 1)
        len = b->cap - 1 - b->used;
    memcpy(b->out + b->used, data, len);
    b->used += len;
    b->out[b->used] = '\0';
    return 1;
}

/* Chunked decoder over an open fd. Returns 1 when the last chunk is seen. */
static int pc_read_chunked(int fd, int (*sink)(void *, const char *, size_t), void *ctx) {
    for (;;) {
        char line[64];
        size_t used = 0;
        long size;
        for (;;) {
            char c;
            if (pc_read_some(fd, &c, 1) != 1)
                return 0;
            if (c == '\n')
                break;
            if (c != '\r' && used + 1 < sizeof(line))
                line[used++] = c;
        }
        line[used] = '\0';
        size = strtol(line, NULL, 16);
        if (size <= 0)
            return 1;
        while (size > 0) {
            char buf[4096];
            int want = size < (long)sizeof(buf) ? (int)size : (int)sizeof(buf);
            int got = pc_read_some(fd, buf, (size_t)want);
            if (got <= 0)
                return 0;
            if (!sink(ctx, buf, (size_t)got))
                return 0;
            size -= got;
        }
        {
            char crlf[2];
            if (pc_read_some(fd, crlf, 2) != 2)
                return 0;
        }
    }
}

static int pc_http_request(const char *method, PcUrl *u, const char *body, char *out, size_t cap, const char *header,
                           const char *value, const char *override_method, char *etag, size_t etag_cap) {
    int fd, code = 0;
    char req[8192];
    char hdrs[4096];
    long content_length = -1;
    int chunked = 0, off;
    size_t body_len = body && *body ? strlen(body) : 0;
    PcOutBuf sinkbuf;

    if (out && cap)
        out[0] = '\0';
    if (etag && etag_cap)
        etag[0] = '\0';
    fd = pc_sock_connect(u);
    if (fd < 0)
        return 0;
    off = snprintf(req, sizeof(req),
                   "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nContent-Type: application/json\r\n", method,
                   u->path, u->host);
    if (header && value && *header)
        off += snprintf(req + off, sizeof(req) - (size_t)off, "%s: %s\r\n", header, value);
    if (override_method)
        off += snprintf(req + off, sizeof(req) - (size_t)off, "X-HTTP-Method-Override: %s\r\n", override_method);
    off += snprintf(req + off, sizeof(req) - (size_t)off, "Content-Length: %u\r\n\r\n", (unsigned)body_len);
    if (off <= 0 || (size_t)off >= sizeof(req)) {
        ds_log_err("http: request too long for %s%s", u->host, u->path);
        goto done;
    }
    if (!pc_send_all(fd, req, (size_t)off)) {
        ds_log_err("http: send failed for %s%s", u->host, u->path);
        goto done;
    }
    if (body_len && !pc_send_all(fd, body, body_len)) {
        ds_log_err("http: body send failed for %s%s", u->host, u->path);
        goto done;
    }
    code = pc_read_headers(fd, hdrs, sizeof(hdrs), &content_length, &chunked, etag, etag_cap);
    if (code <= 0) {
        ds_log_err("http: no response from %s%s", u->host, u->path);
        goto done;
    }
    memset(&sinkbuf, 0, sizeof(sinkbuf));
    sinkbuf.out = out;
    sinkbuf.cap = cap;
    if (chunked) {
        if (!pc_read_chunked(fd, pc_out_sink, &sinkbuf))
            ds_log_err("http: broken chunked body from %s%s", u->host, u->path);
    } else if (content_length >= 0) {
        long left = content_length;
        while (left > 0) {
            char buf[4096];
            int want = left < (long)sizeof(buf) ? (int)left : (int)sizeof(buf);
            int got = pc_read_some(fd, buf, (size_t)want);
            if (got <= 0)
                break;
            pc_out_sink(&sinkbuf, buf, (size_t)got);
            left -= got;
        }
    } else {
        for (;;) {
            char buf[4096];
            int got = pc_read_some(fd, buf, sizeof(buf));
            if (got <= 0)
                break;
            pc_out_sink(&sinkbuf, buf, (size_t)got);
        }
    }
done:
    close(fd);
    return code;
}

static int pc_http_stream(PcUrl *u, const char *accept, DSStreamChunkFn cb, void *ctx) {
    int fd, code = 0, off;
    char req[2048];
    char hdrs[4096];
    long content_length = -1;
    int chunked = 0;
    fd = pc_sock_connect(u);
    if (fd < 0)
        return 0;
    off = snprintf(req, sizeof(req),
                   "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nAccept: %s\r\n"
                   "Content-Type: application/json\r\n\r\n",
                   u->path, u->host, accept);
    if (off <= 0 || (size_t)off >= sizeof(req) || !pc_send_all(fd, req, (size_t)off))
        goto done;
    code = pc_read_headers(fd, hdrs, sizeof(hdrs), &content_length, &chunked, NULL, 0);
    if (code <= 0)
        goto done;
    if (chunked) {
        (void)pc_read_chunked(fd, cb, ctx);
    } else {
        for (;;) {
            char buf[4096];
            int got = pc_read_some(fd, buf, sizeof(buf));
            if (got <= 0)
                break;
            if (!cb(ctx, buf, (size_t)got))
                break;
        }
    }
done:
    close(fd);
    return code;
}
