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
