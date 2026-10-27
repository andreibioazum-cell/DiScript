/* Room directory listing, creation, and FNV-1a password verification. */
#define ROOM_NAME_MAX 24
#define ROOM_LIST_MAX 64

static struct {
    DSMutex lock;
    int status; /* 0 idle, 1 loading, 2 ok, 4 error */
    int count;
    char names[ROOM_LIST_MAX][ROOM_NAME_MAX + 1];
} room_list = {.lock = DS_MUTEX_INIT};

static struct {
    DSMutex lock;
    int status; /* 0 idle, 1 busy, 2 ok, 3 wrong/needed password, 4 exists, 5 error, 6 bad name */
} room_prep = {.lock = DS_MUTEX_INIT};

static int room_name_valid(const char *name) {
    size_t i, n;
    if (!name)
        return 0;
    n = strlen(name);
    if (n < 1 || n > ROOM_NAME_MAX)
        return 0;
    for (i = 0; i < n; i++) {
        char c = name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return 1;
}

/* Hash room password as lowercase FNV-1a hex. */
static void room_pass_hash(const char *pass, char *out, size_t cap) {
    unsigned h = 2166136261u;
    const unsigned char *p = (const unsigned char *)(pass ? pass : "");
    while (*p) {
        h ^= *p++;
        h *= 16777619u;
    }
    if (!pass || !*pass)
        h = 0;
    snprintf(out, cap, "%08x", h);
}

static int parse_room_names(const char *json, char (*out)[ROOM_NAME_MAX + 1], int max_out) {
    const char *p;
    int count = 0;
    if (!json || !out || max_out <= 0)
        return 0;
    p = skip_ws(json);
    if (!p || *p != '{')
        return 0;
    for (p = skip_ws(p + 1); p && *p && *p != '}' && count < max_out;) {
        const char *kend = skip_str(p);
        size_t kl;
        if (!kend)
            break;
        kl = (size_t)(kend - p - 2);
        if (kl >= ROOM_NAME_MAX + 1)
            kl = ROOM_NAME_MAX;
        memcpy(out[count], p + 1, kl);
        out[count][kl] = 0;
        if (room_name_valid(out[count]))
            count++;
        p = skip_ws(kend);
        if (*p != ':')
            break;
        p = skip_ws(p + 1);
        p = skip_val(p);
        if (!p)
            break;
        p = skip_ws(p);
        if (p && *p == ',')
            p = skip_ws(p + 1);
    }
    return count;
}

typedef struct {
    char base[URL];
} RoomListJob;

static void *room_list_job(void *arg) {
    RoomListJob *job = (RoomListJob *)arg;
    char url[URL], resp[16384];
    char names[ROOM_LIST_MAX][ROOM_NAME_MAX + 1];
    int code, count, i;
    snprintf(url, sizeof(url), "%s/rooms.json?shallow=true", job->base);
    code = fb_http("GET", url, NULL, resp, sizeof(resp));
    count = code == 200 ? parse_room_names(resp, names, ROOM_LIST_MAX) : -1;
    if (code != 200 && net_log_ok())
        LOGERR("room list: HTTP %d for %.160s", code, url);
    ds_mutex_lock(room_list.lock);
    if (count >= 0) {
        room_list.count = count;
        for (i = 0; i < count; i++)
            snprintf(room_list.names[i], sizeof(room_list.names[i]), "%s", names[i]);
        room_list.status = 2;
    } else {
        room_list.status = 4;
    }
    ds_mutex_unlock(room_list.lock);
    free(job);
    return NULL;
}

void net_roomlist_fetch(const char *url) {
    const char *base = (url && *url) ? url : net.base;
    RoomListJob *job;
    DSThread t;
    if (!base || !*base)
        return;
    ds_mutex_lock(room_list.lock);
    room_list.status = 1;
    room_list.count = 0;
    ds_mutex_unlock(room_list.lock);
    job = (RoomListJob *)malloc(sizeof(*job));
    if (!job)
        return;
    snprintf(job->base, sizeof(job->base), "%s", base);
    t = ds_thread_start(room_list_job, job);
    if (t)
        ds_thread_detach(t);
    else {
        free(job);
        ds_mutex_lock(room_list.lock);
        room_list.status = 4;
        ds_mutex_unlock(room_list.lock);
    }
}

double net_roomlist_status(void) {
    double s;
    ds_mutex_lock(room_list.lock);
    s = (double)room_list.status;
    ds_mutex_unlock(room_list.lock);
    return s;
}

double net_roomlist_count(void) {
    double n;
    ds_mutex_lock(room_list.lock);
    n = (double)room_list.count;
    ds_mutex_unlock(room_list.lock);
    return n;
}

const char *net_roomlist_name(double idx) {
    static char ret[ROOM_NAME_MAX + 1];
    int i = (int)idx;
    ds_mutex_lock(room_list.lock);
    if (i < 0 || i >= room_list.count)
        ret[0] = 0;
    else
        snprintf(ret, sizeof(ret), "%s", room_list.names[i]);
    ds_mutex_unlock(room_list.lock);
    return ret;
}

static void room_prep_set(int status) {
    ds_mutex_lock(room_prep.lock);
    room_prep.status = status;
    ds_mutex_unlock(room_prep.lock);
}

/* Return connected room name, or empty string when offline. */
const char *net_current_room(void) {
    static char ret[ROOM_NAME_MAX + 1];
    lock();
    snprintf(ret, sizeof(ret), "%s", net.room);
    unlock();
    return ret;
}

double net_room_prep_status(void) {
    double s;
    ds_mutex_lock(room_prep.lock);
    s = (double)room_prep.status;
    ds_mutex_unlock(room_prep.lock);
    return s;
}

void net_room_prep_reset(void) { room_prep_set(0); }

typedef struct {
    char base[URL];
    char name[ROOM_NAME_MAX + 1];
    char pass[64];
    int create;
} RoomPrepJob;

static void *room_prep_job(void *arg) {
    RoomPrepJob *job = (RoomPrepJob *)arg;
    char url[URL], resp[1024], ph[16], want[16], owner[LOGIN_NICK_MAX + 1];
    int code;
    snprintf(url, sizeof(url), "%s/rooms/%s/meta.json", job->base, job->name);
    code = fb_http("GET", url, NULL, resp, sizeof(resp));
    if (job->create) {
        if (code == 200 && resp[0] && strcmp(resp, "null") != 0) {
            room_prep_set(4); /* already exists */
        } else if (code != 200 && code != 404) {
            room_prep_set(5);
        } else {
            char body[BODY];
            room_pass_hash(job->pass, ph, sizeof(ph));
            lg_lock();
            snprintf(owner, sizeof(owner), "%s", lg.session_nick);
            lg_unlock();
            if (!owner[0])
                snprintf(owner, sizeof(owner), "%s", "player");
            snprintf(body, sizeof(body), "{\"owner\":\"%s\",\"ph\":\"%s\",\"w\":\"%s\"}", owner,
                     job->pass[0] ? ph : "", net_write_secret());
            code = fb_http("PUT", url, body, NULL, 0);
            room_prep_set(code == 200 ? 2 : 5);
        }
    } else {
        if (code == 200 && resp[0] && strcmp(resp, "null") != 0) {
            strv(resp, "ph", want, sizeof(want));
            if (want[0]) {
                room_pass_hash(job->pass, ph, sizeof(ph));
                room_prep_set(strcmp(ph, want) == 0 ? 2 : 3);
            } else {
                room_prep_set(2);
            }
        } else if (code == 200 || code == 404) {
            room_prep_set(2); /* legacy room without meta: open */
        } else {
            room_prep_set(5);
        }
    }
    free(job);
    return NULL;
}

void net_room_prepare(const char *url, const char *name, const char *pass, double create) {
    const char *base = (url && *url) ? url : net.base;
    RoomPrepJob *job;
    DSThread t;
    if (!room_name_valid(name)) {
        room_prep_set(6);
        return;
    }
    if (!base || !*base) {
        room_prep_set(5);
        return;
    }
    room_prep_set(1);
    job = (RoomPrepJob *)malloc(sizeof(*job));
    if (!job) {
        room_prep_set(5);
        return;
    }
    snprintf(job->base, sizeof(job->base), "%s", base);
    snprintf(job->name, sizeof(job->name), "%s", name);
    snprintf(job->pass, sizeof(job->pass), "%s", pass ? pass : "");
    job->create = create ? 1 : 0;
    t = ds_thread_start(room_prep_job, job);
    if (t)
        ds_thread_detach(t);
    else {
        free(job);
        room_prep_set(5);
    }
}
