// src/ide/ide_project.c
//
// Project folders: create, load, save, and the small amount of JSON the
// manifest needs. No dependency on a JSON library -- the format is a handful of
// known keys, so a focused reader keeps the build simple and the failure mode
// obvious (an unparsable manifest is reported, never silently overwritten).
#include "ide/ide_project.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#define ide_mkdir(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#define ide_mkdir(p) mkdir((p), 0777)
#endif

/* ---- filesystem helpers ------------------------------------------------- */

int ide_fs_mkdir(const char *path) {
    if (!path || !*path)
        return 0;
    ide_mkdir(path);
    return ide_fs_exists(path);
}

int ide_fs_exists(const char *path) {
    if (!path || !*path)
        return 0;
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat st;
    return stat(path, &st) == 0;
#endif
}

int ide_fs_read_all(const char *path, char **out, size_t *len) {
    FILE *f;
    long size;
    char *buf;
    size_t got;
    if (out) *out = NULL;
    if (len) *len = 0;
    if (!path || !out)
        return 0;
    f = fopen(path, "rb");
    if (!f)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    size = ftell(f);
    if (size < 0 || size > 64 * 1024 * 1024) { /* manifests and scripts only */
        fclose(f);
        return 0;
    }
    rewind(f);
    buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return 0;
    }
    got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) {
        free(buf);
        return 0;
    }
    buf[got] = '\0';
    *out = buf;
    if (len) *len = got;
    return 1;
}

int ide_fs_write_all(const char *path, const void *data, size_t len) {
    FILE *f;
    size_t put;
    if (!path || (!data && len))
        return 0;
    f = fopen(path, "wb");
    if (!f)
        return 0;
    put = len ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    return put == len;
}

/* ---- JSON, only as much as the manifest needs --------------------------- */

/* Copy the string that starts at `p` (just past an opening quote) into out,
 * handling the escapes the writer below can produce. Returns the position after
 * the closing quote, or NULL when unterminated. */
static const char *json_copy_string(const char *p, char *out, size_t cap) {
    size_t n = 0;
    if (!p || *p != '"')
        return NULL;
    p++;
    while (*p && *p != '"') {
        char ch = *p++;
        if (ch == '\\' && *p) {
            char esc = *p++;
            ch = (esc == 'n') ? '\n' : (esc == 't') ? '\t' : esc;
        }
        if (n + 1 < cap)
            out[n++] = ch;
    }
    if (*p != '"')
        return NULL;
    if (cap)
        out[n] = '\0';
    return p + 1;
}

/* Find "key" followed by a string value anywhere in `json`. */
static int json_get_string(const char *json, const char *key, char *out, size_t cap) {
    char pattern[96];
    const char *p;
    if (!json || !key)
        return 0;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p)
        return 0;
    p += strlen(pattern);
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    if (*p != ':')
        return 0;
    p++;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    return json_copy_string(p, out, cap) != NULL;
}

static void json_write_string(FILE *f, const char *s) {
    fputc('"', f);
    for (const char *p = s; p && *p; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '"' || ch == '\\')
            fprintf(f, "\\%c", ch);
        else if (ch == '\n')
            fputs("\\n", f);
        else if (ch < 0x20)
            fprintf(f, "\\u%04x", ch);
        else
            fputc(ch, f);
    }
    fputc('"', f);
}

/* ---- paths -------------------------------------------------------------- */

static void path_join(char *out, size_t cap, const char *dir, const char *rel) {
    if (!out || cap == 0)
        return;
    if (!dir)
        dir = "";
    if (!rel)
        rel = "";
    snprintf(out, cap, "%s/%s", dir, rel);
}

void ide_project_script_path(const IDEProject *p, char *out, size_t cap) {
    const char *rel = (p && p->script_count > 0) ? p->scripts[0].file : "scripts/main.ds";
    path_join(out, cap, p ? p->dir : "", rel);
}

void ide_project_sprite_path(const IDEProject *p, const char *file, char *out, size_t cap) {
    path_join(out, cap, p ? p->dir : "", file ? file : "");
}

/* ---- names -------------------------------------------------------------- */

/* Lowercase, ASCII, dashes for spaces: the name is used as a file name and as
 * an identifier in scripts, so it must not need quoting. */
static void slugify(const char *in, char *out, size_t cap) {
    size_t n = 0;
    if (!cap)
        return;
    if (!in || !*in) {
        snprintf(out, cap, "item");
        return;
    }
    for (const char *p = in; *p && n + 1 < cap; p++) {
        unsigned char ch = (unsigned char)*p;
        if (ch >= 'A' && ch <= 'Z')
            ch = (unsigned char)(ch - 'A' + 'a');
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_')
            out[n++] = (char)ch;
        else if (ch == ' ' || ch == '.')
            out[n++] = '_';
        /* Everything else (Cyrillic, punctuation) is dropped rather than
         * transliterated: the display name keeps the readable form. */
    }
    if (n == 0)
        snprintf(out, cap, "item");
    else
        out[n] = '\0';
}

static void unique_name(char *cand, size_t cap, const char *base, const char *const *taken, int taken_n) {
    for (int attempt = 1; attempt < 1000; attempt++) {
        int clash = 0;
        for (int i = 0; i < taken_n; i++) {
            if (taken[i] && strcmp(cand, taken[i]) == 0) {
                clash = 1;
                break;
            }
        }
        if (!clash)
            return;
        snprintf(cand, cap, "%s%d", base, attempt + 1);
    }
}

/* ---- create / open / save ---------------------------------------------- */

int ide_project_create(IDEProject *p, const char *dir, const char *name) {
    char scripts_dir[512], sprites_dir[512];
    char path[600], empty[1] = {0};
    if (!p || !dir || !*dir)
        return 0;
    memset(p, 0, sizeof(*p));
    snprintf(p->dir, sizeof(p->dir), "%s", dir);
    snprintf(p->name, sizeof(p->name), "%s", (name && *name) ? name : "Game");

    if (!ide_fs_mkdir(p->dir))
        return 0;
    path_join(scripts_dir, sizeof(scripts_dir), p->dir, "scripts");
    path_join(sprites_dir, sizeof(sprites_dir), p->dir, "sprites");
    ide_fs_mkdir(scripts_dir);
    ide_fs_mkdir(sprites_dir);

    snprintf(p->scripts[0].name, sizeof(p->scripts[0].name), "main");
    snprintf(p->scripts[0].file, sizeof(p->scripts[0].file), "scripts/main.ds");
    p->script_count = 1;

    path_join(path, sizeof(path), p->dir, p->scripts[0].file);
    if (!ide_fs_exists(path))
        ide_fs_write_all(path, empty, 0);

    return ide_project_save(p);
}

int ide_project_open(IDEProject *p, const char *dir) {
    char manifest[600];
    char *json = NULL;
    size_t len = 0;
    if (!p || !dir || !*dir)
        return 0;
    memset(p, 0, sizeof(*p));
    snprintf(p->dir, sizeof(p->dir), "%s", dir);

    /* Default name from the folder, so a manifest without one still labels the
     * project sensibly. */
    {
        const char *slash = strrchr(dir, '/');
#ifdef _WIN32
        const char *bslash = strrchr(dir, '\\');
        if (bslash && (!slash || bslash > slash))
            slash = bslash;
#endif
        snprintf(p->name, sizeof(p->name), "%s", slash ? slash + 1 : dir);
    }

    path_join(manifest, sizeof(manifest), p->dir, "project.json");
    if (!ide_fs_read_all(manifest, &json, &len))
        return 0;

    json_get_string(json, "name", p->name, sizeof(p->name));

    /* Walk the sprites array: each object has "name" and "file". */
    {
        const char *sp = strstr(json, "\"sprites\"");
        while (sp && p->sprite_count < IDE_PROJECT_MAX_SPRITES) {
            const char *obj = strchr(sp, '{');
            const char *end = strchr(sp, ']');
            char name[64], file[160];
            if (!obj || (end && obj > end))
                break;
            if (json_get_string(obj, "name", name, sizeof(name)) &&
                json_get_string(obj, "file", file, sizeof(file))) {
                snprintf(p->sprites[p->sprite_count].name,
                         sizeof(p->sprites[p->sprite_count].name), "%s", name);
                snprintf(p->sprites[p->sprite_count].file,
                         sizeof(p->sprites[p->sprite_count].file), "%s", file);
                p->sprite_count++;
            }
            sp = strchr(obj, '}');
            if (!sp)
                break;
            sp++;
        }
    }

    /* Scripts: the manifest stores the main script as a plain string. */
    if (json_get_string(json, "script", p->scripts[0].file, sizeof(p->scripts[0].file))) {
        snprintf(p->scripts[0].name, sizeof(p->scripts[0].name), "main");
        p->script_count = 1;
    } else {
        snprintf(p->scripts[0].name, sizeof(p->scripts[0].name), "main");
        snprintf(p->scripts[0].file, sizeof(p->scripts[0].file), "scripts/main.ds");
        p->script_count = 1;
    }
    free(json);
    return 1;
}

int ide_project_save(IDEProject *p) {
    char manifest[600];
    FILE *f;
    if (!p || !p->dir[0])
        return 0;
    path_join(manifest, sizeof(manifest), p->dir, "project.json");
    f = fopen(manifest, "wb");
    if (!f)
        return 0;
    fputs("{\n  \"version\": 1,\n  \"name\": ", f);
    json_write_string(f, p->name);
    fputs(",\n  \"script\": ", f);
    json_write_string(f, p->script_count > 0 ? p->scripts[0].file : "scripts/main.ds");
    fputs(",\n  \"sprites\": [", f);
    for (int i = 0; i < p->sprite_count; i++) {
        fputs(i ? ",\n    " : "\n    ", f);
        fputs("{\"name\": ", f);
        json_write_string(f, p->sprites[i].name);
        fputs(", \"file\": ", f);
        json_write_string(f, p->sprites[i].file);
        fputs("}", f);
    }
    fputs(p->sprite_count ? "\n  ]\n}\n" : "]\n}\n", f);
    fclose(f);
    return 1;
}

/* ---- sprites ----------------------------------------------------------- */

int ide_project_find_sprite(const IDEProject *p, const char *name) {
    if (!p || !name)
        return -1;
    for (int i = 0; i < p->sprite_count; i++)
        if (strcmp(p->sprites[i].name, name) == 0)
            return i;
    return -1;
}

static const char *extension_of(const char *path) {
    const char *dot = path ? strrchr(path, '.') : NULL;
    return (dot && dot[1]) ? dot : ".png";
}

int ide_project_add_sprite_bytes(IDEProject *p, const void *data, size_t len, const char *display_name,
                                 const char *ext) {
    char slug[64], cand[64], file[200], full[600];
    const char *taken[IDE_PROJECT_MAX_SPRITES];
    if (!p || !data || !len || p->sprite_count >= IDE_PROJECT_MAX_SPRITES)
        return 0;

    slugify(display_name, slug, sizeof(slug));
    for (int i = 0; i < p->sprite_count; i++)
        taken[i] = p->sprites[i].name;
    snprintf(cand, sizeof(cand), "%s", slug);
    unique_name(cand, sizeof(cand), slug, taken, p->sprite_count);

    snprintf(file, sizeof(file), "sprites/%s%s", cand, (ext && ext[0]) ? ext : ".png");
    path_join(full, sizeof(full), p->dir, file);
    if (!ide_fs_write_all(full, data, len))
        return 0;

    snprintf(p->sprites[p->sprite_count].name, sizeof(p->sprites[0].name), "%s", cand);
    snprintf(p->sprites[p->sprite_count].file, sizeof(p->sprites[0].file), "%s", file);
    p->sprite_count++;
    ide_project_save(p);
    return 1;
}

int ide_project_add_sprite_file(IDEProject *p, const char *source_path, const char *display_name) {
    char *data = NULL;
    size_t len = 0;
    int ok;
    if (!p || !source_path)
        return 0;
    /* An empty file is a read failure too, and a 0-byte sprite would be
     * registered and then never draw, which looks like the feature is broken. */
    if (!ide_fs_read_all(source_path, &data, &len) || len == 0) {
        free(data);
        return 0;
    }
    /* Keep the source extension: a photo saved as .jpg stays a .jpg so the
     * decoder is not asked to guess. */
    ok = ide_project_add_sprite_bytes(p, data, len, display_name, extension_of(source_path));
    free(data);
    return ok;
}

int ide_project_remove_sprite(IDEProject *p, const char *name) {
    int idx;
    char full[600];
    if (!p || !name)
        return 0;
    idx = ide_project_find_sprite(p, name);
    if (idx < 0)
        return 0;
    path_join(full, sizeof(full), p->dir, p->sprites[idx].file);
    remove(full);
    for (int i = idx; i + 1 < p->sprite_count; i++)
        p->sprites[i] = p->sprites[i + 1];
    p->sprite_count--;
    ide_project_save(p);
    return 1;
}

int ide_project_add_script(IDEProject *p, const char *name) {
    char slug[64], cand[64], file[200], full[600];
    const char *taken[IDE_PROJECT_MAX_SCRIPTS];
    if (!p || p->script_count >= IDE_PROJECT_MAX_SCRIPTS)
        return 0;
    slugify(name, slug, sizeof(slug));
    for (int i = 0; i < p->script_count; i++)
        taken[i] = p->scripts[i].name;
    snprintf(cand, sizeof(cand), "%s", slug);
    unique_name(cand, sizeof(cand), slug, taken, p->script_count);
    snprintf(file, sizeof(file), "scripts/%s.ds", cand);
    path_join(full, sizeof(full), p->dir, file);
    if (!ide_fs_exists(full))
        ide_fs_write_all(full, "", 0);
    snprintf(p->scripts[p->script_count].name, sizeof(p->scripts[0].name), "%s", cand);
    snprintf(p->scripts[p->script_count].file, sizeof(p->scripts[0].file), "%s", file);
    p->script_count++;
    ide_project_save(p);
    return 1;
}

/* Where projects live: next to the user's home directory when one is known,
 * so a project survives an app update, and the working directory otherwise. */
void ide_project_default_root(char *out, size_t cap) {
    char home[320] = {0};
    if (!out || !cap)
        return;
#ifdef _WIN32
    {
        const char *up = getenv("USERPROFILE");
        if (up && *up)
            snprintf(home, sizeof(home), "%s", up);
    }
#else
    {
        const char *h = getenv("HOME");
        if (h && *h)
            snprintf(home, sizeof(home), "%s", h);
    }
#endif
    if (home[0])
        snprintf(out, cap, "%s/DiScriptProjects", home);
    else
        snprintf(out, cap, "DiScriptProjects");
}
