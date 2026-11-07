/* Asynchronous account password change.
 * Status: 0 idle, 1 running, 2 changed, 3 error, 4 wrong password, 5 invalid new password,
 * 6 signed out, 7 connection error, 8 too many attempts. */

#define PASS_JOB_STALE_MS 30000

static volatile int s_pass_chg_status = 0;
static long long s_pass_chg_started = 0;
static char s_pass_chg_old[64], s_pass_chg_new[64];

/* Read a bare JSON string value such as the legacy /pass field. */
static int pass_json_str(const char *json, char *out, size_t cap) {
    const char *p = skip_ws(json), *e;
    size_t n;
    if (!out || !cap)
        return 0;
    out[0] = 0;
    if (!p || *p != '"')
        return 0;
    e = skip_str(p);
    if (!e)
        return 0;
    n = (size_t)(e - p - 2);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, p + 1, n);
    out[n] = 0;
    return 1;
}

/* Compare the typed password with the legacy /pass field: 1 match, 0 no match, -1 unreachable. */
static int pass_legacy_match(const char *nick, const char *old_pass) {
    char url[URL], resp[512], got[64];
    int code;
    if (!net.base[0] || !nick || !*nick)
        return -1;
    snprintf(url, sizeof(url), "%s/users/%s/pass.json", net.base, nick);
    code = fb_http("GET", url, NULL, resp, sizeof(resp));
    if (code == 404)
        return 0;
    if (code != 200)
        return -1;
    if (!pass_json_str(resp, got, sizeof(got)))
        return 0;
    return strcmp(got, old_pass ? old_pass : "") == 0 ? 1 : 0;
}

static int pass_change_firebase(const char *nick, const char *old_pass, const char *new_pass) {
    char url[FB_URL_MAX], body[512], resp[2048], epass[192], idt[FB_ID_MAX], rt[FB_REFRESH_MAX];
    int code, r, no_such = 0;
    r = fb_verify_current_pass_ex(nick, old_pass, &no_such);
    if (r == 1 && no_such) {
        /* Legacy profile without an auth record: create it with the verified password. */
        if (pass_legacy_match(nick, old_pass) == 1)
            r = fb_sign_in(nick, old_pass, NULL);
    }
    if (r == 1)
        return 4;
    if (r == 2)
        return 7;
    fb_id_token(idt, sizeof(idt));
    if (!idt[0])
        return 7;
    snprintf(epass, sizeof(epass), "%s%s", new_pass, net_pass_suffix());
    fb_auth_endpoint(url, sizeof(url), "/v1/accounts:update");
    snprintf(body, sizeof(body), "{\"idToken\":\"%s\",\"password\":\"%s\",\"returnSecureToken\":true}", idt, epass);
    memset(epass, 0, sizeof(epass));
    code = http("POST", url, body, resp, sizeof(resp));
    memset(body, 0, sizeof(body));
    if (code == 0)
        return 7;
    if (code != 200) {
        if (net_log_ok())
            LOGERR("password change: HTTP %d %.160s", code, resp);
        if (fb_err_is(resp, "WEAK_PASSWORD"))
            return 5;
        if (fb_err_is(resp, "TOO_MANY_ATTEMPTS_TRY_LATER"))
            return 8;
        return 3;
    }
    rt[0] = 0;
    idt[0] = 0;
    strv(resp, "idToken", idt, sizeof(idt));
    strv(resp, "refreshToken", rt, sizeof(rt));
    if (idt[0])
        fb_store_tokens(idt, rt, num(resp, "expiresIn", 3600));
    return 2;
}

static int pass_change_legacy(const char *nick, const char *old_pass, const char *new_pass) {
    char url[URL], body[BODY];
    int match = pass_legacy_match(nick, old_pass);
    if (match < 0)
        return 7;
    if (match == 0)
        return 4;
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    snprintf(body, sizeof(body), "{\"pass\":\"%s\"}", new_pass);
    net_seal_body(body, sizeof(body));
    return fb_http("PATCH", url, body, NULL, 0) == 200 ? 2 : 3;
}

static void *pass_change_job(void *arg) {
    char nick[LOGIN_NICK_MAX + 2] = "", old_pass[64], new_pass[64];
    int result;
    (void)arg;
    snprintf(old_pass, sizeof(old_pass), "%s", s_pass_chg_old);
    snprintf(new_pass, sizeof(new_pass), "%s", s_pass_chg_new);
    memset(s_pass_chg_old, 0, sizeof(s_pass_chg_old));
    memset(s_pass_chg_new, 0, sizeof(s_pass_chg_new));
    lg_lock();
    if (lg.status == NET_LOGIN_OK)
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    lg_unlock();
    if (!nick[0])
        result = 6;
    else if (fb_enabled())
        result = pass_change_firebase(nick, old_pass, new_pass);
    else
        result = pass_change_legacy(nick, old_pass, new_pass);
    if (result == 2) {
        lg_lock();
        if (strcmp(lg.session_nick, nick) == 0)
            snprintf(lg.session_pass, sizeof(lg.session_pass), "%s", new_pass);
        lg_unlock();
        session_save(nick, new_pass);
        if (net_log_ok())
            LOG("password changed for '%s'", nick);
    }
    memset(old_pass, 0, sizeof(old_pass));
    memset(new_pass, 0, sizeof(new_pass));
    s_pass_chg_status = result;
    return NULL;
}

void net_change_password(const char *old_pass, const char *new_pass) {
    DSThread t;
    if (s_pass_chg_status == 1)
        return;
    if (!new_pass || !pass_valid(new_pass)) {
        s_pass_chg_status = 5;
        return;
    }
    if (!old_pass || !*old_pass) {
        s_pass_chg_status = 4;
        return;
    }
    snprintf(s_pass_chg_old, sizeof(s_pass_chg_old), "%s", old_pass);
    snprintf(s_pass_chg_new, sizeof(s_pass_chg_new), "%s", new_pass);
    s_pass_chg_started = now_ms();
    s_pass_chg_status = 1;
    t = ds_thread_start(pass_change_job, NULL);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    memset(s_pass_chg_old, 0, sizeof(s_pass_chg_old));
    memset(s_pass_chg_new, 0, sizeof(s_pass_chg_new));
    s_pass_chg_status = 3;
}

/* Poll password-change status; terminal results (>= 2) reset to idle. */
double net_change_password_status(void) {
    int s = s_pass_chg_status;
    if (s == 1 && s_pass_chg_started && now_ms() - s_pass_chg_started > PASS_JOB_STALE_MS) {
        s = 3;
        s_pass_chg_status = 0;
        return (double)s;
    }
    if (s >= 2)
        s_pass_chg_status = 0;
    return (double)s;
}

/* Admin password reset via the adminResetPassword Cloud Function.
 * Status: 0 idle, 1 running, 2 changed, 3 error, 4 invalid password, 5 invalid nick. */
static volatile int s_admin_reset_status = 0;
static char s_admin_reset_nick[24], s_admin_reset_new[64];

static void *admin_reset_job(void *arg) {
    char nick[24], new_pass[64], idt[FB_ID_MAX], bearer[FB_ID_MAX + 16], body[512], resp[2048], enick[64], epass[96];
    int code, result = 3;
    (void)arg;
    snprintf(nick, sizeof(nick), "%s", s_admin_reset_nick);
    snprintf(new_pass, sizeof(new_pass), "%s", s_admin_reset_new);
    memset(s_admin_reset_new, 0, sizeof(s_admin_reset_new));
    if (!fb_enabled() || !fb_ensure_token())
        goto done;
    fb_id_token(idt, sizeof(idt));
    if (!idt[0])
        goto done;
    snprintf(bearer, sizeof(bearer), "Bearer %s", idt);
    json_escape(nick, enick, sizeof(enick));
    json_escape(new_pass, epass, sizeof(epass));
        /* Build Firebase callable-function request payload. */
        snprintf(body, sizeof(body), "{\"data\":{\"nick\":\"%s\",\"password\":\"%s\"}}", enick, epass);
    code = http_ex("POST", NET_ADMIN_RESET_URL, body, resp, sizeof(resp), "Authorization", bearer, NULL, 0);
    memset(body, 0, sizeof(body));
    memset(bearer, 0, sizeof(bearer));
    if (code == 200 && strstr(resp, "\"result\"") && strstr(resp, "\"ok\":true"))
        result = 2;
    else if (net_log_ok())
        LOGERR("admin password reset for '%s': HTTP %d %.160s", nick, code, resp);
done:
    memset(new_pass, 0, sizeof(new_pass));
    s_admin_reset_status = result;
    return NULL;
}

void net_admin_reset_password(const char *nick, const char *new_pass) {
    DSThread t;
    if (s_admin_reset_status == 1)
        return;
    if (!new_pass || !pass_valid(new_pass)) {
        s_admin_reset_status = 4;
        return;
    }
    if (!nick || !*nick || strlen(nick) >= sizeof(s_admin_reset_nick)) {
        s_admin_reset_status = 5;
        return;
    }
    snprintf(s_admin_reset_nick, sizeof(s_admin_reset_nick), "%s", nick);
    snprintf(s_admin_reset_new, sizeof(s_admin_reset_new), "%s", new_pass);
    s_admin_reset_status = 1;
    t = ds_thread_start(admin_reset_job, NULL);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    memset(s_admin_reset_new, 0, sizeof(s_admin_reset_new));
    s_admin_reset_status = 3;
}

double net_admin_reset_status(void) {
    int s = s_admin_reset_status;
    if (s >= 2)
        s_admin_reset_status = 0;
    return (double)s;
}
