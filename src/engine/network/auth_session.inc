static int fb_verify_current_pass(const char *nick, const char *pass);

/* 1 after an automatic re-sign-in with the saved session password failed: the
 * account then needs a real login instead of silently staying "online". */
static int fb_auth_rejected;

static int fb_refresh_token_now(void) {
    char url[FB_URL_MAX], body[768], resp[2048], rt[FB_REFRESH_MAX];
    char nick[LOGIN_NICK_MAX + 2] = "", pass[64] = "";
    int code = 0;
    if (!fb_enabled())
        return 0;
    ds_mutex_lock(fb.lock);
    snprintf(rt, sizeof(rt), "%s", fb.refresh_token);
    ds_mutex_unlock(fb.lock);
    if (rt[0]) {
        fb_auth_endpoint(url, sizeof(url), "/v1/token");
        snprintf(body, sizeof(body), "{\"grant_type\":\"refresh_token\",\"refresh_token\":\"%s\"}", rt);
        code = http("POST", url, body, resp, sizeof(resp));
        if (code == 200 && strstr(resp, "id_token")) {
            char idt[FB_ID_MAX], nrt[FB_REFRESH_MAX];
            strv(resp, "id_token", idt, sizeof(idt));
            strv(resp, "refresh_token", nrt, sizeof(nrt));
            if (idt[0]) {
                fb_store_tokens(idt, nrt[0] ? nrt : rt, num(resp, "expires_in", 3600));
                return 1;
            }
        }
    }
    /* Re-sign in using cached credentials if refresh token is missing or expired. */
    if (!rt[0] || code == 400 || code == 401) {
        lg_lock();
        if (lg.status == NET_LOGIN_OK && lg.session_nick[0] && lg.session_pass[0]) {
            snprintf(nick, sizeof(nick), "%s", lg.session_nick);
            snprintf(pass, sizeof(pass), "%s", lg.session_pass);
        }
        lg_unlock();
        if (nick[0] && pass[0]) {
            int r = fb_verify_current_pass(nick, pass);
            memset(pass, 0, sizeof(pass));
            if (r == 0)
                return 1;
        }
        memset(pass, 0, sizeof(pass));
    }
    if (rt[0] && net_log_ok())
        LOGERR("firebase token refresh failed: HTTP %d %.120s", code, resp);
    return 0;
}

static int fb_ensure_token(void) {
    int ok = 0, renewed;
    long long now = now_ms();
    ds_mutex_lock(fb.lock);
    ok = fb.id_token[0] != 0 && now < fb.expires_at;
    ds_mutex_unlock(fb.lock);
    if (ok) {
        fb_auth_rejected = 0;
        return 1;
    }
    renewed = fb_refresh_token_now();
    fb_auth_rejected = renewed ? 0 : 1;
    return renewed;
}

static void fb_url(char *dst, size_t cap, const char *url) {
    char tok[FB_ID_MAX];
    size_t n;
    if (!url)
        url = "";
    tok[0] = 0;
    if (fb_enabled()) {
        ds_mutex_lock(fb.lock);
        snprintf(tok, sizeof(tok), "%s", fb.id_token);
        ds_mutex_unlock(fb.lock);
    }
    if (!tok[0]) {
        snprintf(dst, cap, "%s", url);
        return;
    }
    n = (size_t)snprintf(dst, cap, "%s%sauth=%s", url, strchr(url, '?') ? "&" : "?", tok);
    if (n >= cap && net_log_ok())
        LOGERR("firebase: auth URL truncated");
}

/* 1 when the player can read secured nodes: a live token, or a saved session
 * that fb_ensure_token() can sign in with again. */
double net_cloud_ready(void) {
    int ready = 0;
    if (!fb_enabled())
        return 1;
    ds_mutex_lock(fb.lock);
    ready = fb.id_token[0] != 0;
    ds_mutex_unlock(fb.lock);
    if (ready)
        return 1;
    if (fb_auth_rejected)
        return 0;
    lg_lock();
    ready = lg.status == NET_LOGIN_OK && lg.session_nick[0] && lg.session_pass[0];
    lg_unlock();
    return ready ? 1 : 0;
}

/* Throw away both tokens so the next fb_ensure_token() starts from scratch. */
static void fb_forget_tokens(void) {
    ds_mutex_lock(fb.lock);
    fb.id_token[0] = 0;
    fb.refresh_token[0] = 0;
    fb.expires_at = 0;
    ds_mutex_unlock(fb.lock);
}

static int fb_http_ex(const char *method, const char *url, const char *body, char *out, size_t cap, const char *header,
                      const char *value, char *etag, size_t etag_cap) {
    char authed[FB_URL_MAX];
    int code;
    if (!fb_enabled())
        return http_ex(method, url, body, out, cap, header, value, etag, etag_cap);
    fb_ensure_token();
    fb_url(authed, sizeof(authed), url);
    code = http_ex(method, authed, body, out, cap, header, value, etag, etag_cap);
    if (code == 401) {
        /* Published rules answer 401 to every unsigned request, so a stale token
         * is never retried as a guest: that retry could only fail again and the
         * whole app would look offline while the player is still signed in.
         * Renew the token (refresh, then a fresh sign-in from the saved
         * session) and repeat the very same request signed. */
        int renewed = fb_refresh_token_now();
        if (renewed == 0) {
            fb_forget_tokens();
            renewed = fb_ensure_token();
        }
        if (renewed) {
            fb_url(authed, sizeof(authed), url);
            code = http_ex(method, authed, body, out, cap, header, value, etag, etag_cap);
        } else if (net_log_ok()) {
            LOGERR("firebase: HTTP 401 and no saved session to sign in again - %.120s", url);
        }
    }
    if (code == 403 && net_log_ok())
        LOGERR("firebase: HTTP 403 (permission denied) - check firebase.rules.json and FB_KEY");
    return code;
}
static int fb_http(const char *method, const char *url, const char *body, char *out, size_t cap) {
    return fb_http_ex(method, url, body, out, cap, NULL, NULL, NULL, 0);
}

/* Open authenticated Firebase SSE stream. */
static int fb_http_stream(const char *url, DSStreamChunkFn cb, void *ctx) {
    char authed[FB_URL_MAX];
    if (!fb_enabled())
        return http_stream(url, "text/event-stream", cb, ctx);
    fb_ensure_token();
    fb_url(authed, sizeof(authed), url);
    return http_stream(authed, "text/event-stream", cb, ctx);
}

static void fb_email_of(const char *nick, char *dst, size_t cap) {
    static const char domain[] = FB_EMAIL_DOMAIN;
    size_t i = 0;
    if (!nick)
        nick = "";
    for (; nick[i] && i + sizeof(domain) < cap; i++) {
        char c = nick[i];
        dst[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
    }
    memcpy(dst + i, domain, sizeof(domain));
}

static int fb_err_is(const char *resp, const char *code_name) {
    return resp && strstr(resp, code_name) != NULL;
}

/* Migrate account password from legacy suffix to NET_PASS_SECRET. */
static void fb_update_password(const char *pass) {
    char url[FB_URL_MAX], body[512], resp[2048], epass[192], idt[FB_ID_MAX], rt[FB_REFRESH_MAX];
    int code;
    if (!pass || !*pass || !fb_enabled())
        return;
    fb_id_token(idt, sizeof(idt));
    if (!idt[0])
        return;
    snprintf(epass, sizeof(epass), "%s%s", pass, net_pass_suffix());
    fb_auth_endpoint(url, sizeof(url), "/v1/accounts:update");
    snprintf(body, sizeof(body), "{\"idToken\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}", idt, epass);
    code = http("POST", url, body, resp, sizeof(resp));
    if (code != 200) {
        if (net_log_ok())
            LOGERR("firebase auth: password update HTTP %d %.160s", code, resp);
        return;
    }
    rt[0] = 0;
    strv(resp, "idToken", idt, sizeof(idt));
    strv(resp, "refreshToken", rt, sizeof(rt));
    if (idt[0])
        fb_store_tokens(idt, rt, num(resp, "expiresIn", 3600));
    if (net_log_ok())
        LOG("firebase auth: account moved to the new password suffix");
}

/* Attempt Firebase sign-in: 0 success, 1 invalid credentials, 2 network/server error. */
static int fb_try_sign_in(const char *email, const char *epass, int *no_such_account) {
    char url[FB_URL_MAX], body[512], resp[2048], idt[FB_ID_MAX], rt[FB_REFRESH_MAX];
    int code;
    if (no_such_account)
        *no_such_account = 0;
    fb_auth_endpoint(url, sizeof(url), "/v1/accounts:signInWithPassword");
    snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}", email, epass);
    code = http("POST", url, body, resp, sizeof(resp));
    idt[0] = 0;
    rt[0] = 0;
    if (code == 200)
        strv(resp, "idToken", idt, sizeof(idt));
    if (code == 200 && idt[0]) {
        strv(resp, "refreshToken", rt, sizeof(rt));
        fb_store_tokens(idt, rt, num(resp, "expiresIn", 3600));
        return 0;
    }
    if (code == 0) {
        if (net_log_ok())
            LOGERR("firebase auth: network error");
        return 2;
    }
    if (!fb_err_is(resp, "EMAIL_NOT_FOUND") && !fb_err_is(resp, "INVALID_PASSWORD") &&
        !fb_err_is(resp, "INVALID_LOGIN_CREDENTIALS")) {
        if (net_log_ok())
            LOGERR("firebase signIn: HTTP %d %.160s", code, resp);
        return 2;
    }
    if (no_such_account)
        *no_such_account = fb_err_is(resp, "EMAIL_NOT_FOUND") ? 1 : 0;
    return 1;
}

static int fb_verify_current_pass_ex(const char *nick, const char *pass, int *no_such_out) {
    char email[96], epass[192];
    int state, legacy = 0, no_such = 0;
    fb_email_of(nick, email, sizeof(email));
    snprintf(epass, sizeof(epass), "%s%s", pass, net_pass_suffix());
    state = fb_try_sign_in(email, epass, &no_such);
    if (state == 1 && !no_such) {
        snprintf(epass, sizeof(epass), "%s%s", pass, FB_LEGACY_PASS_SUFFIX);
        state = fb_try_sign_in(email, epass, NULL);
        legacy = state == 0;
    }
    memset(epass, 0, sizeof(epass));
    if (no_such_out)
        *no_such_out = no_such;
    if (state == 0 && legacy)
        fb_update_password(pass);
    return state;
}

static int fb_verify_current_pass(const char *nick, const char *pass) {
    return fb_verify_current_pass_ex(nick, pass, NULL);
}

static int fb_sign_in(const char *nick, const char *pass, int *registered) {
    char url[FB_URL_MAX], body[512], resp[2048];
    char email[96], epass[192], idt[FB_ID_MAX], rt[FB_REFRESH_MAX];
    int code, state, no_such_account = 0;
    if (registered)
        *registered = 0;
    state = fb_verify_current_pass_ex(nick, pass, &no_such_account);
    if (state == 2)
        return 2;
    if (state == 0)
        return 0;
    fb_email_of(nick, email, sizeof(email));
    fb_auth_endpoint(url, sizeof(url), "/v1/accounts:signUp");
    snprintf(epass, sizeof(epass), "%s%s", pass, net_pass_suffix());
    snprintf(body, sizeof(body), "{\"email\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}", email, epass);
    code = http("POST", url, body, resp, sizeof(resp));
    idt[0] = 0;
    rt[0] = 0;
    if (code == 200)
        strv(resp, "idToken", idt, sizeof(idt));
    if (code == 200 && idt[0]) {
        strv(resp, "refreshToken", rt, sizeof(rt));
        fb_store_tokens(idt, rt, num(resp, "expiresIn", 3600));
        if (registered)
            *registered = 1;
        return 0;
    }
    if (fb_err_is(resp, "EMAIL_EXISTS") || fb_err_is(resp, "WEAK_PASSWORD")) {

        if (net_log_ok())
            LOG("firebase auth: wrong password for '%s'", nick);
        return 1;
    }
    if (net_log_ok())
        LOGERR("firebase signUp: HTTP %d %.160s", code, resp);
    return 2;
}

static int put_default_user(const char *req_url, const char *nick, const char *pass) {
    char body[BODY], epass[128], enick[64];
    json_escape(pass ? pass : "", epass, sizeof(epass));
    json_escape(nick, enick, sizeof(enick));
    snprintf(body, sizeof(body),
             "{\"nick\":\"%s\",\"cups\":0,\"candies\":0,\"cls\":0,\"azum\":0,\"santa\":0,\"ebuc\":0,\"astra\":0,"
             "\"codecracker\":0,\"level\":0,\"levels\":0,\"ordinary_level\":0,\"ordinary_levels\":0,"
             "\"azum_level\":0,\"azum_levels\":0,\"santa_level\":0,\"santa_levels\":0,"
             "\"ebuc_level\":0,\"ebuc_levels\":0,\"astra_level\":0,\"astra_levels\":0,"
             "\"codecracker_level\":0,\"codecracker_levels\":0,"
             "\"bp\":0,\"azum_skin\":0,"
             "\"ach_flags\":0,\"ach_revives\":0%s%s}",
             enick, fb_enabled() ? "" : ",\"pass\":\"", fb_enabled() ? "" : epass);
    net_seal_body(body, sizeof(body));
    return fb_http("PUT", req_url, body, NULL, 0);
}

static void net_auth_session_ok(const char *nick, const char *pass) {

    int keep = 0;
    int cups = 0, candies = 0, cls = 0, azum = 0, santa = 0, ebuc = 0;
    int lv = 0, lu = 0, ol = 0, ou = 0, al = 0, au = 0, sl = 0, su = 0, el = 0, eul = 0;
    int bp = 0, bpx = 0, sk = 0, astra = 0, atl = 0, atu = 0, cc = 0, ccl = 0, ccu = 0;
    progress_read();
    pg_lock();
    if (pg.loaded) {
        cups = pg.cups;
        candies = pg.candies;
        cls = pg.cls;
        azum = pg.azum;
        santa = pg.santa;
        ebuc = pg.ebuc;
        lv = pg.level;
        lu = pg.levels_unlocked;
        ol = pg.ordinary_level;
        ou = pg.ordinary_levels_unlocked;
        al = pg.azum_level;
        au = pg.azum_levels_unlocked;
        sl = pg.santa_level;
        su = pg.santa_levels_unlocked;
        el = pg.ebuc_level;
        eul = pg.ebuc_levels_unlocked;
        bp = pg.bp_level;
        bpx = pg.bp_xp;
        sk = pg.azum_skin;
        astra = pg.astra;
        atl = pg.astra_level;
        atu = pg.astra_levels_unlocked;
        cc = pg.codecracker;
        ccl = pg.codecracker_level;
        ccu = pg.codecracker_levels_unlocked;
        if (cups > 0 || candies > 0 || azum || santa || ebuc || astra || cc || cls != 0 || bp > 0 || bpx > 0 ||
            sk != 0 || lu > 0 || ou > 0 || au > 0 || su > 0 || eul > 0 || atu > 0 || ccu > 0)
            keep = 1;
    }
    pg_unlock();
    if (keep) {
        lg_lock();
        snprintf(lg.session_nick, sizeof(lg.session_nick), "%s", nick);
        snprintf(lg.session_pass, sizeof(lg.session_pass), "%s", pass);
        lg.status = NET_LOGIN_OK;
        lg_unlock();
        session_save(nick, pass);

        net_save_codecracker(cc, ccl, ccu);
        net_save_astra(astra, atl, atu);
        net_save_progress_all(cups, candies, cls, azum, santa, ebuc, lv, lu, ol, ou, al, au, sl, su, el, eul, bp, sk);
        LOG("moved local progress into new account '%s'", nick);
        return;
    }
    pg_lock();
    pg.cups = 0;
    pg.candies = 0;
    pg.cls = 0;
    pg.azum = 0;
    pg.santa = 0;
    pg.ebuc = 0;
    pg.level = 0;
    pg.levels_unlocked = 0;
    pg.ordinary_level = 0;
    pg.ordinary_levels_unlocked = 0;
    pg.azum_level = 0;
    pg.azum_levels_unlocked = 0;
    pg.santa_level = 0;
    pg.santa_levels_unlocked = 0;
    pg.ebuc_level = 0;
    pg.ebuc_levels_unlocked = 0;
    pg.bp_level = 0;
    pg.bp_xp = 0;
    pg.azum_skin = 0;
    pg.astra = 0;
    pg.astra_level = 0;
    pg.astra_levels_unlocked = 0;
    pg.codecracker = 0;
    pg.codecracker_level = 0;
    pg.codecracker_levels_unlocked = 0;
    pg.loaded = 1;
    pg_unlock();
    pending_cls = 0;
    pending_level = 0;
    pending_skin = 0;
    progress_write(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    lg_lock();
    snprintf(lg.session_nick, sizeof(lg.session_nick), "%s", nick);
    snprintf(lg.session_pass, sizeof(lg.session_pass), "%s", pass);
    lg.status = NET_LOGIN_OK;
    lg_unlock();
    session_save(nick, pass);
}

static int nick_ascii_ok(const char *nick) {
    for (const char *p = nick; p && *p; p++) {
        unsigned char c = (unsigned char)*p;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            return 0;
    }
    return 1;
}
