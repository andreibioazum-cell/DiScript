static int ascii_ieq_str(const char *a, const char *b) {
    if (!a || !b)
        return 0;
    while (*a && *b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int parse_shallow_user_nick(const char *json, const char *typed, char *out, size_t cap) {
    const char *p = skip_ws(json);
    if (!p || *p != '{' || !typed || !out || !cap)
        return 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}';) {
        const char *kend = skip_str(p);
        char key[LOGIN_NICK_MAX + 2] = "";
        size_t kl;
        if (!kend)
            break;
        kl = (size_t)(kend - p - 2);
        if (kl < sizeof(key)) {
            memcpy(key, p + 1, kl);
            key[kl] = 0;
            if (nick_valid(key) && ascii_ieq_str(key, typed)) {
                snprintf(out, cap, "%s", key);
                return 1;
            }
        }
        p = skip_ws(kend);
        if (*p != ':')
            break;
        p = skip_val(skip_ws(p + 1));
        if (!p)
            break;
        p = skip_ws(p);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    return 0;
}

/* Resolve case-insensitive user key under /users for existing accounts. */
static int resolve_canonical_user_nick(const char *typed, char *out, size_t cap) {
    char url[URL];
    char *resp;
    int ok = 0;
    if (!net.base[0] || !typed || !*typed)
        return 0;
    resp = (char *)malloc(65536);
    if (!resp)
        return 0;
    snprintf(url, sizeof(url), "%s/users.json?shallow=true", net.base);
    if (fb_http("GET", url, NULL, resp, 65536) == 200)
        ok = parse_shallow_user_nick(resp, typed, out, cap);
    free(resp);
    return ok;
}

double net_auth(const char *url, const char *nick, const char *pass) {
    if (!nick || !nick_valid(nick))
        return (double)NET_LOGIN_BAD_NICK;
    if (!pass || !pass_valid(pass))
        return (double)NET_LOGIN_BAD_PASS;
    const char *base = (url && *url) ? url : net.base;
    if (!base || !*base)
        return (double)NET_ERROR;
    if (!net.base[0] && url && *url) {
        snprintf(net.base, sizeof(net.base), "%s", url);
        size_t n = strlen(net.base);
        while (n && net.base[n - 1] == '/') net.base[--n] = 0;
    }

    char active_nick[LOGIN_NICK_MAX + 2];
    char req_url[URL], resp[RESP];
    snprintf(active_nick, sizeof(active_nick), "%s", nick);
    snprintf(req_url, sizeof(req_url), "%s/users/%s.json", net.base, active_nick);
    if (fb_enabled()) {
        int registered = 0;
        int r;
        if (!nick_ascii_ok(active_nick)) {

            LOGERR("'%s': in secured mode nick must be latin letters, digits or '_'", active_nick);
            return (double)NET_LOGIN_BAD_NICK;
        }
        r = fb_sign_in(active_nick, pass, &registered);
        if (r == 1)
            return (double)NET_LOGIN_WRONG_PASS;
        if (r != 0)
            return (double)NET_ERROR;
        int code = fb_http("GET", req_url, NULL, resp, sizeof(resp));
        if (code != 200 && code != 404) {
            if (net_log_ok())
                LOGERR("net_auth: profile GET HTTP %d for %s", code, req_url);
            return (double)NET_ERROR;
        }
        const char *p = skip_ws(resp);
        if (code == 200 && (!p || !*p))
            return (double)NET_ERROR;
        int missing = (code == 404) || (p && !strncmp(p, "null", 4) && (!p[4] || p[4] == ' ' || p[4] == '\n' || p[4] == '\r'));
        if (missing && !registered) {
            char canonical[LOGIN_NICK_MAX + 2] = "";
            if (resolve_canonical_user_nick(active_nick, canonical, sizeof(canonical)) &&
                strcmp(canonical, active_nick) != 0) {
                snprintf(active_nick, sizeof(active_nick), "%s", canonical);
                snprintf(req_url, sizeof(req_url), "%s/users/%s.json", net.base, active_nick);
                code = fb_http("GET", req_url, NULL, resp, sizeof(resp));
                if (code != 200 && code != 404)
                    return (double)NET_ERROR;
                p = skip_ws(resp);
                if (code == 200 && (!p || !*p))
                    return (double)NET_ERROR;
                missing = (code == 404) ||
                          (p && !strncmp(p, "null", 4) && (!p[4] || p[4] == ' ' || p[4] == '\n' || p[4] == '\r'));
            }
        }
        if (missing) {
            int put_code = put_default_user(req_url, active_nick, NULL);
            if (put_code != 200) {
                if (net_log_ok())
                    LOGERR("net_auth: create user record failed HTTP %d", put_code);
                return (double)NET_ERROR;
            }
            net_auth_session_ok(active_nick, pass);
            LOG("%s and logged in: '%s'", registered ? "registered" : "account restored", active_nick);
            return (double)NET_LOGIN_OK;
        }
        if (p && *p == '{') {
            int restored = apply_user_json_keep_local(resp);
            lg_lock();
            snprintf(lg.session_nick, sizeof(lg.session_nick), "%s", active_nick);
            snprintf(lg.session_pass, sizeof(lg.session_pass), "%s", pass);
            lg.status = NET_LOGIN_OK;
            lg_unlock();
            session_save(active_nick, pass);
            if (restored)
                push_pg_to_cloud();
            LOG("logged in: '%s' (cups=%d, candies=%d)", active_nick, num_int(resp, "cups", 0), num_int(resp, "candies", 0));
            return (double)NET_LOGIN_OK;
        }
        return (double)NET_ERROR;
    }

    int code = http("GET", req_url, NULL, resp, sizeof(resp));
    if (code != 200 && code != 404) {
        if (net_log_ok())
            LOGERR("net_auth: network/server error HTTP %d connecting to %s", code, req_url);
        return (double)NET_ERROR;
    }

    const char *p = skip_ws(resp);
    if (code == 200 && (!p || !*p))
        return (double)NET_ERROR;
    if (code == 404 || (p && !strncmp(p, "null", 4) && (!p[4] || p[4] == ' ' || p[4] == '\n' || p[4] == '\r'))) {
        int put_code = put_default_user(req_url, active_nick, pass);
        if (put_code != 200) {
            if (net_log_ok())
                LOGERR("net_auth: register failed HTTP %d", put_code);
            return (double)NET_ERROR;
        }
        net_auth_session_ok(active_nick, pass);
        LOG("registered and logged in: '%s'", active_nick);
        return (double)NET_LOGIN_OK;
    }

    if (p && *p == '{') {
        char got_pass[64] = "";
        strv(resp, "pass", got_pass, sizeof(got_pass));
        if (strcmp(got_pass, pass) == 0) {
            int restored = apply_user_json_keep_local(resp);
            lg_lock();
            snprintf(lg.session_nick, sizeof(lg.session_nick), "%s", active_nick);
            snprintf(lg.session_pass, sizeof(lg.session_pass), "%s", pass);
            lg.status = NET_LOGIN_OK;
            lg_unlock();
            session_save(active_nick, pass);
            if (restored)
                push_pg_to_cloud();
            LOG("logged in: '%s' (cups=%d, candies=%d)", active_nick, num_int(resp, "cups", 0), num_int(resp, "candies", 0));
            return (double)NET_LOGIN_OK;
        } else {
            if (net_log_ok())
                LOG("wrong password for '%s'", active_nick);
            return (double)NET_LOGIN_WRONG_PASS;
        }
    }

    return (double)NET_ERROR;
}
