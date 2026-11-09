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

/* Make every folder of a relative path ("pack/textures"), so an archive with
 * subfolders can be unpacked: a pack is usually textures/ + gifts/. */
static void packs_mkdir_parents(const char *rel) {
    char path[192];
    int i;
    snprintf(path, sizeof(path), "%s", rel);
    for (i = 0; path[i]; i++) {
        if (path[i] != '/')
            continue;
        path[i] = 0;
        net_packs_mkdir(path);
        path[i] = '/';
    }
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
            packs_mkdir_parents(rel_file);
            if (net_packs_write(rel_file, data, size) != 1) {
                ds_log("packs: cannot write %s", rel_file);
                ok = 0;
            }
        }
    }
    return ok;
}

/* List packs, unpacking archives dropped next to the folders. Every folder in
 * <data>/packs is a pack; a .zip next to it is unpacked once and then counts as
 * one too, so an exported archive can simply be dropped into the folder. */
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

/* --- texture pack export ---------------------------------------------------
 * The built-in textures are the base of every pack, so the game hands them out
 * as a ready .zip: unzip it, repaint the PNGs you want, zip it back and import
 * it through the packs screen. Entries are stored uncompressed, exactly what
 * the pack reader expects. */

#define PACK_EXPORT_MAX (6 * 1024 * 1024)
#define PACK_EXPORT_PNG_MAX 64
#define PACK_EXPORT_FILES (PACK_EXPORT_PNG_MAX + 1)
#define PACK_EXPORT_LIST 16384
#define PACK_CHUNK_MAX (1024 * 1024)
#define PACK_EXPORT_NAME_MAX 96
#define PACK_EXPORT_FILE "cb4_textures.zip"
#define PACK_EXPORT_GUIDE_FILE "TEXTURE_NAMES.txt"

/* Raw texture bytes and the archive are two buffers on purpose: zip_build
 * writes the local headers from the start of its output, so it must not share
 * memory with the data it is copying. */
static unsigned char pack_export_data[PACK_EXPORT_MAX];
static unsigned char *pack_export_zip;
static unsigned char pack_chunk[PACK_CHUNK_MAX];
static int pack_export_size;
static double pack_export_wait;
static ZipEntry pack_export_entries[PACK_EXPORT_FILES];
static char pack_export_names[PACK_EXPORT_FILES][PACK_EXPORT_NAME_MAX];
static char pack_export_sources[PACK_EXPORT_FILES][PACK_EXPORT_NAME_MAX];

/* This file travels inside every exported archive. The filename, not a sprite's
 * appearance, is what connects it to a fighter and a pose. */
static const unsigned char pack_export_guide[] =
    "CUBIC BATTLE 4 TEXTURE PACK - FILE NAMES\n"
    "==========================================\n\n"
    "Keep every filename exactly as written. The filename selects the character\n"
    "and pose; the PNG pixels only change its appearance.\n\n"
    "FIGHTER SPRITES\n"
    "ordinary.png                 Ordinary Cube - idle\n"
    "ordinary_punch.png           Ordinary Cube - punch\n"
    "ordinary_gold.png            Pumpkin Cube special skin - idle\n"
    "ordinary_gold_punch.png      Pumpkin Cube special skin - punch\n"
    "azum.png                     Azum - idle\n"
    "azum_punch.png               Azum - punch\n"
    "zombie_azum.png              Azum Zombie skin - idle\n"
    "zombie_azum_punch.png        Azum Zombie skin - punch\n"
    "santa.png                    Santa - idle\n"
    "santa_punch.png              Santa - punch\n"
    "ebuc.png                     buK / ebuC - idle\n"
    "ebuc_punch.png               buK / ebuC - punch\n"
    "ebuc_elf.png                 Elf buK skin - idle\n"
    "ebuc_elf_punch.png           Elf buK skin - punch\n"
    "astra.png                    Astra - idle\n"
    "astra_punch.png              Astra - punch\n\n"
    "ABILITY AND WORLD SPRITES\n"
    "dispenser.png                buK dispenser\n"
    "ebuc_elf_dispenser.png       Elf buK dispenser\n"
    "grass.png                    arena ground\n"
    "snow.png                     snow ground\n"
    "snowflake.png                Santa snowflake projectile\n"
    "candy.png                    candy pickup\n"
    "Event columns.png            Columns event image\n"
    "npc_cube.png                 lobby cube NPC\n"
    "npc_helpy.png                lobby Helpy NPC\n\n"
    "SHOP CARD IMAGES\n"
    "ordinary_card.png, azum_card.png, zombie_azum_card.png, santa_card.png\n"
    "ebuc_card.png, ebuc_elf_card.png, astra_card.png, astra_xastra_card.png\n"
    "codecracker_card.png         class-card renders only, not battle sprites\n\n"
    "GIFT STICKERS\n"
    "gift_bear.png, gift_cake.png, gift_emerald.png, gift_poop.png, gift_rocket.png\n\n"
    "EXAMPLE: to give the Ordinary Cube Santa art, copy BOTH files:\n"
    "  santa.png       -> ordinary.png\n"
    "  santa_punch.png -> ordinary_punch.png\n"
    "Do not edit ordinary_gold.png or ordinary_gold_punch.png for that job: they\n"
    "belong only to the separate Pumpkin skin.\n\n"
    "IMPORT SAFETY\n"
    "Importing enables this custom pack only. It never overwrites the game's\n"
    "built-in textures. In the Packs screen choose Built-in / Obychnye to turn\n"
    "the custom pack off and return to normal textures.\n";

/* Remember one PNG by the name the loader asks for it. */
static int pack_export_add(int used, const char *source) {
    const char *file = strrchr(source, '/');
    int n;
    file = file ? file + 1 : source;
    n = snprintf(pack_export_names[used], PACK_EXPORT_NAME_MAX, "%s", file);
    if (n <= 0 || n >= PACK_EXPORT_NAME_MAX)
        return 0;
    snprintf(pack_export_sources[used], PACK_EXPORT_NAME_MAX, "%s", source);
    return 1;
}

/* Build the archive in memory. Returns its size, or -1 with the message set. */
static int pack_export_build(void) {
    static char listing[PACK_EXPORT_LIST];
    int used = 0, p = 0, i, total = 0, size, count = 0;
    packs_msg_set(packs_tr("Готовлю пак...", "Building the pack..."));
    if (ds_asset_list("", listing, sizeof(listing)) <= 0) {
        packs_msg_set(packs_tr("Текстуры не читаются", "Cannot read the textures"));
        return -1;
    }
    /* Listing lines are "f|<name>|" / "d|<name>|". */
    while (listing[p] && used < PACK_EXPORT_PNG_MAX) {
        char kind = listing[p], name[160];
        int k = 0;
        p = p + 2;
        while (listing[p] && listing[p] != '|' && k < (int)sizeof(name) - 1)
            name[k++] = listing[p++];
        name[k] = 0;
        if (listing[p] == '|')
            p = p + 1;
        if (listing[p] == '\n')
            p = p + 1;
        if (kind != 'f' || k == 0 || packs_ends_with(name, ".png") != 1)
            continue;
        /* Flat on purpose: the pack loader looks files up by their bare asset
         * name (grass.png, gift_bear.png), so the archive mirrors that. */
        if (pack_export_add(used, name) == 1)
            used = used + 1;
    }
    if (used <= 0) {
        packs_msg_set(packs_tr("В игре нет текстур", "The game has no textures"));
        return -1;
    }
    for (i = 0; i < used; i++) {
        size = ds_asset_extract(pack_export_sources[i], pack_chunk, PACK_CHUNK_MAX);
        if (size <= 0)
            continue;
        if (total + size > PACK_EXPORT_MAX)
            break;
        memcpy(pack_export_data + total, pack_chunk, (size_t)size);
        pack_export_entries[count].name = pack_export_names[i];
        pack_export_entries[count].data = pack_export_data + total;
        pack_export_entries[count].len = (unsigned int)size;
        total = total + size;
        count = count + 1;
    }
    if (total <= 0) {
        packs_msg_set(packs_tr("Текстуры не читаются", "Cannot read the textures"));
        return -1;
    }
    /* Add a filename guide after the PNGs. It is a normal text file and is
     * ignored by the texture loader when the archive is imported. */
    if (count >= PACK_EXPORT_FILES) {
        packs_msg_set(packs_tr("Слишком много текстур", "Too many textures"));
        return -1;
    }
    snprintf(pack_export_names[count], PACK_EXPORT_NAME_MAX, "%s", PACK_EXPORT_GUIDE_FILE);
    pack_export_entries[count].name = pack_export_names[count];
    pack_export_entries[count].data = pack_export_guide;
    pack_export_entries[count].len = (unsigned int)(sizeof(pack_export_guide) - 1);
    count = count + 1;
    if (!pack_export_zip) {
        pack_export_zip = (unsigned char *)malloc(PACK_EXPORT_MAX);
        if (!pack_export_zip) {
            packs_msg_set(packs_tr("Пак не помещается в память", "The pack does not fit in memory"));
            return -1;
        }
    }
    size = zip_build(pack_export_entries, count, pack_export_zip, PACK_EXPORT_MAX);
    if (size <= 0) {
        packs_msg_set(packs_tr("Пак не помещается в память", "The pack does not fit in memory"));
        return -1;
    }
    ds_log("texture pack export: %d files, %d bytes", count, size);
    return size;
}

/* Finish an export that waits for the Android document picker: the picker
 * returns its file descriptor a few frames later, so the bytes are handed over
 * once it shows up and the request is dropped when the player backs out. */
static void pack_export_tick(void) {
    int kind = 0;
    int fd;
    if (pack_export_size <= 0)
        return;
    pack_export_wait = pack_export_wait + dt;
    fd = net_fs_take_fd(&kind);
    if (fd >= 0 && kind == 2) {
        if (net_fs_write_fd(fd, pack_export_zip, pack_export_size) == 1) {
            packs_msg_set(packs_tr("Пак сохранён", "Pack saved"));
            ds_log("texture pack exported to the picked document");
        } else {
            packs_msg_set(packs_tr("Не удалось записать .zip", "Could not write the .zip"));
        }
        net_fs_close_fd(fd);
        pack_export_size = 0;
        return;
    }
    if (fd >= 0)
        net_fs_close_fd(fd);
    if (pack_export_wait > 120.0) {
        /* Too long for a dialog that is still open: the player cancelled. */
        pack_export_size = 0;
    }
}

/* Save the archive: the system dialog on PC, the document picker on Android. */
static void pack_export_save(void) {
    int size = pack_export_build();
    if (size <= 0)
        return;
    pack_export_size = size;
    pack_export_wait = 0;
#if defined(__ANDROID__)
    if (net_fs_pick_export(PACK_EXPORT_FILE) != 1) {
        pack_export_size = 0;
        packs_msg_set(packs_tr("Системный выбор файла недоступен", "No system file picker here"));
        return;
    }
    packs_msg_set(packs_tr("Выбери, куда сохранить .zip", "Pick where to save the .zip"));
#else
    {
        char target[1024];
        if (ds_pick_save_file(PACK_EXPORT_FILE, target, (int)sizeof(target)) != 1) {
            pack_export_size = 0;
            return;
        }
        if (net_fs_write(target, pack_export_zip, size) == 1) {
            packs_msg_set(packs_tr("Пак сохранён", "Pack saved"));
            ds_log("texture pack exported: %s", target);
        } else {
            packs_msg_set(packs_tr("Не удалось записать .zip", "Could not write the .zip"));
        }
        pack_export_size = 0;
    }
#endif
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
