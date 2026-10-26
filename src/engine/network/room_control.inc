typedef struct {
    char url[FB_URL_MAX];
    char nick[LOGIN_NICK_MAX + 1];
    int banned;
} BanJob;
static void *ban_write_job(void *arg) {
    BanJob *j = (BanJob *)arg;
    int code;
    if (!j)
        return NULL;
    if (j->banned) {
        code = fb_http("PUT", j->url, "true", NULL, 0);
        if (code == 200) {
            LOG("ban %s: saved to cloud", j->nick);
            ban_sync();
        } else {
            LOGERR("ban %s: cloud write FAILED (HTTP %d), ban is local only", j->nick, code);
        }
    } else {
        code = fb_http("DELETE", j->url, NULL, NULL, 0);
        if (code == 200) {
            LOG("unban %s: removed from cloud", j->nick);
            ban_sync();
        } else {
            LOGERR("unban %s: cloud delete FAILED (HTTP %d)", j->nick, code);
        }
    }
    free(j);
    return NULL;
}
static void ban_write_async(const char *url, const char *nick, int banned) {
    BanJob *j = (BanJob *)malloc(sizeof(*j));
    DSThread t;
    if (!j)
        return;
        /* Store plain URL; fb_http attaches and refreshes auth token. */
        snprintf(j->url, sizeof(j->url), "%s", url);
    snprintf(j->nick, sizeof(j->nick), "%s", nick);
    j->banned = banned;
    t = ds_thread_start(ban_write_job, j);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    free(j);
}

void net_ban_set(const char *nick, double banned) {
    if (!nick || !*nick)
        return;
    if (banned >= 0.5 && ban_protected_admin_nick(nick)) {
        LOGERR("ban: refusing to ban protected admin '%s'", nick);
        return;
    }
    if (!nick_valid(nick)) {
        LOGERR("ban: invalid nick '%s'", nick);
        return;
    }
    if (banned >= 0.5) {
        add_ban_to_file(nick);
        ban_cache_put(nick);
        if (net.base[0]) {
            char url[URL];
            snprintf(url, sizeof(url), "%s/bans/%s.json", net.base, nick);
            ban_write_async(url, nick, 1);
        } else {
            LOGERR("ban %s: no server, ban is local only", nick);
        }
        LOG("ban %s", nick);
    } else {
        remove_ban_from_file(nick);
        ban_cache_drop(nick);
        if (net.base[0]) {
            char url[URL];
            snprintf(url, sizeof(url), "%s/bans/%s.json", net.base, nick);
            ban_write_async(url, nick, 0);
        }
        LOG("unban %s", nick);
    }
}

/* Write active event mode to Firebase with bounded retries. */
static void *event_write_job(void *arg) {
    HttpJob *j = (HttpJob *)arg;
    int code = 0, attempt;
    if (!j)
        return NULL;
    for (attempt = 0; attempt < 3; attempt++) {
        if (attempt > 0)
            sleep_ms(250 * attempt);
        code = fb_http("PUT", j->url, j->body, NULL, 0);
        if (code == 200)
            break;
    }
    if (code == 200)
        LOG("event saved to cloud (%s)", j->body);
    else
        LOGERR("event write FAILED (HTTP %d), event is local only", code);
    free(j);
    return NULL;
}
static void event_write_async(const char *url, const char *body) {
    HttpJob *j = (HttpJob *)malloc(sizeof(*j));
    DSThread t;
    if (!j)
        return;
    snprintf(j->url, sizeof(j->url), "%s", url);
    snprintf(j->body, sizeof(j->body), "%s", body);
    t = ds_thread_start(event_write_job, j);
    if (t) {
        ds_thread_detach(t);
        return;
    }
    free(j);
}

/* Hold locally selected event mode briefly while server write is in flight. */
#define EVENT_LOCAL_HOLD_MS 3000
static long long event_local_until_ms;

void net_event_set(double mode) {
    int m = (int)mode;
    if (m < 0)
        m = 0;
    /* Event modes: 0 off, 1 disco, 2 plates, 3 Ded Moroz, 4 cube, 5 Columns. */
    if (m > 5)
        m = 5;
    if (!net.base[0])
        return;
    char url[URL], body[32];
    snprintf(url, sizeof(url), "%s/event.json", net.base);
    snprintf(body, sizeof(body), "%d", m);
    event_write_async(url, body);
    lock();
    net.event = m;
    event_local_until_ms = now_ms() + EVENT_LOCAL_HOLD_MS;
    unlock();
    LOG("event set %d", m);
}

static const char *banner_color_names[] = {"white",  "red",    "green", "blue", "yellow",
                                           "orange", "purple", "pink",  "cyan", "black"};
static int banner_color_known(const char *word) {
    size_t i;
    if (!word || !*word)
        return 0;
    for (i = 0; i < sizeof(banner_color_names) / sizeof(banner_color_names[0]); i++)
        if (!strcmp(word, banner_color_names[i]))
            return 1;
    return 0;
}
double net_chat_is_text_cmd(const char *msg) {
    if (!msg)
        return 0;
    if (strlen(msg) >= 5 && strncmp(msg, "text ", 5) == 0) {
        const char *t = msg + 5;
        while (*t == ' ' || *t == '\t') t++;
        if (*t == '"') {
            /* Reject empty or unterminated quoted banner text. */
            const char *close = strchr(t + 1, '"');
            return close && close > t + 1;
        }
        if (*t)
            return 1;
    }
    return 0;
}
static void banner_split_cmd(const char *msg, char *text_out, size_t tcap, char *color_out, size_t ccap) {
    const char *t, *c, *last_space = NULL;
    char word[BANNER_COLOR_MAX];
    size_t n;
    if (tcap)
        text_out[0] = 0;
    snprintf(color_out, ccap, "white");
    if (!msg || strncmp(msg, "text ", 5) != 0)
        return;
    t = msg + 5;
    while (*t == ' ' || *t == '\t') t++;

    if (*t == '"') {
        /* Parse quoted banner text and optional trailing color name. */
        const char *close = strchr(t + 1, '"');
        if (close) {
            n = (size_t)(close - (t + 1));
            if (n >= tcap)
                n = tcap - 1;
            memcpy(text_out, t + 1, n);
            text_out[n] = 0;
            c = close + 1;
            while (*c == ' ' || *c == '\t') c++;
            if (*c) {
                snprintf(word, sizeof(word), "%s", c);
                n = strlen(word);
                while (n > 0 && (word[n - 1] == ' ' || word[n - 1] == '\t')) word[--n] = 0;
                if (banner_color_known(word))
                    snprintf(color_out, ccap, "%s", word);
            }
            return;
        }
    }

    for (c = t; *c; c++)
        if ((*c == ' ' || *c == '\t') && c[1] && c[1] != ' ' && c[1] != '\t')
            last_space = c;
    if (last_space) {

        snprintf(word, sizeof(word), "%s", last_space + 1);
        n = strlen(word);
        while (n > 0 && (word[n - 1] == ' ' || word[n - 1] == '\t')) word[--n] = 0;
        if (banner_color_known(word)) {
            n = (size_t)(last_space - t);
            while (n > 0 && (t[n - 1] == ' ' || t[n - 1] == '\t')) n--;
            if (n >= tcap)
                n = tcap - 1;
            memcpy(text_out, t, n);
            text_out[n] = 0;
            snprintf(color_out, ccap, "%s", word);
            return;
        }
    }
    snprintf(text_out, tcap, "%s", t);
    /* Strip surrounding quotes before storing banner text. */
    n = strlen(text_out);
    if (n >= 2 && text_out[0] == '"' && text_out[n - 1] == '"') {
        memmove(text_out, text_out + 1, n - 2);
        text_out[n - 2] = 0;
    }
}
const char *net_chat_text_cmd_text(const char *msg) {
    banner_split_cmd(msg, s_text_cmd_text_ret, sizeof(s_text_cmd_text_ret), s_text_cmd_color_ret,
                     sizeof(s_text_cmd_color_ret));
    return s_text_cmd_text_ret;
}
const char *net_chat_text_cmd_color(const char *msg) {
    banner_split_cmd(msg, s_text_cmd_text_ret, sizeof(s_text_cmd_text_ret), s_text_cmd_color_ret,
                     sizeof(s_text_cmd_color_ret));
    return s_text_cmd_color_ret;
}

void net_banner_clear(void) {
    char url[URL];
    if (!net.started || !net.run)
        return;
    snprintf(url, sizeof(url), "%s/banner.json", net.base);
    event_write_async(url, "null");
    LOG("banner cleared");
}

void net_banner_send(const char *text, const char *color) {
    char url[URL], body[BODY], esc[CHAT_TEXT_MAX * 2], ecol[BANNER_COLOR_MAX * 2];
    if (!net.started || !net.run)
        return;
    if (!text || !*text)
        return;
    if (!color || !*color)
        color = "white";
    json_escape(text, esc, sizeof(esc));
    json_escape(color, ecol, sizeof(ecol));
    snprintf(url, sizeof(url), "%s/banner.json", net.base);
    snprintf(body, sizeof(body), "{\"ts\":%lld,\"text\":\"%s\",\"color\":\"%s\"}", now_ms(), esc, ecol);
    event_write_async(url, body);
    LOG("banner send: %s (%s)", text, color);
}

double net_chat_is_ban(const char *msg) {
    if (!msg)
        return 0;

    if (strlen(msg) >= 4 && strncmp(msg, "ban ", 4) == 0) {

        const char *t = msg + 4;
        while (*t == ' ' || *t == '\t') t++;
        if (*t)
            return 1;
    }
    return 0;
}
double net_chat_is_unban(const char *msg) {
    if (!msg)
        return 0;
    if (strlen(msg) >= 6 && strncmp(msg, "unban ", 6) == 0) {
        const char *t = msg + 6;
        while (*t == ' ' || *t == '\t') t++;
        if (*t)
            return 1;
    }
    return 0;
}
const char *net_chat_ban_target(const char *msg) {
    s_ban_target_ret[0] = 0;
    if (!msg)
        return s_ban_target_ret;
    if (strncmp(msg, "ban ", 4) != 0)
        return s_ban_target_ret;
    const char *t = msg + 4;
    while (*t == ' ' || *t == '\t') t++;
    size_t i = 0;
    while (t[i] && i < sizeof(s_ban_target_ret) - 1 && t[i] != ' ' && t[i] != '\t' && t[i] != '\n' && t[i] != '\r') {
        s_ban_target_ret[i] = t[i];
        i++;
    }
    s_ban_target_ret[i] = 0;
    return s_ban_target_ret;
}
const char *net_chat_unban_target(const char *msg) {
    s_ban_target_ret[0] = 0;
    if (!msg)
        return s_ban_target_ret;
    if (strncmp(msg, "unban ", 6) != 0)
        return s_ban_target_ret;
    const char *t = msg + 6;
    while (*t == ' ' || *t == '\t') t++;
    size_t i = 0;
    while (t[i] && i < sizeof(s_ban_target_ret) - 1 && t[i] != ' ' && t[i] != '\t' && t[i] != '\n' && t[i] != '\r') {
        s_ban_target_ret[i] = t[i];
        i++;
    }
    s_ban_target_ret[i] = 0;
    return s_ban_target_ret;
}

/* Admin command: passwd <nick> <new password> (no spaces in either). */
static const char *passwd_args(const char *msg) {
    if (!msg || strncmp(msg, "passwd ", 7) != 0)
        return NULL;
    const char *t = msg + 7;
    while (*t == ' ' || *t == '\t') t++;
    return *t ? t : NULL;
}
double net_chat_is_passwd(const char *msg) {
    const char *t = passwd_args(msg);
    if (!t)
        return 0;
    while (*t && *t != ' ' && *t != '\t') t++;
    while (*t == ' ' || *t == '\t') t++;
    return *t ? 1 : 0;
}
const char *net_chat_passwd_target(const char *msg) {
    const char *t = passwd_args(msg);
    size_t i = 0;
    s_passwd_target_ret[0] = 0;
    if (!t)
        return s_passwd_target_ret;
    while (t[i] && i < sizeof(s_passwd_target_ret) - 1 && t[i] != ' ' && t[i] != '\t') {
        s_passwd_target_ret[i] = t[i];
        i++;
    }
    s_passwd_target_ret[i] = 0;
    return s_passwd_target_ret;
}
const char *net_chat_passwd_new(const char *msg) {
    const char *t = passwd_args(msg);
    size_t i = 0;
    s_passwd_new_ret[0] = 0;
    if (!t)
        return s_passwd_new_ret;
    while (*t && *t != ' ' && *t != '\t') t++;
    while (*t == ' ' || *t == '\t') t++;
    while (t[i] && i < sizeof(s_passwd_new_ret) - 1 && t[i] != ' ' && t[i] != '\t') {
        s_passwd_new_ret[i] = t[i];
        i++;
    }
    s_passwd_new_ret[i] = 0;
    return s_passwd_new_ret;
}

typedef struct {
    char nick[24];
    int cups;
} LeaderboardEntry;

typedef struct {
    DSMutex lock;
    int status;
    int count;
    LeaderboardEntry entries[16];
} Leaderboard;
static Leaderboard lb = {.lock = DS_MUTEX_INIT};
