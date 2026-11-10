#define SETTINGS_FILE "settings.dat"

typedef struct {
    DSMutex lock;
    int loaded, has_file;
    int language, hitboxes, musicvol, winter, theme, mode, chosen, graphics;
    int legal;
    int ppin, page; /* Parental control: device-only pin hash and age mode. */
    int pin2;       /* Extra account password: device-only hash, never leaves the phone. */
    int pdc, pdv; /* Personal data consent timestamp and revision. */
} Settings;
static Settings st = {.lock = DS_MUTEX_INIT};

static void st_lock(void) { ds_mutex_lock(st.lock); }
static void st_unlock(void) { ds_mutex_unlock(st.lock); }

static char sys_lang[8]; /* Device ISO 639-1 language code. */

void net_set_system_language(const char *code) {
    st_lock();
    snprintf(sys_lang, sizeof(sys_lang), "%s", code ? code : "");
    st_unlock();
}

/* Default language on fresh install: Russian for "ru" or unknown locale, English otherwise. */
static int system_language_default(void) {
    int ru;
    st_lock();
    ru = !sys_lang[0] || (sys_lang[0] == 'r' && sys_lang[1] == 'u');
    st_unlock();
    return ru;
}
static void settings_read(void) {
    char path[320], line[128];
    FILE *f;
        /* -1: no language in the file, use the phone language. */
    int language = -1, hitboxes = 1, musicvol = 70, has_file = 0;
    int winter = 0, legal = 0, theme = 1, mode = 0, graphics = 0, pdc = 0, pdv = 0;
    int ppin = 0, page = 0, pin2 = 0;
    /* -1 = no saved choice yet; an old style line counts as a choice. */
    int chosen = -1;
    st_lock();
    if (st.loaded) {
        st_unlock();
        return;
    }
    st_unlock();
    data_file_path(path, sizeof(path), SETTINGS_FILE);
    f = fopen(path, "r");
    if (f) {
        has_file = 1;
        while (fgets(line, sizeof(line), f)) {
            int value = 0;
            if (sscanf(line, "lang %d", &value) == 1) {
                language = value;
                continue;
            }
            if (sscanf(line, "hitboxes %d", &value) == 1) {
                hitboxes = value;
                continue;
            }
            if (sscanf(line, "musicvol %d", &value) == 1) {
                musicvol = value;
                continue;
            }
            if (sscanf(line, "winter %d", &value) == 1) {
                winter = value;
                continue;
            }
            if (sscanf(line, "theme %d", &value) == 1) {
                theme = value;
                continue;
            }
            if (sscanf(line, "style %d", &value) == 1) {
                mode = value;
                if (chosen < 0) {
                    chosen = 1;
                }
                continue;
            }
            if (sscanf(line, "chosen %d", &value) == 1) {
                chosen = value;
                continue;
            }
            if (sscanf(line, "graphics %d", &value) == 1) {
                graphics = value;
                continue;
            }
            if (sscanf(line, "legal %d", &value) == 1) {
                legal = value;
                continue;
            }
            if (sscanf(line, "ppin %d", &value) == 1) {
                ppin = value;
                continue;
            }
            if (sscanf(line, "pin2 %d", &value) == 1) {
                pin2 = value;
                continue;
            }
            if (sscanf(line, "page %d", &value) == 1) {
                page = value;
                continue;
            }
            if (sscanf(line, "pdc %d", &value) == 1) {
                pdc = value;
                continue;
            }
            if (sscanf(line, "pdv %d", &value) == 1) {
                pdv = value;
                continue;
            }
        }
        fclose(f);
    }
    if (language < 0)
        language = system_language_default();
    st_lock();
    st.language = language ? 1 : 0;
    st.hitboxes = hitboxes ? 1 : 0;
    st.musicvol = musicvol < 0 ? 0 : (musicvol > 100 ? 100 : musicvol);
    st.winter = winter ? 1 : 0;
    st.theme = theme ? 1 : 0;
    st.mode = mode >= 0 && mode <= 1 ? mode : 0;
    st.chosen = chosen > 0 ? 1 : 0;
    st.graphics = graphics >= 0 && graphics <= 2 ? graphics : 0;
    st.legal = legal;
    st.ppin = ppin > 0 ? ppin : 0;
    st.pin2 = pin2 > 0 ? pin2 : 0;
    st.page = page >= 0 && page <= 2 ? page : 0;
    st.pdc = pdc > 0 ? pdc : 0;
    st.pdv = pdc > 0 && pdv > 0 ? pdv : 0;
    st.has_file = has_file;
    st.loaded = 1;
    st_unlock();
}
static void settings_write(void) {
    char path[320], text[256];
    settings_read();
    data_file_path(path, sizeof(path), SETTINGS_FILE);
    st_lock();
    snprintf(text, sizeof(text),
             "lang %d\nhitboxes %d\nmusicvol %d\nwinter %d\ntheme %d\nstyle %d\nchosen %d\ngraphics %d\nlegal %d\n"
             "ppin %d\npin2 %d\npage %d\npdc %d\npdv %d\n",
             st.language, st.hitboxes, st.musicvol, st.winter, st.theme, st.mode, st.chosen, st.graphics, st.legal,
             st.ppin, st.pin2, st.page, st.pdc, st.pdv);
    st.has_file = 1;
    st_unlock();
    file_write_atomic(path, text);
}
static void settings_push_to_cloud(void) {
    char nick[LOGIN_NICK_MAX + 1] = "";
    char body[BODY];
    int language, hitboxes, musicvol, winter;
    settings_read();
    st_lock();
    language = st.language;
    hitboxes = st.hitboxes;
    musicvol = st.musicvol;
    winter = st.winter;
    st_unlock();
    lg_lock();
    if (lg.status == NET_LOGIN_OK && lg.session_nick[0]) {
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    }
    lg_unlock();
    if (!nick[0] || !net.base[0])
        return;
    char url[URL];
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    snprintf(body, sizeof(body), "{\"lang\":%d,\"hitboxes\":%d,\"musicvol\":%d,\"winter\":%d}",
             language, hitboxes, musicvol, winter);
    http_patch_async(url, body);
}
double net_load_language(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.language;
    st_unlock();
    return v;
}
double net_load_hitboxes(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.hitboxes;
    st_unlock();
    return v;
}

double net_load_music_volume(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.musicvol;
    st_unlock();
    return v;
}

void net_save_music_volume(double volume) {
    int v = (int)volume;
    if (v < 0)
        v = 0;
    if (v > 100)
        v = 100;
    settings_read();
    st_lock();
    st.musicvol = v;
    st.loaded = 1;
    st_unlock();
    settings_write();
    settings_push_to_cloud();
}

double net_load_winter_theme(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.winter;
    st_unlock();
    return v;
}

void net_save_winter_theme(double on) {
    settings_read();
    st_lock();
    st.winter = on ? 1 : 0;
    st.loaded = 1;
    st_unlock();
    settings_write();
    settings_push_to_cloud();
}

/* Return local menu theme flag. */
double net_load_new_theme(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.theme;
    st_unlock();
    return v;
}

void net_save_new_theme(double on) {
    settings_read();
    st_lock();
    st.theme = on ? 1 : 0;
    st.loaded = 1;
    st_unlock();
    settings_write();
}

/* Menu theme: 0 ordinary, 1 New Year. Anything else is treated as ordinary. */
double net_load_theme_mode(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.mode;
    st_unlock();
    return v;
}

void net_save_theme_mode(double mode) {
    settings_read();
    st_lock();
    st.mode = mode >= 0 && mode <= 1 ? (int)mode : 0;
    st.loaded = 1;
    st_unlock();
    settings_write();
}

/* Whether the player ever pressed the theme button. Old saves with a style
 * line count as an explicit choice, so their theme is respected at once. */
double net_load_theme_chosen(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.chosen;
    st_unlock();
    return v;
}

void net_save_theme_chosen(double on) {
    settings_read();
    st_lock();
    st.chosen = on ? 1 : 0;
    st.loaded = 1;
    st_unlock();
    settings_write();
}

/* Rendering preference is device-local: a phone and a PC can support different
 * antialiasing paths, so this is intentionally not synchronized to cloud. */
double net_load_graphics_mode(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.graphics;
    st_unlock();
    return v;
}

void net_save_graphics_mode(double mode) {
    settings_read();
    st_lock();
    st.graphics = mode >= 0 && mode <= 2 ? (int)mode : 0;
    st.loaded = 1;
    st_unlock();
    settings_write();
}

void net_save_settings(double language, double hitboxes) {
    settings_read();
    if (language < 0)
        language = system_language_default();
    st_lock();
    st.language = language ? 1 : 0;
    st.hitboxes = hitboxes ? 1 : 0;
    st.loaded = 1;
    st_unlock();
    settings_write();
    settings_push_to_cloud();
}

int settings_sync_with_cloud(const char *resp) {
    int had_file, language, hitboxes, musicvol, winter;
    settings_read();
    st_lock();
    had_file = st.has_file;
    st_unlock();
    if (had_file) {
        settings_push_to_cloud();
        return 1;
    }
    language = num_int(resp, "lang", -1);
    hitboxes = num_int(resp, "hitboxes", -1);
    musicvol = num_int(resp, "musicvol", -1);
    winter = num_int(resp, "winter", -1);
    if (language < 0 && hitboxes < 0 && musicvol < 0 && winter < 0)
        return 0;
    st_lock();
    if (language >= 0)
        st.language = language ? 1 : 0;
    if (hitboxes >= 0)
        st.hitboxes = hitboxes ? 1 : 0;
    if (musicvol >= 0)
        st.musicvol = musicvol > 100 ? 100 : musicvol;
    if (winter >= 0)
        st.winter = winter ? 1 : 0;
    st.loaded = 1;
    st_unlock();
    settings_write();
    if (net_log_ok())
        LOG("settings restored from cloud (lang=%d hitboxes=%d musicvol=%d winter=%d)", language, hitboxes, musicvol,
            winter);
    return 0;
}

void settings_mark_legal(void) {
    settings_read();
    st_lock();
    st.legal = (int)time(NULL);
    st.loaded = 1;
    st_unlock();
    settings_write();
}

double settings_legal_ts(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.legal;
    st_unlock();
    return v;
}

/* Record personal data consent version and timestamp locally (0 = withdrawn). */
void settings_set_pd_consent(double version) {
    settings_read();
    st_lock();
    st.pdv = version > 0 ? (int)version : 0;
    st.pdc = version > 0 ? (int)time(NULL) : 0;
    st.loaded = 1;
    st_unlock();
    settings_write();
}

double settings_pd_consent_version(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.pdv;
    st_unlock();
    return v;
}

/* Upload personal data consent version and timestamp to cloud profile. */
void net_pd_consent_upload(void) {
    char nick[LOGIN_NICK_MAX + 1] = "", url[URL], body[BODY];
    int pdc, pdv;
    settings_read();
    st_lock();
    pdc = st.pdc;
    pdv = st.pdv;
    st_unlock();
    if (pdc <= 0 || pdv <= 0)
        return;
    lg_lock();
    if (lg.status == NET_LOGIN_OK && lg.session_nick[0])
        snprintf(nick, sizeof(nick), "%s", lg.session_nick);
    lg_unlock();
    if (!nick[0] || !net.base[0])
        return;
    snprintf(url, sizeof(url), "%s/users/%s.json", net.base, nick);
    snprintf(body, sizeof(body), "{\"pd_consent_ts\":%d,\"pd_consent_ver\":%d}", pdc, pdv);
    http_patch_async(url, body);
}

/* Device-only parental control: pin hash and age mode never leave the phone. */
double net_load_parent_pin(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.ppin;
    st_unlock();
    return v;
}

void net_save_parent_pin(int hash) {
    settings_read();
    st_lock();
    st.ppin = hash > 0 ? hash : 0;
    st_unlock();
    settings_write();
}

/* Extra account password: same device-only rule as the parental pin. */
double net_load_pin2(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.pin2;
    st_unlock();
    return v;
}

void net_save_pin2(int hash) {
    settings_read();
    st_lock();
    st.pin2 = hash > 0 ? hash : 0;
    st_unlock();
    settings_write();
}

double net_load_parent_age(void) {
    double v;
    settings_read();
    st_lock();
    v = (double)st.page;
    st_unlock();
    return v;
}

void net_save_parent_age(int age) {
    settings_read();
    st_lock();
    st.page = age >= 0 && age <= 2 ? age : 0;
    st_unlock();
    settings_write();
}

