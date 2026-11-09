/* Texture pack files: <pack>/<asset name> wins over the packaged asset. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Active texture pack folder: its files replace packaged assets by name. */
static char asset_pack_dir[320];

void ds_assets_pack_dir(const char *dir) {
    if (!dir || !*dir)
        asset_pack_dir[0] = 0;
    else
        snprintf(asset_pack_dir, sizeof(asset_pack_dir), "%s", dir);
}

static int read_file_bytes(const char *path, uint8_t **out, size_t *sz) {
    FILE *f = fopen(path, "rb");
    long len;
    uint8_t *buf;
    if (!f)
        return 0;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    len = ftell(f);
    if (len <= 0) {
        fclose(f);
        return 0;
    }
    rewind(f);
    buf = (uint8_t *)malloc((size_t)len);
    if (!buf || fread(buf, 1, (size_t)len, f) != (size_t)len) {
        free(buf);
        fclose(f);
        return 0;
    }
    fclose(f);
    *out = buf;
    *sz = (size_t)len;
    return 1;
}

static const char *norm_name(const char *n) {
    if (!n)
        return NULL;
    while (strncmp(n, "./", 2) == 0) n += 2;
    if (strncmp(n, "assets/textures/", 16) == 0)
        n += 16;
    else if (strncmp(n, "textures/", 9) == 0)
        n += 9;
    else if (strncmp(n, "game/assets/", 12) == 0)
        n += 12;
    else if (strncmp(n, "assets/", 7) == 0)
        n += 7;
    if (!*n || *n == '/' || strchr(n, '\\'))
        return NULL;
    for (const char *c = n; *c;) {
        const char *s = c;
        while (*c && *c != '/') c++;
        if (c - s == 2 && s[0] == '.' && s[1] == '.')
            return NULL;
        if (*c == '/')
            c++;
    }
    return n;
}
/* Pack file wins over the packaged asset, full relative path first, then bare name. */
static int open_pack_file(const char *n, uint8_t **out, size_t *sz) {
    char path[512];
    const char *slash;
    if (!asset_pack_dir[0] || !n || !*n)
        return 0;
    if (strlen(asset_pack_dir) + strlen(n) + 2 < sizeof(path)) {
        snprintf(path, sizeof(path), "%s/%s", asset_pack_dir, n);
        if (read_file_bytes(path, out, sz)) {
            ds_log("pack asset: %s", n);
            return 1;
        }
    }
    slash = strrchr(n, '/');
    if (!slash || !slash[1])
        return 0;
    snprintf(path, sizeof(path), "%s/%s", asset_pack_dir, slash + 1);
    return read_file_bytes(path, out, sz);
}

/* List the files of one asset folder, newline separated. Both platforms have
 * AAssetManager_openDir, so the texture pack exporter needs no per-platform
 * code; subfolders are not listed, and every asset the game packs by default
 * (textures, gift stickers) sits directly in the asset root. */
int ds_asset_list(const char *dir, char *out, int cap) {
    AAssetDir *d;
    const char *name;
    int count = 0;
    int used = 0;
    if (!amgr || !out || cap <= 0)
        return 0;
    out[0] = 0;
    d = AAssetManager_openDir(amgr, dir ? dir : "");
    if (!d)
        return 0;
    while ((name = AAssetDir_getNextFileName(d)) != NULL) {
        int len = (int)strlen(name);
        if (len <= 0 || name[0] == '.')
            continue;
        if (used + len + 5 >= cap)
            break;
        memcpy(out + used, "f|", 2);
        memcpy(out + used + 2, name, (size_t)len);
        out[used + 2 + len] = '|';
        out[used + 3 + len] = '\n';
        out[used + 4 + len] = '\0';
        used = used + len + 4;
        count = count + 1;
    }
    AAssetDir_close(d);
    return count;
}

/* The exporter reads whole files, exactly like the texture loader does. */
int ds_asset_extract(const char *name, void *buf, int cap) { return ds_asset_read(name, buf, cap); }

static int open_asset(const char *n, uint8_t **out, size_t *sz) {
    if (!n || !out || !sz)
        return 0;
    if (open_pack_file(n, out, sz))
        return 1;
    if (!amgr)
        return 0;
    AAsset *a = AAssetManager_open(amgr, n, AASSET_MODE_BUFFER);
    if (!a)
        return 0;
    off_t len = AAsset_getLength(a);
    if (len <= 0 || (uint64_t)len > SIZE_MAX) {
        AAsset_close(a);
        return 0;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)len);
    if (!buf) {
        AAsset_close(a);
        return 0;
    }
    size_t off = 0;
    while (off < (size_t)len) {
        int nr = AAsset_read(a, buf + off, (size_t)len - off);
        if (nr <= 0)
            break;
        off += nr;
    }
    AAsset_close(a);
    if (off != (size_t)len) {
        free(buf);
        return 0;
    }
    *out = buf;
    *sz = (size_t)len;
    return 1;
}

int ds_asset_size(const char *name) {
    uint8_t *data = NULL;
    size_t sz = 0;
    int size;
    if (!open_asset(name, &data, &sz))
        return -1;
    size = (int)sz;
    free(data);
    return size;
}

int ds_asset_read(const char *name, void *buf, int cap) {
    uint8_t *enc = NULL;
    size_t sz = 0;
    int copied;
    if (!buf || cap <= 0)
        return -1;
    if (!open_asset(name, &enc, &sz) || sz > (size_t)cap) {
        free(enc);
        return -1;
    }
    memcpy(buf, enc, sz);
    copied = (int)sz;
    free(enc);
    return copied;
}

static Texture *find_tx(const char *n) {
    for (Texture *t = textures; t; t = t->next)
        if (strcmp(t->name, n) == 0)
            return t;
    return NULL;
}

/* Decode a PNG from the asset chain into a texture record. */
static Texture *load_png(const char *req) {
    const char *n = norm_name(req);
    if (!n) {
        ds_log_err("texture not loaded: invalid PNG asset path '%s'", req ? req : "(null)");
        return NULL;
    }
    Texture *t = find_tx(n);
    if (t && t->pixels) {

        if (!t->gpu.uploaded)
            ds_vk_pending_texture(t);
        return t;
    }
    if (!amgr) {
        ds_log_err("texture not loaded: no asset manager for '%s'", n);
        return NULL;
    }
    if (!t) {

        t = (Texture *)calloc(1, sizeof(*t));
        if (!t) {
            ds_log_err("texture not loaded: out of memory caching '%s'", n);
            return NULL;
        }
        t->name = strdup_safe(n);
        if (!t->name) {
            free(t);
            ds_log_err("texture not loaded: out of memory caching name '%s'", n);
            return NULL;
        }
        t->next = textures;
        textures = t;
    }

    uint8_t *enc = NULL;
    size_t enc_sz = 0;
    if (!open_asset(n, &enc, &enc_sz) || enc_sz > (size_t)INT_MAX) {
        free(enc);
        ds_log_err("texture not loaded: asset not found or unreadable: '%s'", n);
        return NULL;
    }
    int ch = 0;
    stbi_uc *dec = stbi_load_from_memory(enc, (int)enc_sz, &t->w, &t->h, &ch, STBI_rgb_alpha);
    free(enc);
    if (!dec || t->w <= 0 || t->h <= 0) {
        ds_log_err("texture not loaded: could not decode PNG '%s': %s", n,
                   stbi_failure_reason() ? stbi_failure_reason() : "unknown");
        stbi_image_free(dec);
        return NULL;
    }
    if ((size_t)t->w > SIZE_MAX / (size_t)t->h ||
        (size_t)t->w * (size_t)t->h > SIZE_MAX / sizeof(*t->pixels)) {
        stbi_image_free(dec);
        ds_log_err("texture not loaded: dimensions overflow for PNG '%s'", n);
        t->w = t->h = 0;
        return NULL;
    }
    size_t pc = (size_t)t->w * (size_t)t->h;
    t->pixels = (uint32_t *)malloc(pc * sizeof(*t->pixels));
    if (!t->pixels) {
        stbi_image_free(dec);
        ds_log_err("texture not loaded: out of memory uploading PNG '%s'", n);
        return NULL;
    }
    t->opaque = 1;
    for (size_t i = 0; i < pc; i++) {
        uint8_t a = dec[i * 4 + 3];
        t->pixels[i] =
            dec[i * 4] | ((uint32_t)dec[i * 4 + 1] << 8) | ((uint32_t)dec[i * 4 + 2] << 16) | ((uint32_t)a << 24);
        if (a != 255)
            t->opaque = 0;
    }
    stbi_image_free(dec);
    /* Apply grayscale filter if active. */
    texture_gray_apply(t);
    ds_vk_pending_texture(t);
    ds_log("texture loaded: %s (%dx%d)", n, t->w, t->h);
    return t;
}
/* Drop a mod texture override and load the packaged asset again. */
