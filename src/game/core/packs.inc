/* Texture packs: any packed asset can be replaced by a file with the same
 * name inside <data>/packs/<pack>/. Archives dropped into that folder are
 * unpacked on start, so a pack can be shipped as a single .zip. */

#include <stdio.h>

#define PACKS_MAX 32
#define PACK_NAME_MAX 40
#define PACK_ZIP_MAX (4 * 1024 * 1024)
#define PACKS_LIST_MAX 3072

static char packs_dir[256];
static char packs_names[PACKS_MAX][PACK_NAME_MAX + 1];
static int packs_count;
static int packs_loaded;
static char packs_active[PACK_NAME_MAX + 1];
static char packs_msg[96];
static double packs_msg_t;

static void packs_msg_set(const char *m) {
    snprintf(packs_msg, sizeof(packs_msg), "%s", m ? m : "");
    packs_msg_t = 4.0;
}

static const char *packs_tr(const char *ru, const char *en) { return language == 1 ? ru : en; }

static int packs_root(void) {
    if (!packs_dir[0] && net_packs_root(packs_dir, sizeof(packs_dir)) != 1)
        packs_dir[0] = 0;
    return packs_dir[0] ? 1 : 0;
}

/* Folder that belongs to a pack name. */
static void packs_dir_of(const char *name, char *dst, size_t cap) {
    char root[256];
    if (packs_root() == 1)
        snprintf(root, sizeof(root), "%s", packs_dir);
    else
        snprintf(root, sizeof(root), "%s", "packs");
    snprintf(dst, cap, "%s/%s", root, name ? name : "");
}

static int packs_name_ok(const char *name) {
    size_t i, n = name ? strlen(name) : 0;
    if (n < 1 || n > PACK_NAME_MAX)
        return 0;
    if (name[0] == '.')
        return 0;
    for (i = 0; i < n; i++) {
        char c = name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == ' ' || c == '.'))
            return 0;
    }
    return 1;
}

static int packs_ends_with(const char *s, const char *tail) {
    size_t n = s ? strlen(s) : 0, t = tail ? strlen(tail) : 0;
    if (!n || !t || t > n)
        return 0;
    return strcmp(s + n - t, tail) == 0;
}

static void packs_add(const char *name) {
    if (packs_count >= PACKS_MAX || packs_name_ok(name) != 1)
        return;
    snprintf(packs_names[packs_count], PACK_NAME_MAX + 1, "%s", name);
    packs_count = packs_count + 1;
}

/* Unpack <data>/packs/<name>.zip into <data>/packs/<name>/; 1 on success. */
static int packs_unzip(const char *name) {
    static unsigned char zip[PACK_ZIP_MAX];
    static char entries[PACKS_LIST_MAX];
    char rel[128];
    int len, count, p = 0, i = 0, ok = 1;
    snprintf(rel, sizeof(rel), "%s.zip", name);
    len = net_packs_read(rel, zip, PACK_ZIP_MAX);
    if (len <= 0)
        return 0;
    count = zip_list(zip, len, entries, sizeof(entries));
    if (count <= 0)
        return 0;
    while (entries[p] && i < count) {
        char entry[128];
        int k = 0;
        while (entries[p] && entries[p] != '\n' && k < (int)sizeof(entry) - 1)
            entry[k++] = entries[p++];
        entry[k] = 0;
        if (entries[p] == '\n')
            p = p + 1;
        i = i + 1;
        if (!entry[0] || entry[k - 1] == '/')
            continue;
        if (strstr(entry, "..") != NULL || entry[0] == '/')
            continue;
        {
            static unsigned char data[1024 * 1024];
            char rel_file[192];
            int size = zip_extract(zip, len, entry, data, (int)sizeof(data));
            if (size <= 0) {
                ok = 0;
                continue;
            }
            snprintf(rel_file, sizeof(rel_file), "%s/%s", name, entry);
            if (net_packs_write(rel_file, data, size) != 1)
                ok = 0;
        }
    }
    return ok;
}

/* List packs, unpacking archives dropped next to the folders. */
static void packs_scan(void) {
    static char names[PACKS_LIST_MAX];
    int n, p = 0, seen = 0;
    packs_count = 0;
    memset(packs_names, 0, sizeof(packs_names));
    if (packs_root() != 1)
        return;
    net_packs_mkdir("");
    n = net_packs_list("", names, sizeof(names));
    while (names[p] && seen < n) {
        char one[128];
        int k = 0, is_dir = 0;
        while (names[p] && names[p] != '\n' && k < (int)sizeof(one) - 1)
            one[k++] = names[p++];
        one[k] = 0;
        if (names[p] == '\n')
            p = p + 1;
        seen = seen + 1;
        if (k > 1 && one[k - 1] == '/') {
            one[k - 1] = 0;
            is_dir = 1;
        }
        if (is_dir == 1) {
            packs_add(one);
        } else if (packs_ends_with(one, ".zip") == 1 && one[0] != '.') {
            one[k - 4] = 0;
            if (packs_name_ok(one) == 1 && packs_unzip(one) == 1)
                packs_add(one);
        }
    }
    packs_loaded = 1;
}

static void packs_save_active(void) {
    if (packs_root() != 1)
        return;
    net_packs_write("active.txt", packs_active, (int)strlen(packs_active));
}

static void packs_load_active(void) {
    char got[PACK_NAME_MAX + 2];
    int n;
    packs_active[0] = 0;
    if (packs_root() != 1)
        return;
    n = net_packs_read("active.txt", got, sizeof(got) - 1);
    if (n <= 0)
        return;
    while (n > 0 && (got[n - 1] == '\n' || got[n - 1] == '\r' || got[n - 1] == ' '))
        n = n - 1;
    got[n] = 0;
    if (packs_name_ok(got) == 1)
        snprintf(packs_active, sizeof(packs_active), "%s", got);
}

/* Point the asset loader at the active pack and refresh what is already on screen. */
static void packs_apply(void) {
    char dir[320];
    if (packs_active[0]) {
        packs_dir_of(packs_active, dir, sizeof(dir));
        ds_assets_pack_dir(dir);
    } else {
        ds_assets_pack_dir("");
    }
    ds_assets_reload_textures();
}

static void packs_set_active(const char *name) {
    if (!name || !*name || strcmp(name, packs_active) == 0) {
        packs_active[0] = 0;
        packs_msg_set(packs_tr("Пак выключен", "Pack off"));
    } else {
        snprintf(packs_active, sizeof(packs_active), "%s", name);
        packs_msg_set(packs_tr("Пак включён", "Pack on"));
    }
    packs_save_active();
    packs_apply();
    snprintf(packs_msg, sizeof(packs_msg), "%s: %s", packs_tr("Пак", "Pack"),
             packs_active[0] ? packs_active : packs_tr("обычные текстуры", "built-in textures"));
    packs_msg_t = 4.0;
    ds_log("texture pack active: %s", packs_active[0] ? packs_active : "(none)");
}

/* Import a .zip picked in the file manager: store it and unpack right away. */
static void packs_import_zip(const unsigned char *data, int len, const char *source) {
    char base[PACK_NAME_MAX + 1], rel[160];
    int i = 0, o = 0;
    const char *slash;
    if (!data || len <= 0 || len > PACK_ZIP_MAX) {
        packs_msg_set(packs_tr("Файл не подошёл", "Cannot use this file"));
        return;
    }
    if (packs_root() != 1) {
        packs_msg_set(packs_tr("Папка паков недоступна", "Pack folder is not available"));
        return;
    }
    slash = source ? strrchr(source, '/') : NULL;
    source = slash ? slash + 1 : (source ? source : "pack");
    while (source[i] && o < PACK_NAME_MAX) {
        char c = source[i++];
        if (c == '.')
            break;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
            c == ' ')
            base[o++] = c;
    }
    base[o] = 0;
    while (o > 0 && base[o - 1] == ' ')
        base[--o] = 0;
    if (!base[0])
        snprintf(base, sizeof(base), "%s", "pack");
    snprintf(rel, sizeof(rel), "%s.zip", base);
    if (net_packs_write(rel, data, len) != 1) {
        packs_msg_set(packs_tr("Не удалось записать .zip", "Could not write the .zip"));
        return;
    }
    if (packs_unzip(base) != 1) {
        packs_msg_set(packs_tr("В архиве нет файлов", "The archive has no files"));
        return;
    }
    packs_scan();
    packs_set_active(base);
    packs_msg_set(packs_tr("Пак установлен", "Pack installed"));
    ds_log("texture pack imported: %s", base);
}

/* Called once on start, before the first frame is drawn. */
static void packs_init(void) {
    packs_root();
    packs_load_active();
    packs_scan();
    {
        char dir[320];
        if (packs_active[0]) {
            packs_dir_of(packs_active, dir, sizeof(dir));
            ds_assets_pack_dir(dir);
        }
    }
    if (packs_active[0])
        ds_log("texture pack: %s", packs_active);
}
