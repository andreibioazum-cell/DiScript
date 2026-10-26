/* WinHTTP HTTPS/HTTP transport with shared keep-alive session for Windows builds. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>

#ifndef WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY
#define WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY 4
#endif

static HINTERNET pc_http_session = NULL;
static DSMutex pc_http_session_lock = DS_MUTEX_INIT;

static HINTERNET pc_http_session_get(void) {
    HINTERNET s;
    ds_mutex_lock(pc_http_session_lock);
    if (!pc_http_session) {
        pc_http_session = WinHttpOpen(L"CubicBattle4/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
        if (!pc_http_session)
            pc_http_session = WinHttpOpen(L"CubicBattle4/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                          WINHTTP_NO_PROXY_BYPASS, 0);
        if (!pc_http_session && net_log_ok())
            ds_log_err("http: WinHttpOpen failed (%lu)", (unsigned long)GetLastError());
    }
    s = pc_http_session;
    ds_mutex_unlock(pc_http_session_lock);
    return s;
}

static wchar_t *pc_wide(const char *s) {
    int n = MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, NULL, 0);
    wchar_t *w;
    if (n <= 0)
        return NULL;
    w = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!w)
        return NULL;
    MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, w, n);
    return w;
}

static HINTERNET pc_open_request(const char *method, PcUrl *u, const char *header, const char *value,
                                 const char *override_method, int streaming, HINTERNET *out_conn) {
    HINTERNET session = pc_http_session_get();
    HINTERNET conn = NULL, req = NULL;
    wchar_t *whost = NULL, *wpath = NULL, *wmethod = NULL;
    DWORD flags = u->https ? WINHTTP_FLAG_SECURE : 0;
    int tmo = net_fast ? 1200 : TIMEOUT;
    int rtmo = streaming ? 90000 : tmo;

    if (out_conn)
        *out_conn = NULL;
    if (!session)
        return NULL;
    whost = pc_wide(u->host);
    wpath = pc_wide(u->path);
    wmethod = pc_wide(method);
    if (!whost || !wpath || !wmethod)
        goto done;
    conn = WinHttpConnect(session, whost, (INTERNET_PORT)u->port, 0);
    if (!conn) {
        if (net_log_ok())
            ds_log_err("http: connect to %s:%d failed (%lu)", u->host, u->port, (unsigned long)GetLastError());
        goto done;
    }
    req = WinHttpOpenRequest(conn, wmethod, wpath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) {
        if (net_log_ok())
            ds_log_err("http: open request failed (%lu)", (unsigned long)GetLastError());
        goto done;
    }
    WinHttpSetTimeouts(req, tmo, tmo, tmo, rtmo);
    {
        char hdrs[1200];
        hdrs[0] = '\0';
        strcat(hdrs, "Content-Type: application/json\r\n");
        if (header && value && *header) {
            strncat(hdrs, header, 200);
            strncat(hdrs, ": ", 4);
            strncat(hdrs, value, 800);
            strncat(hdrs, "\r\n", 4);
        }
        if (override_method) {
            strcat(hdrs, "X-HTTP-Method-Override: ");
            strncat(hdrs, override_method, 40);
            strcat(hdrs, "\r\n");
        }
        {
            wchar_t *wh = pc_wide(hdrs);
            int ok = wh && WinHttpAddRequestHeaders(req, wh, (DWORD)-1L,
                                                    WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
            free(wh);
            if (!ok && net_log_ok())
                ds_log_err("http: cannot set headers (%lu)", (unsigned long)GetLastError());
        }
    }
done:
    free(whost);
    free(wpath);
    free(wmethod);
    if (!req) {
        if (conn)
            WinHttpCloseHandle(conn);
    } else if (out_conn) {
        *out_conn = conn;
    }
    return req;
}

static DWORD pc_query_status(HINTERNET req) {
    DWORD code = 0, len = sizeof(code);
    if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                             &code, &len, WINHTTP_NO_HEADER_INDEX))
        return 0;
    return code;
}

static void pc_query_etag(HINTERNET req, char *etag, size_t etag_cap) {
    DWORD len = 0;
    if (!etag || !etag_cap)
        return;
    if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_CUSTOM, L"ETag", WINHTTP_NO_OUTPUT_BUFFER, &len,
                             WINHTTP_NO_HEADER_INDEX))
        return;
    {
        wchar_t *buf = (wchar_t *)malloc(len + sizeof(wchar_t));
        char *utf8;
        int n;
        if (!buf)
            return;
        if (!WinHttpQueryHeaders(req, WINHTTP_QUERY_CUSTOM, L"ETag", buf, &len, WINHTTP_NO_HEADER_INDEX)) {
            free(buf);
            return;
        }
        buf[len / sizeof(wchar_t) - 1] = L'\0';
        n = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
        utf8 = n > 0 ? (char *)malloc((size_t)n) : NULL;
        if (utf8) {
            WideCharToMultiByte(CP_UTF8, 0, buf, -1, utf8, n, NULL, NULL);
            snprintf(etag, etag_cap, "%s", utf8);
            free(utf8);
        }
        free(buf);
    }
}

static int pc_http_request(const char *method, PcUrl *u, const char *body, char *out, size_t cap, const char *header,
                           const char *value, const char *override_method, char *etag, size_t etag_cap) {
    HINTERNET conn = NULL, req;
    DWORD code = 0;
    size_t total = 0;
    int ok = 0;
    const char *send_body = body && *body ? body : NULL;
    DWORD body_len = send_body ? (DWORD)strlen(send_body) : 0;

    if (out && cap)
        out[0] = '\0';
    if (etag && etag_cap)
        etag[0] = '\0';
    req = pc_open_request(method, u, header, value, override_method, 0, &conn);
    if (!req)
        return 0;
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, (LPVOID)send_body, body_len, body_len, 0) &&
        WinHttpReceiveResponse(req, NULL)) {
        code = pc_query_status(req);
        pc_query_etag(req, etag, etag_cap);
        for (;;) {
            char buf[4096];
            DWORD got = 0;
            if (!WinHttpReadData(req, buf, sizeof(buf), &got) || got == 0)
                break;
            if (out && cap && total + 1 < cap) {
                size_t take = (total + (size_t)got < cap) ? (size_t)got : (cap - 1 - total);
                memcpy(out + total, buf, take);
                total += take;
                out[total] = '\0';
            }
            ok = 1;
        }
        ok = 1;
    } else if (net_log_ok()) {
        ds_log_err("http: %s %s://%s%s failed (%lu)", method, u->https ? "https" : "http", u->host, u->path,
                   (unsigned long)GetLastError());
    }
    WinHttpCloseHandle(req);
    if (conn)
        WinHttpCloseHandle(conn);
    return ok ? (int)code : 0;
}

static int pc_http_stream(PcUrl *u, const char *accept, DSStreamChunkFn cb, void *ctx) {
    HINTERNET conn = NULL;
    HINTERNET req = pc_open_request("GET", u, "Accept", accept, NULL, 1, &conn);
    DWORD code = 0;
    if (!req)
        return 0;
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(req, NULL)) {
        code = pc_query_status(req);
        if (code == 200 && cb(ctx, "", 0)) {
            for (;;) {
                char buf[4096];
                DWORD got = 0;
                if (!WinHttpReadData(req, buf, sizeof(buf), &got) || got == 0)
                    break;
                if (!cb(ctx, buf, (size_t)got))
                    break;
            }
        }
    } else if (net_log_ok()) {
        ds_log_err("http: stream %s%s failed (%lu)", u->host, u->path, (unsigned long)GetLastError());
    }
    WinHttpCloseHandle(req);
    if (conn)
        WinHttpCloseHandle(conn);
    return (int)code;
}
